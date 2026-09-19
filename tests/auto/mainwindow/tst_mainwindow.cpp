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
 */

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileSystemModel>
#include <QFrame>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QScopedPointer>
#include <QScreen>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QtTest>

#include <functional>

#include "../../../src/clipboardmanager.h"
#include "../../../src/configdialog.h"
#include "../../../src/filecontent.h"
#include "../../../src/firstrunwizard.h"
#include "../../../src/mainwindow.h"
#include "../../../src/passworddisplaypanel.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/util.h"
#include "../testsettings.h"

namespace {

/**
 * @brief Collects the char format of every text fragment in @p document by
 * walking its blocks, so a test can assert on the formats the document
 * actually stores rather than on the widget's current insertion format.
 */
QList<QTextCharFormat> fragmentFormats(const QTextDocument *document) {
  QList<QTextCharFormat> formats;
  for (QTextBlock block = document->begin(); block.isValid();
       block = block.next()) {
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
      const QTextFragment fragment = it.fragment();
      if (fragment.isValid()) {
        formats.append(fragment.charFormat());
      }
    }
  }
  return formats;
}

} // namespace

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
  void setUiElementsEnabledDisablesTreeView();
  void setUiElementsEnabledEnablesTreeView();
  void backendCompletionReachesTheWindowThroughQtPass();
  void reencryptKeepsUiDisabledUntilEnd();
  void reencryptProgressSurvivesQueuedEnd();
  void flashTextSetsContent();
  void flashTextErrorDoesNotCrash();
  void flashTextHtmlRenderedInBrowser();
  void flashTextNonErrorClearsErrorForeground();
  void showStatusMessageAppearsInStatusBar();
  void deselectDoesNotCrash();
  void onProcessOutputAppendsToPanel();
  void onProcessOutputSkippedWhenPanelHidden();
  void passwordFromFileToClipboardCopiesFirstLine();
  void passwordFromFileToClipboardSkipsOtpSecret();
  void copyRequestAnswersOnlyItsOwnEntry();
  void otpRequestAnswersOnlyItsOwnEntry();
  void otpRequestKeepsThePasswordOffTheClipboard();
  void otpFastPathCopiesTheVisibleCodeWithoutDecrypting();
  void cancelOtpRequestForgetsBothPendingRequests();
  void clipboardAutoclearSurvivesShowingAnotherEntry();
  void showTextAsQRCodeReportsMissingQrencode();
  void textBrowserFollowsRuntimePaletteChange();
  void fieldFrameBorderFollowsRuntimePaletteChange();
  void toolBarDropsStaleStylePaletteAfterThemeSwitch();
  void toolBarKeepsHeaderTintInSameTheme();
  void firstRunWizardCancelledStopsStartup();
  void firstRunWizardSetsUpTheStore();
  void windowFlagsAreOnlyRebuiltWhenAlwaysOnTopChanges();
  void restoreWindowAppliesSavedGeometry();
  void restoreWindowCentresWhenNothingSaved();
  void menuBarCarriesEveryToolbarActionAndTheMenuOnlyOnes();
  void menuBarCanBeHiddenAndComesBackWithCtrlM();
  void gitButtonsOnlyExistWhenGitIsInUse();
  void searchMatchesWordsLiterallyAndInOrder();
  void quitIsAnActionNotAStrayShortcut();
  void quitIsWiredToTheApplication();
  void closeWindowHonoursHideOnClose();
  void closeEventSavesGeometryAlsoWhenHidingToTray();

private:
  auto runFirstRunFlow(const std::function<bool(FirstRunWizard *)> &onWizard,
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
 * @brief A backend completion travels Pass -> QtPass -> MainWindow: the
 * output lands in the text browser, the grey stderr chatter too, the status
 * bar gets the key-generation message and the interface is enabled again.
 * This is the window's half of the QtPass signal contract; tst_qtpass only
 * sees the signals leave.
 */
void tst_mainwindow::backendCompletionReachesTheWindowThroughQtPass() {
  auto *treeView = m_window->findChild<QTreeView *>(QStringLiteral("treeView"));
  auto *browser =
      m_window->findChild<QTextBrowser *>(QStringLiteral("textBrowser"));
  QVERIFY(treeView != nullptr && browser != nullptr);

  m_window->setUiElementsEnabled(false);
  emit QtPassSettings::getPass()
      -> finishedGitPull(QStringLiteral("pulled via qtpass"),
                         QStringLiteral("already up to date"));
  QVERIFY2(treeView->isEnabled(), "operationFinished must re-enable the UI");
  const QString shown = browser->toPlainText();
  QVERIFY2(shown.contains(QStringLiteral("pulled via qtpass")),
           qPrintable(shown));
  QVERIFY2(shown.contains(QStringLiteral("already up to date")),
           qPrintable(shown));

  m_window->setUiElementsEnabled(false);
  emit QtPassSettings::getPass()
      -> processErrorExit(1, QStringLiteral("gpg died"));
  QVERIFY(treeView->isEnabled());
  QVERIFY(browser->toPlainText().contains(QStringLiteral("gpg died")));

  emit QtPassSettings::getPass()
      -> finishedGenerateGPGKeys(QString(), QString());
  QVERIFY2(m_window->statusBar()->currentMessage().contains(
               QStringLiteral("generated successfully")),
           qPrintable(m_window->statusBar()->currentMessage()));
}

/**
 * @brief Between startReencryptPath() and endReencryptPath() the interface
 * stays disabled and a cancellable progress dialog is shown.
 *
 * The re-encryption now runs on a worker thread, so the git commands queued by
 * Init/Move/Copy finish (and call setUiElementsEnabled(true)) while files are
 * still being rewritten; that call must be ignored until the run ends.
 */
void tst_mainwindow::reencryptKeepsUiDisabledUntilEnd() {
  auto *treeView = m_window->findChild<QTreeView *>(QStringLiteral("treeView"));
  QVERIFY2(treeView != nullptr, "treeView widget must exist");

  m_window->startReencryptPath();
  QVERIFY2(!treeView->isEnabled(), "treeView must be disabled during a run");
  auto *progress = m_window->findChild<QProgressDialog *>();
  QVERIFY2(progress != nullptr, "a progress dialog must be shown");
  QVERIFY2(progress->isVisible(), "the progress dialog must be visible");

  // Calling startReencryptPath twice (MainWindow::reencryptPath does so
  // preemptively, then the ImitatePass signal repeats it) is idempotent.
  m_window->startReencryptPath();
  QCOMPARE(m_window->findChildren<QProgressDialog *>().size(), 1);

  m_window->reencryptProgress(2, 5);
  QCOMPARE(progress->maximum(), 5);
  QCOMPARE(progress->value(), 2);

  // An unrelated completion must not release the UI while the worker runs.
  m_window->setUiElementsEnabled(true);
  QVERIFY2(!treeView->isEnabled(),
           "treeView must stay disabled until endReencryptPath");

  m_window->endReencryptPath();
  QVERIFY2(treeView->isEnabled(), "treeView must be re-enabled at the end");
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QVERIFY2(m_window->findChild<QProgressDialog *>() == nullptr,
           "the progress dialog must be gone after the run");

  // The guard is released: the normal enable/disable cycle works again.
  m_window->setUiElementsEnabled(false);
  QVERIFY(!treeView->isEnabled());
  m_window->setUiElementsEnabled(true);
  QVERIFY(treeView->isEnabled());
}

/**
 * @brief A completion queued behind the last progress event must not crash.
 *
 * The worker emits reencryptProgress(N, N) and immediately queues
 * finishReencrypt(), so both sit in the GUI thread's event queue together.
 * QProgressDialog::setValue() on a modal dialog calls processEvents(), which
 * delivers that completion (and thus endReencryptPath()) while
 * reencryptProgress() is still on the stack: the dialog is hidden and
 * m_reencryptProgress reset to null under its feet. Nothing may touch the
 * pointer after setValue() returns.
 */
void tst_mainwindow::reencryptProgressSurvivesQueuedEnd() {
  auto *treeView = m_window->findChild<QTreeView *>(QStringLiteral("treeView"));
  QVERIFY2(treeView != nullptr, "treeView widget must exist");

  m_window->startReencryptPath();
  auto *progress = m_window->findChild<QProgressDialog *>();
  QVERIFY2(progress != nullptr, "a progress dialog must be shown");
  QVERIFY2(progress->isVisible(), "the progress dialog must be visible");
  QVERIFY2(progress->isModal(),
           "the dialog must be modal for setValue() to process events");

  // Mirror the worker's first report. QProgressDialog only starts processing
  // events from setValue() once its minimum-duration timer (0 ms here) has
  // fired, so let it.
  m_window->reencryptProgress(0, 5);
  QTest::qWait(10);

  // Same ordering as the worker's last two posts: the completion is already
  // queued when the final progress event is handled.
  QMetaObject::invokeMethod(
      m_window.data(), [this]() { m_window->endReencryptPath(); },
      Qt::QueuedConnection);
  m_window->reencryptProgress(5, 5);

  QVERIFY2(treeView->isEnabled(),
           "the queued endReencryptPath must have run inside setValue()");
  QVERIFY2(!progress->isVisible(), "the dialog must be hidden by the end");
  QCOMPARE(progress->maximum(), 5);
  QCOMPARE(progress->value(), 5);

  // A late progress event after the run is a no-op, not a crash.
  m_window->reencryptProgress(6, 6);
  QCOMPARE(progress->value(), 5);

  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QVERIFY2(m_window->findChild<QProgressDialog *>() == nullptr,
           "the progress dialog must be gone after the run");
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
 * @brief A plain-text non-error flashText() shown after an error must not
 * stay red, and must not pin an explicit foreground either.
 *
 * flashText(isError=true) merges Qt::red into the browser's current char
 * format and setPlainText() re-applies that format to the whole new document,
 * so without a reset the next plain-text message inherits the red. The reset
 * has to remove the ForegroundBrush property rather than set a palette colour:
 * an explicit foreground would stop the text from following a runtime
 * light/dark palette switch (#946).
 */
void tst_mainwindow::flashTextNonErrorClearsErrorForeground() {
  auto *browser =
      m_window->findChild<QTextBrowser *>(QStringLiteral("textBrowser"));
  QVERIFY2(browser != nullptr, "textBrowser must exist");

  m_window->flashText(QStringLiteral("something failed"), true);
  QCOMPARE(browser->toPlainText(), QStringLiteral("something failed"));
  const QList<QTextCharFormat> errorFormats =
      fragmentFormats(browser->document());
  QVERIFY2(!errorFormats.isEmpty(),
           "the error message must produce a fragment");
  for (const QTextCharFormat &format : errorFormats) {
    QVERIFY2(format.hasProperty(QTextFormat::ForegroundBrush),
             "an error message must carry an explicit foreground");
    QCOMPARE(format.foreground().color(), QColor(Qt::red));
  }

  m_window->flashText(QStringLiteral("all good"), false);
  QCOMPARE(browser->toPlainText(), QStringLiteral("all good"));
  const QList<QTextCharFormat> okFormats = fragmentFormats(browser->document());
  QVERIFY2(!okFormats.isEmpty(),
           "the non-error message must produce a fragment");
  for (const QTextCharFormat &format : okFormats) {
    QVERIFY2(!format.hasProperty(QTextFormat::ForegroundBrush),
             "a non-error message shown after an error must not carry an "
             "explicit foreground (neither the stale red nor a pinned "
             "palette colour)");
  }
  QVERIFY2(
      !browser->currentCharFormat().hasProperty(QTextFormat::ForegroundBrush),
      "the insertion format must not keep an explicit foreground for "
      "the next message");
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

// QTRY_* macros return void; this variant returns a value from a helper.
#define QTRY_VERIFY_WITH_TIMEOUT_RETURN(expr, timeout, ret)                    \
  do {                                                                         \
    QElapsedTimer timer;                                                       \
    timer.start();                                                             \
    while (!(expr) && timer.elapsed() < (timeout)) {                           \
      QTest::qWait(20);                                                        \
    }                                                                          \
    if (!(expr)) {                                                             \
      return ret;                                                              \
    }                                                                          \
  } while (false)

namespace {
/**
 * Create <name>.gpg in the store, make it the tree's current entry and press
 * Ctrl+C on it, i.e. arm a copy request for it. The backend's decrypt of the
 * fake file fails later on the event loop; the tests below answer the
 * request synchronously first, the way a real finishedShow would.
 */
auto selectEntry(MainWindow *window, const QString &storeDir,
                 const QString &name) -> bool {
  const QString path = QDir(storeDir).filePath(name + QStringLiteral(".gpg"));
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly)) {
    return false;
  }
  f.write("not really encrypted");
  f.close();
  auto *tree = window->findChild<QTreeView *>(QStringLiteral("treeView"));
  auto *proxy = qobject_cast<QSortFilterProxyModel *>(tree->model());
  auto *fs = qobject_cast<QFileSystemModel *>(proxy->sourceModel());
  QModelIndex src;
  QTRY_VERIFY_WITH_TIMEOUT_RETURN((src = fs->index(path)).isValid(), 5000,
                                  false);
  tree->setCurrentIndex(proxy->mapFromSource(src));
  return true;
}

auto armCopyRequest(MainWindow *window, const QString &storeDir,
                    const QString &name) -> bool {
  return selectEntry(window, storeDir, name) &&
         QMetaObject::invokeMethod(window, "copyPasswordFromTreeview",
                                   Qt::DirectConnection);
}

/// Ctrl+G on the selected entry: with no OTP row on screen this queues a
/// decrypt and remembers the entry as the pending OTP request.
auto armOtpRequest(MainWindow *window, const QString &storeDir,
                   const QString &name) -> bool {
  return selectEntry(window, storeDir, name) &&
         QMetaObject::invokeMethod(window, "onOtp", Qt::DirectConnection);
}

/// RFC 6238 appendix B seed; the code changes every 30 s, so tests only
/// check its shape.
const QString kOtpUri = QStringLiteral(
    "otpauth://totp/Example:alice?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ&"
    "issuer=Example");
/// The pass-otp layout: password first, the otpauth URI as a bare line.
const QString kOtpEntry =
    QStringLiteral("hunter2\n") + kOtpUri + QStringLiteral("\n");

auto looksLikeOtpCode(const QString &s) -> bool {
  static const QRegularExpression six(QStringLiteral("^[0-9]{6}$"));
  return six.match(s).hasMatch();
}
} // namespace

/**
 * @brief Ctrl+G on an entry with no code on screen decrypts it; only that
 *        entry's decrypt yields a code, and only once.
 */
void tst_mainwindow::otpRequestAnswersOnlyItsOwnEntry() {
  AppSettings s = QtPassSettings::load();
  s.useSelection = false;
  s.useAutoclear = false;
  s.useOtp = true;
  s.clipBoardType = Enums::CLIPBOARD_ON_DEMAND;
  QtPassSettings::save(s);
  QClipboard *clip = QApplication::clipboard();
  clip->setText(QStringLiteral("sentinel"));

  QVERIFY(armOtpRequest(m_window.data(), m_storeDir.path(),
                        QStringLiteral("otp-me")));
  m_window->otpFromFileToClipboard(kOtpEntry, QStringLiteral("someone-else"));
  QCOMPARE(clip->text(), QStringLiteral("sentinel"));

  m_window->otpFromFileToClipboard(kOtpEntry, QStringLiteral("otp-me"));
  QVERIFY2(looksLikeOtpCode(clip->text()),
           qPrintable("expected a six-digit code, got: " + clip->text()));

  clip->setText(QStringLiteral("sentinel"));
  m_window->otpFromFileToClipboard(kOtpEntry, QStringLiteral("otp-me"));
  QVERIFY2(clip->text() == QStringLiteral("sentinel"),
           "a consumed request must not answer twice");
}

/**
 * @brief In "always copy" mode a decrypt normally puts the password on the
 *        clipboard; the decrypt that answers an OTP request must not — the
 *        user asked for a code, and two clipboard writes in one turn can
 *        leave the Windows clipboard empty.
 */
void tst_mainwindow::otpRequestKeepsThePasswordOffTheClipboard() {
  AppSettings s = QtPassSettings::load();
  s.useSelection = false;
  s.useAutoclear = false;
  s.useOtp = true;
  s.clipBoardType = Enums::CLIPBOARD_ALWAYS;
  QtPassSettings::save(s);
  QClipboard *clip = QApplication::clipboard();
  clip->setText(QStringLiteral("sentinel"));

  QVERIFY(armOtpRequest(m_window.data(), m_storeDir.path(),
                        QStringLiteral("otp-always")));
  // passShowHandler runs first for the same finishedShow ...
  m_window->passShowHandler(kOtpEntry, QStringLiteral("otp-always"));
  QVERIFY2(clip->text() == QStringLiteral("sentinel"),
           "the password must not be copied for an OTP request");
  // ... then the OTP handler copies the code.
  m_window->otpFromFileToClipboard(kOtpEntry, QStringLiteral("otp-always"));
  QVERIFY(looksLikeOtpCode(clip->text()));
}

/**
 * @brief When the pane already shows a live code for the selected entry,
 *        Ctrl+G copies it without another decrypt (and without touching
 *        the pending-request state).
 */
void tst_mainwindow::otpFastPathCopiesTheVisibleCodeWithoutDecrypting() {
  AppSettings s = QtPassSettings::load();
  s.useSelection = false;
  s.useAutoclear = false;
  s.useOtp = true;
  s.hideContent = false;
  s.displayAsIs = false;
  s.clipBoardType = Enums::CLIPBOARD_ON_DEMAND;
  QtPassSettings::save(s);
  QClipboard *clip = QApplication::clipboard();
  clip->setText(QStringLiteral("sentinel"));

  QVERIFY(
      selectEntry(m_window.data(), m_storeDir.path(), QStringLiteral("shown")));
  // Simulate the tree click's decrypt landing: the pane renders the OTP row.
  QMetaObject::invokeMethod(
      m_window.data(), "on_treeView_clicked", Qt::DirectConnection,
      Q_ARG(QModelIndex,
            m_window->findChild<QTreeView *>(QStringLiteral("treeView"))
                ->currentIndex()));
  m_window->passShowHandler(kOtpEntry, QStringLiteral("shown"));

  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onOtp",
                                    Qt::DirectConnection));
  QVERIFY2(
      looksLikeOtpCode(clip->text()),
      qPrintable("fast path must copy the visible code, got: " + clip->text()));
  // No request is pending afterwards: a stray decrypt answer does nothing.
  clip->setText(QStringLiteral("sentinel"));
  m_window->otpFromFileToClipboard(kOtpEntry, QStringLiteral("shown"));
  QCOMPARE(clip->text(), QStringLiteral("sentinel"));
}

/**
 * @brief A failed decrypt never emits finishedShow; cancelOtpRequest() (on
 *        processErrorExit and deselect) must forget both the OTP and the
 *        Ctrl+C request, or a later decrypt of that entry would answer them.
 */
void tst_mainwindow::cancelOtpRequestForgetsBothPendingRequests() {
  AppSettings s = QtPassSettings::load();
  s.useSelection = false;
  s.useAutoclear = false;
  s.useOtp = true;
  s.clipBoardType = Enums::CLIPBOARD_ON_DEMAND;
  QtPassSettings::save(s);
  QClipboard *clip = QApplication::clipboard();
  clip->setText(QStringLiteral("sentinel"));

  // Both requests pending at once, one cancel.
  QVERIFY(armCopyRequest(m_window.data(), m_storeDir.path(),
                         QStringLiteral("copy-cancel")));
  QVERIFY(armOtpRequest(m_window.data(), m_storeDir.path(),
                        QStringLiteral("otp-cancel")));
  m_window->cancelOtpRequest();

  m_window->passwordFromFileToClipboard(QStringLiteral("late"),
                                        QStringLiteral("copy-cancel"));
  QCOMPARE(clip->text(), QStringLiteral("sentinel"));
  m_window->otpFromFileToClipboard(kOtpEntry, QStringLiteral("otp-cancel"));
  QCOMPARE(clip->text(), QStringLiteral("sentinel"));
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

  QVERIFY(armCopyRequest(m_window.data(), m_storeDir.path(),
                         QStringLiteral("copyme")));
  m_window->passwordFromFileToClipboard(QStringLiteral("hunter2\nlogin: alice"),
                                        QStringLiteral("copyme"));
  QCOMPARE(clip->text(), QStringLiteral("hunter2"));
}

/**
 * @brief finishedShow names its file, so a decrypt of another entry — a tree
 *        click that finished first — must not be copied, and the request
 *        stays armed for its own answer.
 */
void tst_mainwindow::copyRequestAnswersOnlyItsOwnEntry() {
  AppSettings s = QtPassSettings::load();
  s.useSelection = false;
  s.useAutoclear = false;
  QtPassSettings::save(s);
  QClipboard *clip = QApplication::clipboard();
  clip->setText(QStringLiteral("sentinel"));

  QVERIFY(armCopyRequest(m_window.data(), m_storeDir.path(),
                         QStringLiteral("wanted")));
  m_window->passwordFromFileToClipboard(QStringLiteral("other-secret"),
                                        QStringLiteral("someone-else"));
  QCOMPARE(clip->text(), QStringLiteral("sentinel"));
  m_window->passwordFromFileToClipboard(QStringLiteral("the-one"),
                                        QStringLiteral("wanted"));
  QCOMPARE(clip->text(), QStringLiteral("the-one"));
  // Consumed: the same content arriving again is not copied twice.
  clip->setText(QStringLiteral("sentinel"));
  m_window->passwordFromFileToClipboard(QStringLiteral("the-one"),
                                        QStringLiteral("wanted"));
  QCOMPARE(clip->text(), QStringLiteral("sentinel"));
}

namespace {
/**
 * Arm a one-second autoclear on the window's ClipboardManager and return it.
 * The interval is read from settings when setAutoclearTimer() runs, so save
 * first.
 */
auto armAutoclear(MainWindow *window) -> ClipboardManager * {
  AppSettings s = QtPassSettings::load();
  s.useSelection = false;
  s.useAutoclear = true;
  s.autoclearSeconds = 1;
  QtPassSettings::save(s);
  auto *clipboard = window->findChild<ClipboardManager *>();
  if (clipboard != nullptr) {
    clipboard->setAutoclearTimer();
  }
  return clipboard;
}
} // namespace

/**
 * @brief Regression for #1607: showing another entry while a copied
 *        password is waiting for autoclear used to overwrite the tracker, so
 *        the timer no longer recognised the clipboard and left the password
 *        there indefinitely.
 */
void tst_mainwindow::clipboardAutoclearSurvivesShowingAnotherEntry() {
  ClipboardManager *clipboard = armAutoclear(m_window.data());
  QVERIFY2(clipboard != nullptr, "MainWindow must own a ClipboardManager");
  {
    AppSettings s = QtPassSettings::load();
    s.clipBoardType = Enums::CLIPBOARD_ON_DEMAND;
    QtPassSettings::save(s);
  }
  QClipboard *clip = QApplication::clipboard();

  clipboard->copyText(QStringLiteral("hunter2"));
  m_window->passShowHandler(QStringLiteral("otherpass\nlogin: bob"));
  QCOMPARE(clip->text(), QStringLiteral("hunter2"));
  QTRY_VERIFY_WITH_TIMEOUT(clip->text().isEmpty(), 3000);
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
  QVERIFY(armCopyRequest(m_window.data(), m_storeDir.path(),
                         QStringLiteral("otpentry")));
  m_window->passwordFromFileToClipboard(otpUri, QStringLiteral("otpentry"));

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
  // Styles may tint Base slightly (Fusion has rendered #101010 as
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
 * @brief setWindowFlags() on a top-level widget destroys and recreates the
 *        native window. That used to happen on every Settings OK because the
 *        "always on top: off" branch reset the flags unconditionally; the
 *        rebuild is what left ui->lineEdit dangling. Re-applying an unchanged
 *        setting must keep the same native window; changing it must toggle
 *        exactly the one hint.
 */
/**
 * @brief A saved geometry is what the window comes back with. Before, the
 *        stored pos/size were applied on top of restoreGeometry() and main()
 *        re-centred the window unconditionally, so the saved position was
 *        always discarded.
 */
void tst_mainwindow::restoreWindowAppliesSavedGeometry() {
  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  // The window's minimum size depends on the platform fonts (463 wide on
  // the Linux runner, 613 on FreeBSD), so ask for sizes well above it and
  // record what the widget actually took rather than assuming. Keep the
  // frame inside the offscreen screen (800x600): restoreGeometry() clamps to
  // the available screen, which is a feature, not what is tested.
  const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
  const QSize big(avail.width() * 85 / 100, avail.height() * 80 / 100);
  const QSize small(avail.width() * 75 / 100, avail.height() * 70 / 100);
  if (small.width() <= m_window->minimumSizeHint().width() + 20) {
    QSKIP("screen too small to resize the main window twice");
  }
  m_window->move(avail.left() + 37, avail.top() + 41);
  m_window->resize(big);
  QTRY_VERIFY(m_window->size().width() > small.width());
  const QSize savedSize = m_window->size();
  const QPoint savedPos = m_window->pos();
  QtPassSettings::setGeometry(m_window->saveGeometry());

  m_window->move(avail.left() + 10, avail.top() + 10);
  m_window->resize(small);
  QTRY_VERIFY(m_window->size() != savedSize);

  m_window->restoreWindow();
  QTRY_COMPARE(m_window->size(), savedSize);
  QTRY_COMPARE(m_window->pos(), savedPos);
}

namespace {
auto menuActionNames(QMenuBar *bar, const QString &menuName) -> QStringList {
  QStringList names;
  for (QAction *top : bar->actions()) {
    QMenu *menu = top->menu();
    if (menu == nullptr || menu->objectName() != menuName) {
      continue;
    }
    for (QAction *action : menu->actions()) {
      if (!action->isSeparator()) {
        names << action->objectName();
      }
    }
  }
  return names;
}
} // namespace

/**
 * @brief Settings > Show menu bar (Ctrl+M) hides the bar for those who liked
 *        the bare window, remembers the choice, and still answers Ctrl+M
 *        while the bar - and with it the menu the action sits in - is gone.
 */
void tst_mainwindow::menuBarCanBeHiddenAndComesBackWithCtrlM() {
#ifdef Q_OS_MACOS
  QSKIP("the menu bar is the system's on macOS");
#else
  auto *toggle =
      m_window->findChild<QAction *>(QStringLiteral("actionShowMenuBar"));
  QVERIFY(toggle != nullptr);
  QVERIFY(toggle->isCheckable());
  QVERIFY2(!toggle->isChecked() &&
               !m_window->menuBar()->isVisibleTo(m_window.get()),
           "the window starts bare; the menu bar is opt-in");
  QCOMPARE(toggle->shortcut(), QKeySequence(QStringLiteral("Ctrl+M")));

  toggle->trigger();
  QVERIFY2(m_window->menuBar()->isVisibleTo(m_window.get()),
           "the bar shows on the first toggle");
  QVERIFY2(QtPassSettings::load().showMenuBar, "and the choice is saved");
  toggle->trigger();
  QVERIFY2(!m_window->menuBar()->isVisibleTo(m_window.get()),
           "and hides again");
  QVERIFY(!QtPassSettings::load().showMenuBar);

  // The action is on the window itself, so the shortcut survives its menu.
  QVERIFY(m_window->actions().contains(toggle));
  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  m_window->activateWindow();
  QTest::qWaitForWindowActive(m_window.data());
  QTest::keyClick(m_window.get(), Qt::Key_M, Qt::ControlModifier);
  QVERIFY2(m_window->menuBar()->isVisibleTo(m_window.get()),
           "Ctrl+M brings the bar back");
  QVERIFY(QtPassSettings::load().showMenuBar);
#endif
}

/**
 * @brief The search box matches its words literally, in order, with anything
 *        between them; a bracket used to be regex syntax that made the
 *        filter silently stop applying.
 */
void tst_mainwindow::searchMatchesWordsLiterallyAndInOrder() {
  QVERIFY(QDir(m_storeDir.path()).mkpath(QStringLiteral("work/acme")));
  QVERIFY(selectEntry(m_window.data(), m_storeDir.path(),
                      QStringLiteral("work/acme/vpn")));
  QVERIFY(selectEntry(m_window.data(), m_storeDir.path(),
                      QStringLiteral("notes [draft]")));
  auto *tree = m_window->findChild<QTreeView *>(QStringLiteral("treeView"));
  auto *proxy = qobject_cast<QSortFilterProxyModel *>(tree->model());
  auto *fs = qobject_cast<QFileSystemModel *>(proxy->sourceModel());
  auto *search = m_window->findChild<QLineEdit *>(QStringLiteral("lineEdit"));
  QVERIFY(tree != nullptr && proxy != nullptr && search != nullptr);
  const QModelIndex vpn = fs->index(
      QDir(m_storeDir.path()).filePath(QStringLiteral("work/acme/vpn.gpg")));
  const QModelIndex notes = fs->index(
      QDir(m_storeDir.path()).filePath(QStringLiteral("notes [draft].gpg")));
  QVERIFY(vpn.isValid() && notes.isValid());
  const auto filter = [&](const QString &text) {
    search->setText(text);
    QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onTimeoutSearch",
                                      Qt::DirectConnection));
  };

  filter(QStringLiteral("work vpn"));
  QVERIFY2(proxy->mapFromSource(vpn).isValid(),
           "words in order with anything between them match");
  QVERIFY(!proxy->mapFromSource(notes).isValid());

  filter(QStringLiteral("[draft"));
  QVERIFY2(proxy->mapFromSource(notes).isValid(),
           "a bracket is a character to find, not regex syntax");
  QVERIFY(!proxy->mapFromSource(vpn).isValid());

  filter(QStringLiteral("("));
  QVERIFY2(!proxy->mapFromSource(vpn).isValid() &&
               !proxy->mapFromSource(notes).isValid(),
           "an unmatched parenthesis filters everything out instead of "
           "leaving the previous filter in place");
}

/**
 * @brief Push and Pull are for Git; without it they are not greyed out but
 *        gone, from the toolbar and the menu alike, and come back when Git
 *        is switched on again.
 */
void tst_mainwindow::gitButtonsOnlyExistWhenGitIsInUse() {
  auto *push = m_window->findChild<QAction *>(QStringLiteral("actionPush"));
  auto *pull = m_window->findChild<QAction *>(QStringLiteral("actionUpdate"));
  QVERIFY(push != nullptr && pull != nullptr);
  {
    AppSettings s = QtPassSettings::load();
    s.useGit = false;
    QtPassSettings::save(s);
  }
  m_window->setUiElementsEnabled(true);
  QVERIFY2(!push->isVisible() && !pull->isVisible(),
           "no Git, no push and pull buttons");
  {
    AppSettings s = QtPassSettings::load();
    s.useGit = true;
    s.gitExecutable = QStringLiteral("/usr/bin/git");
    QtPassSettings::save(s);
  }
  m_window->setUiElementsEnabled(true);
  QVERIFY2(push->isVisible() && pull->isVisible() && push->isEnabled(),
           "with Git they are back and usable");
  m_window->setUiElementsEnabled(false);
  QVERIFY2(push->isVisible() && !push->isEnabled(),
           "while an operation runs they stay but are disabled");
}

/**
 * @brief The icon-only toolbar was the only way to reach Push, Pull, Users
 *        and Config, and there was no About or FAQ at all. Every toolbar
 *        action now also sits in a menu, and the menu-only ones exist, with
 *        the roles macOS needs to build its application menu.
 */
void tst_mainwindow::menuBarCarriesEveryToolbarActionAndTheMenuOnlyOnes() {
  QMenuBar *bar = m_window->menuBar();
  QVERIFY(bar != nullptr);
  QStringList menus;
  for (QAction *top : bar->actions()) {
    menus << top->menu()->objectName();
  }
  QCOMPARE(menus,
           (QStringList{QStringLiteral("menuFile"), QStringLiteral("menuStore"),
                        QStringLiteral("menuSettings"),
                        QStringLiteral("menuHelp")}));

  QCOMPARE(menuActionNames(bar, QStringLiteral("menuFile")),
           (QStringList{
               QStringLiteral("actionAddPassword"),
               QStringLiteral("actionAddFolder"), QStringLiteral("actionEdit"),
               QStringLiteral("actionDelete"), QStringLiteral("actionClose"),
               QStringLiteral("actionQuit")}));
  QCOMPARE(menuActionNames(bar, QStringLiteral("menuStore")),
           (QStringList{
               QStringLiteral("actionUsers"), QStringLiteral("actionUpdate"),
               QStringLiteral("actionPush"), QStringLiteral("actionOtp")}));
  QCOMPARE(menuActionNames(bar, QStringLiteral("menuSettings")),
           (QStringList{QStringLiteral("actionConfig"),
                        QStringLiteral("actionShowMenuBar")}));
  QCOMPARE(
      menuActionNames(bar, QStringLiteral("menuHelp")),
      (QStringList{QStringLiteral("actionFaq"), QStringLiteral("actionAbout"),
                   QStringLiteral("actionAboutQt")}));

  // The toolbar and the menus share the QAction objects, so enabled state
  // stays in step without extra bookkeeping.
  auto *toolBar = m_window->findChild<QToolBar *>(QStringLiteral("toolBar"));
  QVERIFY(toolBar != nullptr);
  for (QAction *action : toolBar->actions()) {
    if (action->isSeparator()) {
      continue;
    }
    bool inAMenu = false;
    for (QAction *top : bar->actions()) {
      inAMenu = inAMenu || top->menu()->actions().contains(action);
    }
    QVERIFY2(inAMenu, qPrintable(action->objectName() + " must be in a menu"));
  }

  auto *config = m_window->findChild<QAction *>(QStringLiteral("actionConfig"));
  auto *about = m_window->findChild<QAction *>(QStringLiteral("actionAbout"));
  auto *quit = m_window->findChild<QAction *>(QStringLiteral("actionQuit"));
  QVERIFY(config != nullptr && about != nullptr && quit != nullptr);
  QCOMPARE(config->menuRole(), QAction::PreferencesRole);
  QCOMPARE(about->menuRole(), QAction::AboutRole);
  QCOMPARE(quit->menuRole(), QAction::QuitRole);
}

/**
 * @brief Ctrl+Q used to be a bare QShortcut; it is the Quit action now and
 *        nothing else claims the sequence.
 */
void tst_mainwindow::quitIsAnActionNotAStrayShortcut() {
  auto *quit = m_window->findChild<QAction *>(QStringLiteral("actionQuit"));
  QVERIFY(quit != nullptr);
  QCOMPARE(quit->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_Q));
  for (QShortcut *shortcut : m_window->findChildren<QShortcut *>()) {
    QVERIFY2(shortcut->key() != QKeySequence(Qt::CTRL | Qt::Key_Q),
             "no QShortcut may compete with the Quit action");
  }
}

/**
 * @brief Quit ends the application (#1788). QApplication::quit() cannot be
 *        fired inside the suite, so the connection is probed:
 *        disconnect() reports whether one existed, and the window is
 *        rebuilt for the next test anyway.
 */
void tst_mainwindow::quitIsWiredToTheApplication() {
  auto *quit = m_window->findChild<QAction *>(QStringLiteral("actionQuit"));
  QVERIFY(quit != nullptr);
  QVERIFY2(QObject::disconnect(quit, &QAction::triggered, nullptr, nullptr),
           "Quit must be connected");
}

/**
 * @brief File > Close window is what the old bare Ctrl+Q did: close(),
 *        which the configurable "hide on close" turns into a hide.
 */
void tst_mainwindow::closeWindowHonoursHideOnClose() {
  auto *closeAction =
      m_window->findChild<QAction *>(QStringLiteral("actionClose"));
  QVERIFY(closeAction != nullptr);
  QCOMPARE(closeAction->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_W));
  {
    AppSettings s = QtPassSettings::load();
    s.hideOnClose = true;
    QtPassSettings::save(s);
  }
  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  closeAction->trigger();
  QVERIFY2(!m_window->isVisible(), "the window must hide, not quit");
}

/**
 * @brief With nothing saved the window is centred on the screen under the
 *        pointer (the primary one here), not left at 0,0 and not moved so
 *        that its top-left sits on the screen centre, which is what the old
 *        getPos() fallback did.
 */
void tst_mainwindow::restoreWindowCentresWhenNothingSaved() {
  if (QGuiApplication::platformName().startsWith(QLatin1String("wayland")))
    QSKIP("Wayland lets the compositor place windows; centring is a no-op");
  QtPassSettings::setGeometry(QByteArray());
  m_window->resize(600, 400);
  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  m_window->restoreWindow();

  const QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
  const QPoint centre = m_window->frameGeometry().center();
  // Allow for frame decoration rounding.
  QVERIFY2(qAbs(centre.x() - screen.center().x()) <= 2 &&
               qAbs(centre.y() - screen.center().y()) <= 2,
           qPrintable(QStringLiteral("window centre %1,%2 vs screen %3,%4")
                          .arg(centre.x())
                          .arg(centre.y())
                          .arg(screen.center().x())
                          .arg(screen.center().y())));
}

/**
 * @brief A tray user closes the window only ever to hide it; that path used
 *        to save nothing, so their geometry was lost on every quit.
 */
void tst_mainwindow::closeEventSavesGeometryAlsoWhenHidingToTray() {
  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
  m_window->move(avail.left() + 22, avail.top() + 11);
  m_window->resize(avail.width() * 85 / 100, avail.height() * 80 / 100);
  QtPassSettings::setGeometry(QByteArray());
  {
    AppSettings s = QtPassSettings::load();
    s.hideOnClose = true;
    QtPassSettings::save(s);
  }

  m_window->close(); // hide branch: event ignored, window hidden
  QTRY_VERIFY(!m_window->isVisible());
  const QByteArray saved = QtPassSettings::getGeometry();
  QVERIFY2(!saved.isEmpty(), "hide-on-close must still save the geometry");

  {
    AppSettings s = QtPassSettings::load();
    s.hideOnClose = false;
    QtPassSettings::save(s);
  }
}

void tst_mainwindow::windowFlagsAreOnlyRebuiltWhenAlwaysOnTopChanges() {
  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  QVERIFY(!m_window->windowFlags().testFlag(Qt::WindowStaysOnTopHint));
  const WId before = m_window->winId();

  // Same setting, applied again (what a Settings OK does): no rebuild.
  {
    AppSettings s = QtPassSettings::load();
    s.alwaysOnTop = false;
    QtPassSettings::save(s);
  }
  m_window->restoreWindow();
  m_window->restoreWindow();
  QCOMPARE(m_window->winId(), before);
  QVERIFY(m_window->isVisible());
  QVERIFY(!m_window->windowFlags().testFlag(Qt::WindowStaysOnTopHint));

  // Turning it on adds the hint and keeps the window shown.
  {
    AppSettings s = QtPassSettings::load();
    s.alwaysOnTop = true;
    QtPassSettings::save(s);
  }
  m_window->restoreWindow();
  QVERIFY(m_window->windowFlags().testFlag(Qt::WindowStaysOnTopHint));
  QVERIFY(m_window->windowFlags().testFlag(Qt::Window));
  QVERIFY(m_window->isVisible());
  const WId afterEnable = m_window->winId();

  // Applying "on" again: still no rebuild.
  m_window->restoreWindow();
  QCOMPARE(m_window->winId(), afterEnable);

  // Turning it off removes only that hint.
  {
    AppSettings s = QtPassSettings::load();
    s.alwaysOnTop = false;
    QtPassSettings::save(s);
  }
  m_window->restoreWindow();
  QVERIFY(!m_window->windowFlags().testFlag(Qt::WindowStaysOnTopHint));
  QVERIFY(m_window->windowFlags().testFlag(Qt::Window));
  QVERIFY(m_window->isVisible());
}

// ---------------------------------------------------------------------------
// First-run configuration loop (MainWindow constructor -> config() -> wizard)

/**
 * @brief Construct a MainWindow while driving every modal dialog the first-run
 * flow opens.
 *
 * Each FirstRunWizard is handed to @p onWizard, which either walks it to
 * Finish (returns true) or leaves it to be rejected; any other dialog is
 * rejected. Every dialog is driven once, and a dialog that stays open
 * afterwards is rejected by the guard so the test cannot hang in a modal
 * loop.
 *
 * @param onWizard Drives one wizard sighting; true means it was accepted.
 * @param initSucceeded Receives MainWindow::initSucceeded() of the window.
 * @return Number of wizards that were shown.
 */
auto tst_mainwindow::runFirstRunFlow(
    const std::function<bool(FirstRunWizard *)> &onWizard, bool *initSucceeded)
    -> int {
  int wizardsSeen = 0;
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

    if (auto *wizard = qobject_cast<FirstRunWizard *>(modal)) {
      ++wizardsSeen;
      if (!onWizard(wizard)) {
        wizard->reject();
      }
      return;
    }
    modal->reject(); // message box, keygen or file dialog: never proceed
  });
  poker.start();

  MainWindow w;
  poker.stop();
  *initSucceeded = w.initSucceeded();
  return wizardsSeen;
}

/**
 * @brief Cancelling the first-run wizard must not start the application: the
 * constructor reports failure (main() exits on that) and asks no second
 * time.
 */
void tst_mainwindow::firstRunWizardCancelledStopsStartup() {
  m_window.reset();
  const AppSettings saved = QtPassSettings::load();
  QTemporaryDir scratch;
  QVERIFY2(scratch.isValid(), "temp dir must be created");
  const QString missingStore =
      QDir::cleanPath(QDir(scratch.path()).filePath(QStringLiteral("store")));
  QtPassSettings::setPassStore(missingStore);
  QVERIFY2(!Util::configIsValid(QtPassSettings::load()),
           "a missing store must make the configuration invalid");

  bool initSucceeded = true;
  const int wizards =
      runFirstRunFlow([](FirstRunWizard *) { return false; }, &initSucceeded);
  QtPassSettings::save(saved);

  QCOMPARE(wizards, 1);
  QVERIFY2(!initSucceeded, "a cancelled wizard must not start the application");
  // The backend creates the folder on construction; the wizard must not
  // have initialised it.
  QVERIFY2(
      !QFile::exists(QDir(missingStore).filePath(QStringLiteral(".gpg-id"))),
      "cancel initialises nothing");
}

/**
 * @brief Walking the wizard to Finish with a store that does not exist yet
 * creates and initialises it for the listed secret key, and startup
 * succeeds on that configuration.
 */
void tst_mainwindow::firstRunWizardSetsUpTheStore() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as the gpg stand-in");
#else
  m_window.reset();
  const AppSettings saved = QtPassSettings::load();
  QTemporaryDir scratch;
  QVERIFY2(scratch.isValid(), "temp dir must be created");
  const QString store =
      QDir::cleanPath(QDir(scratch.path()).filePath(QStringLiteral("store")));
  // A gpg that knows exactly one secret key and succeeds at everything else.
  const QString gpg = scratch.filePath(QStringLiteral("gpg"));
  {
    QFile script(gpg);
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write(
        "#!/bin/sh\n"
        "case \"$*\" in\n"
        "*--list-secret-keys*)\n"
        "printf '%s\\n' "
        "'sec:u:4096:1:31850CF72D9CDDE9:1774947438:::u:::escarESCA:::+:::23::0:"
        "' "
        "'fpr:::::::::13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9:' "
        "'uid:u::::1774947438::CBF23008234AA5F88824CE76140F482FAE34923E::Test "
        "Key <test@example.org>::::::::::0:'\n"
        ";;\n"
        "esac\n"
        "exit 0\n");
    script.close();
    QVERIFY(QFile::setPermissions(gpg, QFile::ReadOwner | QFile::WriteOwner |
                                           QFile::ExeOwner));
  }
  {
    AppSettings s = QtPassSettings::load();
    s.passStore = store;
    s.gpgExecutable = gpg;
    s.gitExecutable.clear();
    s.useGit = false;
    s.usePass = false;
    QtPassSettings::save(s);
  }
  QVERIFY2(!Util::configIsValid(QtPassSettings::load()),
           "a missing store must make the configuration invalid");

  bool initSucceeded = false;
  const int wizards = runFirstRunFlow(
      [](FirstRunWizard *wizard) {
        for (int i = 0; i < 4; ++i) {
          if (!wizard->currentPage()->isComplete()) {
            return false;
          }
          if (wizard->currentId() == FirstRunWizard::StorePage) {
            // The machine's git may be found; a commit needs a configured
            // identity, so keep the new store out of Git here.
            if (auto *git = wizard->currentPage()->findChild<QCheckBox *>()) {
              git->setChecked(false);
            }
          }
          wizard->next();
        }
        wizard->accept();
        return wizard->result() == QDialog::Accepted;
      },
      &initSucceeded);
  const AppSettings after = QtPassSettings::load();
  QtPassSettings::save(saved);

  QCOMPARE(wizards, 1);
  QVERIFY2(initSucceeded, "startup must succeed on the wizard's store");
  QVERIFY(QFile::exists(QDir(store).filePath(QStringLiteral(".gpg-id"))));
  QCOMPARE(QDir::cleanPath(after.passStore), store);
  QCOMPARE(after.gpgExecutable, gpg);
#endif
}

QTEST_MAIN(tst_mainwindow)
#include "tst_mainwindow.moc"
