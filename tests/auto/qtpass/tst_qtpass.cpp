// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QtTest>

#include "../../../src/pass.h"
#include "../../../src/qtpass.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/realpass.h"
#include "../../../src/settingsconstants.h"
#include "../../../src/util.h"
#include "../testsettings.h"

/**
 * @brief QtPass without a window: the backends' completion signals come out
 *        as formatted output, status text and "finished" signals, and init()
 *        migrates the settings of older versions. Both backends are wired,
 *        so the tests emit from whichever is convenient.
 */
class tst_qtpass : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { isolateTestSettings(); }
  void init();

  void formatOutputEscapesLinksAndBreaksLines();
  void formatOutputKeepsWhiteSpace();
  void formatOutputFoldsCarriageReturns();
  void formatOutputCutsRunsOfSpacesQtCannotLayOut();
  void failedProcessShowsRedErrorAndFinishes();
  void successfulProcessShowsGreyChatterOnce();
  void silentFailureOnlyFinishes();
  void finishedProcessShowsOutputAndChatter();
  void storeChangePushesOnlyWhenAutoPushIsOn();
  void heldStoreIsNotPushedAutomatically();
  void insertReselectsTheEntry();
  void keyGenerationReportsSuccess();
  void bothBackendsAreListenedTo();
  void initSetsFreshInstallDefaults();
  void initTurnsNativeOtpOnOnceForOlderProfiles();
};

void tst_qtpass::init() {
  QtPassSettings::getInstance()->clear();
  QtPassSettings::getInstance()->sync();
}

void tst_qtpass::formatOutputEscapesLinksAndBreaksLines() {
  const QString html =
      QtPass::formatOutput(QStringLiteral("see https://qtpass.org <b>\nnext"),
                           QStringLiteral("["), QStringLiteral("]"));
  QVERIFY(html.startsWith('['));
  QVERIFY(html.endsWith(']'));
  QVERIFY2(html.contains(QStringLiteral("<a href=\"https://qtpass.org\"")),
           qPrintable(html));
  QVERIFY2(html.contains(QStringLiteral("&lt;b&gt;")), qPrintable(html));
  QVERIFY2(html.contains(QStringLiteral("<br />next")), qPrintable(html));
  QVERIFY(!html.contains('\n'));
}

/**
 * @brief Rich text collapses white space: "a  b" was shown, and Select All +
 *        Copy handed back, "a b", indentation was dropped and a tab became a
 *        space (#210). What formatOutput() returns reads back as the text it
 *        was given, also after more output is appended through toHtml(), as
 *        MainWindow::flashText() appends it.
 */
void tst_qtpass::formatOutputKeepsWhiteSpace() {
  const QString text =
      QStringLiteral("ridge  velvet harbor\n    indented\ntab\there\n"
                     "trailing   \nsee https://qtpass.org/a  b");
  QTextDocument document;
  document.setHtml(QtPass::formatOutput(text));
  QCOMPARE(document.toPlainText(), text);

  const QString chatter =
      QtPass::formatOutput(QStringLiteral(" 2 files changed,  3 insertions"),
                           QStringLiteral("<span style=\"color: darkgray;\">"),
                           QStringLiteral("</span><br />"));
  document.setHtml(document.toHtml() + chatter);
  QCOMPARE(document.toPlainText(),
           text + QStringLiteral("\n 2 files changed,  3 insertions\n"));
}

/**
 * @brief With white space kept, a "\r\n" is still one line end, and a lone
 *        "\r" stays a space inside its line: as a block of its own, an entry
 *        of carriage returns would slip past the display cap, which counts
 *        "\n" only.
 */
void tst_qtpass::formatOutputFoldsCarriageReturns() {
  QTextDocument document;
  document.setHtml(QtPass::formatOutput(QStringLiteral("one  1\r\ntwo\r\n")));
  QCOMPARE(document.toPlainText(), QStringLiteral("one  1\ntwo\n"));

  document.setHtml(QtPass::formatOutput(QStringLiteral("a\rb").repeated(1000)));
  QCOMPARE(document.blockCount(), 1);
  QCOMPARE(document.toPlainText(), QStringLiteral("a b").repeated(1000));
}

/**
 * @brief With white space kept, Qt's line layout never finishes a wrapped
 *        line with 65536 spaces in a row: one crafted entry froze the window
 *        for good. The run is cut first, so the text lays out.
 */
void tst_qtpass::formatOutputCutsRunsOfSpacesQtCannotLayOut() {
  QTextDocument document;
  document.setHtml(QtPass::formatOutput(QStringLiteral("a") +
                                        QString(65536, QLatin1Char(' ')) +
                                        QStringLiteral("b")));
  // Compared before the layout runs, so a regression fails here, not hangs.
  QCOMPARE(document.toPlainText(),
           QStringLiteral("a") + QString(Util::kMaxSpaceRun, QLatin1Char(' ')) +
               QStringLiteral("b"));
  document.setTextWidth(600);
  QVERIFY(document.documentLayout()->documentSize().height() > 0);
}

void tst_qtpass::failedProcessShowsRedErrorAndFinishes() {
  QtPass qtPass;
  Pass *real = QtPassSettings::getRealPass();
  QSignalSpy output(&qtPass, &QtPass::outputReady);
  QSignalSpy finished(&qtPass, &QtPass::operationFinished);

  emit real->processErrorExit(
      1, QStringLiteral("gpg: decryption failed\nNo secret key"));

  QCOMPARE(output.count(), 1);
  const QString html = output.first().first().toString();
  QVERIFY2(html.startsWith(QStringLiteral("<span style=\"color: red;\">")),
           qPrintable(html));
  QVERIFY2(html.contains(QStringLiteral("decryption failed<br />No secret")),
           qPrintable(html));
  QVERIFY(html.endsWith(QStringLiteral("</span><br />")));
  QCOMPARE(finished.count(), 1);
}

void tst_qtpass::successfulProcessShowsGreyChatterOnce() {
  QtPass qtPass;
  Pass *real = QtPassSettings::getRealPass();
  QSignalSpy output(&qtPass, &QtPass::outputReady);
  QSignalSpy finished(&qtPass, &QtPass::operationFinished);

  emit real->processErrorExit(0, QStringLiteral("Already up to date."));

  QCOMPARE(output.count(), 1);
  QVERIFY(output.first().first().toString().startsWith(
      QStringLiteral("<span style=\"color: darkgray;\">")));
  QCOMPARE(finished.count(), 1);
}

void tst_qtpass::silentFailureOnlyFinishes() {
  QtPass qtPass;
  Pass *real = QtPassSettings::getRealPass();
  QSignalSpy output(&qtPass, &QtPass::outputReady);
  QSignalSpy finished(&qtPass, &QtPass::operationFinished);

  emit real->processErrorExit(2, QString());

  QCOMPARE(output.count(), 0);
  QCOMPARE(finished.count(), 1);
}

void tst_qtpass::finishedProcessShowsOutputAndChatter() {
  QtPass qtPass;
  Pass *real = QtPassSettings::getRealPass();
  QSignalSpy output(&qtPass, &QtPass::outputReady);
  QSignalSpy finished(&qtPass, &QtPass::operationFinished);

  emit real->finishedGitPull(QStringLiteral("Updating 1..2"),
                             QStringLiteral("From origin"));

  QCOMPARE(output.count(), 2);
  QCOMPARE(output.at(0).first().toString(),
           QtPass::formatOutput(QStringLiteral("Updating 1..2")));
  QCOMPARE(
      output.at(1).first().toString(),
      QtPass::formatOutput(QStringLiteral("From origin"),
                           QStringLiteral("<span style=\"color: darkgray;\">"),
                           QStringLiteral("</span><br />")));
  // One operation, one re-enable, however many pieces of output it had.
  QCOMPARE(finished.count(), 1);
}

void tst_qtpass::storeChangePushesOnlyWhenAutoPushIsOn() {
  QtPass qtPass;
  Pass *real = QtPassSettings::getRealPass();
  QSignalSpy push(&qtPass, &QtPass::pushRequested);
  QSignalSpy finished(&qtPass, &QtPass::operationFinished);

  emit real->finishedRemove(QString(), QString());
  QCOMPARE(push.count(), 0);
  QCOMPARE(finished.count(), 1);

  AppSettings s = QtPassSettings::load();
  s.autoPush = true;
  QtPassSettings::save(s);
  emit real->finishedMove(QString(), QString());
  emit real->finishedCopy(QString(), QString());
  emit real->finishedGitInit(QString(), QString());
  emit real->finishedInit(QString(), QString());
  QCOMPARE(push.count(), 4);
  QCOMPARE(finished.count(), 5);
}

namespace {
/// Reaches Pass's protected hold controls on any backend object (named
/// through a derived class, a member pointer applies to the base).
struct HoldOpener : RealPass {
  static void hold(Pass *pass) { (pass->*(&HoldOpener::holdAutoPush))(); }
  static void release(Pass *pass) { (pass->*(&HoldOpener::releaseAutoPush))(); }
};
} // namespace

/**
 * @brief While the store's automatic pushes are held (a re-encryption did
 *        not finish), an operation's automatic push is not requested and
 *        the user is told why; released, it is requested again.
 */
void tst_qtpass::heldStoreIsNotPushedAutomatically() {
  QtPass qtPass;
  Pass *real = QtPassSettings::getRealPass();
  QSignalSpy push(&qtPass, &QtPass::pushRequested);
  QSignalSpy status(&qtPass, &QtPass::statusMessage);
  const AppSettings saved = QtPassSettings::load();
  const auto restore = qScopeGuard([&saved] { QtPassSettings::save(saved); });
  QTemporaryDir store;
  QVERIFY(store.isValid());
  AppSettings s = saved;
  s.passStore = store.path();
  s.autoPush = true;
  s.usePass = true;
  QtPassSettings::save(s);
  QCOMPARE(QtPassSettings::getPass(), real);
  real->init(QtPassSettings::load());
  HoldOpener::hold(real);
  const auto release = qScopeGuard([real] { HoldOpener::release(real); });
  QVERIFY(real->autoPushHeld());

  emit real->finishedInsert(QString(), QString());
  emit real->finishedRemove(QString(), QString());
  QCOMPARE(push.count(), 0);
  QVERIFY(!status.isEmpty());
  QVERIFY2(status.last().at(0).toString().contains(
               QStringLiteral("Not pushing automatically")),
           qPrintable(status.last().at(0).toString()));

  HoldOpener::release(real);
  emit real->finishedRemove(QString(), QString());
  QCOMPARE(push.count(), 1);
}

void tst_qtpass::insertReselectsTheEntry() {
  QtPass qtPass;
  Pass *real = QtPassSettings::getRealPass();
  QSignalSpy inserted(&qtPass, &QtPass::entryInserted);
  QSignalSpy push(&qtPass, &QtPass::pushRequested);
  QSignalSpy finished(&qtPass, &QtPass::operationFinished);
  QSignalSpy output(&qtPass, &QtPass::outputReady);

  AppSettings s = QtPassSettings::load();
  s.autoPush = true;
  QtPassSettings::save(s);
  emit real->finishedInsert(QString(), QString());

  QCOMPARE(inserted.count(), 1);
  QCOMPARE(push.count(), 1);
  QCOMPARE(finished.count(), 1);
  QCOMPARE(output.count(), 0); // nothing printed, nothing to show
}

void tst_qtpass::keyGenerationReportsSuccess() {
  QtPass qtPass;
  Pass *real = QtPassSettings::getRealPass();
  QSignalSpy status(&qtPass, &QtPass::statusMessage);
  QSignalSpy output(&qtPass, &QtPass::outputReady);
  QSignalSpy finished(&qtPass, &QtPass::operationFinished);

  emit real->finishedGenerateGPGKeys(
      QStringLiteral("gpg: key ABCD marked as ultimately trusted"), QString());

  QCOMPARE(status.count(), 1);
  QVERIFY(status.first().first().toString().contains(
      QStringLiteral("generated successfully")));
  QCOMPARE(status.first().at(1).toInt(), 10000);
  QCOMPARE(output.count(), 1);
  QCOMPARE(finished.count(), 1);
}

void tst_qtpass::bothBackendsAreListenedTo() {
  QtPass qtPass;
  Pass *real = QtPassSettings::getRealPass();
  Pass *imitate = QtPassSettings::getImitatePass();
  QSignalSpy finished(&qtPass, &QtPass::operationFinished);

  emit imitate->processErrorExit(1, QString());
  emit real->processErrorExit(1, QString());

  QCOMPARE(finished.count(), 2);
}

void tst_qtpass::initSetsFreshInstallDefaults() {
  QVERIFY(QtPassSettings::getVersion().isEmpty());
  QTemporaryDir store;
  AppSettings s = QtPassSettings::load();
  s.passStore = store.path();
  s.autoclearSeconds = 1;
  s.autoclearPanelSeconds = 2;
  s.pwgenExecutable = QStringLiteral("/usr/bin/pwgen");
  s.passTemplate.clear();
  QtPassSettings::save(s);

  QtPass qtPass;
  qtPass.init();

  const AppSettings after = QtPassSettings::load();
  QCOMPARE(QtPassSettings::getVersion(), QString::fromLatin1(VERSION));
  QCOMPARE(after.autoclearSeconds, 10);
  QCOMPARE(after.autoclearPanelSeconds, 10);
  QVERIFY(after.usePwgen);
  QCOMPARE(after.passTemplate, QStringLiteral("login\nurl\nOTP"));
  QVERIFY(QtPassSettings::getInstance()
              ->value(SettingsConstants::otpMigratedToNative, false)
              .toBool());
}

void tst_qtpass::initTurnsNativeOtpOnOnceForOlderProfiles() {
  QTemporaryDir store;
  QtPassSettings::setVersion(QStringLiteral("1.7.0"));
  AppSettings s = QtPassSettings::load();
  s.passStore = store.path();
  s.useOtp = false;
  s.autoclearSeconds = 3; // an existing profile's choices are kept
  QtPassSettings::save(s);

  {
    QtPass qtPass;
    qtPass.init();
  }
  AppSettings after = QtPassSettings::load();
  QVERIFY2(after.useOtp, "the stale pass-otp opt-out is lifted once");
  QCOMPARE(after.autoclearSeconds, 3);
  QCOMPARE(QtPassSettings::getVersion(), QString::fromLatin1(VERSION));

  // A deliberate opt-out after the migration survives the next start.
  after.useOtp = false;
  QtPassSettings::save(after);
  {
    QtPass qtPass;
    qtPass.init();
  }
  QVERIFY(!QtPassSettings::load().useOtp);
}

QTEST_MAIN(tst_qtpass)
#include "tst_qtpass.moc"
