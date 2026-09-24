// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @brief Widget tests for MainWindow.
 *
 * These tests exercise the public surface of MainWindow in isolation.
 * Each test gets a fresh window instance; a single QTemporaryDir serves as
 * the pass store for the whole run.
 *
 * Where a path needs the backend to answer (decrypts, grep, key listing, git),
 * the window is rebuilt over shell scripts standing in for gpg and git
 * (rebuildWithFakeGpg, writeFakeGit), so nothing reaches a keyring, an agent
 * or a remote. Modal dialogs and popup menus are driven by ModalDriver.
 *
 * Deliberately not covered here:
 * - the UI watchdog (a 30 s constant, no seam to shorten it)
 * - closeEvent's quit branch (QApplication::quit() would end the run)
 * - destroyTrayIcon() with a live icon (needs a real system tray)
 */

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileSystemModel>
#include <QFrame>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QScopedPointer>
#include <QScreen>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QTreeWidget>
#include <QUrl>
#include <QtTest>

#include <functional>

#include "../../../src/clipboardmanager.h"
#include "../../../src/configdialog.h"
#include "../../../src/exportpublickeydialog.h"
#include "../../../src/filecontent.h"
#include "../../../src/firstrunwizard.h"
#include "../../../src/mainwindow.h"
#include "../../../src/pass.h"
#include "../../../src/passworddialog.h"
#include "../../../src/passworddisplaypanel.h"
#include "../../../src/processoutputpanel.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/trayicon.h"
#include "../../../src/usersdialog.h"
#include "../../../src/util.h"
#include "../testpass.h"
#include "../testsettings.h"

// QMessageBox::setWindowTitle() is a no-op on macOS ("Message boxes on the
// mac do not have a title", qmessagebox.cpp), so the titles the driver
// records are empty there. The count of boxes still holds; each test also
// asserts on the box's text.
#ifdef Q_OS_MACOS
#define COMPARE_BOX_TITLES(actual, expected)                                   \
  QCOMPARE((actual).size(), (expected).size())
#else
#define COMPARE_BOX_TITLES(actual, expected) QCOMPARE(actual, expected)
#endif

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

/**
 * @brief Drives every modal dialog and popup menu that opens while it lives.
 *
 * The handler runs once per new modal or popup widget, from a timer, so a
 * blocking exec() started on the test's own stack still gets answered. A
 * handler may itself open a nested dialog (a menu action that asks a
 * question): the timer keeps ticking inside that nested loop and the driver
 * re-enters for the new widget. Anything still open after the handler ran is
 * closed from the stuck guard, so a test cannot hang in a modal loop.
 */
class ModalDriver {
public:
  using Handler = std::function<void(QWidget *)>;

  explicit ModalDriver(Handler handler) : m_handler(std::move(handler)) {
    schedule();
  }
  ~ModalDriver() = default;
  ModalDriver(const ModalDriver &) = delete;
  auto operator=(const ModalDriver &) -> ModalDriver & = delete;

  /// Widgets the handler was invoked for, in order of appearance.
  int seen = 0;
  /// Class names of those widgets, for a failure message.
  QStringList seenClasses;
  /// Window titles of the message boxes the handler saw, for asserting.
  QStringList boxTitles;
  /// Plain text of the message boxes the handler saw.
  QStringList boxTexts;

private:
  /// One fresh single-shot timer per tick: a repeating QTimer does not fire
  /// again while its own slot is still on the stack, which is exactly the
  /// case when the handler opens a nested dialog from inside a menu.
  void schedule() {
    QTimer::singleShot(20, &m_context, [this]() { tick(); });
  }

  void tick() {
    schedule();
    QWidget *widget = QApplication::activeModalWidget();
    if (widget == nullptr) {
      widget = QApplication::activePopupWidget();
    }
    if (widget == nullptr) {
      return;
    }
    if (widget->property("tst_driven").toBool()) {
      if (++m_stuckTicks > 150) { // 3 s: the handler did not close it
        m_stuckTicks = 0;
        widget->close();
      }
      return;
    }
    widget->setProperty("tst_driven", true);
    m_stuckTicks = 0;
    ++seen;
    seenClasses << QString::fromLatin1(widget->metaObject()->className());
    if (auto *box = qobject_cast<QMessageBox *>(widget)) {
      boxTitles << box->windowTitle();
      boxTexts << box->text();
    }
    m_handler(widget);
  }

  Handler m_handler;
  /// Owns the pending tick: destroying the driver cancels it.
  QObject m_context;
  int m_stuckTicks = 0;
};

/// Click the given standard button of a message box; anything else is closed.
auto answerBox(QMessageBox::StandardButton button) -> ModalDriver::Handler {
  return [button](QWidget *widget) {
    if (auto *box = qobject_cast<QMessageBox *>(widget)) {
      if (QAbstractButton *b = box->button(button)) {
        b->click();
        return;
      }
      box->accept();
      return;
    }
    if (qobject_cast<QProgressDialog *>(widget) != nullptr) {
      return; // a running operation's own dialog: not a question to answer
    }
    if (auto *dialog = qobject_cast<QDialog *>(widget)) {
      dialog->reject();
      return;
    }
    widget->close();
  };
}

/// Type @p text into a QInputDialog and accept it; message boxes are
/// acknowledged, any other dialog is rejected.
auto typeIntoInputDialog(const QString &text) -> ModalDriver::Handler {
  return [text](QWidget *widget) {
    if (auto *input = qobject_cast<QInputDialog *>(widget)) {
      input->setTextValue(text);
      input->accept();
      return;
    }
    answerBox(QMessageBox::Ok)(widget);
  };
}

/**
 * @brief Receives what the application asks the desktop to open, so a test
 * can assert the URL without a browser or file manager being launched.
 */
class UrlCatcher : public QObject {
  Q_OBJECT
public:
  QList<QUrl> urls;
public Q_SLOTS:
  void catchUrl(const QUrl &url) { urls << url; }
};

/**
 * @brief Write an executable shell script standing in for gpg: it lists one
 * public key, "exports" an armored block (or fails for the key NOKEY) and
 * "decrypts" a file by printing it as is. Nothing reaches a keyring or an
 * agent.
 * @return The script path, or an empty string when it could not be written.
 */
auto writeFakeGpg(const QString &dir) -> QString {
  const QString gpg = QDir(dir).filePath(QStringLiteral("gpg"));
  QFile script(gpg);
  if (!script.open(QIODevice::WriteOnly)) {
    return {};
  }
  script.write(
      "#!/bin/sh\n"
      "for a in \"$@\"; do last=\"$a\"; done\n"
      "case \"$*\" in\n"
      "*--list-secret-keys*|*--list-keys*)\n"
      "printf '%s\\n' "
      "'pub:u:4096:1:31850CF72D9CDDE9:1774947438:::u:::escaESCA::::::23::0:' "
      "'fpr:::::::::13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9:' "
      "'uid:u::::1774947438::CBF23008234AA5F88824CE76140F482FAE34923E::Test "
      "Key <test@example.org>::::::::::0:'\n"
      ";;\n"
      "*--export*NOKEY*)\n"
      "echo 'gpg: nothing exported' >&2\n"
      "exit 2\n"
      ";;\n"
      "*--export*)\n"
      "printf '%s\\n' '-----BEGIN PGP PUBLIC KEY BLOCK-----' 'mQINBFakeKey' "
      "'-----END PGP PUBLIC KEY BLOCK-----'\n"
      ";;\n"
      "-d\\ *|*\\ -d\\ *)\n"
      "cat \"$last\"\n"
      ";;\n"
      "esac\n"
      "exit 0\n");
  script.close();
  if (!QFile::setPermissions(gpg, QFile::ReadOwner | QFile::WriteOwner |
                                      QFile::ExeOwner)) {
    return {};
  }
  return gpg;
}

/**
 * @brief Write an executable shell script standing in for git: it appends
 * one line per call to @p log, the physical working directory first and
 * then the arguments (see fakeGitCalls), and exits with @p exitCode.
 * @return The script path, or an empty string when it could not be written.
 */
auto writeFakeGit(const QString &dir, const QString &log, int exitCode)
    -> QString {
  const QString git = QDir(dir).filePath(QStringLiteral("git"));
  QFile script(git);
  if (!script.open(QIODevice::WriteOnly)) {
    return {};
  }
  script.write(QStringLiteral("#!/bin/sh\n"
                              "echo \"$(pwd -P) $@\" >> '%1'\n"
                              "echo 'fake git says no' >&2\n"
                              "exit %2\n")
                   .arg(log)
                   .arg(exitCode)
                   .toUtf8());
  script.close();
  if (!QFile::setPermissions(git, QFile::ReadOwner | QFile::WriteOwner |
                                      QFile::ExeOwner)) {
    return {};
  }
  return git;
}

/**
 * @brief The calls a fake git logged, as (working directory, arguments)
 * pairs in the order they were made; the directory is canonical so it
 * compares with QFileInfo::canonicalFilePath() of the store.
 */
auto fakeGitCalls(const QString &log) -> QList<QPair<QString, QString>> {
  QList<QPair<QString, QString>> calls;
  QFile logFile(log);
  if (!logFile.open(QIODevice::ReadOnly)) {
    return calls;
  }
  const QStringList lines =
      QString::fromUtf8(logFile.readAll()).split(u'\n', Qt::SkipEmptyParts);
  for (const QString &line : lines) {
    const qsizetype space = line.indexOf(u' ');
    calls << qMakePair(line.left(space), line.mid(space + 1));
  }
  return calls;
}

/**
 * @brief Counts the decrypts the backend answers, with content or with a
 * failure, from the moment it is created.
 *
 * A test that starts a decrypt through the tree (a click, Return, Ctrl+G)
 * and answers it itself must still wait for the backend's own answer before
 * its window goes: the backend outlives the window, and a late answer would
 * otherwise land in the next test's window. The files are not encrypted, so
 * with the real gpg the answer is a failure.
 */
class ShowAnswers {
public:
  ShowAnswers()
      : m_shown(QtPassSettings::getPass(), &Pass::finishedShow),
        m_failed(QtPassSettings::getPass(), &Pass::processErrorExit) {}

  /// Spin until @p count answers arrived; false when they did not in time.
  auto waitFor(int count, int timeoutMs = 10000) -> bool {
    QElapsedTimer timer;
    timer.start();
    while (m_shown.count() + m_failed.count() < count &&
           timer.elapsed() < timeoutMs) {
      QTest::qWait(20);
    }
    return m_shown.count() + m_failed.count() >= count;
  }

private:
  QSignalSpy m_shown;
  QSignalSpy m_failed;
};

} // namespace

class tst_mainwindow : public QObject {
  Q_OBJECT

  QTemporaryDir m_storeDir;
  /// GNUPGHOME for every gpg this suite starts: the real gpg the tree clicks
  /// run (on files that are not encrypted) must never open the user's
  /// keyring, nor reach the user's agent.
  QTemporaryDir m_gnupgHome;
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
  void refusedShowClearsThePreviousEntryAndReleasesTheInterface();
  void clipboardAutoclearSurvivesShowingAnotherEntry();
  void showTextAsQRCodeReportsMissingQrencode();
  void textBrowserFollowsRuntimePaletteChange();
  void fieldFrameBorderFollowsRuntimePaletteChange();
  void toolsAreaBarDropsStaleStylePaletteAfterThemeSwitch_data();
  void toolsAreaBarDropsStaleStylePaletteAfterThemeSwitch();
  void toolsAreaBarKeepsHeaderTintInSameTheme_data();
  void toolsAreaBarKeepsHeaderTintInSameTheme();
  void toolsAreaBarKeepsAnyHeaderBeforeAThemeSwitch_data();
  void toolsAreaBarKeepsAnyHeaderBeforeAThemeSwitch();
  void firstRunWizardCancelledStopsStartup();
  void firstRunWizardSetsUpTheStore();
  void windowFlagsAreOnlyRebuiltWhenAlwaysOnTopChanges();
  void restoreWindowAppliesSavedGeometry();
  void restoreWindowCentresWhenNothingSaved();
  void menuBarCarriesEveryToolbarActionAndTheMenuOnlyOnes();
  void menuBarCanBeHiddenAndComesBackWithCtrlM();
  void altTapShowsTheHiddenMenuBarUntilTappedAgain();
  void altPeekEndsOnEscapeAndAfterAShortcut();
  void altMnemonicOpensAMenuWhileTheBarIsHidden();
  void altLeavesAMenuBarShownByChoiceAlone();
  void processOutputIsToggledFromTheSettingsMenu();
  void gitButtonsOnlyExistWhenGitIsInUse();
  void searchMatchesWordsLiterallyAndInOrder();
  void quitIsAnActionNotAStrayShortcut();
  void quitIsWiredToTheApplication();
  void closeWindowHonoursHideOnClose();
  void closeEventSavesGeometryAlsoWhenHidingToTray();

  void constructorAppliesMonospaceAndNoWrap();
  void backendOutputReachesTheConsoleUnlessSensitive();
  void faqOpensTheWebsite();
  void clickingNothingClearsThePanel();
  void staleDecryptDoesNotRepaintThePanel();
  void hiddenContentIsClearedFromThePanelOnTheTimer();
  void otpAnswerWithoutContentReportsTheFailedDecrypt();
  void otpAnswerWithoutACodeSaysSo();
  void otpNeedsASelectionAndTheSetting();
  void editRequestIsIgnoredWhileEditIsDisabled();
  void editPullsFirstWhenAutoPullIsOn();
  void doubleClickEditsAnEntryButNotAFolder();
  void pullAndPushRunGitInTheStore();
  void grepModeChangesTheSearchBox();
  void grepFindsContentAndNavigatesToTheEntry();
  void deselectLeavesGrepModeWhenResultsAreShown();
  void searchForAFolderWithoutFilesSelectsNothing();
  void enterInTheSearchBoxOpensTheFirstMatch();
  void messageFromAnotherInstanceShowsAndSearches();
  void configDialogAcceptedReappliesTheSettings();
  void configDialogCancelledChangesNothing();
  void aboutBoxNamesTheProgramAndLicence();
  void profilesFillTheBoxAndSwitchTheStore();
  void trayIconFollowsTheSetting();
  void keyPressesReachTheWindow();
  void downArrowInTheSearchBoxMovesToTheTree();
  void browserContextMenuBelongsToTheWindow();
  void contextMenuOnEmptySpaceOffersFolderActions();
  void contextMenuOnAPasswordOffersEditRenameDelete();
  void contextMenuOnAFolderOffersSharing();
  void shareMenuExportsThePublicKey();
  void shareMenuReencryptAsksFirst();
  void addFolderCreatesItWithAGpgId();
  void addFolderRefusesEscapesAndDuplicates();
  void renameFolderMovesIt();
  void renamePasswordMovesIt();
  void deleteNeedsASelection();
  void deletePasswordAsksFirst();
  void deleteFolderWarnsAboutStrayFiles();
  void deleteLinkedFolderRemovesOnlyTheLink();
  void usersDialogOpensForTheStoreButNotForALink();
  void addPasswordOffersTheStoreFolders();
  void reencryptCancelIsReportedInTheStatusBar();

private:
  auto runFirstRunFlow(const std::function<bool(FirstRunWizard *)> &onWizard,
                       bool *initSucceeded) -> int;
  static void toolsAreaBars();
  auto toolsAreaBar(const QString &name) -> QWidget *;
  void rebuildWindow(const std::function<void(AppSettings &)> &tweak);
  auto rebuildWithFakeGpg(QTemporaryDir &scratch,
                          const std::function<void(AppSettings &)> &tweak = {})
      -> QString;
  auto selectPath(const QString &absolutePath) -> bool;
  auto treeView() -> QTreeView *;
  auto browser() -> QTextBrowser *;
  auto searchBox() -> QLineEdit *;
  auto passwordName() -> QLabel *;
};

void tst_mainwindow::initTestCase() {
  isolateTestSettings();
  QVERIFY2(m_storeDir.isValid(), "temp store dir must be created");
  QVERIFY2(m_gnupgHome.isValid(), "temp GNUPGHOME must be created");
  // Pass::init() hands the inherited GNUPGHOME to every gpg it starts, and
  // the blocking calls inherit the process environment: set it before the
  // first backend is built.
  QVERIFY(qputenv("GNUPGHOME", m_gnupgHome.path().toLocal8Bit()));

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
    s.showMenuBar = false;
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
 * @brief Clicking a planted link entry starts no decrypt; the refusal must
 *        still behave like a failed operation: the previous entry's fields,
 *        text and OTP code leave the panel, the interface is enabled again
 *        at once, and onOtp() finds no code to copy for the refused name.
 */
void tst_mainwindow::
    refusedShowClearsThePreviousEntryAndReleasesTheInterface() {
#ifdef Q_OS_WIN
  QSKIP("uses a symlink");
#else
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

  // A real entry on screen, with an OTP code and a leftover line.
  QVERIFY(
      selectEntry(m_window.data(), m_storeDir.path(), QStringLiteral("shown")));
  m_window->passShowHandler(kOtpEntry + QStringLiteral("leftover line\n"),
                            QStringLiteral("shown"));
  auto *browser =
      m_window->findChild<QTextBrowser *>(QStringLiteral("textBrowser"));
  QVERIFY(browser != nullptr);
  QVERIFY(browser->toPlainText().contains(QStringLiteral("leftover line")));

  // Then a planted link, clicked. The refusal's message box is dismissed
  // from a timer so the click can return.
  QTemporaryDir outside;
  QVERIFY(outside.isValid());
  {
    QFile f(outside.filePath(QStringLiteral("secret.gpg")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
  }
  const QString bank =
      QDir(m_storeDir.path()).filePath(QStringLiteral("Bank.gpg"));
  QVERIFY(QFile::link(outside.filePath(QStringLiteral("secret.gpg")), bank));
  const auto cleanup = qScopeGuard([&bank] { QFile::remove(bank); });
  auto *tree = m_window->findChild<QTreeView *>(QStringLiteral("treeView"));
  auto *proxy = qobject_cast<QSortFilterProxyModel *>(tree->model());
  auto *fs = qobject_cast<QFileSystemModel *>(proxy->sourceModel());
  QModelIndex src;
  QTRY_VERIFY_WITH_TIMEOUT((src = fs->index(bank)).isValid(), 5000);
  tree->setCurrentIndex(proxy->mapFromSource(src));
  int boxes = 0;
  QTimer poker;
  poker.setInterval(20);
  QObject::connect(&poker, &QTimer::timeout, [&boxes]() {
    if (auto *box =
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
      ++boxes;
      box->accept();
    }
  });
  poker.start();
  QMetaObject::invokeMethod(m_window.data(), "on_treeView_clicked",
                            Qt::DirectConnection,
                            Q_ARG(QModelIndex, tree->currentIndex()));
  QTRY_VERIFY_WITH_TIMEOUT(boxes == 1, 5000);
  poker.stop();

  QCOMPARE(
      m_window->findChild<QLabel *>(QStringLiteral("passwordName"))->text(),
      QStringLiteral("Bank"));
  QVERIFY2(!browser->toPlainText().contains(QStringLiteral("leftover line")),
           qPrintable(browser->toPlainText()));
  QVERIFY2(browser->toPlainText().contains(QStringLiteral("link")),
           "the reason is what the browser shows");
  QTRY_VERIFY_WITH_TIMEOUT(tree->isEnabled(), 3000);
  // No code on the panel and nothing pending: onOtp() decrypts, which is
  // refused again, and copies nothing.
  // QFileSystemModel re-resolves a fresh symlink node when the directory
  // changes under it, which can retire the index: select the row again, as a
  // user with the row highlighted has it.
  QTRY_VERIFY_WITH_TIMEOUT((src = fs->index(bank)).isValid(), 5000);
  tree->setCurrentIndex(proxy->mapFromSource(src));
  poker.start();
  // Only critical() is counted: the window's start-up gpg calls (key listing
  // with the real gpg) can finish, and fail, inside these nested loops on a
  // slow runner, and they report through processErrorExit.
  QSignalSpy critSpy(QtPassSettings::getPass(), &Pass::critical);
  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onOtp",
                                    Qt::DirectConnection));
  QCOMPARE(critSpy.count(), 1);
  QTRY_VERIFY_WITH_TIMEOUT(boxes == 2, 5000);
  poker.stop();
  QCOMPARE(clip->text(), QStringLiteral("sentinel"));
  QTRY_VERIFY_WITH_TIMEOUT(tree->isEnabled(), 3000);
#endif
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

  // PaletteChange reaches the window through the event loop, and on some
  // platforms (Windows, seen on CI) not within a single processEvents(); the
  // frame may also have been rebuilt by then. Wait for any field frame to
  // carry the new colour rather than the one pointer to carry it at once.
  const auto frameWithColour = [this, &mid]() -> QFrame * {
    for (QFrame *candidate : m_window->findChildren<QFrame *>()) {
      if (candidate->styleSheet().contains(mid.name())) {
        return candidate;
      }
    }
    return nullptr;
  };
  QTRY_VERIFY2_WITH_TIMEOUT(
      frameWithColour() != nullptr,
      qPrintable(QStringLiteral("no field frame took the palette's mid "
                                "colour; first frame has: ") +
                 frame->styleSheet()),
      3000);
}

/**
 * @brief The menu bar and the toolbar, the two bars Breeze paints as its
 * "tools area", each as a test row.
 */
void tst_mainwindow::toolsAreaBars() {
  QTest::addColumn<QString>("bar");
  QTest::newRow("toolBar") << QStringLiteral("toolBar");
  QTest::newRow("menuBar") << QStringLiteral("menuBar");
}

auto tst_mainwindow::toolsAreaBar(const QString &name) -> QWidget * {
  if (name == QLatin1String("menuBar")) {
    return m_window->menuBar();
  }
  return m_window->findChild<QToolBar *>(name);
}

/**
 * @brief A bar palette left over from the previous theme is dropped.
 *
 * KDE's Breeze style stamps a "header" palette on the menu bar and top
 * toolbars and, after a runtime light/dark switch, re-applies it from a
 * cached kdeglobals — i.e. with the old theme's colours (#1669 for the
 * toolbar, #1868 for the menu bar). Simulate that: switch the application
 * palette and let that settle, then mark the bar the way Breeze does, give
 * it a dark palette while the application is light, and check that
 * MainWindow resets it on the bar's own PaletteChange. The menu bar is
 * hidden by default and must be reset all the same, so that showing it
 * later shows the right theme.
 */
void tst_mainwindow::toolsAreaBarDropsStaleStylePaletteAfterThemeSwitch_data() {
  toolsAreaBars();
}

void tst_mainwindow::toolsAreaBarDropsStaleStylePaletteAfterThemeSwitch() {
  QFETCH(QString, bar);
  QWidget *widget = toolsAreaBar(bar);
  QVERIFY2(widget != nullptr, "MainWindow must have the bar");
#ifndef Q_OS_MACOS
  QCOMPARE(widget->isHidden(), bar == QLatin1String("menuBar"));
#endif
  const QPalette original = QApplication::palette();
  auto restore =
      qScopeGuard([&original] { QApplication::setPalette(original); });

  QPalette light = original;
  light.setColor(QPalette::Window, QColor(0xef, 0xf0, 0xf1));
  QApplication::setPalette(light);
  // Let the switch reach the window and its bars before the stale stamp:
  // what follows must be caught by the bar's own PaletteChange, not by the
  // toolbar's from the switch itself.
  QTest::qWait(50);

  QPalette staleDark = light;
  staleDark.setColor(QPalette::Window, QColor(0x29, 0x2c, 0x30));
  widget->setProperty("breeze_has_toolsarea_palette", true);
  widget->setPalette(staleDark);
  QVERIFY2(widget->testAttribute(Qt::WA_SetPalette),
           "the stale stamp must have taken before the reset is judged");

  QTRY_VERIFY2(!widget->testAttribute(Qt::WA_SetPalette),
               "stale dark bar palette must be dropped on a light app");
  QCOMPARE(widget->palette().color(QPalette::Window),
           light.color(QPalette::Window));
  QVERIFY2(widget->autoFillBackground(),
           "bar must paint its own background over the style's stale "
           "tools-area fill");
  QVERIFY2(widget->backgroundRole() == QPalette::Window,
           "that background is the window colour, not the bar's default "
           "Button role");
}

/**
 * @brief A header tint in the same theme as the application is left alone.
 */
void tst_mainwindow::toolsAreaBarKeepsHeaderTintInSameTheme_data() {
  toolsAreaBars();
}

void tst_mainwindow::toolsAreaBarKeepsHeaderTintInSameTheme() {
  QFETCH(QString, bar);
  QWidget *widget = toolsAreaBar(bar);
  QVERIFY2(widget != nullptr, "MainWindow must have the bar");
  const QPalette original = QApplication::palette();
  auto restore =
      qScopeGuard([&original] { QApplication::setPalette(original); });

  QPalette light = original;
  light.setColor(QPalette::Window, QColor(0xef, 0xf0, 0xf1));
  QApplication::setPalette(light);
  QTest::qWait(50);

  QPalette headerTint = light;
  headerTint.setColor(QPalette::Window, QColor(0xe3, 0xe5, 0xe7));
  widget->setPalette(headerTint);

  QTest::qWait(50); // let the deferred check run
  QVERIFY2(widget->testAttribute(Qt::WA_SetPalette),
           "a same-theme header tint must be kept");
  QCOMPARE(widget->palette().color(QPalette::Window), QColor(0xe3, 0xe5, 0xe7));
}

/**
 * @brief Before any application palette change, a header palette is the
 *        theme's own however far it sits from the window colour: a scheme
 *        with a dark header on a light body is stamped at startup by the
 *        same Breeze code, and QtPass must not undo the scheme.
 */
void tst_mainwindow::toolsAreaBarKeepsAnyHeaderBeforeAThemeSwitch_data() {
  toolsAreaBars();
}

void tst_mainwindow::toolsAreaBarKeepsAnyHeaderBeforeAThemeSwitch() {
  QFETCH(QString, bar);
  QWidget *widget = toolsAreaBar(bar);
  QVERIFY2(widget != nullptr, "MainWindow must have the bar");
  QVERIFY2(!widget->testAttribute(Qt::WA_SetPalette),
           "a fresh window has no bar palette of its own");

  QPalette darkHeader = QApplication::palette();
  darkHeader.setColor(QPalette::Window, QColor(0x31, 0x36, 0x3b));
  widget->setProperty("breeze_has_toolsarea_palette", true);
  widget->setPalette(darkHeader);

  QTest::qWait(50); // let the deferred check run
  QVERIFY2(widget->testAttribute(Qt::WA_SetPalette),
           "no theme switch happened, so the header is the scheme's");
  QCOMPARE(widget->palette().color(QPalette::Window), QColor(0x31, 0x36, 0x3b));
  QVERIFY2(!widget->autoFillBackground(),
           "a kept header is painted by the style, not by the bar itself");
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
 * @brief Settings > Show process output shows and hides the console dock,
 *        remembers the choice, and follows a change made elsewhere.
 */
void tst_mainwindow::processOutputIsToggledFromTheSettingsMenu() {
  auto *toggle =
      m_window->findChild<QAction *>(QStringLiteral("actionShowProcessOutput"));
  QVERIFY(toggle != nullptr);
  QVERIFY(toggle->isCheckable());
  auto *dock = m_window->findChild<ProcessOutputPanel *>();
  QVERIFY(dock != nullptr);
  const bool initial = toggle->isChecked();
  QCOMPARE(dock->isVisibleTo(m_window.get()), initial);

  toggle->trigger();
  QCOMPARE(dock->isVisibleTo(m_window.get()), !initial);
  QCOMPARE(QtPassSettings::load().showProcessOutput, !initial);
  toggle->trigger();
  QCOMPARE(dock->isVisibleTo(m_window.get()), initial);
  QCOMPARE(QtPassSettings::load().showProcessOutput, initial);
}

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

namespace {
/** @brief A fresh, active main window with the menu bar hidden by choice. */
auto bareActiveWindow(QScopedPointer<MainWindow> &window) -> bool {
  AppSettings s = QtPassSettings::load();
  s.showMenuBar = false;
  QtPassSettings::save(s);
  window.reset(new MainWindow);
  window->show();
  if (!QTest::qWaitForWindowExposed(window.data())) {
    return false;
  }
  window->activateWindow();
  return QTest::qWaitForWindowActive(window.data()) &&
         !window->menuBar()->isVisible();
}
} // namespace

/**
 * @brief A lone Alt tap shows the hidden menu bar, a second one hides it, and
 *        the stored choice stays "hidden" throughout.
 */
void tst_mainwindow::altTapShowsTheHiddenMenuBarUntilTappedAgain() {
#ifdef Q_OS_MACOS
  QSKIP("the menu bar is the system's on macOS");
#else
  QVERIFY(bareActiveWindow(m_window));
  QMenuBar *bar = m_window->menuBar();

  QTest::keyPress(m_window.get(), Qt::Key_Alt);
  QVERIFY2(bar->isVisible(), "Alt shows the bar at once, for mnemonics");
  QTest::keyRelease(m_window.get(), Qt::Key_Alt);
  QTest::qWait(50);
  QVERIFY2(bar->isVisible(), "and a lone tap leaves it up");
  QVERIFY(!QtPassSettings::load().showMenuBar);

  QTest::keyClick(m_window.get(), Qt::Key_Alt);
  QTRY_VERIFY2(!bar->isVisible(), "the second tap hides it again");
  QVERIFY(!QtPassSettings::load().showMenuBar);
#endif
}

/**
 * @brief Escape ends a peek, and so does an Alt shortcut that was not a menu,
 *        such as Alt+Left in the tree.
 */
void tst_mainwindow::altPeekEndsOnEscapeAndAfterAShortcut() {
#ifdef Q_OS_MACOS
  QSKIP("the menu bar is the system's on macOS");
#else
  QVERIFY(bareActiveWindow(m_window));
  QMenuBar *bar = m_window->menuBar();

  QTest::keyClick(m_window.get(), Qt::Key_Alt);
  QVERIFY(bar->isVisible());
  QTest::keyClick(m_window.get(), Qt::Key_Escape);
  QTRY_VERIFY2(!bar->isVisible(), "Escape hides the peeking bar");

  QTest::keyPress(m_window.get(), Qt::Key_Alt);
  QVERIFY(bar->isVisible());
  QTest::keyClick(m_window.get(), Qt::Key_Left, Qt::AltModifier);
  QTest::keyRelease(m_window.get(), Qt::Key_Alt);
  QTRY_VERIFY2(!bar->isVisible(),
               "an Alt shortcut that opened no menu does not leave it up");
#endif
}

/**
 * @brief Alt+F opens the File menu although the bar was hidden, and the bar
 *        goes again once the menu closes.
 */
void tst_mainwindow::altMnemonicOpensAMenuWhileTheBarIsHidden() {
#ifdef Q_OS_MACOS
  QSKIP("the menu bar is the system's on macOS");
#else
  QVERIFY(bareActiveWindow(m_window));
  QMenuBar *bar = m_window->menuBar();
  auto *fileMenu = m_window->findChild<QMenu *>(QStringLiteral("menuFile"));
  QVERIFY(fileMenu != nullptr);

  QTest::keyPress(m_window.get(), Qt::Key_Alt);
  QTest::keyClick(m_window.get(), Qt::Key_F, Qt::AltModifier);
  QTRY_VERIFY2(fileMenu->isVisible(), "Alt+F opens File");
  QTest::keyRelease(fileMenu, Qt::Key_Alt);
  QVERIFY2(bar->isVisible(), "the bar stays while its menu is open");

  fileMenu->close();
  QTRY_VERIFY2(!bar->isVisible(), "and goes when the menu closes");
  QVERIFY(!QtPassSettings::load().showMenuBar);
#endif
}

/**
 * @brief With the bar shown by choice Alt taps change nothing: they must not
 *        hide what the user asked for.
 */
void tst_mainwindow::altLeavesAMenuBarShownByChoiceAlone() {
#ifdef Q_OS_MACOS
  QSKIP("the menu bar is the system's on macOS");
#else
  QVERIFY(bareActiveWindow(m_window));
  auto *toggle =
      m_window->findChild<QAction *>(QStringLiteral("actionShowMenuBar"));
  QVERIFY(toggle != nullptr);

  // Checking "Show menu bar" during a peek keeps the bar for good.
  QTest::keyClick(m_window.get(), Qt::Key_Alt);
  toggle->trigger();
  QVERIFY(QtPassSettings::load().showMenuBar);

  QTest::keyClick(m_window.get(), Qt::Key_Alt);
  QTest::keyClick(m_window.get(), Qt::Key_Alt);
  QTest::keyClick(m_window.get(), Qt::Key_Escape);
  QTest::qWait(50);
  QVERIFY2(m_window->menuBar()->isVisible(), "Alt and Escape leave it alone");
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
                        QStringLiteral("actionShowMenuBar"),
                        QStringLiteral("actionShowProcessOutput")}));
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

// ---------------------------------------------------------------------------
// Helpers for the tests below

/**
 * @brief Tear the window down, change the settings it will be built on, and
 * build it again. save() also drops the cached backend, so the new window's
 * Pass takes the changed settings too.
 */
void tst_mainwindow::rebuildWindow(
    const std::function<void(AppSettings &)> &tweak) {
  m_window.reset();
  AppSettings s = QtPassSettings::load();
  tweak(s);
  QtPassSettings::save(s);
  m_window.reset(new MainWindow);
}

/**
 * @brief Rebuild the window over a fake gpg written into @p scratch, so
 * every decrypt the window asks for succeeds with the file's own bytes and
 * no keyring or agent is touched.
 * @return The fake gpg's path; empty when it could not be written.
 */
auto tst_mainwindow::rebuildWithFakeGpg(
    QTemporaryDir &scratch, const std::function<void(AppSettings &)> &tweak)
    -> QString {
  const QString gpg = writeFakeGpg(scratch.path());
  if (gpg.isEmpty()) {
    return {};
  }
  rebuildWindow([&](AppSettings &s) {
    s.gpgExecutable = gpg;
    s.useGit = false;
    s.hideContent = false;
    s.displayAsIs = false;
    s.useAutoclearPanel = false;
    s.useSelection = false;
    s.useAutoclear = false;
    s.clipBoardType = Enums::CLIPBOARD_NEVER;
    if (tweak) {
      tweak(s);
    }
  });
  return gpg;
}

/// Make the entry or folder at @p absolutePath the tree's current index.
auto tst_mainwindow::selectPath(const QString &absolutePath) -> bool {
  auto *tree = treeView();
  auto *proxy = qobject_cast<QSortFilterProxyModel *>(tree->model());
  auto *fs = qobject_cast<QFileSystemModel *>(proxy->sourceModel());
  QModelIndex src;
  QTRY_VERIFY_WITH_TIMEOUT_RETURN((src = fs->index(absolutePath)).isValid(),
                                  5000, false);
  tree->setCurrentIndex(proxy->mapFromSource(src));
  return tree->currentIndex().isValid();
}

auto tst_mainwindow::treeView() -> QTreeView * {
  return m_window->findChild<QTreeView *>(QStringLiteral("treeView"));
}

auto tst_mainwindow::browser() -> QTextBrowser * {
  return m_window->findChild<QTextBrowser *>(QStringLiteral("textBrowser"));
}

auto tst_mainwindow::searchBox() -> QLineEdit * {
  return m_window->findChild<QLineEdit *>(QStringLiteral("lineEdit"));
}

auto tst_mainwindow::passwordName() -> QLabel * {
  return m_window->findChild<QLabel *>(QStringLiteral("passwordName"));
}

// ---------------------------------------------------------------------------

/**
 * @brief The constructor honours the monospace and no-line-wrapping
 *        preferences straight away, not only after a Settings round trip.
 */
void tst_mainwindow::constructorAppliesMonospaceAndNoWrap() {
  rebuildWindow([](AppSettings &s) {
    s.useMonospace = true;
    s.noLineWrapping = true;
  });
  QCOMPARE(browser()->font().styleHint(), QFont::Monospace);
  QCOMPARE(browser()->lineWrapMode(), QTextBrowser::NoWrap);

  rebuildWindow([](AppSettings &s) {
    s.useMonospace = false;
    s.noLineWrapping = false;
  });
  QVERIFY2(browser()->font().styleHint() != QFont::Monospace,
           "the default font comes back when the preference is off");
  QCOMPARE(browser()->lineWrapMode(), QTextBrowser::WidgetWidth);
}

/**
 * @brief Backend chatter lands in the console panel, stdout and stderr
 *        alike, except for the processes whose output may hold secrets:
 *        a decrypt's plaintext must never end up in a long-lived panel.
 */
void tst_mainwindow::backendOutputReachesTheConsoleUnlessSensitive() {
  auto *console =
      m_window->findChild<QTextEdit *>(QStringLiteral("processOutputEdit"));
  QVERIFY2(console != nullptr, "processOutputEdit must exist");

  emit QtPassSettings::getPass() -> finishedAnyWithPid(
      QStringLiteral("pull-stdout-line"), QStringLiteral("pull-stderr-line"),
      Enums::GIT_PULL);
  const QString shown = console->toPlainText();
  QVERIFY2(shown.contains(QStringLiteral("pull-stdout-line")),
           qPrintable(shown));
  QVERIFY2(shown.contains(QStringLiteral("pull-stderr-line")),
           qPrintable(shown));

  emit QtPassSettings::getPass()
      -> finishedAnyWithPid(QStringLiteral("hunter2-plaintext"),
                            QStringLiteral("gpg: decrypted"), Enums::PASS_SHOW);
  QVERIFY2(!console->toPlainText().contains(QStringLiteral("hunter2")),
           "a decrypt's output must not reach the console");
  QVERIFY2(!console->toPlainText().contains(QStringLiteral("decrypted")),
           "not even its stderr");
}

/**
 * @brief Help > FAQ opens the FAQ page of the website. The desktop's URL
 *        handler is replaced for the test so no browser starts.
 */
void tst_mainwindow::faqOpensTheWebsite() {
  UrlCatcher catcher;
  QDesktopServices::setUrlHandler(QStringLiteral("https"), &catcher,
                                  "catchUrl");
  const auto restore = qScopeGuard(
      [] { QDesktopServices::unsetUrlHandler(QStringLiteral("https")); });

  auto *faq = m_window->findChild<QAction *>(QStringLiteral("actionFaq"));
  QVERIFY(faq != nullptr);
  faq->trigger();
  QCOMPARE(catcher.urls.size(), 1);
  QCOMPARE(catcher.urls.first(),
           QUrl(QStringLiteral("https://qtpass.org/faq")));
}

/**
 * @brief A click that lands on nothing (the tree's current index invalid)
 *        empties the panel and the name and leaves Edit disabled; Delete
 *        stays enabled as before.
 */
void tst_mainwindow::clickingNothingClearsThePanel() {
  m_window->passShowHandler(QStringLiteral("secret\nleftover line"));
  QVERIFY(browser()->toPlainText().contains(QStringLiteral("leftover line")));
  passwordName()->setText(QStringLiteral("stale name"));

  treeView()->setCurrentIndex(QModelIndex());
  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "on_treeView_clicked",
                                    Qt::DirectConnection,
                                    Q_ARG(QModelIndex, QModelIndex())));
  QVERIFY2(passwordName()->text().isEmpty(),
           qPrintable(passwordName()->text()));
  QVERIFY2(browser()->toPlainText().isEmpty(),
           qPrintable(browser()->toPlainText()));
  auto *edit = m_window->findChild<QAction *>(QStringLiteral("actionEdit"));
  auto *del = m_window->findChild<QAction *>(QStringLiteral("actionDelete"));
  QVERIFY(edit != nullptr && del != nullptr);
  QVERIFY(!edit->isEnabled());
  QVERIFY(del->isEnabled());
}

/**
 * @brief A slower decrypt of an entry the user has since left must not
 *        repaint the panel: only the entry most recently asked for may.
 */
void tst_mainwindow::staleDecryptDoesNotRepaintThePanel() {
  ShowAnswers answers;
  QVERIFY(
      selectEntry(m_window.data(), m_storeDir.path(), QStringLiteral("fresh")));
  QVERIFY(QMetaObject::invokeMethod(
      m_window.data(), "on_treeView_clicked", Qt::DirectConnection,
      Q_ARG(QModelIndex, treeView()->currentIndex())));
  QCOMPARE(passwordName()->text(), QStringLiteral("fresh"));

  m_window->passShowHandler(QStringLiteral("old-secret\nstale leftover"),
                            QStringLiteral("stale"));
  QVERIFY2(!browser()->toPlainText().contains(QStringLiteral("stale leftover")),
           qPrintable(browser()->toPlainText()));

  m_window->passShowHandler(QStringLiteral("new-secret\nfresh leftover"),
                            QStringLiteral("fresh"));
  QVERIFY2(browser()->toPlainText().contains(QStringLiteral("fresh leftover")),
           qPrintable(browser()->toPlainText()));
  QVERIFY2(answers.waitFor(1), "the click's own decrypt must come back");
}

/**
 * @brief With "hide content" on, the panel shows a placeholder instead of the
 *        entry, and with the panel autoclear on, the placeholder gives way
 *        to the "hidden" notice when the timer fires.
 */
void tst_mainwindow::hiddenContentIsClearedFromThePanelOnTheTimer() {
  rebuildWindow([](AppSettings &s) {
    s.hideContent = true;
    s.useAutoclearPanel = true;
    s.autoclearPanelSeconds = 1;
    s.clipBoardType = Enums::CLIPBOARD_NEVER;
  });
  const auto restore = qScopeGuard([] {
    AppSettings s = QtPassSettings::load();
    s.hideContent = false;
    s.useAutoclearPanel = false;
    QtPassSettings::save(s);
  });

  m_window->passShowHandler(QStringLiteral("secret\nurl: example.org"));
  QString shown = browser()->toPlainText();
  QVERIFY2(shown.contains(QStringLiteral("Content hidden")), qPrintable(shown));
  QVERIFY2(!shown.contains(QStringLiteral("example.org")), qPrintable(shown));

  QTRY_VERIFY_WITH_TIMEOUT(browser()->toPlainText().contains(
                               QStringLiteral("Password and content hidden")),
                           3000);
}

/**
 * @brief An OTP request answered with empty content is a failed decrypt,
 *        which is said as such, and the interface is released again.
 */
void tst_mainwindow::otpAnswerWithoutContentReportsTheFailedDecrypt() {
  AppSettings s = QtPassSettings::load();
  s.useOtp = true;
  QtPassSettings::save(s);
  ShowAnswers answers;
  QVERIFY(armOtpRequest(m_window.data(), m_storeDir.path(),
                        QStringLiteral("otp-empty")));
  QVERIFY(!treeView()->isEnabled());

  m_window->otpFromFileToClipboard(QString(), QStringLiteral("otp-empty"));
  QVERIFY2(browser()->toPlainText().contains(
               QStringLiteral("Could not decrypt this password entry")),
           qPrintable(browser()->toPlainText()));
  QVERIFY(treeView()->isEnabled());
  QVERIFY2(answers.waitFor(1), "Ctrl+G's own decrypt must come back");
}

/**
 * @brief When the panel shows no code (content hidden) the answer is parsed
 *        afresh; an entry without an otpauth line yields a clear message and
 *        leaves the clipboard alone.
 */
void tst_mainwindow::otpAnswerWithoutACodeSaysSo() {
  rebuildWindow([](AppSettings &s) {
    s.useOtp = true;
    s.hideContent = true;
    s.useSelection = false;
    s.useAutoclear = false;
    s.clipBoardType = Enums::CLIPBOARD_ON_DEMAND;
  });
  const auto restore = qScopeGuard([] {
    AppSettings s = QtPassSettings::load();
    s.hideContent = false;
    QtPassSettings::save(s);
  });
  QClipboard *clip = QApplication::clipboard();
  clip->setText(QStringLiteral("sentinel"));

  ShowAnswers answers;
  QVERIFY(armOtpRequest(m_window.data(), m_storeDir.path(),
                        QStringLiteral("otp-none")));
  m_window->passShowHandler(QStringLiteral("hunter2\nlogin: alice\n"),
                            QStringLiteral("otp-none"));
  m_window->otpFromFileToClipboard(QStringLiteral("hunter2\nlogin: alice\n"),
                                   QStringLiteral("otp-none"));
  QVERIFY2(browser()->toPlainText().contains(
               QStringLiteral("No OTP code found in this password entry")),
           qPrintable(browser()->toPlainText()));
  QCOMPARE(clip->text(), QStringLiteral("sentinel"));

  // The same, with an otpauth line: parsed and copied without a visible row.
  QVERIFY(armOtpRequest(m_window.data(), m_storeDir.path(),
                        QStringLiteral("otp-hidden")));
  m_window->otpFromFileToClipboard(kOtpEntry, QStringLiteral("otp-hidden"));
  QVERIFY2(looksLikeOtpCode(clip->text()),
           qPrintable("expected a six-digit code, got: " + clip->text()));
  QVERIFY2(answers.waitFor(2), "both Ctrl+G decrypts must come back");
}

/**
 * @brief Ctrl+G without a selected entry, or with OTP switched off, says so
 *        in the panel instead of failing silently.
 */
void tst_mainwindow::otpNeedsASelectionAndTheSetting() {
  m_window->deselect();
  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onOtp",
                                    Qt::DirectConnection));
  QVERIFY2(browser()->toPlainText().contains(
               QStringLiteral("No password selected for OTP generation")),
           qPrintable(browser()->toPlainText()));

  {
    AppSettings s = QtPassSettings::load();
    s.useOtp = false;
    QtPassSettings::save(s);
  }
  const auto restore = qScopeGuard([] {
    AppSettings s = QtPassSettings::load();
    s.useOtp = true;
    QtPassSettings::save(s);
  });
  QVERIFY(selectEntry(m_window.data(), m_storeDir.path(),
                      QStringLiteral("otp-off")));
  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onOtp",
                                    Qt::DirectConnection));
  QVERIFY2(browser()->toPlainText().contains(
               QStringLiteral("No OTP code found in this password entry")),
           qPrintable(browser()->toPlainText()));
  QVERIFY2(treeView()->isEnabled(), "nothing was started, nothing to wait for");
}

/**
 * @brief A double-click on the panel edits the shown entry only while Edit
 *        is available; with nothing selected it must not open a dialog.
 */
void tst_mainwindow::editRequestIsIgnoredWhileEditIsDisabled() {
  auto *panel = m_window->findChild<PasswordDisplayPanel *>();
  QVERIFY(panel != nullptr);
  auto *edit = m_window->findChild<QAction *>(QStringLiteral("actionEdit"));
  QVERIFY(edit != nullptr);
  m_window->deselect();
  QVERIFY(!edit->isEnabled());

  int dialogs = 0;
  ModalDriver driver([&dialogs](QWidget *w) {
    ++dialogs;
    answerBox(QMessageBox::Ok)(w);
  });
  emit panel->editRequested();
  QTest::qWait(100);
  QCOMPARE(dialogs, 0);
}

/**
 * @brief Editing an entry with Git and auto-pull on first pulls, blocking,
 *        and a failed pull is reported in the status bar; the edit dialog
 *        then opens for the entry and asks the backend to decrypt it.
 */
void tst_mainwindow::editPullsFirstWhenAutoPullIsOn() {
#ifdef Q_OS_WIN
  QSKIP("uses shell scripts as the gpg and git stand-ins");
#else
  QTemporaryDir scratch;
  QVERIFY(scratch.isValid());
  const QString log = scratch.filePath(QStringLiteral("git.log"));
  const QString git = writeFakeGit(scratch.path(), log, 1);
  QVERIFY(!git.isEmpty());
  QVERIFY(!rebuildWithFakeGpg(scratch, [&git](AppSettings &s) {
             s.useGit = true;
             s.autoPull = true;
             s.gitExecutable = git;
           }).isEmpty());
  const auto restore = qScopeGuard([] {
    AppSettings s = QtPassSettings::load();
    s.useGit = false;
    s.autoPull = false;
    QtPassSettings::save(s);
  });
  QVERIFY(selectEntry(m_window.data(), m_storeDir.path(),
                      QStringLiteral("edit-me")));
  m_window->setUiElementsEnabled(true);
  auto *edit = m_window->findChild<QAction *>(QStringLiteral("actionEdit"));
  QVERIFY(edit != nullptr && edit->isEnabled());

  QString dialogTitle;
  ModalDriver driver([&dialogTitle](QWidget *w) {
    if (auto *dialog = qobject_cast<PasswordDialog *>(w)) {
      dialogTitle = dialog->windowTitle();
      dialog->reject();
      return;
    }
    answerBox(QMessageBox::Ok)(w);
  });
  QSignalSpy shown(QtPassSettings::getPass(), &Pass::finishedShow);
  auto *panel = m_window->findChild<PasswordDisplayPanel *>();
  QVERIFY(panel != nullptr);
  emit panel->editRequested();

  QVERIFY2(dialogTitle.contains(QStringLiteral("edit-me")),
           qPrintable(QStringLiteral("dialog title: ") + dialogTitle));
  const auto calls = fakeGitCalls(log);
  QVERIFY2(calls.size() == 1,
           qPrintable(QStringLiteral("one blocking pull expected, git ran %1 "
                                     "times")
                          .arg(calls.size())));
  // The blocking pull names the store with -C: Executor::executeBlocking
  // sets no working directory.
  const QStringList args = calls.first().second.split(u' ');
  QCOMPARE(args.size(), 3);
  QCOMPARE(args.at(0), QStringLiteral("-C"));
  QCOMPARE(QDir::cleanPath(args.at(1)), QDir::cleanPath(m_storeDir.path()));
  QCOMPARE(args.at(2), QStringLiteral("pull"));
  const QString status = m_window->statusBar()->currentMessage();
  QVERIFY2(status.contains(QStringLiteral("Git pull failed")),
           qPrintable(status));
  QVERIFY2(status.contains(QStringLiteral("fake git says no")),
           qPrintable(status));
  QVERIFY2(shown.count() == 1 || shown.wait(3000),
           "the dialog's decrypt must have been asked for");
#endif
}

/**
 * @brief A double-click on an entry opens the edit dialog for it and asks
 *        the backend to decrypt it; a double-click on a folder opens nothing.
 */
void tst_mainwindow::doubleClickEditsAnEntryButNotAFolder() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as the gpg stand-in");
#else
  QTemporaryDir scratch;
  QVERIFY(scratch.isValid());
  QVERIFY(!rebuildWithFakeGpg(scratch).isEmpty());
  const QDir store(m_storeDir.path());
  QVERIFY(store.mkpath(QStringLiteral("dbl-folder")));
  QVERIFY(selectPath(store.filePath(QStringLiteral("dbl-folder"))));

  int dialogs = 0;
  QString dialogTitle;
  ModalDriver driver([&](QWidget *w) {
    ++dialogs;
    if (auto *dialog = qobject_cast<PasswordDialog *>(w)) {
      dialogTitle = dialog->windowTitle();
      dialog->reject();
      return;
    }
    answerBox(QMessageBox::Ok)(w);
  });
  QVERIFY(QMetaObject::invokeMethod(
      m_window.data(), "on_treeView_doubleClicked", Qt::DirectConnection,
      Q_ARG(QModelIndex, treeView()->currentIndex())));
  QTest::qWait(100);
  QCOMPARE(dialogs, 0);

  QVERIFY(selectEntry(m_window.data(), m_storeDir.path(),
                      QStringLiteral("dbl-entry")));
  QSignalSpy shown(QtPassSettings::getPass(), &Pass::finishedShow);
  QVERIFY(QMetaObject::invokeMethod(
      m_window.data(), "on_treeView_doubleClicked", Qt::DirectConnection,
      Q_ARG(QModelIndex, treeView()->currentIndex())));
  QCOMPARE(dialogs, 1);
  QVERIFY2(dialogTitle.contains(QStringLiteral("dbl-entry")),
           qPrintable(QStringLiteral("dialog title: ") + dialogTitle));
  QVERIFY2(shown.count() == 1 || shown.wait(5000),
           "the dialog's decrypt must have been asked for");
  QCOMPARE(shown.first().at(1).toString(), QStringLiteral("dbl-entry"));
#endif
}

/**
 * @brief The Pull and Push actions run `git pull` and `git push` in the
 *        store through the backend, with the status bar saying so.
 */
void tst_mainwindow::pullAndPushRunGitInTheStore() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as the git stand-in");
#else
  QTemporaryDir scratch;
  QVERIFY(scratch.isValid());
  const QString log = scratch.filePath(QStringLiteral("git.log"));
  const QString git = writeFakeGit(scratch.path(), log, 0);
  QVERIFY(!git.isEmpty());
  rebuildWindow([&git](AppSettings &s) {
    s.useGit = true;
    s.gitExecutable = git;
  });
  const auto restore = qScopeGuard([] {
    AppSettings s = QtPassSettings::load();
    s.useGit = false;
    QtPassSettings::save(s);
  });
  auto *pull = m_window->findChild<QAction *>(QStringLiteral("actionUpdate"));
  auto *push = m_window->findChild<QAction *>(QStringLiteral("actionPush"));
  QVERIFY(pull != nullptr && push != nullptr);

  QSignalSpy pulled(QtPassSettings::getPass(), &Pass::finishedGitPull);
  pull->trigger();
  QCOMPARE(m_window->statusBar()->currentMessage(),
           QStringLiteral("Updating password-store"));
  QVERIFY2(pulled.count() == 1 || pulled.wait(5000), "git pull must finish");

  QSignalSpy pushed(QtPassSettings::getPass(), &Pass::finishedGitPush);
  push->trigger();
  QVERIFY2(pushed.count() == 1 || pushed.wait(5000), "git push must finish");

  // Both ran in the store (the executor's working directory), as bare
  // `git pull` and `git push`.
  const auto calls = fakeGitCalls(log);
  QCOMPARE(calls.size(), 2);
  QCOMPARE(calls.at(0).second, QStringLiteral("pull"));
  QCOMPARE(calls.at(1).second, QStringLiteral("push"));
  const QString store = QFileInfo(m_storeDir.path()).canonicalFilePath();
  QCOMPARE(calls.at(0).first, store);
  QCOMPARE(calls.at(1).first, store);
#endif
}

/**
 * @brief Toggling content search re-labels the search box, tells which regex
 *        dialect applies (PCRE natively, POSIX BRE through pass), and
 *        toggling it off restores the file filter's box.
 */
void tst_mainwindow::grepModeChangesTheSearchBox() {
  auto *grep = m_window->findChild<QToolButton *>(QStringLiteral("grepButton"));
  QVERIFY(grep != nullptr && grep->isCheckable());
  auto *search = searchBox();
  search->setText(QStringLiteral("leftover filter"));

  grep->setChecked(true);
  QCOMPARE(search->placeholderText(), QStringLiteral("Search content (regex)"));
  QVERIFY2(search->toolTip().contains(QStringLiteral("Perl-compatible")),
           qPrintable(search->toolTip()));
  QVERIFY2(search->text().isEmpty(), "entering grep mode clears the filter");

  grep->setChecked(false);
  QCOMPARE(search->placeholderText(), QStringLiteral("Search password"));
  QVERIFY(search->toolTip().isEmpty());
  QVERIFY(treeView()->isVisibleTo(m_window.data()));

  // The pass backend greps with POSIX basic regular expressions.
  QtPassSettings::setUsePass(true);
  const auto restore = qScopeGuard([] { QtPassSettings::setUsePass(false); });
  grep->setChecked(true);
  QVERIFY2(search->toolTip().contains(QStringLiteral("POSIX")),
           qPrintable(search->toolTip()));
  grep->setChecked(false);
}

/**
 * @brief Enter in content-search mode greps the store through the backend;
 *        the matches are listed per entry, clicking a match opens that entry
 *        in the tree, and Enter on an empty query drops the results.
 */
void tst_mainwindow::grepFindsContentAndNavigatesToTheEntry() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as the gpg stand-in");
#else
  QTemporaryDir scratch;
  QVERIFY(scratch.isValid());
  QVERIFY(!rebuildWithFakeGpg(scratch, [](AppSettings &s) {
             s.useGrepSearch = true;
           }).isEmpty());
  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  QVERIFY(selectEntry(m_window.data(), m_storeDir.path(),
                      QStringLiteral("grep-target")));
  auto *grep = m_window->findChild<QToolButton *>(QStringLiteral("grepButton"));
  auto *results =
      m_window->findChild<QTreeWidget *>(QStringLiteral("grepResultsList"));
  QVERIFY(grep != nullptr && results != nullptr);
  grep->setChecked(true);

  QSignalSpy finished(QtPassSettings::getPass(), &Pass::finishedGrep);
  searchBox()->setText(QStringLiteral("really encrypted"));
  QVERIFY(QMetaObject::invokeMethod(
      m_window.data(), "on_lineEdit_returnPressed", Qt::DirectConnection));
  QCOMPARE(m_window->statusBar()->currentMessage(),
           QStringLiteral("Searching…"));
  QVERIFY2(finished.count() == 1 || finished.wait(10000),
           "the grep must finish");
  QTRY_VERIFY(results->isVisible());
  QVERIFY(!treeView()->isVisible());
  QVERIFY2(results->topLevelItemCount() >= 1, "every fake entry matches");
  QVERIFY2(
      m_window->statusBar()->currentMessage().contains(QStringLiteral("match")),
      qPrintable(m_window->statusBar()->currentMessage()));
  QTreeWidgetItem *entry = nullptr;
  for (int i = 0; i < results->topLevelItemCount(); ++i) {
    if (results->topLevelItem(i)->text(0) == QLatin1String("grep-target")) {
      entry = results->topLevelItem(i);
    }
  }
  QVERIFY2(entry != nullptr, "the entry written for this test is listed");
  QCOMPARE(entry->childCount(), 1);
  QCOMPARE(entry->child(0)->text(0), QStringLiteral("not really encrypted"));

  // Clicking the match line opens the entry: the tree is back, current on
  // it, and the fake decrypt lands in the panel. With the panel autoclear on
  // the matches (which are content) are dropped from the list as well.
  {
    AppSettings s = QtPassSettings::load();
    s.useAutoclearPanel = true;
    QtPassSettings::save(s);
  }
  QSignalSpy shown(QtPassSettings::getPass(), &Pass::finishedShow);
  emit results->itemClicked(entry->child(0), 0);
  QCOMPARE(passwordName()->text(), QStringLiteral("grep-target"));
  QVERIFY(treeView()->isVisible());
  QVERIFY(!results->isVisible());
  QCOMPARE(results->topLevelItemCount(), 0);
  QVERIFY2(shown.count() == 1 || shown.wait(5000), "the entry is decrypted");
  QCOMPARE(shown.first().at(1).toString(), QStringLiteral("grep-target"));
  {
    AppSettings s = QtPassSettings::load();
    s.useAutoclearPanel = false;
    QtPassSettings::save(s);
  }

  // Enter on an empty query while a search runs cancels it: the wait cursor
  // goes, and the results arriving later are discarded.
  finished.clear();
  searchBox()->setText(QStringLiteral("really"));
  QVERIFY(QMetaObject::invokeMethod(
      m_window.data(), "on_lineEdit_returnPressed", Qt::DirectConnection));
  QVERIFY2(QApplication::overrideCursor() != nullptr,
           "a running search shows the wait cursor");
  searchBox()->clear();
  QVERIFY(QMetaObject::invokeMethod(
      m_window.data(), "on_lineEdit_returnPressed", Qt::DirectConnection));
  QVERIFY2(QApplication::overrideCursor() == nullptr,
           "the cancel restores the cursor");
  QVERIFY(!results->isVisible());
  QVERIFY2(finished.count() == 1 || finished.wait(10000),
           "the cancelled grep still finishes");
  QVERIFY2(!results->isVisible() && results->topLevelItemCount() == 0,
           "results of a cancelled search are not shown");
  QVERIFY(treeView()->isEnabled());

  // Leaving grep mode while a search runs restores the cursor too.
  finished.clear();
  searchBox()->setText(QStringLiteral("really"));
  QVERIFY(QMetaObject::invokeMethod(
      m_window.data(), "on_lineEdit_returnPressed", Qt::DirectConnection));
  QVERIFY(QApplication::overrideCursor() != nullptr);
  grep->setChecked(false);
  QVERIFY2(QApplication::overrideCursor() == nullptr,
           "leaving grep mode restores the cursor");
  QVERIFY2(finished.count() == 1 || finished.wait(10000),
           "the abandoned grep still finishes");
  QVERIFY2(!results->isVisible() && results->topLevelItemCount() == 0,
           "and shows nothing outside grep mode");
#endif
}

/**
 * @brief Clearing the panel while grep results are on screen (the deselect
 *        of an empty click) puts the tree back and leaves grep mode, with
 *        the search box relabelled and the grep button released.
 */
void tst_mainwindow::deselectLeavesGrepModeWhenResultsAreShown() {
  rebuildWindow([](AppSettings &s) {
    s.useGrepSearch = true;
    s.useAutoclearPanel = false;
    s.autoclearPanelSeconds = 1;
  });
  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  auto *grep = m_window->findChild<QToolButton *>(QStringLiteral("grepButton"));
  auto *results =
      m_window->findChild<QTreeWidget *>(QStringLiteral("grepResultsList"));
  QVERIFY(grep != nullptr && results != nullptr);
  grep->setChecked(true);

  m_window->onGrepFinished(
      {{QStringLiteral("work/acme/vpn"), {QStringLiteral("login: alice")}}});
  QTRY_VERIFY(results->isVisible());
  QCOMPARE(results->topLevelItemCount(), 1);
  QCOMPARE(results->topLevelItem(0)->child(0)->text(0),
           QStringLiteral("login: alice"));

  m_window->deselect();
  QVERIFY(!results->isVisible());
  QVERIFY(treeView()->isVisible());
  QVERIFY2(!grep->isChecked(), "grep mode is left");
  QCOMPARE(searchBox()->placeholderText(), QStringLiteral("Search password"));

  // No matches: the tree stays and the status bar says so.
  grep->setChecked(true);
  m_window->onGrepFinished({});
  QCOMPARE(m_window->statusBar()->currentMessage(),
           QStringLiteral("No matches found."));
  QVERIFY(!results->isVisible());

  // With the panel autoclear on, matches (content) are cleared by its timer.
  {
    AppSettings s = QtPassSettings::load();
    s.useAutoclearPanel = true;
    QtPassSettings::save(s);
  }
  const auto restore = qScopeGuard([] {
    AppSettings s = QtPassSettings::load();
    s.useAutoclearPanel = false;
    QtPassSettings::save(s);
  });
  m_window->onGrepFinished(
      {{QStringLiteral("work/acme/vpn"), {QStringLiteral("login: alice")}}});
  QVERIFY(results->isVisible());
  QTRY_VERIFY_WITH_TIMEOUT(!results->isVisible(), 3000);
  QCOMPARE(results->topLevelItemCount(), 0);
  QVERIFY2(browser()->toPlainText().contains(
               QStringLiteral("Password and content hidden")),
           qPrintable(browser()->toPlainText()));
  QVERIFY2(!grep->isChecked(), "the timer's clear leaves grep mode too");
}

/**
 * @brief A filter that matches only a folder without any entry selects
 *        nothing: the first-file search comes back empty-handed rather than
 *        selecting the folder, and Edit and Delete stay disabled.
 */
void tst_mainwindow::searchForAFolderWithoutFilesSelectsNothing() {
  QVERIFY(QDir(m_storeDir.path()).mkpath(QStringLiteral("hollow-folder")));
  QVERIFY(selectPath(
      QDir(m_storeDir.path()).filePath(QStringLiteral("hollow-folder"))));
  searchBox()->setText(QStringLiteral("hollow-folder"));
  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onTimeoutSearch",
                                    Qt::DirectConnection));
  auto *tree = treeView();
  QVERIFY2(tree->model()->rowCount(tree->rootIndex()) >= 1,
           "the folder itself matches the filter");
  QVERIFY2(!tree->currentIndex().isValid(),
           "no file to select in a folder without files");
  auto *edit = m_window->findChild<QAction *>(QStringLiteral("actionEdit"));
  auto *del = m_window->findChild<QAction *>(QStringLiteral("actionDelete"));
  QVERIFY(!edit->isEnabled() && !del->isEnabled());

  // Emptying the box collapses the tree and deselects.
  passwordName()->setText(QStringLiteral("stale"));
  searchBox()->clear();
  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onTimeoutSearch",
                                    Qt::DirectConnection));
  QVERIFY(passwordName()->text().isEmpty());
  QVERIFY(!tree->isExpanded(tree->model()->index(0, 0, tree->rootIndex())));
}

/**
 * @brief Enter in the search box opens the first entry the filter left: it
 *        becomes current and is decrypted into the panel.
 */
void tst_mainwindow::enterInTheSearchBoxOpensTheFirstMatch() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as the gpg stand-in");
#else
  QTemporaryDir scratch;
  QVERIFY(scratch.isValid());
  QVERIFY(!rebuildWithFakeGpg(scratch).isEmpty());
  QVERIFY(selectEntry(m_window.data(), m_storeDir.path(),
                      QStringLiteral("enter-me")));
  treeView()->setCurrentIndex(QModelIndex());

  searchBox()->setText(QStringLiteral("enter-me"));
  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onTimeoutSearch",
                                    Qt::DirectConnection));
  QSignalSpy shown(QtPassSettings::getPass(), &Pass::finishedShow);
  QVERIFY(QMetaObject::invokeMethod(
      m_window.data(), "on_lineEdit_returnPressed", Qt::DirectConnection));
  QCOMPARE(passwordName()->text(), QStringLiteral("enter-me"));
  QVERIFY2(shown.count() == 1 || shown.wait(5000), "the entry is decrypted");
  QCOMPARE(shown.first().at(1).toString(), QStringLiteral("enter-me"));
#endif
}

/**
 * @brief A message from a second instance brings the window up: empty, it
 *        focuses the search box; with text, it types that into the search
 *        box and presses Enter, which opens the first entry the tree offers
 *        (the filter itself follows on the search timer) and decrypts it.
 */
void tst_mainwindow::messageFromAnotherInstanceShowsAndSearches() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as the gpg stand-in");
#else
  QTemporaryDir scratch;
  QVERIFY(scratch.isValid());
  QVERIFY(!rebuildWithFakeGpg(scratch).isEmpty());
  QVERIFY(selectEntry(m_window.data(), m_storeDir.path(),
                      QStringLiteral("from-afar")));
  m_window->hide();

  m_window->messageAvailable(QString());
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  QVERIFY2(m_window->isVisible(), "an empty message still shows the window");
  QCOMPARE(m_window->focusWidget(), searchBox());

  QSignalSpy shown(QtPassSettings::getPass(), &Pass::finishedShow);
  m_window->messageAvailable(QStringLiteral("from-afar"));
  QCOMPARE(searchBox()->text(), QStringLiteral("from-afar"));
  QVERIFY2(m_window->statusBar()->currentMessage().contains(
               QStringLiteral("Looking for: from-afar")),
           qPrintable(m_window->statusBar()->currentMessage()));
  QVERIFY2(!passwordName()->text().isEmpty(),
           "Enter on the query opened an entry");
  QVERIFY2(shown.count() >= 1 || shown.wait(5000), "and decrypted it");
  QCOMPARE(shown.first().at(1).toString(), passwordName()->text());
#endif
}

/**
 * @brief Settings OK re-applies what the dialog changed without a restart:
 *        the browser font and wrapping, the menu bar, the tray icon, and a
 *        grep mode that is left when content search is switched off.
 */
void tst_mainwindow::configDialogAcceptedReappliesTheSettings() {
#ifdef Q_OS_MACOS
  QSKIP("the menu bar is the system's on macOS");
#else
  rebuildWindow([](AppSettings &s) {
    s.useGrepSearch = true;
    s.useMonospace = false;
    s.noLineWrapping = false;
    s.showMenuBar = false;
  });
  const auto restore = qScopeGuard([] {
    AppSettings s = QtPassSettings::load();
    s.useMonospace = false;
    s.noLineWrapping = false;
    s.showMenuBar = false;
    s.useGrepSearch = false;
    s.useTrayIcon = false;
    QtPassSettings::save(s);
  });
  auto *grep = m_window->findChild<QToolButton *>(QStringLiteral("grepButton"));
  QVERIFY(grep != nullptr);
  grep->setChecked(true);
  QCOMPARE(searchBox()->placeholderText(),
           QStringLiteral("Search content (regex)"));
  {
    // What the user ticks in the dialog: it reads these when it opens.
    AppSettings s = QtPassSettings::load();
    s.useMonospace = true;
    s.noLineWrapping = true;
    s.showMenuBar = true;
    s.useGrepSearch = false;
    s.useTrayIcon = true;
    QtPassSettings::save(s);
  }

  ModalDriver driver([](QWidget *w) {
    if (auto *dialog = qobject_cast<ConfigDialog *>(w)) {
      dialog->accept();
      return;
    }
    answerBox(QMessageBox::Ok)(w);
  });
  auto *config = m_window->findChild<QAction *>(QStringLiteral("actionConfig"));
  QVERIFY(config != nullptr);
  config->trigger();

  QCOMPARE(driver.seen, 1);
  QCOMPARE(browser()->font().styleHint(), QFont::Monospace);
  QCOMPARE(browser()->lineWrapMode(), QTextBrowser::NoWrap);
  QVERIFY2(m_window->menuBar()->isVisibleTo(m_window.data()),
           "the menu bar follows the accepted setting");
  QVERIFY2(!grep->isChecked(), "content search off leaves grep mode");
  QCOMPARE(searchBox()->placeholderText(), QStringLiteral("Search password"));
  QVERIFY2(!grep->isVisibleTo(m_window.data()), "and hides the button");
  // The tray icon is created on the spot, where the desktop offers a tray.
  QCOMPARE(m_window->findChild<TrayIcon *>() != nullptr,
           QSystemTrayIcon::isSystemTrayAvailable());
#endif
}

/**
 * @brief A cancelled Settings dialog reports the cancel and changes nothing
 *        about the window.
 */
void tst_mainwindow::configDialogCancelledChangesNothing() {
  const QTextBrowser::LineWrapMode wrap = browser()->lineWrapMode();
  ModalDriver driver(answerBox(QMessageBox::Cancel));
  QVERIFY2(!m_window->config(), "a rejected dialog is a cancel");
  QCOMPARE(driver.seen, 1);
  QCOMPARE(browser()->lineWrapMode(), wrap);
}

/**
 * @brief Help > About shows the standard box with the program name, the
 *        project link and the licence.
 */
void tst_mainwindow::aboutBoxNamesTheProgramAndLicence() {
  ModalDriver driver(answerBox(QMessageBox::Ok));
  auto *about = m_window->findChild<QAction *>(QStringLiteral("actionAbout"));
  QVERIFY(about != nullptr);
  about->trigger();
#ifdef Q_OS_MACOS
  // QMessageBox::about() show()s its box on macOS instead of exec()ing it, so
  // it is never the active modal widget the driver watches for.
  QPointer<QMessageBox> box;
  QTRY_VERIFY([&box]() {
    for (QWidget *top : QApplication::topLevelWidgets()) {
      auto *candidate = qobject_cast<QMessageBox *>(top);
      if (candidate != nullptr && candidate->isVisible()) {
        box = candidate;
        return true;
      }
    }
    return false;
  }());
  const QString text = box->text();
  box->close();
#else
  QCOMPARE(driver.seen, 1);
  QCOMPARE(driver.boxTitles, QStringList{QStringLiteral("About QtPass")});
  const QString text = driver.boxTexts.value(0);
#endif
  QVERIFY2(text.contains(QStringLiteral("QtPass ")), qPrintable(text));
  QVERIFY2(text.contains(QStringLiteral("qtpass.org")), qPrintable(text));
  QVERIFY2(text.contains(QStringLiteral("GNU GPL")), qPrintable(text));
  QVERIFY2(text.contains(QString::number(QDate::currentDate().year())),
           qPrintable(text));
}

/**
 * @brief With profiles configured the profile box lists them, the active
 *        one selected, and picking another switches the store the tree
 *        shows and the settings point at.
 */
void tst_mainwindow::profilesFillTheBoxAndSwitchTheStore() {
  QTemporaryDir other;
  QVERIFY(other.isValid());
  const QString otherStore = QDir::cleanPath(other.path());
  {
    QFile gpgId(QDir(otherStore).filePath(QStringLiteral(".gpg-id")));
    QVERIFY(gpgId.open(QIODevice::WriteOnly));
    gpgId.write("0000000000000000\n");
  }
  const QString mainStore = QDir::cleanPath(m_storeDir.path());
  Profiles profiles;
  Profile mainProfile;
  mainProfile.path = mainStore;
  Profile otherProfile;
  otherProfile.path = otherStore;
  otherProfile.useGit = false;
  profiles.insert(QStringLiteral("main"), mainProfile);
  profiles.insert(QStringLiteral("other"), otherProfile);
  QtPassSettings::setProfiles(profiles);
  rebuildWindow(
      [](AppSettings &s) { s.activeProfile = QStringLiteral("main"); });
  const auto restore = qScopeGuard([this] {
    m_window.reset();
    QtPassSettings::setProfiles({});
    AppSettings s = QtPassSettings::load();
    s.activeProfile.clear();
    s.passStore = QDir::cleanPath(m_storeDir.path());
    QtPassSettings::save(s);
  });

  auto *box = m_window->findChild<QComboBox *>(QStringLiteral("profileBox"));
  auto *widget =
      m_window->findChild<QWidget *>(QStringLiteral("profileWidget"));
  QVERIFY(box != nullptr && widget != nullptr);
  QVERIFY2(widget->isVisibleTo(m_window.data()),
           "the profile row shows once profiles exist");
  QVERIFY(box->isEnabled());
  QCOMPARE(box->count(), 2);
  QCOMPARE(box->itemText(0), QStringLiteral("main"));
  QCOMPARE(box->itemText(1), QStringLiteral("other"));
  QCOMPARE(box->currentText(), QStringLiteral("main"));

  passwordName()->setText(QStringLiteral("stale"));
  box->setCurrentText(QStringLiteral("other"));
  QCOMPARE(m_window->statusBar()->currentMessage(),
           QStringLiteral("Profile changed to other"));
  const AppSettings after = QtPassSettings::load();
  QCOMPARE(QDir::cleanPath(after.passStore), otherStore);
  QCOMPARE(after.activeProfile, QStringLiteral("other"));
  QVERIFY2(!after.useGit, "the profile's own Git flag is taken over");
  QVERIFY2(passwordName()->text().isEmpty(), "the switch deselects");
  auto *proxy = qobject_cast<QSortFilterProxyModel *>(treeView()->model());
  auto *fs = qobject_cast<QFileSystemModel *>(proxy->sourceModel());
  QCOMPARE(QDir::cleanPath(fs->rootPath()), otherStore);

  // The handler ignores the profile that is already active (the box does
  // not even emit for it, so call the slot as the box would).
  m_window->statusBar()->clearMessage();
  passwordName()->setText(QStringLiteral("kept"));
  QVERIFY(QMetaObject::invokeMethod(
      m_window.data(), "on_profileBox_currentTextChanged", Qt::DirectConnection,
      Q_ARG(QString, QStringLiteral("other"))));
  QVERIFY(m_window->statusBar()->currentMessage().isEmpty());
  QCOMPARE(passwordName()->text(), QStringLiteral("kept"));
}

/**
 * @brief restoreWindow() creates the tray icon when the setting asks for
 *        one and drops it again when it does not; where no system tray is
 *        available the icon is released straight away. "Start minimized"
 *        hides the window a moment later.
 */
void tst_mainwindow::trayIconFollowsTheSetting() {
  const bool trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  {
    AppSettings s = QtPassSettings::load();
    s.useTrayIcon = true;
    s.startMinimized = true;
    QtPassSettings::save(s);
  }
  const auto restore = qScopeGuard([] {
    AppSettings s = QtPassSettings::load();
    s.useTrayIcon = false;
    s.startMinimized = false;
    QtPassSettings::save(s);
  });

  m_window->restoreWindow();
  auto *tray = m_window->findChild<TrayIcon *>();
  QCOMPARE(tray != nullptr, trayAvailable);
  QTRY_VERIFY2_WITH_TIMEOUT(!m_window->isVisible(), "start minimized hides",
                            2000);

  {
    AppSettings s = QtPassSettings::load();
    s.useTrayIcon = false;
    QtPassSettings::save(s);
  }
  m_window->restoreWindow();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QVERIFY2(m_window->findChild<TrayIcon *>() == nullptr,
           "the tray icon goes when the setting is switched off");
}

/**
 * @brief Keys the window handles itself: Escape empties the search box,
 *        Return opens the current entry, Delete with nothing selected asks
 *        nothing (it used to be able to delete the whole store, #556).
 */
void tst_mainwindow::keyPressesReachTheWindow() {
  ShowAnswers answers;
  searchBox()->setText(QStringLiteral("typed"));
  QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(m_window.data(), &escape);
  QVERIFY2(searchBox()->text().isEmpty(), "Escape clears the search box");

  treeView()->setCurrentIndex(QModelIndex());
  int dialogs = 0;
  ModalDriver driver([&dialogs](QWidget *w) {
    ++dialogs;
    answerBox(QMessageBox::No)(w);
  });
  QKeyEvent del(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
  QApplication::sendEvent(m_window.data(), &del);
  QTest::qWait(100);
  QCOMPARE(dialogs, 0);

  QVERIFY(selectEntry(m_window.data(), m_storeDir.path(),
                      QStringLiteral("return-me")));
  QKeyEvent ret(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
  QApplication::sendEvent(m_window.data(), &ret);
  QCOMPARE(passwordName()->text(), QStringLiteral("return-me"));
  QVERIFY2(answers.waitFor(1), "Return's own decrypt must come back");
}

/**
 * @brief Arrow down in the search box hands the focus to the tree, so the
 *        keyboard goes on to the filtered entries.
 */
void tst_mainwindow::downArrowInTheSearchBoxMovesToTheTree() {
  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  searchBox()->setFocus();
  QCOMPARE(m_window->focusWidget(), searchBox());

  QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
  QApplication::sendEvent(searchBox(), &down);
  QCOMPARE(m_window->focusWidget(), treeView());
}

/**
 * @brief The browser's context menu is reparented to the main window, so a
 *        stylesheet on the browser cannot leak into it.
 */
void tst_mainwindow::browserContextMenuBelongsToTheWindow() {
  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  QWidget *menuParent = nullptr;
  bool wasMenu = false;
  ModalDriver driver([&](QWidget *w) {
    wasMenu = qobject_cast<QMenu *>(w) != nullptr;
    menuParent = w->parentWidget();
    w->close();
  });
  emit browser() -> customContextMenuRequested(QPoint(5, 5));
  QCOMPARE(driver.seen, 1);
  QVERIFY(wasMenu);
  QCOMPARE(menuParent, m_window.data());
}

namespace {
/// The texts of a menu's actions, separators as "-".
auto actionTexts(QMenu *menu) -> QStringList {
  QStringList texts;
  for (QAction *action : menu->actions()) {
    texts << (action->isSeparator() ? QStringLiteral("-") : action->text());
  }
  return texts;
}

auto actionByText(QMenu *menu, const QString &text) -> QAction * {
  for (QAction *action : menu->actions()) {
    if (action->text() == text) {
      return action;
    }
  }
  return nullptr;
}
} // namespace

/**
 * @brief Right-clicking empty space in the tree offers the folder actions
 *        for the store root and no entry actions; "Open folder" hands the
 *        store to the file manager.
 */
void tst_mainwindow::contextMenuOnEmptySpaceOffersFolderActions() {
  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  // An impossible filter empties the tree, so any point is empty space.
  searchBox()->setText(QStringLiteral("no-such-entry-anywhere"));
  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onTimeoutSearch",
                                    Qt::DirectConnection));
  auto *del = m_window->findChild<QAction *>(QStringLiteral("actionDelete"));
  del->setEnabled(true);

  UrlCatcher catcher;
  QDesktopServices::setUrlHandler(QStringLiteral("file"), &catcher, "catchUrl");
  const auto restore = qScopeGuard(
      [] { QDesktopServices::unsetUrlHandler(QStringLiteral("file")); });

  QStringList texts;
  ModalDriver driver([&texts](QWidget *w) {
    auto *menu = qobject_cast<QMenu *>(w);
    if (menu == nullptr) {
      w->close();
      return;
    }
    texts = actionTexts(menu);
    if (QAction *open = actionByText(
            menu, QStringLiteral("Open folder with file manager"))) {
      open->trigger();
    }
    menu->close();
  });
  emit treeView() -> customContextMenuRequested(QPoint(10, 10));
  QCOMPARE(driver.seen, 1);
  QCOMPARE(texts, (QStringList{QStringLiteral("Open folder with file manager"),
                               QStringLiteral("Add folder"),
                               QStringLiteral("Add password"),
                               QStringLiteral("Users")}));
  QVERIFY2(!del->isEnabled(), "nothing is selected, nothing to delete");
  QCOMPARE(catcher.urls.size(), 1);
  QCOMPARE(QDir::cleanPath(catcher.urls.first().toLocalFile()),
           QDir::cleanPath(m_storeDir.path()));
}

/**
 * @brief Right-clicking an entry offers Edit, Rename password and Delete,
 *        and nothing meant for folders.
 */
void tst_mainwindow::contextMenuOnAPasswordOffersEditRenameDelete() {
  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  QVERIFY(selectEntry(m_window.data(), m_storeDir.path(),
                      QStringLiteral("menu-entry")));
  searchBox()->setText(QStringLiteral("menu-entry"));
  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onTimeoutSearch",
                                    Qt::DirectConnection));
  auto *tree = treeView();
  QTRY_VERIFY(tree->currentIndex().isValid());
  const QPoint pos = tree->visualRect(tree->currentIndex()).center();
  QVERIFY(tree->indexAt(pos) == tree->currentIndex());

  QStringList texts;
  ModalDriver driver([&texts](QWidget *w) {
    if (auto *menu = qobject_cast<QMenu *>(w)) {
      texts = actionTexts(menu);
    }
    w->close();
  });
  emit tree->customContextMenuRequested(pos);
  QCOMPARE(driver.seen, 1);
  QCOMPARE(texts, (QStringList{QStringLiteral("Edit"), QStringLiteral("-"),
                               QStringLiteral("Rename password"),
                               QStringLiteral("Delete")}));
}

/**
 * @brief Right-clicking a folder offers the folder actions, Rename folder,
 *        Delete and a Share submenu whose entries are enabled when a .gpg-id
 *        and gpg are at hand; "What is this?" explains sharing.
 */
void tst_mainwindow::contextMenuOnAFolderOffersSharing() {
  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  QVERIFY(QDir(m_storeDir.path()).mkpath(QStringLiteral("shared-folder")));
  QVERIFY(selectEntry(m_window.data(), m_storeDir.path(),
                      QStringLiteral("shared-folder/inside")));
  QVERIFY(selectPath(
      QDir(m_storeDir.path()).filePath(QStringLiteral("shared-folder"))));
  searchBox()->setText(QStringLiteral("shared-folder"));
  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onTimeoutSearch",
                                    Qt::DirectConnection));
  QVERIFY(selectPath(
      QDir(m_storeDir.path()).filePath(QStringLiteral("shared-folder"))));
  auto *tree = treeView();
  const QPoint pos = tree->visualRect(tree->currentIndex()).center();
  QVERIFY(tree->indexAt(pos) == tree->currentIndex());

  QStringList texts;
  QStringList shareTexts;
  QList<bool> shareEnabled;
  ModalDriver driver([&](QWidget *w) {
    auto *menu = qobject_cast<QMenu *>(w);
    if (menu == nullptr) {
      answerBox(QMessageBox::Ok)(w);
      return;
    }
    texts = actionTexts(menu);
    QPointer<QMenu> keep(menu);
    if (QAction *share = actionByText(menu, QStringLiteral("Share"))) {
      QMenu *sub = share->menu();
      shareTexts = actionTexts(sub);
      for (QAction *action : sub->actions()) {
        shareEnabled << action->isEnabled();
      }
      if (QAction *help = actionByText(sub, QStringLiteral("What is this?"))) {
        help->trigger(); // opens a box; the driver answers it from a tick
      }
    }
    if (keep) {
      keep->close();
    }
  });
  emit tree->customContextMenuRequested(pos);
  QCOMPARE(texts,
           (QStringList{QStringLiteral("Open folder with file manager"),
                        QStringLiteral("Add folder"),
                        QStringLiteral("Add password"), QStringLiteral("Users"),
                        QStringLiteral("-"), QStringLiteral("Rename folder"),
                        QStringLiteral("Delete"), QStringLiteral("Share")}));
  QCOMPARE(shareTexts, (QStringList{QStringLiteral("Re-encrypt all passwords"),
                                    QStringLiteral("Export my public key..."),
                                    QStringLiteral("Add recipient..."),
                                    QStringLiteral("What is this?")}));
  QCOMPARE(shareEnabled, (QList<bool>{true, true, true, true}));
  QCOMPARE(driver.seen, 2);
  COMPARE_BOX_TITLES(driver.boxTitles,
                     QStringList{QStringLiteral("Sharing passwords with GPG")});
  QVERIFY2(driver.boxTexts.value(0).contains(
               QStringLiteral("Export your public key")),
           qPrintable(driver.boxTexts.value(0)));
}

namespace {
/**
 * Open the tree's context menu on @p folder (the tree filtered to it) and
 * trigger the Share submenu action @p actionText; everything it opens is
 * handed to @p onDialog. Returns false when the menu or the action was not
 * found.
 */
auto triggerShareAction(MainWindow *window, const QString &storeDir,
                        const QString &folder, const QString &actionText,
                        const ModalDriver::Handler &onDialog,
                        QStringList *boxTitles = nullptr,
                        QStringList *boxTexts = nullptr,
                        const std::function<void()> &beforeTrigger = {})
    -> bool {
  auto *tree = window->findChild<QTreeView *>(QStringLiteral("treeView"));
  auto *search = window->findChild<QLineEdit *>(QStringLiteral("lineEdit"));
  auto *proxy = qobject_cast<QSortFilterProxyModel *>(tree->model());
  auto *fs = qobject_cast<QFileSystemModel *>(proxy->sourceModel());
  const QString path = QDir(storeDir).filePath(folder);
  QModelIndex src;
  QTRY_VERIFY_WITH_TIMEOUT_RETURN((src = fs->index(path)).isValid(), 5000,
                                  false);
  search->setText(folder);
  QMetaObject::invokeMethod(window, "onTimeoutSearch", Qt::DirectConnection);
  tree->setCurrentIndex(proxy->mapFromSource(fs->index(path)));
  if (!tree->currentIndex().isValid()) {
    return false;
  }
  const QPoint pos = tree->visualRect(tree->currentIndex()).center();
  if (tree->indexAt(pos) != tree->currentIndex()) {
    return false;
  }
  bool found = false;
  ModalDriver driver([&](QWidget *w) {
    auto *menu = qobject_cast<QMenu *>(w);
    if (menu == nullptr) {
      onDialog(w);
      return;
    }
    QPointer<QMenu> keep(menu);
    if (QAction *share = actionByText(menu, QStringLiteral("Share"))) {
      if (QAction *action = actionByText(share->menu(), actionText)) {
        found = true;
        if (beforeTrigger) {
          beforeTrigger();
        }
        action->trigger();
      }
    }
    if (keep) {
      keep->close();
    }
  });
  emit tree->customContextMenuRequested(pos);
  if (boxTitles != nullptr) {
    *boxTitles = driver.boxTitles;
  }
  if (boxTexts != nullptr) {
    *boxTexts = driver.boxTexts;
  }
  return found;
}
} // namespace

/**
 * @brief Share > Export my public key explains itself when no signing key
 *        is configured, and with one runs gpg --export and shows the armored
 *        key in the export dialog.
 */
void tst_mainwindow::shareMenuExportsThePublicKey() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as the gpg stand-in");
#else
  QTemporaryDir scratch;
  QVERIFY(scratch.isValid());
  const QString gpg = rebuildWithFakeGpg(
      scratch, [](AppSettings &s) { s.passSigningKey.clear(); });
  QVERIFY(!gpg.isEmpty());
  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  QVERIFY(QDir(m_storeDir.path()).mkpath(QStringLiteral("export-folder")));

  QStringList titles;
  QStringList texts;
  QVERIFY(triggerShareAction(m_window.data(), m_storeDir.path(),
                             QStringLiteral("export-folder"),
                             QStringLiteral("Export my public key..."),
                             answerBox(QMessageBox::Ok), &titles, &texts));
  COMPARE_BOX_TITLES(titles, QStringList{QStringLiteral("Export public key")});
  QVERIFY2(texts.value(0).contains(QStringLiteral("No signing key")),
           qPrintable(texts.value(0)));

  {
    AppSettings s = QtPassSettings::load();
    s.passSigningKey = QStringLiteral("31850CF72D9CDDE9");
    QtPassSettings::save(s);
  }
  const auto restore = qScopeGuard([] {
    AppSettings s = QtPassSettings::load();
    s.passSigningKey.clear();
    QtPassSettings::save(s);
  });
  QString exported;
  QVERIFY(triggerShareAction(
      m_window.data(), m_storeDir.path(), QStringLiteral("export-folder"),
      QStringLiteral("Export my public key..."), [&exported](QWidget *w) {
        if (auto *dialog = qobject_cast<ExportPublicKeyDialog *>(w)) {
          if (auto *text = dialog->findChild<QPlainTextEdit *>()) {
            exported = text->toPlainText();
          } else if (auto *edit = dialog->findChild<QTextEdit *>()) {
            exported = edit->toPlainText();
          }
          dialog->reject();
          return;
        }
        answerBox(QMessageBox::Ok)(w);
      }));
  QVERIFY2(exported.contains(QStringLiteral("BEGIN PGP PUBLIC KEY BLOCK")),
           qPrintable(QStringLiteral("export dialog showed: ") + exported));
  QVERIFY2(exported.contains(QStringLiteral("mQINBFakeKey")),
           qPrintable(exported));

  // gpg refusing the export: reported with gpg's own words.
  {
    AppSettings s = QtPassSettings::load();
    s.passSigningKey = QStringLiteral("NOKEY");
    QtPassSettings::save(s);
  }
  QVERIFY(triggerShareAction(m_window.data(), m_storeDir.path(),
                             QStringLiteral("export-folder"),
                             QStringLiteral("Export my public key..."),
                             answerBox(QMessageBox::Ok), &titles, &texts));
  COMPARE_BOX_TITLES(titles, QStringList{QStringLiteral("Export public key")});
  QVERIFY2(texts.value(0).contains(
               QStringLiteral("Could not export public key for NOKEY")),
           qPrintable(texts.value(0)));
  QVERIFY2(texts.value(0).contains(QStringLiteral("nothing exported")),
           qPrintable(texts.value(0)));
#endif
}

/**
 * @brief Share > Re-encrypt asks before rewriting anything and does nothing
 *        on No; a folder that vanished since the menu opened is reported
 *        instead of asked about. Add recipient opens the users dialog.
 */
void tst_mainwindow::shareMenuReencryptAsksFirst() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as the gpg stand-in");
#else
  QTemporaryDir scratch;
  QVERIFY(scratch.isValid());
  QVERIFY(!rebuildWithFakeGpg(scratch).isEmpty());
  m_window->show();
  QVERIFY(QTest::qWaitForWindowExposed(m_window.data()));
  const QString folder = QStringLiteral("reenc-folder");
  QVERIFY(QDir(m_storeDir.path()).mkpath(folder));
  QVERIFY(selectEntry(m_window.data(), m_storeDir.path(),
                      folder + QStringLiteral("/inside")));

  QStringList titles;
  QStringList texts;
  QVERIFY(triggerShareAction(m_window.data(), m_storeDir.path(), folder,
                             QStringLiteral("Re-encrypt all passwords"),
                             answerBox(QMessageBox::No), &titles, &texts));
  COMPARE_BOX_TITLES(titles,
                     QStringList{QStringLiteral("Re-encrypt passwords")});
  QVERIFY2(texts.value(0).contains(folder), qPrintable(texts.value(0)));
  QVERIFY2(m_window->findChild<QProgressDialog *>() == nullptr,
           "No starts nothing");
  QVERIFY(treeView()->isEnabled());

  // Add recipient: the users dialog for that folder.
  bool usersSeen = false;
  QVERIFY(triggerShareAction(
      m_window.data(), m_storeDir.path(), folder,
      QStringLiteral("Add recipient..."), [&usersSeen](QWidget *w) {
        if (auto *dialog = qobject_cast<UsersDialog *>(w)) {
          usersSeen = true;
          dialog->reject();
          return;
        }
        answerBox(QMessageBox::Ok)(w);
      }));
  QVERIFY2(usersSeen, "Add recipient opens the users dialog");

  // The folder goes away between the menu opening and the click.
  const QString doomed = QStringLiteral("gone-folder");
  const QString doomedPath = QDir(m_storeDir.path()).filePath(doomed);
  QVERIFY(QDir(m_storeDir.path()).mkpath(doomed));
  QVERIFY(triggerShareAction(m_window.data(), m_storeDir.path(), doomed,
                             QStringLiteral("Re-encrypt all passwords"),
                             answerBox(QMessageBox::Ok), &titles, &texts,
                             [doomedPath] { QDir().rmdir(doomedPath); }));
  COMPARE_BOX_TITLES(titles, QStringList{QStringLiteral("Error")});
  QVERIFY2(texts.value(0).contains(QStringLiteral("Directory does not exist")),
           qPrintable(texts.value(0)));
  QVERIFY2(texts.value(0).contains(doomed), qPrintable(texts.value(0)));

  // Yes on a folder without entries: the run starts, holds the interface
  // with its progress dialog, and ends with nothing to rewrite.
  const QString empty = QStringLiteral("reenc-empty");
  QVERIFY(QDir(m_storeDir.path()).mkpath(empty));
  QVERIFY(triggerShareAction(
      m_window.data(), m_storeDir.path(), empty,
      QStringLiteral("Re-encrypt all passwords"),
      [](QWidget *w) {
        if (qobject_cast<QProgressDialog *>(w) != nullptr) {
          return; // the run's own dialog; it goes when the run ends
        }
        answerBox(QMessageBox::Yes)(w);
      },
      &titles, &texts));
  COMPARE_BOX_TITLES(titles,
                     QStringList{QStringLiteral("Re-encrypt passwords")});
  QTRY_VERIFY_WITH_TIMEOUT(treeView()->isEnabled(), 10000);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QVERIFY(m_window->findChild<QProgressDialog *>() == nullptr);
  QVERIFY2(m_window->statusBar()->currentMessage().contains(
               QStringLiteral("Re-encryption completed")),
           qPrintable(m_window->statusBar()->currentMessage()));

  // Add recipient on a folder behind a link is refused.
  QTemporaryDir outside;
  QVERIFY(outside.isValid());
  const QString link =
      QDir(m_storeDir.path()).filePath(QStringLiteral("share-link"));
  QVERIFY(QFile::link(outside.path(), link));
  const auto cleanup = qScopeGuard([&link] { QFile::remove(link); });
  usersSeen = false;
  QVERIFY(triggerShareAction(
      m_window.data(), m_storeDir.path(), QStringLiteral("share-link"),
      QStringLiteral("Add recipient..."),
      [&usersSeen](QWidget *w) {
        usersSeen = usersSeen || qobject_cast<UsersDialog *>(w) != nullptr;
        answerBox(QMessageBox::Ok)(w);
      },
      &titles, &texts));
  QVERIFY(!usersSeen);
  COMPARE_BOX_TITLES(titles,
                     QStringList{QStringLiteral("Not a folder of the store")});
  QVERIFY2(texts.value(0).contains(QStringLiteral("share-link")),
           qPrintable(texts.value(0)));
#endif
}

/**
 * @brief "Add folder" asks for a name, creates the folder in the current
 *        one and, with "add .gpg-id" on and no signing key, seeds its
 *        .gpg-id from the recipients in effect for the parent (#1682 left
 *        an empty file there). A cancelled prompt creates nothing.
 */
void tst_mainwindow::addFolderCreatesItWithAGpgId() {
  {
    AppSettings s = QtPassSettings::load();
    s.addGPGId = true;
    s.passSigningKey.clear();
    QtPassSettings::save(s);
  }
  treeView()->setCurrentIndex(QModelIndex());
  const QDir store(m_storeDir.path());
  {
    ModalDriver driver(typeIntoInputDialog(QStringLiteral("brand-new")));
    QVERIFY(QMetaObject::invokeMethod(m_window.data(), "addFolder",
                                      Qt::DirectConnection));
    QCOMPARE(driver.seen, 1);
  }
  QVERIFY2(QFileInfo(store.filePath(QStringLiteral("brand-new"))).isDir(),
           "the folder is created in the store root");
  QFile gpgId(store.filePath(QStringLiteral("brand-new/.gpg-id")));
  QVERIFY2(gpgId.open(QIODevice::ReadOnly), ".gpg-id must be seeded");
  QCOMPARE(QString::fromUtf8(gpgId.readAll()).trimmed(),
           QStringLiteral("0000000000000000"));

  {
    ModalDriver driver(answerBox(QMessageBox::Cancel));
    QVERIFY(QMetaObject::invokeMethod(m_window.data(), "addFolder",
                                      Qt::DirectConnection));
    QCOMPARE(driver.seen, 1);
  }
  QCOMPARE(store.entryList(QStringList{QStringLiteral("brand-*")}, QDir::Dirs),
           QStringList{QStringLiteral("brand-new")});
}

/**
 * @brief A folder name that resolves outside the store is refused with a
 *        warning, and a name already taken is reported as a failed create.
 */
void tst_mainwindow::addFolderRefusesEscapesAndDuplicates() {
  treeView()->setCurrentIndex(QModelIndex());
  const QDir store(m_storeDir.path());
  const QString escaped =
      QDir::cleanPath(store.filePath(QStringLiteral("../escaped-folder")));
  QVERIFY(!QFileInfo::exists(escaped));
  {
    ModalDriver driver(
        typeIntoInputDialog(QStringLiteral("../escaped-folder")));
    QVERIFY(QMetaObject::invokeMethod(m_window.data(), "addFolder",
                                      Qt::DirectConnection));
    QCOMPARE(driver.seen, 2);
    COMPARE_BOX_TITLES(driver.boxTitles,
                       QStringList{QStringLiteral("Invalid name")});
    QVERIFY2(driver.boxTexts.value(0).contains(
                 QStringLiteral("outside the password store")),
             qPrintable(driver.boxTexts.value(0)));
  }
  QVERIFY2(!QFileInfo::exists(escaped), "nothing is created outside");

  QVERIFY(store.mkpath(QStringLiteral("taken")));
  {
    ModalDriver driver(typeIntoInputDialog(QStringLiteral("taken")));
    QVERIFY(QMetaObject::invokeMethod(m_window.data(), "addFolder",
                                      Qt::DirectConnection));
    QCOMPARE(driver.seen, 2);
    COMPARE_BOX_TITLES(driver.boxTitles, QStringList{QStringLiteral("Error")});
    QVERIFY2(driver.boxTexts.value(0).contains(
                 QStringLiteral("Failed to create folder")),
             qPrintable(driver.boxTexts.value(0)));
  }

  // No recipients to seed from (an empty root .gpg-id): the folder is made
  // but the missing .gpg-id is reported.
  const QString rootGpgId = store.filePath(QStringLiteral(".gpg-id"));
  QFile gpgId(rootGpgId);
  QVERIFY(gpgId.open(QIODevice::ReadOnly));
  const QByteArray recipients = gpgId.readAll();
  gpgId.close();
  const auto restoreGpgId = qScopeGuard([&rootGpgId, &recipients] {
    QFile f(rootGpgId);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      f.write(recipients);
    }
  });
  QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Truncate));
  gpgId.close();
  {
    AppSettings s = QtPassSettings::load();
    s.addGPGId = true;
    s.passSigningKey.clear();
    QtPassSettings::save(s);
  }
  treeView()->setCurrentIndex(QModelIndex());
  {
    ModalDriver driver(typeIntoInputDialog(QStringLiteral("seedless")));
    QVERIFY(QMetaObject::invokeMethod(m_window.data(), "addFolder",
                                      Qt::DirectConnection));
    QCOMPARE(driver.seen, 2);
    COMPARE_BOX_TITLES(driver.boxTitles, QStringList{QStringLiteral("Error")});
    QVERIFY2(driver.boxTexts.value(0).contains(
                 QStringLiteral("Failed to create .gpg-id file")),
             qPrintable(driver.boxTexts.value(0)));
  }
  QVERIFY(QFileInfo(store.filePath(QStringLiteral("seedless"))).isDir());
  QVERIFY(
      !QFileInfo::exists(store.filePath(QStringLiteral("seedless/.gpg-id"))));
}

/**
 * @brief "Rename folder" moves the current folder to the typed name inside
 *        its parent; a name escaping the store is refused; cancel keeps it.
 */
void tst_mainwindow::renameFolderMovesIt() {
  {
    AppSettings s = QtPassSettings::load();
    s.useGit = false;
    QtPassSettings::save(s);
  }
  const QDir store(m_storeDir.path());
  QVERIFY(store.mkpath(QStringLiteral("ren-src")));
  QVERIFY(selectPath(store.filePath(QStringLiteral("ren-src"))));
  {
    ModalDriver driver(answerBox(QMessageBox::Cancel));
    QVERIFY(QMetaObject::invokeMethod(m_window.data(), "renameFolder",
                                      Qt::DirectConnection));
    QCOMPARE(driver.seen, 1);
  }
  QVERIFY(QFileInfo(store.filePath(QStringLiteral("ren-src"))).isDir());
  // The file system model settles the new folder's node while a dialog runs,
  // which can retire the current index: select again before each action.
  QVERIFY(selectPath(store.filePath(QStringLiteral("ren-src"))));
  {
    ModalDriver driver(typeIntoInputDialog(QStringLiteral("../ren-escape")));
    QVERIFY(QMetaObject::invokeMethod(m_window.data(), "renameFolder",
                                      Qt::DirectConnection));
    COMPARE_BOX_TITLES(driver.boxTitles,
                       QStringList{QStringLiteral("Invalid name")});
  }
  QVERIFY(QFileInfo(store.filePath(QStringLiteral("ren-src"))).isDir());
  QVERIFY(!QFileInfo::exists(
      QDir::cleanPath(store.filePath(QStringLiteral("../ren-escape")))));

  QVERIFY(selectPath(store.filePath(QStringLiteral("ren-src"))));
  ModalDriver driver(typeIntoInputDialog(QStringLiteral("ren-dst")));
  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "renameFolder",
                                    Qt::DirectConnection));
  QVERIFY2(driver.seen == 1,
           qPrintable(driver.seenClasses.join(QStringLiteral(", ")) +
                      driver.boxTexts.join(QStringLiteral(" | "))));
  QTRY_VERIFY(QFileInfo(store.filePath(QStringLiteral("ren-dst"))).isDir());
  QVERIFY(!QFileInfo::exists(store.filePath(QStringLiteral("ren-src"))));
}

/**
 * @brief "Rename password" offers the entry's name without .gpg, moves the
 *        file to the typed name next to it, and refuses a name that would
 *        leave the store.
 */
void tst_mainwindow::renamePasswordMovesIt() {
  {
    AppSettings s = QtPassSettings::load();
    s.useGit = false;
    QtPassSettings::save(s);
  }
  const QDir store(m_storeDir.path());
  QVERIFY(selectEntry(m_window.data(), m_storeDir.path(),
                      QStringLiteral("ren-file")));
  QString offered;
  {
    ModalDriver driver([&offered](QWidget *w) {
      if (auto *input = qobject_cast<QInputDialog *>(w)) {
        offered = input->textValue();
        input->setTextValue(QStringLiteral("../ren-file-escape"));
        input->accept();
        return;
      }
      answerBox(QMessageBox::Ok)(w);
    });
    QVERIFY(QMetaObject::invokeMethod(m_window.data(), "renamePassword",
                                      Qt::DirectConnection));
    QCOMPARE(offered, QStringLiteral("ren-file"));
    COMPARE_BOX_TITLES(driver.boxTitles,
                       QStringList{QStringLiteral("Invalid name")});
  }
  QVERIFY(QFileInfo::exists(store.filePath(QStringLiteral("ren-file.gpg"))));

  QVERIFY(selectPath(store.filePath(QStringLiteral("ren-file.gpg"))));
  ModalDriver driver(typeIntoInputDialog(QStringLiteral("ren-file-2")));
  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "renamePassword",
                                    Qt::DirectConnection));
  QVERIFY2(driver.seen == 1,
           qPrintable(driver.seenClasses.join(QStringLiteral(", ")) +
                      driver.boxTexts.join(QStringLiteral(" | "))));
  QTRY_VERIFY(
      QFileInfo::exists(store.filePath(QStringLiteral("ren-file-2.gpg"))));
  QVERIFY(!QFileInfo::exists(store.filePath(QStringLiteral("ren-file.gpg"))));
}

/**
 * @brief Delete with nothing selected does nothing at all: no question, no
 *        removal (#556: it used to offer to delete the whole store).
 */
void tst_mainwindow::deleteNeedsASelection() {
  treeView()->setCurrentIndex(QModelIndex());
  const int before =
      QDir(m_storeDir.path())
          .entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)
          .size();
  ModalDriver driver(answerBox(QMessageBox::Yes));
  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onDelete",
                                    Qt::DirectConnection));
  QTest::qWait(50);
  QCOMPARE(driver.seen, 0);
  QCOMPARE(
      QDir(m_storeDir.path())
          .entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)
          .size(),
      before);
}

/**
 * @brief Deleting an entry asks first, naming it; No keeps the file, Yes
 *        removes it through the backend.
 */
void tst_mainwindow::deletePasswordAsksFirst() {
  {
    AppSettings s = QtPassSettings::load();
    s.useGit = false;
    QtPassSettings::save(s);
  }
  const QString file =
      QDir(m_storeDir.path()).filePath(QStringLiteral("doomed-file.gpg"));
  QVERIFY(selectEntry(m_window.data(), m_storeDir.path(),
                      QStringLiteral("doomed-file")));
  {
    ModalDriver driver(answerBox(QMessageBox::No));
    QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onDelete",
                                      Qt::DirectConnection));
    COMPARE_BOX_TITLES(driver.boxTitles,
                       QStringList{QStringLiteral("Delete password?")});
    QVERIFY2(driver.boxTexts.value(0).contains(QStringLiteral("doomed-file")),
             qPrintable(driver.boxTexts.value(0)));
  }
  QVERIFY2(QFileInfo::exists(file), "No keeps the entry");

  QVERIFY(selectPath(file));
  ModalDriver driver(answerBox(QMessageBox::Yes));
  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onDelete",
                                    Qt::DirectConnection));
  QCOMPARE(driver.seen, 1);
  QTRY_VERIFY2(!QFileInfo::exists(file), "Yes removes the entry");
}

/**
 * @brief Deleting a folder says the whole content goes, and points out
 *        files in it that are not encrypted entries before asking.
 */
void tst_mainwindow::deleteFolderWarnsAboutStrayFiles() {
  {
    AppSettings s = QtPassSettings::load();
    s.useGit = false;
    QtPassSettings::save(s);
  }
  const QDir store(m_storeDir.path());
  QVERIFY(store.mkpath(QStringLiteral("doomed-dir")));
  {
    QFile stray(store.filePath(QStringLiteral("doomed-dir/notes.txt")));
    QVERIFY(stray.open(QIODevice::WriteOnly));
    stray.write("plain text");
  }
  QVERIFY(selectPath(store.filePath(QStringLiteral("doomed-dir"))));
  {
    ModalDriver driver(answerBox(QMessageBox::No));
    QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onDelete",
                                      Qt::DirectConnection));
    COMPARE_BOX_TITLES(driver.boxTitles,
                       QStringList{QStringLiteral("Delete folder?")});
    const QString text = driver.boxTexts.value(0);
    QVERIFY2(text.contains(QStringLiteral("whole content")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("unexpected files")),
             qPrintable(text));
  }
  QVERIFY(QFileInfo(store.filePath(QStringLiteral("doomed-dir"))).isDir());

  // Only entries inside: no warning about the content.
  QVERIFY(
      QFile::remove(store.filePath(QStringLiteral("doomed-dir/notes.txt"))));
  QVERIFY(selectEntry(m_window.data(), m_storeDir.path(),
                      QStringLiteral("doomed-dir/entry")));
  QVERIFY(selectPath(store.filePath(QStringLiteral("doomed-dir"))));
  ModalDriver driver(answerBox(QMessageBox::Yes));
  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onDelete",
                                    Qt::DirectConnection));
  QCOMPARE(driver.seen, 1);
  QVERIFY2(!driver.boxTexts.value(0).contains(QStringLiteral("unexpected")),
           qPrintable(driver.boxTexts.value(0)));
  QTRY_VERIFY2(!QFileInfo::exists(store.filePath(QStringLiteral("doomed-dir"))),
               "Yes removes the folder and its entries");
}

/**
 * @brief A linked folder in the tree is deleted as a link: the question
 *        says so, and what it points to stays. An entry seen through a link
 *        is refused, as it is not the store's to delete.
 */
void tst_mainwindow::deleteLinkedFolderRemovesOnlyTheLink() {
#ifdef Q_OS_WIN
  QSKIP("uses a symlink");
#else
  {
    AppSettings s = QtPassSettings::load();
    s.useGit = false;
    QtPassSettings::save(s);
  }
  QTemporaryDir outside;
  QVERIFY(outside.isValid());
  {
    QFile f(outside.filePath(QStringLiteral("behind.gpg")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
  }
  const QDir store(m_storeDir.path());
  const QString link = store.filePath(QStringLiteral("linked-dir"));
  QVERIFY(QFile::link(outside.path(), link));
  const auto cleanup = qScopeGuard([&link] { QFile::remove(link); });

  // The entry behind the link: refused, nothing asked.
  QVERIFY(selectPath(link + QStringLiteral("/behind.gpg")));
  {
    ModalDriver driver(answerBox(QMessageBox::Yes));
    QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onDelete",
                                      Qt::DirectConnection));
    COMPARE_BOX_TITLES(driver.boxTitles, QStringList{QStringLiteral(
                                             "Not a folder of the store")});
    QVERIFY2(driver.boxTexts.value(0).contains(QStringLiteral("symbolic link")),
             qPrintable(driver.boxTexts.value(0)));
  }
  QVERIFY(QFileInfo::exists(outside.filePath(QStringLiteral("behind.gpg"))));

  // The link itself: asked as a link, removed as a link.
  QVERIFY(selectPath(link));
  ModalDriver driver(answerBox(QMessageBox::Yes));
  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onDelete",
                                    Qt::DirectConnection));
  COMPARE_BOX_TITLES(driver.boxTitles,
                     QStringList{QStringLiteral("Delete link?")});
  QVERIFY2(driver.boxTexts.value(0).contains(QStringLiteral("left alone")),
           qPrintable(driver.boxTexts.value(0)));
  QTRY_VERIFY(!QFileInfo(link).isSymLink() && !QFileInfo::exists(link));
  QVERIFY2(QFileInfo::exists(outside.filePath(QStringLiteral("behind.gpg"))),
           "what the link pointed to is untouched");
#endif
}

/**
 * @brief Users opens the recipients dialog for the current folder; for a
 *        folder behind a link it refuses with a message instead.
 */
void tst_mainwindow::usersDialogOpensForTheStoreButNotForALink() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as the gpg stand-in and a symlink");
#else
  QTemporaryDir scratch;
  QVERIFY(scratch.isValid());
  QVERIFY(!rebuildWithFakeGpg(scratch).isEmpty());
  treeView()->setCurrentIndex(QModelIndex());
  bool usersSeen = false;
  {
    ModalDriver driver([&usersSeen](QWidget *w) {
      if (auto *dialog = qobject_cast<UsersDialog *>(w)) {
        usersSeen = true;
        dialog->reject();
        return;
      }
      answerBox(QMessageBox::Ok)(w);
    });
    auto *users = m_window->findChild<QAction *>(QStringLiteral("actionUsers"));
    QVERIFY(users != nullptr);
    users->trigger();
    QVERIFY2(usersSeen, "the users dialog opens for the store root");
    QVERIFY2(driver.boxTitles.isEmpty(),
             qPrintable(driver.boxTitles.join(QStringLiteral(", "))));
  }

  QTemporaryDir outside;
  QVERIFY(outside.isValid());
  const QString link =
      QDir(m_storeDir.path()).filePath(QStringLiteral("users-link"));
  QVERIFY(QFile::link(outside.path(), link));
  const auto cleanup = qScopeGuard([&link] { QFile::remove(link); });
  QVERIFY(selectPath(link));
  usersSeen = false;
  ModalDriver driver([&usersSeen](QWidget *w) {
    usersSeen = usersSeen || qobject_cast<UsersDialog *>(w) != nullptr;
    answerBox(QMessageBox::Ok)(w);
  });
  QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onUsers",
                                    Qt::DirectConnection));
  QVERIFY2(!usersSeen, "no dialog for a folder behind a link");
  COMPARE_BOX_TITLES(driver.boxTitles,
                     QStringList{QStringLiteral("Not a folder of the store")});
  QVERIFY2(driver.boxTexts.value(0).contains(QStringLiteral("users-link")),
           qPrintable(driver.boxTexts.value(0)));
#endif
}

/**
 * @brief "Add password" opens the entry dialog with every real folder of
 *        the store to choose from, the tree's current folder preselected,
 *        and the store's templates with the folder's default chosen.
 */
void tst_mainwindow::addPasswordOffersTheStoreFolders() {
  const QDir store(m_storeDir.path());
  QVERIFY(store.mkpath(QStringLiteral("offered/deeper")));
  QVERIFY(store.mkpath(QStringLiteral(".hidden-offered/inside")));
  // Store templates, with the deeper folder's own default.
  const QString templatesFile = store.filePath(QStringLiteral(".templates"));
  {
    QFile f(templatesFile);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("[work]\nlogin\nurl\n\n[home]\nlogin\n");
    QFile d(store.filePath(QStringLiteral("offered/deeper/.default_template")));
    QVERIFY(d.open(QIODevice::WriteOnly));
    d.write("home\n");
  }
  const auto cleanup =
      qScopeGuard([&templatesFile] { QFile::remove(templatesFile); });
  QVERIFY(selectPath(store.filePath(QStringLiteral("offered/deeper"))));
  QStringList folders;
  QString current;
  QStringList templates;
  QString chosenTemplate;
  bool escaped = false;
  ModalDriver driver([&](QWidget *w) {
    if (auto *dialog = qobject_cast<PasswordDialog *>(w)) {
      if (auto *box =
              dialog->findChild<QComboBox *>(QStringLiteral("folderBox"))) {
        for (int i = 0; i < box->count(); ++i) {
          folders << box->itemData(i).toString();
        }
        current = box->currentData().toString();
      }
      if (auto *box =
              dialog->findChild<QComboBox *>(QStringLiteral("templateBox"))) {
        for (int i = 0; i < box->count(); ++i) {
          templates << box->itemText(i);
        }
        chosenTemplate = box->currentText();
      }
      dialog->reject();
      return;
    }
    escaped = true;
    answerBox(QMessageBox::Ok)(w);
  });
  auto *add =
      m_window->findChild<QAction *>(QStringLiteral("actionAddPassword"));
  QVERIFY(add != nullptr);
  add->trigger();
  QCOMPARE(driver.seen, 1);
  QVERIFY(!escaped);
  QVERIFY2(folders.contains(QString()), "the store root is offered");
  QVERIFY2(folders.contains(QStringLiteral("offered")),
           qPrintable(folders.join(QStringLiteral(", "))));
  QVERIFY2(folders.contains(QStringLiteral("offered/deeper")),
           qPrintable(folders.join(QStringLiteral(", "))));
  QVERIFY2(!folders.contains(QStringLiteral(".hidden-offered")) &&
               !folders.contains(QStringLiteral(".hidden-offered/inside")),
           qPrintable(QStringLiteral("hidden folders are not offered: ") +
                      folders.join(QStringLiteral(", "))));
  QCOMPARE(current, QStringLiteral("offered/deeper"));
  QCOMPARE(templates,
           (QStringList{QStringLiteral("home"), QStringLiteral("work")}));
  QCOMPARE(chosenTemplate, QStringLiteral("home"));
  QCOMPARE(m_window->focusWidget(), treeView());
}

/**
 * @brief Cancel on the re-encryption progress dialog says so in the status
 *        bar (the backend's cancel flag is not observable from here), and
 *        the end of the run releases the interface.
 */
void tst_mainwindow::reencryptCancelIsReportedInTheStatusBar() {
  m_window->startReencryptPath();
  auto *progress = m_window->findChild<QProgressDialog *>();
  QVERIFY2(progress != nullptr, "a progress dialog must be shown");
  auto *cancel = progress->findChild<QPushButton *>();
  QVERIFY2(cancel != nullptr, "the dialog has its Cancel button");
  cancel->click();
  QCOMPARE(m_window->statusBar()->currentMessage(),
           QStringLiteral("Cancelling re-encryption"));
  m_window->endReencryptPath();
  QVERIFY(treeView()->isEnabled());
}

QTEST_MAIN(tst_mainwindow)
#include "tst_mainwindow.moc"
