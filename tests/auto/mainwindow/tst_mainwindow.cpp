// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @brief Widget tests for MainWindow.
 *
 * These tests exercise the public surface of MainWindow in isolation.
 * Each test gets a fresh window instance; a single QTemporaryDir serves as
 * the pass store for the whole run.
 *
 * Coverage deferred here (requires live GPG / pass / git operations):
 * - addPassword / addFolder / renameFolder / renamePassword — invoke Pass
 * - on_treeView_clicked / doubleClicked — need a populated store model
 * - onGrepFinished — depends on a running grep
 * - generateKeyPair — spawns gpg key generation
 */

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QPalette>
#include <QScopedPointer>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeView>
#include <QtTest>

#include "../../../src/filecontent.h"
#include "../../../src/mainwindow.h"
#include "../../../src/passworddisplaypanel.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/util.h"
#include "../testsettings.h"

class tst_mainwindow : public QObject {
  Q_OBJECT

  QTemporaryDir m_storeDir;
  QScopedPointer<MainWindow> m_window;
  QString m_gpgPath;

private Q_SLOTS:
  void initTestCase();
  void init();
  void cleanup();
  void cleanupTestCase();

  void constructionDoesNotCrash();
  void getKeygenDialogInitiallyNull();
  void cleanKeygenDialogWithNullIsHarmless();
  void setUiElementsEnabledDisablesTreeView();
  void setUiElementsEnabledEnablesTreeView();
  void flashTextSetsContent();
  void flashTextErrorDoesNotCrash();
  void flashTextHtmlRenderedInBrowser();
  void showStatusMessageAppearsInStatusBar();
  void deselectDoesNotCrash();
  void onProcessOutputAppendsToPanel();
  void onProcessOutputSkippedWhenPanelHidden();
  void passwordFromFileToClipboardCopiesFirstLine();
  void passwordFromFileToClipboardSkipsOtpSecret();
  void showTextAsQRCodeReportsMissingQrencode();
  void textBrowserFollowsRuntimePaletteChange();
  void fieldFrameBorderFollowsRuntimePaletteChange();
  void toolBarDropsStaleStylePaletteAfterThemeSwitch();
  void toolBarKeepsHeaderTintInSameTheme();
};

void tst_mainwindow::initTestCase() {
  isolateTestSettings();
  QVERIFY2(m_storeDir.isValid(), "temp store dir must be created");

  // Minimal valid pass store: just a .gpg-id file
  QFile gpgId(QDir::cleanPath(
      QDir(m_storeDir.path()).filePath(QStringLiteral(".gpg-id"))));
  QVERIFY2(gpgId.open(QIODevice::WriteOnly), ".gpg-id must be writable");
  gpgId.write("0000000000000000\n");
  gpgId.close();

  // Point QtPassSettings at the temp store and use gpg (not pass) mode so
  // configIsValid() only requires the .gpg-id file + a gpg binary.
  QtPassSettings::setPassStore(QDir::cleanPath(m_storeDir.path()));
  QtPassSettings::setUsePass(false);

  // Verify gpg is reachable. We also pre-set the executable path so that
  // QtPass::init() → initExecutables() finds it even when only "gpg" (not
  // "gpg2") exists in PATH (e.g. Ubuntu CI where gpg2 is not a separate
  // binary). Without this, initExecutables() leaves the path empty,
  // configIsValid() returns false, and config() opens a blocking modal
  // dialog that times out the test after 300 s.
  m_gpgPath = Util::findBinaryInPath(QStringLiteral("gpg2"));
  if (m_gpgPath.isEmpty())
    m_gpgPath = Util::findBinaryInPath(QStringLiteral("gpg"));
  if (m_gpgPath.isEmpty())
    QSKIP("gpg not available — skipping MainWindow construction tests");
  {
    AppSettings s = QtPassSettings::load();
    s.gpgExecutable = m_gpgPath;
    QtPassSettings::save(s);
  }
}

void tst_mainwindow::init() {
  // Re-apply store settings in case a previous test modified them.
  QtPassSettings::setPassStore(QDir::cleanPath(m_storeDir.path()));
  QtPassSettings::setUsePass(false);
  {
    AppSettings s = QtPassSettings::load();
    s.showProcessOutput = true;
    QtPassSettings::save(s);
  }
  // Re-apply gpg path: initExecutables() inside the constructor overwrites
  // the setting to findBinaryInPath("gpg2"), which is empty on systems where
  // only "gpg" exists. Setting it here ensures configIsValid() sees a valid
  // executable and does not fall back to the blocking config() dialog.
  {
    AppSettings s = QtPassSettings::load();
    s.gpgExecutable = m_gpgPath;
    QtPassSettings::save(s);
  }
  m_window.reset(new MainWindow);
}

void tst_mainwindow::cleanup() { m_window.reset(); }

void tst_mainwindow::cleanupTestCase() {
  // Settings live in the isolated directory from isolateTestSettings();
  // nothing to restore.
}

// ---------------------------------------------------------------------------

/**
 * @brief Constructor completes without crashing when a valid store exists.
 */
void tst_mainwindow::constructionDoesNotCrash() {
  // init() already constructed the window; reaching this line is the test.
  QVERIFY2(m_window != nullptr, "MainWindow must have been constructed");
}

/**
 * @brief getKeyGenDialog() returns nullptr before any keygen is started.
 */
void tst_mainwindow::getKeygenDialogInitiallyNull() {
  QCOMPARE(m_window->getKeyGenDialog(), nullptr);
}

/**
 * @brief cleanKeygenDialog() is a no-op (and harmless) when no dialog exists.
 */
void tst_mainwindow::cleanKeygenDialogWithNullIsHarmless() {
  QCOMPARE(m_window->getKeyGenDialog(), nullptr);
  m_window->cleanKeygenDialog();
  QCOMPARE(m_window->getKeyGenDialog(), nullptr);
}

/**
 * @brief setUiElementsEnabled(false) disables the tree view and search field.
 */
void tst_mainwindow::setUiElementsEnabledDisablesTreeView() {
  auto *treeView = m_window->findChild<QTreeView *>(QStringLiteral("treeView"));
  QVERIFY2(treeView != nullptr, "treeView widget must exist");

  m_window->setUiElementsEnabled(false);
  QVERIFY2(!treeView->isEnabled(), "treeView must be disabled");
}

/**
 * @brief setUiElementsEnabled(true) re-enables the tree view.
 */
void tst_mainwindow::setUiElementsEnabledEnablesTreeView() {
  auto *treeView = m_window->findChild<QTreeView *>(QStringLiteral("treeView"));
  QVERIFY2(treeView != nullptr, "treeView widget must exist");

  m_window->setUiElementsEnabled(false);
  m_window->setUiElementsEnabled(true);
  QVERIFY2(treeView->isEnabled(), "treeView must be re-enabled");
}

/**
 * @brief flashText() sets the text browser content.
 */
void tst_mainwindow::flashTextSetsContent() {
  auto *browser =
      m_window->findChild<QTextBrowser *>(QStringLiteral("textBrowser"));
  QVERIFY2(browser != nullptr, "textBrowser must exist");

  m_window->flashText(QStringLiteral("hello mainwindow test"), false);
  QVERIFY2(
      browser->toPlainText().contains(QStringLiteral("hello mainwindow test")),
      "textBrowser must contain the flashed text");
}

/**
 * @brief flashText() with isError=true does not crash.
 */
void tst_mainwindow::flashTextErrorDoesNotCrash() {
  auto *browser =
      m_window->findChild<QTextBrowser *>(QStringLiteral("textBrowser"));
  QVERIFY2(browser != nullptr, "textBrowser must exist");

  m_window->flashText(QStringLiteral("error text"), true);
  QVERIFY2(browser->toPlainText().contains(QStringLiteral("error text")),
           "textBrowser must contain the error text");
}

/**
 * @brief flashText() with isHtml=true renders HTML into the browser.
 */
void tst_mainwindow::flashTextHtmlRenderedInBrowser() {
  auto *browser =
      m_window->findChild<QTextBrowser *>(QStringLiteral("textBrowser"));
  QVERIFY2(browser != nullptr, "textBrowser must exist");

  m_window->flashText(QStringLiteral("<b>bold</b>"), false, true);
  // Qt's rich text engine converts <b> to font-weight CSS internally, so
  // toHtml() never emits literal <b> tags. Verify the text content appears
  // and that the HTML was rendered (not escaped as &lt;b&gt;).
  QVERIFY2(browser->toHtml().contains(QStringLiteral("bold")),
           "flashText with isHtml=true must set content in textBrowser");
  QVERIFY2(
      !browser->toHtml().contains(QStringLiteral("&lt;b&gt;bold&lt;/b&gt;")),
      "flashText with isHtml=true must not escape HTML tags");
}

/**
 * @brief showStatusMessage() writes to the status bar.
 */
void tst_mainwindow::showStatusMessageAppearsInStatusBar() {
  m_window->showStatusMessage(QStringLiteral("status test"), 60000);
  QCOMPARE(m_window->statusBar()->currentMessage(),
           QStringLiteral("status test"));
}

/**
 * @brief deselect() does not crash when nothing is selected.
 */
void tst_mainwindow::deselectDoesNotCrash() {
  m_window->deselect();
  // No assertion needed — reaching this line means no crash / assert fired.
}

/**
 * @brief onProcessOutput() appends text to the process output panel when the
 *        panel is visible.
 */
void tst_mainwindow::onProcessOutputAppendsToPanel() {
  auto *outputEdit =
      m_window->findChild<QTextEdit *>(QStringLiteral("processOutputEdit"));
  QVERIFY2(outputEdit != nullptr, "processOutputEdit must exist");

  m_window->onProcessOutput(QStringLiteral("process output line"), false);
  QVERIFY2(
      outputEdit->toPlainText().contains(QStringLiteral("process output line")),
      "processOutputEdit must contain the appended line");
}

/**
 * @brief onProcessOutput() does nothing when the panel is hidden
 *        (isShowProcessOutput == false).
 */
void tst_mainwindow::onProcessOutputSkippedWhenPanelHidden() {
  AppSettings s = QtPassSettings::load();
  s.showProcessOutput = false;
  QtPassSettings::save(s);

  auto *outputEdit =
      m_window->findChild<QTextEdit *>(QStringLiteral("processOutputEdit"));
  QVERIFY2(outputEdit != nullptr, "processOutputEdit must exist");

  const QString before = outputEdit->toPlainText();
  m_window->onProcessOutput(QStringLiteral("should not appear"), false);
  QCOMPARE(outputEdit->toPlainText(), before);
}

/**
 * @brief passwordFromFileToClipboard() copies the first line of a normal entry.
 */
void tst_mainwindow::passwordFromFileToClipboardCopiesFirstLine() {
  AppSettings s = QtPassSettings::load();
  s.useSelection = false;
  s.useAutoclear = false;
  QtPassSettings::save(s);

  QClipboard *clip = QApplication::clipboard();
  clip->setText(QStringLiteral("sentinel"));

  QVERIFY2(QMetaObject::invokeMethod(
               m_window.data(), "passwordFromFileToClipboard",
               Qt::DirectConnection,
               Q_ARG(QString, QStringLiteral("hunter2\nlogin: alice"))),
           "invoking passwordFromFileToClipboard must succeed");

  QCOMPARE(clip->text(), QStringLiteral("hunter2"));
}

/**
 * @brief passwordFromFileToClipboard() never copies an otpauth:// shared
 * secret.
 *
 * An entry created by `pass otp insert` has the otpauth URI as its first line;
 * copying it would leak the TOTP seed. Regression guard for the Ctrl+C path,
 * analogous to tst_passworddisplaypanel::otpUriAsPasswordIsNeverRendered.
 */
void tst_mainwindow::passwordFromFileToClipboardSkipsOtpSecret() {
  AppSettings s = QtPassSettings::load();
  s.useSelection = false;
  s.useAutoclear = false;
  QtPassSettings::save(s);

  QClipboard *clip = QApplication::clipboard();
  clip->setText(QStringLiteral("sentinel"));

  const QString otpUri = QStringLiteral(
      "otpauth://totp/Example:alice?secret=JBSWY3DPEHPK3PXP&issuer=Example");
  QVERIFY2(
      QMetaObject::invokeMethod(m_window.data(), "passwordFromFileToClipboard",
                                Qt::DirectConnection, Q_ARG(QString, otpUri)),
      "invoking passwordFromFileToClipboard must succeed");

  QCOMPARE(clip->text(), QStringLiteral("sentinel"));
  QVERIFY2(!clip->text().contains(QStringLiteral("JBSWY3DPEHPK3PXP")),
           "otpauth secret must never reach the clipboard");
}

/**
 * @brief showTextAsQRCode() reports a qrencode binary that cannot start.
 *
 * QProcess leaves exitStatus() at NormalExit and exitCode() at 0 when the
 * executable does not exist, so the old exitStatus()||exitCode() check took
 * the success branch and opened an empty QR dialog. The display panel's
 * qrRequested signal is the production entry point into
 * QtPass::showTextAsQRCode.
 */
void tst_mainwindow::showTextAsQRCodeReportsMissingQrencode() {
  const QString bogus =
      QDir(m_storeDir.path()).filePath(QStringLiteral("no-such-qrencode"));
  QVERIFY(!QFile::exists(bogus));
  AppSettings s = QtPassSettings::load();
  s.qrencodeExecutable = bogus;
  QtPassSettings::save(s);

  auto *panel = m_window->findChild<PasswordDisplayPanel *>();
  QVERIFY2(panel != nullptr, "MainWindow must own a PasswordDisplayPanel");

  // Would block in QDialog::exec() if the error path were not taken.
  emit panel->qrRequested(QStringLiteral("hunter2"));

  const QString msg = m_window->statusBar()->currentMessage();
  QVERIFY2(msg.contains(QStringLiteral("qrencode")),
           qPrintable(QStringLiteral("unexpected status: ") + msg));
  QVERIFY2(msg.contains(bogus), "status message should name the missing path");
}

/**
 * @brief The content browser repaints in the new Base colour after the
 * application palette changes at runtime (KDE day/night theme switch).
 *
 * The browser used to carry a "background: palette(base)" stylesheet; the
 * style-sheet engine resolves palette() once at polish time and does not
 * repolish on ApplicationPaletteChange, so the widget kept the previous
 * theme's colour. Without the stylesheet the native Base role follows along.
 */
void tst_mainwindow::textBrowserFollowsRuntimePaletteChange() {
  auto *browser =
      m_window->findChild<QTextBrowser *>(QStringLiteral("textBrowser"));
  QVERIFY2(browser != nullptr, "MainWindow must have a textBrowser");
  // The regression was a "background: palette(base)" stylesheet: the
  // style-sheet engine resolves palette() once at polish time. Any stylesheet
  // on the browser reintroduces that class of bug.
  QVERIFY2(browser->styleSheet().isEmpty(),
           qPrintable(QStringLiteral("textBrowser carries a stylesheet: ") +
                      browser->styleSheet()));
  const QPalette original = QApplication::palette();
  auto restore =
      qScopeGuard([&original] { QApplication::setPalette(original); });

  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));

  QPalette dark = original;
  dark.setColor(QPalette::Base, QColor(0x10, 0x10, 0x10));
  QApplication::setPalette(dark);
  QCoreApplication::processEvents();
  // Styles may tint Base slightly (Qt 5.15 Fusion renders #101010 as
  // #070c10), so compare lightness rather than the exact colour.
  const QColor darkPixel =
      browser->grab().toImage().pixelColor(browser->rect().center());
  QVERIFY2(darkPixel.lightness() < 64,
           qPrintable(QStringLiteral("browser still light after dark "
                                     "palette: ") +
                      darkPixel.name()));

  QPalette light = original;
  light.setColor(QPalette::Base, QColor(0xfa, 0xfa, 0xfa));
  QApplication::setPalette(light);
  QCoreApplication::processEvents();
  const QColor lightPixel =
      browser->grab().toImage().pixelColor(browser->rect().center());
  QVERIFY2(lightPixel.lightness() > 192,
           qPrintable(QStringLiteral("browser still dark after light "
                                     "palette: ") +
                      lightPixel.name()));
}

/**
 * @brief Field frames re-derive their border colour from QPalette::Mid when
 * the application palette changes while an entry is displayed.
 */
void tst_mainwindow::fieldFrameBorderFollowsRuntimePaletteChange() {
  const QPalette original = QApplication::palette();
  auto restore =
      qScopeGuard([&original] { QApplication::setPalette(original); });
  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));

  auto *panel = m_window->findChild<PasswordDisplayPanel *>();
  QVERIFY2(panel != nullptr, "MainWindow must own a PasswordDisplayPanel");
  panel->displayFields(QStringLiteral("secret"),
                       NamedValues{{"url", "https://example.org"}},
                       QtPassSettings::load());
  // findChild<QFrame*>() would also match QMainWindow internals; take the
  // frame that actually carries the panel border stylesheet.
  QFrame *frame = nullptr;
  for (QFrame *candidate : m_window->findChildren<QFrame *>()) {
    if (candidate->styleSheet().contains(QStringLiteral("border-radius"))) {
      frame = candidate;
      break;
    }
  }
  QVERIFY2(frame != nullptr, "displayFields() must create a field frame");

  QPalette changed = original;
  const QColor mid(0x12, 0x34, 0x56);
  changed.setColor(QPalette::Mid, mid);
  QApplication::setPalette(changed);
  QCoreApplication::processEvents();

  QVERIFY2(
      frame->styleSheet().contains(mid.name()),
      qPrintable(QStringLiteral("stylesheet still: ") + frame->styleSheet()));
}

/**
 * @brief A toolbar palette left over from the previous theme is dropped.
 *
 * KDE's Breeze style stamps a "header" palette on top toolbars and, after a
 * runtime light/dark switch, re-applies it from a cached kdeglobals — i.e.
 * with the old theme's colours. Simulate that: mark the toolbar the way
 * Breeze does, give it a dark palette while the application is light, and
 * check that MainWindow resets it.
 */
void tst_mainwindow::toolBarDropsStaleStylePaletteAfterThemeSwitch() {
  auto *bar = m_window->findChild<QToolBar *>(QStringLiteral("toolBar"));
  QVERIFY2(bar != nullptr, "MainWindow must have the toolBar");
  const QPalette original = QApplication::palette();
  auto restore =
      qScopeGuard([&original] { QApplication::setPalette(original); });

  QPalette light = original;
  light.setColor(QPalette::Window, QColor(0xef, 0xf0, 0xf1));
  QApplication::setPalette(light);

  QPalette staleDark = light;
  staleDark.setColor(QPalette::Window, QColor(0x29, 0x2c, 0x30));
  bar->setProperty("breeze_has_toolsarea_palette", true);
  bar->setPalette(staleDark);
  QVERIFY(bar->testAttribute(Qt::WA_SetPalette));

  QTRY_VERIFY2(!bar->testAttribute(Qt::WA_SetPalette),
               "stale dark toolbar palette must be dropped on a light app");
  QCOMPARE(bar->palette().color(QPalette::Window),
           light.color(QPalette::Window));
  QVERIFY2(bar->autoFillBackground(),
           "toolbar must paint its own background over the style's stale "
           "tools-area fill");
}

/**
 * @brief A header tint in the same theme as the application is left alone.
 */
void tst_mainwindow::toolBarKeepsHeaderTintInSameTheme() {
  auto *bar = m_window->findChild<QToolBar *>(QStringLiteral("toolBar"));
  QVERIFY2(bar != nullptr, "MainWindow must have the toolBar");
  const QPalette original = QApplication::palette();
  auto restore =
      qScopeGuard([&original] { QApplication::setPalette(original); });

  QPalette light = original;
  light.setColor(QPalette::Window, QColor(0xef, 0xf0, 0xf1));
  QApplication::setPalette(light);

  QPalette headerTint = light;
  headerTint.setColor(QPalette::Window, QColor(0xe3, 0xe5, 0xe7));
  bar->setPalette(headerTint);

  QTest::qWait(50); // let the deferred check run
  QVERIFY2(bar->testAttribute(Qt::WA_SetPalette),
           "a same-theme header tint must be kept");
  QCOMPARE(bar->palette().color(QPalette::Window), QColor(0xe3, 0xe5, 0xe7));
}

QTEST_MAIN(tst_mainwindow)
#include "tst_mainwindow.moc"
