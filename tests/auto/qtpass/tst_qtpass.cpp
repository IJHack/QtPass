// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QSignalSpy>
#include <QtTest>

#include "../../../src/pass.h"
#include "../../../src/qtpass.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/settingsconstants.h"
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
  void failedProcessShowsRedErrorAndFinishes();
  void successfulProcessShowsGreyChatterOnce();
  void silentFailureOnlyFinishes();
  void finishedProcessShowsOutputAndChatter();
  void storeChangePushesOnlyWhenAutoPushIsOn();
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
  QCOMPARE(output.at(0).first().toString(), QStringLiteral("Updating 1..2"));
  QVERIFY(output.at(1).first().toString().contains(
      QStringLiteral("darkgray;\">From origin")));
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
