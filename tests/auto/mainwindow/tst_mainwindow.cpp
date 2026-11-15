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
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileSystemModel>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QScopedPointer>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTextDocumentFragment>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QTreeWidget>
#include <QtTest>

#include <functional>

#include "../../../src/configdialog.h"
#include "../../../src/filecontent.h"
#include "../../../src/mainwindow.h"
#include "../../../src/passworddisplaypanel.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/trayicon.h"
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
  void entryTextKeepsItsWhiteSpace();
  void cancellingAContentSearchStopsIt();
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
  void closeWithoutATrayIconDoesNotHide();
  void startMinimizedWithoutATrayIconStaysShown();
  void firstRunAcceptedWithInvalidStoreAsksAgainUntilCancelled();
  void firstRunAcceptedWithInvalidStoreAsksAgainUntilValid();
  void returnInTheSearchBoxEndsThere();
  void returnOnAContentSearchResultOpensIt();

private:
  void rebuildWithGrepSearch();
  auto runFirstRunFlow(
      const std::function<bool(ConfigDialog *, int)> &onConfigDialog,
      bool *initSucceeded) -> int;
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
 * @brief The text of an entry is shown, and so selected and copied, with the
 *        white space it is stored with. As rich text, "ridge  velvet" in a
 *        recovery phrase came out as "ridge velvet", indentation was dropped
 *        and a tab became a space (#210); with "Show the file content as-is"
 *        that went for the password line too.
 */
void tst_mainwindow::entryTextKeepsItsWhiteSpace() {
  auto *browser =
      m_window->findChild<QTextBrowser *>(QStringLiteral("textBrowser"));
  QVERIFY2(browser != nullptr, "textBrowser must exist");
  const QString text = QStringLiteral("backup phrase: ridge  velvet harbor\n"
                                      "    indented\n"
                                      "tab\there\n"
                                      "trailing   \n"
                                      "see https://example.org/a  b");
  // What Select All and Copy in the browser's menu hand to the clipboard.
  const auto copied = [browser] {
    browser->selectAll();
    return browser->textCursor().selection().toPlainText();
  };
  // A click on an entry empties the browser before its decrypt comes back.
  browser->clear();
  m_window->passShowHandler(QStringLiteral("hunter2\n") + text);
  QCOMPARE(browser->toPlainText(), text);
  QCOMPARE(copied(), text);

  // Output arriving later is appended through toHtml(): the entry's text
  // stays as it was, and the output keeps its own columns.
  emit QtPassSettings::getPass() -> finishedGitPull(
      QStringLiteral(" 2 files changed,  3 insertions"), QString());
  QCOMPARE(copied(),
           text + QStringLiteral("\n 2 files changed,  3 insertions"));

  // A "\r\n" is still one line end, and a lone "\r" stays a space.
  browser->clear();
  emit QtPassSettings::getPass()
      -> finishedGitPull(QStringLiteral("one  1\r\ntwo\rthree"), QString());
  QCOMPARE(copied(), QStringLiteral("one  1\ntwo three"));

  // Kept white space hangs Qt's layout on 65536 spaces in a row, so a long
  // run is cut before it is shown.
  browser->clear();
  emit QtPassSettings::getPass() -> finishedGitPull(
      QStringLiteral("a") +
          QString(Util::kMaxSpaceRun + 500, QLatin1Char(' ')) +
          QStringLiteral("b"),
      QString());
  QCOMPARE(browser->toPlainText(),
           QStringLiteral("a") + QString(Util::kMaxSpaceRun, QLatin1Char(' ')) +
               QStringLiteral("b"));

  {
    AppSettings s = QtPassSettings::load();
    s.displayAsIs = true;
    QtPassSettings::save(s);
  }
  const auto restore = qScopeGuard([] {
    AppSettings s = QtPassSettings::load();
    s.displayAsIs = false;
    QtPassSettings::save(s);
  });
  const QString entry = QStringLiteral("pass  word\n") + text;
  browser->clear();
  m_window->passShowHandler(entry);
  QCOMPARE(copied(), entry);
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  QSKIP("pixel grab is not deterministic on the Qt 5 offscreen platform");
#endif
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

/**
 * @brief "Hide on close" hides the window only when a tray icon can bring it
 *        back. Without one (none asked for, or no system tray in this
 *        session) a close hid the window while QtPass ran on invisibly, with
 *        the last entry in it and the password on the clipboard; the only way
 *        back was to start qtpass again. Now the close goes ahead.
 */
void tst_mainwindow::closeWithoutATrayIconDoesNotHide() {
  if (QSystemTrayIcon::isSystemTrayAvailable())
    QSKIP("needs a session without a system tray");
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  // The close goes on to QApplication::quit(), which on Qt 5 is exit(0): it
  // marks the thread to quit, and every later event loop of this run (modal
  // dialogs, QSignalSpy::wait()) would return at once.
  QSKIP("Qt 5's QApplication::quit() would end the event loops of later tests");
#endif
  // QApplication::quit() does nothing without a running event loop on Qt 6;
  // should a Qt send its Quit event all the same, it must not end this run.
  struct SwallowQuit : QObject {
    auto eventFilter(QObject *watched, QEvent *event) -> bool override {
      return watched == qApp && event->type() == QEvent::Quit;
    }
  } swallowQuit;
  qApp->installEventFilter(&swallowQuit);
  const AppSettings saved = QtPassSettings::load();
  auto restore = qScopeGuard([&swallowQuit, &saved] {
    qApp->removeEventFilter(&swallowQuit);
    QtPassSettings::save(saved);
  });
  // Asked for a tray icon, but the session has none; then not asked for,
  // as the settings dialog saves it when "Show tray icon" is unticked.
  for (const bool useTrayIcon : {true, false}) {
    {
      AppSettings s = QtPassSettings::load();
      s.useTrayIcon = useTrayIcon;
      s.hideOnClose = true;
      QtPassSettings::save(s);
    }
    m_window->restoreWindow();
    QCOMPARE(m_window->findChild<TrayIcon *>(), nullptr);
    m_window->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
    QCloseEvent close;
    QApplication::sendEvent(m_window.data(), &close);
    QVERIFY2(close.isAccepted(),
             "without a tray icon, a close must not turn into a hide");
  }
}

/**
 * @brief "Start minimized" hides the window only into a tray icon: without a
 *        system tray nothing would bring the hidden window back.
 */
void tst_mainwindow::startMinimizedWithoutATrayIconStaysShown() {
  if (QSystemTrayIcon::isSystemTrayAvailable())
    QSKIP("needs a session without a system tray");
  const AppSettings saved = QtPassSettings::load();
  auto restore = qScopeGuard([&saved] { QtPassSettings::save(saved); });
  {
    AppSettings s = QtPassSettings::load();
    s.useTrayIcon = true;
    s.startMinimized = true;
    QtPassSettings::save(s);
  }
  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  m_window->restoreWindow();
  QCOMPARE(m_window->findChild<TrayIcon *>(), nullptr);
  QTest::qWait(100); // the deferred hide would have run by now
  QVERIFY2(m_window->isVisible(),
           "without a system tray, start minimized must not hide");
}

// ---------------------------------------------------------------------------
// First-run configuration loop (QtPass::init() -> MainWindow::config())

/**
 * @brief Construct a MainWindow while driving every modal dialog the first-run
 * flow opens.
 *
 * The wizard's message boxes are declined ("Create password-store?" -> No, the
 * "not initialised" notice -> Ok), the store picker and any other dialog are
 * rejected, and each ConfigDialog is handed to @p onConfigDialog together with
 * its 1-based sighting count; a true return clicks OK, false cancels it. Every
 * dialog is driven once, and a dialog that stays open afterwards is rejected
 * by the guard so the test cannot hang in a modal loop.
 *
 * @param onConfigDialog Decides per ConfigDialog sighting whether to accept.
 * @param initSucceeded Receives MainWindow::initSucceeded() of the window.
 * @return Number of ConfigDialog instances that were shown.
 */
auto tst_mainwindow::runFirstRunFlow(
    const std::function<bool(ConfigDialog *, int)> &onConfigDialog,
    bool *initSucceeded) -> int {
  int configDialogsSeen = 0;
  int stuckTicks = 0;
  QTimer poker;
  poker.setInterval(50);
  QObject::connect(&poker, &QTimer::timeout, [&]() {
    auto *modal = qobject_cast<QDialog *>(QApplication::activeModalWidget());
    if (modal == nullptr) {
      return;
    }
    if (modal->property("tst_driven").toBool()) {
      // Already handled but still open: the flow is stuck, so end it.
      if (++stuckTicks > 100) {
        stuckTicks = 0;
        modal->reject();
      }
      return;
    }
    modal->setProperty("tst_driven", true);
    stuckTicks = 0;

    if (auto *cfg = qobject_cast<ConfigDialog *>(modal)) {
      ++configDialogsSeen;
      if (!onConfigDialog(cfg, configDialogsSeen)) {
        cfg->reject();
        return;
      }
      auto *box =
          cfg->findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
      if (box != nullptr && box->button(QDialogButtonBox::Ok) != nullptr) {
        box->button(QDialogButtonBox::Ok)->click();
      } else {
        cfg->accept();
      }
      return;
    }
    if (auto *msg = qobject_cast<QMessageBox *>(modal)) {
      if (auto *no = msg->button(QMessageBox::No)) {
        no->click(); // "Create password-store?" -> keep the store missing
      } else if (auto *ok = msg->button(QMessageBox::Ok)) {
        ok->click();
      } else {
        msg->reject();
      }
      return;
    }
    modal->reject(); // store picker, keygen or users dialog: never proceed
  });
  poker.start();

  MainWindow w;
  poker.stop();
  *initSucceeded = w.initSucceeded();
  return configDialogsSeen;
}

/**
 * @brief Giving up on a content search stops it: emptying the box and
 *        pressing Return, releasing the content search button, or the panel
 *        autoclear leaving content search. Each only reset the window's
 *        flags, while the native backend's search went on decrypting the rest
 *        of the store, one unseen gpg at a time, and its result was dropped at
 *        the end.
 */
void tst_mainwindow::cancellingAContentSearchStopsIt() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as the gpg stand-in");
#else
  QTemporaryDir scratch;
  QVERIFY(scratch.isValid());
  // Ten entries that take a second each to decrypt: a search left running
  // takes ten seconds, a stopped one ends after the entry it is on.
  const QString store = QDir(scratch.path()).filePath(QStringLiteral("store"));
  QVERIFY(QDir().mkpath(store));
  for (int i = 0; i < 10; ++i) {
    QFile entry(QDir(store).filePath(QStringLiteral("entry%1.gpg").arg(i)));
    QVERIFY(entry.open(QIODevice::WriteOnly));
    QVERIFY(entry.write("not really encrypted") > 0);
  }
  const QString gpg = QDir(scratch.path()).filePath(QStringLiteral("gpg"));
  {
    QFile script(gpg);
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write("#!/bin/sh\n"
                 "case \" $* \" in *\" -d \"*) exec sleep 1 ;; esac\n"
                 "exit 0\n");
    script.close();
    QVERIFY(QFile::setPermissions(gpg, QFile::ReadOwner | QFile::WriteOwner |
                                           QFile::ExeOwner));
  }
  ImitatePass *pass = QtPassSettings::getImitatePass();
  QVERIFY(QtPassSettings::getPass() == pass);
  AppSettings s = QtPassSettings::load();
  s.passStore = QDir::cleanPath(store) + QLatin1Char('/');
  s.gpgExecutable = gpg;
  pass->init(s);
  const auto restore =
      qScopeGuard([pass] { pass->init(QtPassSettings::load()); });

  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  auto *grep = m_window->findChild<QToolButton *>(QStringLiteral("grepButton"));
  auto *box = m_window->findChild<QLineEdit *>(QStringLiteral("lineEdit"));
  auto *results =
      m_window->findChild<QTreeWidget *>(QStringLiteral("grepResultsList"));
  QVERIFY(grep != nullptr && box != nullptr && results != nullptr);
  QSignalSpy finished(pass, &Pass::finishedGrep);
  const auto search = [&] {
    finished.clear();
    grep->setChecked(true);
    box->setText(QStringLiteral("login"));
    QVERIFY(QMetaObject::invokeMethod(
        m_window.data(), "on_lineEdit_returnPressed", Qt::DirectConnection));
    QTest::qWait(300);
    QVERIFY2(finished.isEmpty(), "the search is still decrypting");
  };

  search();
  box->clear();
  QVERIFY(QMetaObject::invokeMethod(
      m_window.data(), "on_lineEdit_returnPressed", Qt::DirectConnection));
  QVERIFY2(finished.count() == 1 || finished.wait(5000),
           "an emptied search box stops the search");

  search();
  grep->setChecked(false);
  QVERIFY2(finished.count() == 1 || finished.wait(5000),
           "releasing the button stops the search");

  search();
  results->setVisible(true); // an earlier search's results on screen
  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "clearPanel",
                                    Qt::DirectConnection, Q_ARG(bool, false)));
  QVERIFY2(finished.count() == 1 || finished.wait(5000),
           "the panel autoclear leaving content search stops the search");
#endif
}

/**
 * @brief Accepting the first-run dialog with a still-missing store must not
 * start the application; the dialog is shown again, and cancelling it then
 * makes the constructor report failure (main() exits on that).
 *
 * Regression test for the follow-up to #1689: config() used to clear the
 * fresh-start flag on any accept, so QtPass::init() returned true for an
 * invalid configuration and the window opened on a non-existent store.
 */
void tst_mainwindow::firstRunAcceptedWithInvalidStoreAsksAgainUntilCancelled() {
  m_window.reset();
  const AppSettings saved = QtPassSettings::load();
  QTemporaryDir scratch;
  QVERIFY2(scratch.isValid(), "temp dir must be created");
  const QString missingStore =
      QDir::cleanPath(QDir(scratch.path()).filePath(QStringLiteral("store")));
  QVERIFY2(!QDir(missingStore).exists(), "the store must not exist yet");
  QtPassSettings::setPassStore(missingStore);
  QVERIFY2(!Util::configIsValid(QtPassSettings::load()),
           "a missing store must make the configuration invalid");

  bool initSucceeded = true;
  const int dialogs = runFirstRunFlow(
      [](ConfigDialog *, int sighting) { return sighting == 1; },
      &initSucceeded);
  QtPassSettings::save(saved);

  QCOMPARE(dialogs, 2);
  QVERIFY2(!initSucceeded,
           "an accepted but still invalid first-run configuration must not "
           "start the application once the retry is cancelled");
}

/**
 * @brief The retry after an accepted-but-invalid first-run dialog is a real
 * second chance: once the store is initialised and the dialog accepted again,
 * startup succeeds.
 */
void tst_mainwindow::firstRunAcceptedWithInvalidStoreAsksAgainUntilValid() {
  m_window.reset();
  const AppSettings saved = QtPassSettings::load();
  QTemporaryDir scratch;
  QVERIFY2(scratch.isValid(), "temp dir must be created");
  const QString store =
      QDir::cleanPath(QDir(scratch.path()).filePath(QStringLiteral("store")));
  QVERIFY2(!QDir(store).exists(), "the store must not exist yet");
  QtPassSettings::setPassStore(store);
  QVERIFY2(!Util::configIsValid(QtPassSettings::load()),
           "a missing store must make the configuration invalid");

  bool storeCreated = false;
  bool initSucceeded = false;
  const int dialogs = runFirstRunFlow(
      [&](ConfigDialog *, int sighting) {
        if (sighting == 2) {
          // Initialise the store behind the dialog's back, then accept.
          storeCreated = QDir().mkpath(store);
          QFile gpgId(QDir(store).filePath(QStringLiteral(".gpg-id")));
          storeCreated = storeCreated && gpgId.open(QIODevice::WriteOnly) &&
                         gpgId.write("0000000000000000\n") > 0;
          gpgId.close();
        }
        return true;
      },
      &initSucceeded);
  QtPassSettings::save(saved);

  QCOMPARE(dialogs, 2);
  QVERIFY2(storeCreated, "the store must have been initialised on the retry");
  QVERIFY2(initSucceeded,
           "startup must succeed once the retried configuration is valid");
}

// ---------------------------------------------------------------------------
// Return in the search box and on content search results

namespace {
/**
 * Create <name>.gpg in the store and wait until the tree's model has it. Its
 * decrypt fails later on the event loop; the tests below only count it.
 */
auto addEntry(MainWindow *window, const QString &storeDir, const QString &name)
    -> QModelIndex {
  const QString path = QDir(storeDir).filePath(name + QStringLiteral(".gpg"));
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly)) {
    return {};
  }
  f.write("not really encrypted");
  f.close();
  auto *tree = window->findChild<QTreeView *>(QStringLiteral("treeView"));
  auto *proxy = qobject_cast<QSortFilterProxyModel *>(tree->model());
  auto *fs = qobject_cast<QFileSystemModel *>(proxy->sourceModel());
  if (!QTest::qWaitFor([&] { return fs->index(path).isValid(); }, 5000)) {
    return {};
  }
  return proxy->mapFromSource(fs->index(path));
}

/// Create <name>.gpg in the store and make it the tree's current entry.
auto selectEntry(MainWindow *window, const QString &storeDir,
                 const QString &name) -> bool {
  const QModelIndex index = addEntry(window, storeDir, name);
  if (!index.isValid()) {
    return false;
  }
  window->findChild<QTreeView *>(QStringLiteral("treeView"))
      ->setCurrentIndex(index);
  return true;
}
} // namespace

/**
 * @brief Build the window again with content search enabled, and shown.
 */
void tst_mainwindow::rebuildWithGrepSearch() {
  m_window.reset();
  AppSettings s = QtPassSettings::load();
  s.useGrepSearch = true;
  QtPassSettings::save(s);
  m_window.reset(new MainWindow);
  m_window->show();
}

/**
 * @brief Return in the search box ends there. QLineEdit leaves the key
 *        unaccepted, so the window's own Return opened the tree's current
 *        entry as well: in content search an entry nobody asked for (back in
 *        clear after the panel autoclear), on an empty box too, and by name
 *        the first hit a second time.
 */
void tst_mainwindow::returnInTheSearchBoxEndsThere() {
  const auto restore = qScopeGuard([] {
    AppSettings s = QtPassSettings::load();
    s.useGrepSearch = false;
    QtPassSettings::save(s);
  });
  rebuildWithGrepSearch();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  QVERIFY(
      selectEntry(m_window.data(), m_storeDir.path(), QStringLiteral("bank")));
  auto *name = m_window->findChild<QLabel *>(QStringLiteral("passwordName"));
  auto *box = m_window->findChild<QLineEdit *>(QStringLiteral("lineEdit"));
  auto *grep = m_window->findChild<QToolButton *>(QStringLiteral("grepButton"));
  QVERIFY(name != nullptr && box != nullptr && grep != nullptr);
  // The label as nothing opened it; no Return below may change it.
  const QString unopened = name->text();
  QVERIFY(unopened != QStringLiteral("bank"));
  grep->setChecked(true);
  const auto pressReturn = [box] {
    QKeyEvent ret(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(box, &ret);
  };

  // Qt 5 needs the signal's argument type registered for QSignalSpy.
  qRegisterMetaType<QList<QPair<QString, QStringList>>>();
  QSignalSpy searched(QtPassSettings::getPass(), &Pass::finishedGrep);
  box->setText(QStringLiteral("ssid"));
  pressReturn();
  QVERIFY2(searched.count() == 1 || searched.wait(10000),
           "Return runs the content search");
  QCOMPARE(name->text(), unopened);

  // An empty box ends the content search, and opens nothing either.
  box->clear();
  pressReturn();
  QCOMPARE(name->text(), unopened);

  // By name, Return opens the first hit, and decrypts it once.
  grep->setChecked(false);
  QSignalSpy shown(QtPassSettings::getPass(), &Pass::finishedShow);
  QSignalSpy failed(QtPassSettings::getPass(), &Pass::processErrorExit);
  box->setText(QStringLiteral("bank"));
  auto *tree = m_window->findChild<QTreeView *>(QStringLiteral("treeView"));
  QVERIFY(tree != nullptr);
  // 1.8 filters once the search delay has run (main also runs a pending
  // search on Return, #2170): wait for it, so Return acts on the filtered
  // tree and not on whatever the store lists first.
  QTRY_VERIFY_WITH_TIMEOUT(
      tree->currentIndex().data().toString().startsWith(QStringLiteral("bank")),
      5000);
  pressReturn();
  QCOMPARE(name->text(), QStringLiteral("bank"));
  QTRY_VERIFY_WITH_TIMEOUT(shown.count() + failed.count() >= 1, 10000);
  QTest::qWait(300);
  QCOMPARE(shown.count() + failed.count(), 1);
}

/**
 * @brief While content search results hide the tree, Return on a result
 *        opens that result. It opened the hidden tree's current entry.
 */
void tst_mainwindow::returnOnAContentSearchResultOpensIt() {
  const auto restore = qScopeGuard([] {
    AppSettings s = QtPassSettings::load();
    s.useGrepSearch = false;
    QtPassSettings::save(s);
  });
  rebuildWithGrepSearch();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  m_window->activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(m_window.data()));
  QVERIFY(
      addEntry(m_window.data(), m_storeDir.path(), QStringLiteral("guest-wifi"))
          .isValid());
  QVERIFY(
      selectEntry(m_window.data(), m_storeDir.path(), QStringLiteral("bank")));
  auto *name = m_window->findChild<QLabel *>(QStringLiteral("passwordName"));
  auto *tree = m_window->findChild<QTreeView *>(QStringLiteral("treeView"));
  auto *grep = m_window->findChild<QToolButton *>(QStringLiteral("grepButton"));
  auto *results =
      m_window->findChild<QTreeWidget *>(QStringLiteral("grepResultsList"));
  QVERIFY(name != nullptr && tree != nullptr && grep != nullptr &&
          results != nullptr);
  grep->setChecked(true);
  // What a finished content search leaves: the results list over the tree.
  auto *result = new QTreeWidgetItem(results);
  result->setText(0, QStringLiteral("guest-wifi"));
  result->setData(0, Qt::UserRole, QStringLiteral("guest-wifi"));
  results->setVisible(true);
  tree->setVisible(false);
  results->setCurrentItem(result);
  results->setFocus();
  QTRY_VERIFY(results->hasFocus());

  QSignalSpy shown(QtPassSettings::getPass(), &Pass::finishedShow);
  QSignalSpy failed(QtPassSettings::getPass(), &Pass::processErrorExit);
  QKeyEvent ret(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
  QApplication::sendEvent(results, &ret);
  QCOMPARE(name->text(), QStringLiteral("guest-wifi"));
  QVERIFY(!tree->isHidden());
  // The backend outlives the window: its decrypt must be answered before the
  // test ends, or the gpg still running is torn down at exit.
  QTRY_VERIFY_WITH_TIMEOUT(shown.count() + failed.count() >= 1, 10000);
}

QTEST_MAIN(tst_mainwindow)
#include "tst_mainwindow.moc"
