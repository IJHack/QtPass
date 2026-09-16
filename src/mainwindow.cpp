// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

#include "configdialog.h"
#include "enums.h"
#include "executor.h"
#include "exportpublickeydialog.h"
#include "filecontent.h"
#include "passworddialog.h"
#include "passworddisplaypanel.h"
#include "pathvalidator.h"
#include "qpushbuttonasqrcode.h"
#include "qpushbuttonshowpassword.h"
#include "qpushbuttonwithclipboard.h"
#include "qtcompat.h"
#include "qtpass.h"
#include "qtpasssettings.h"
#include "templateio.h"
#include "totp.h"
#include "trayicon.h"
#include "ui_mainwindow.h"
#include "usersdialog.h"
#include "util.h"
#include "windowstatestore.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDialog>
#include <QDirIterator>
#include <QDockWidget>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <utility>

/**
 * @brief MainWindow::MainWindow handles all of the main functionality and also
 * the main window.
 * @param searchText for searching from cli
 * @param parent pointer
 */
MainWindow::MainWindow(const QString &searchText, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
#ifdef __APPLE__
  // extra treatment for mac os
  // see https://doc.qt.io/qt-6/qkeysequence.html#qt_set_sequence_auto_mnemonic
  qt_set_sequence_auto_mnemonic(true);
#endif
  ui->setupUi(this);

  m_qtPass = new QtPass(this);

  // register shortcut ctrl/cmd + Q to close the main window
  new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q), this, this,
                &MainWindow::close);
  // register shortcut ctrl/cmd + C to copy the currently selected password
  new QShortcut(QKeySequence(QKeySequence::StandardKey::Copy), this, this,
                &MainWindow::copyPasswordFromTreeview);

  model.setNameFilters(QStringList() << "*.gpg");
  model.setNameFilterDisables(false);

  QString passStore = QtPassSettings::getPassStore(Util::findPasswordStore());

  QModelIndex rootDir = model.setRootPath(passStore);
  model.fetchMore(rootDir);

  proxyModel.setModelAndStore(&model, passStore);
  proxyModel.setPass(QtPassSettings::getPass());

  ui->treeView->setModel(&proxyModel);
  ui->treeView->setRootIndex(proxyModel.mapFromSource(rootDir));
  ui->treeView->setColumnHidden(1, true);
  ui->treeView->setColumnHidden(2, true);
  ui->treeView->setColumnHidden(3, true);
  ui->treeView->setHeaderHidden(true);
  ui->treeView->setIndentation(15);
  ui->treeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  ui->treeView->sortByColumn(0, Qt::AscendingOrder);
  connect(ui->treeView, &QWidget::customContextMenuRequested, this,
          &MainWindow::showContextMenu);
  connect(ui->treeView, &DeselectableTreeView::emptyClicked, this,
          &MainWindow::deselect);

  {
    const AppSettings s = QtPassSettings::load();
    if (s.useMonospace) {
      QFont monospace("Monospace");
      monospace.setStyleHint(QFont::Monospace);
      ui->textBrowser->setFont(monospace);
    }
    if (s.noLineWrapping) {
      ui->textBrowser->setLineWrapMode(QTextBrowser::NoWrap);
    }
    clearPanelTimer.setInterval(MS_PER_SECOND * s.autoclearPanelSeconds);
  }
  ui->textBrowser->setOpenExternalLinks(true);
  ui->textBrowser->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->textBrowser, &QWidget::customContextMenuRequested, this,
          &MainWindow::showBrowserContextMenu);

  updateProfileBox();

  m_displayPanel = new PasswordDisplayPanel(
      ui->gridLayout, ui->verticalLayoutPassword, this, this);
  connect(m_displayPanel, &PasswordDisplayPanel::copyRequested, m_qtPass,
          &QtPass::copyTextToClipboard);
  connect(m_displayPanel, &PasswordDisplayPanel::qrRequested, m_qtPass,
          &QtPass::showTextAsQRCode);

  QtPassSettings::getPass()->updateEnv();
  clearPanelTimer.setSingleShot(true);
  connect(&clearPanelTimer, &QTimer::timeout, this, [this]() { clearPanel(); });

  searchTimer.setInterval(350);
  searchTimer.setSingleShot(true);

  connect(&searchTimer, &QTimer::timeout, this, &MainWindow::onTimeoutSearch);

  // Install the search-box key filter once, not on every setUiElementsEnabled
  // call.
  ui->lineEdit->installEventFilter(this);
  ui->toolBar->installEventFilter(this);

  // Safety net: if a backend operation disables the UI but never signals
  // completion, re-enable after a timeout so the window can't get stuck.
  m_uiWatchdog.setSingleShot(true);
  m_uiWatchdog.setInterval(UiWatchdogMs);
  connect(&m_uiWatchdog, &QTimer::timeout, this, [this]() {
    showStatusMessage(tr("Operation timed out; re-enabling interface."));
    // Drop any in-flight OTP request so a late finishedShow cannot be mistaken
    // for the answer to it.
    cancelOtpRequest();
    setUiElementsEnabled(true);
  });

  initToolBarButtons();
  initStatusBar();
  initProcessOutputPanel();

  connect(QtPassSettings::getPass(), &Pass::finishedAnyWithPid, this,
          [this](const QString &out, const QString &err, Enums::PROCESS pid) {
            // Never route potentially-secret output through the panel:
            // - PASS_SHOW goes via a dedicated signal to
            //   the main text browser (which clears on a timer).
            // - PASS_GREP returns lines from password files; #252 must
            //   not leak those into a long-lived panel.
            // - PASS_INSERT's stdin is the password; stdout normally
            //   carries gpg/git progress only, but exclude defensively
            //   in case a future code path uses --echo or similar.
            if (isSensitiveProcess(pid)) {
              return;
            }
            if (!out.isEmpty()) {
              onProcessOutput(out, false, pid);
            }
            if (!err.isEmpty()) {
              onProcessOutput(err, true, pid);
            }
          });

  ui->lineEdit->setClearButtonEnabled(true);
  updateGrepButtonVisibility();

  setUiElementsEnabled(true);

  ui->lineEdit->setText(searchText);

  // Record whether startup configuration succeeded instead of calling
  // QApplication::quit() here: quit() before the event loop runs (exec() is
  // still ahead in main()) is a documented no-op, so a cancelled first-run
  // wizard used to leave the half-configured window showing anyway. main()
  // consults initSucceeded() and exits before show() when this is false.
  m_initSucceeded = m_qtPass->init();
  if (!m_initSucceeded) {
    return;
  }

  // Initial focus is handled in showEvent() once the window is actually
  // mapped. Scheduling it here via a 10 ms QTimer was racy: if the timer
  // fires while the window has not yet been realised — e.g. an
  // ActivationChange queued by main()'s `activateWindow()` call before
  // `show()`, or a nested QDialog::exec() inside init() — the
  // QLineEdit's internal text engine hasn't been wired up and
  // selectAll() segfaults inside Qt (see #1187, #1188).
}

MainWindow::~MainWindow() {
  // Quit from the tray menu or a SIGTERM never delivers closeEvent(); the
  // window still knows its last geometry here.
  saveWindowState();
  delete m_qtPass;
}

/**
 * @brief MainWindow::saveWindowState persist geometry and dock/toolbar
 *        layout. Idempotent, cheap, called from closeEvent() and the
 *        destructor.
 */
void MainWindow::saveWindowState() {
  QtPassSettings::setGeometry(saveGeometry());
  QtPassSettings::setSavestate(saveState());
}

/**
 * @brief MainWindow::focusInput selects any text (if applicable) in the search
 * box and sets focus to it. Allows for easy searching, called at application
 * start and when receiving empty message in MainWindow::messageAvailable when
 * compiled with SINGLE_APP=1 (default).
 */
void MainWindow::focusInput() {
  // Resolve the QLineEdit through the live widget tree rather than the
  // cached `ui->lineEdit` pointer.
  //
  // On a fresh-config first launch the constructor calls
  // `m_qtPass->init()` → `MainWindow::config()`, and `config()`'s
  // `applyWindowFlagsSettings()` does `setWindowFlags(...)` + `show()`
  // on the main window. `setWindowFlags` on a top-level widget rebuilds
  // the native window via `setParent(nullptr, flags)`; under Qt 6.11
  // we observed the QLineEdit attached to the centralWidget gets
  // destroyed in that rebuild while `ui->lineEdit` still holds its old
  // address — leading to a SIGSEGV inside `QWidget::testAttribute`
  // (called from `QLineEdit::isVisible` / `selectAll`). `findChild<>()`
  // walks the current hierarchy and returns null cleanly when the
  // widget is gone, so `focusInput` becomes a safe no-op instead of a
  // use-after-free.
  if (!isVisible()) {
    return;
  }
  auto *lineEdit = findChild<QLineEdit *>(QStringLiteral("lineEdit"));
  if (lineEdit == nullptr || !lineEdit->isVisible()) {
    return;
  }
  lineEdit->selectAll();
  lineEdit->setFocus();
  // Only mark the first-show focus pulse as done once it's actually
  // landed; setting it eagerly in showEvent() would consume the
  // one-shot if focusInput returned early (mid-rebuild widget state)
  // and we'd never retry.
  m_firstShowCompleted = true;
}

/**
 * @brief MainWindow::changeEvent sets focus to the search box on activation
 * and re-derives palette-dependent styling on a theme change.
 * @param event
 */
void MainWindow::changeEvent(QEvent *event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::PaletteChange && m_displayPanel != nullptr) {
    // Desktop switched light/dark (e.g. KDE day/night). Top-level widgets
    // receive PaletteChange for that (ApplicationPaletteChange goes to the
    // QApplication object). Styling that bakes palette colours into
    // stylesheets must be re-derived by hand.
    m_displayPanel->refreshPalette();
  }
  if (event->type() == QEvent::ActivationChange && isActiveWindow() &&
      isVisible()) {
    // Defer one event-loop tick so the synchronous activation dispatch
    // chain (`QApplicationPrivate::setActiveWindow` → `notify_helper`)
    // unwinds before we touch widget state — calling `focusInput()`
    // inline from this stack has segfaulted in past iterations because
    // mid-rebuild ui state isn't fully wired up yet.
    QMetaObject::invokeMethod(this, &MainWindow::focusInput,
                              Qt::QueuedConnection);
  }
}

/**
 * @brief First-show hook: run the initial focusInput() pulse once the
 *        window is actually mapped. The widget's internal data is fully
 *        initialised by this point, so QLineEdit::selectAll() is safe.
 * @param event Show event passed to the base class.
 */
void MainWindow::showEvent(QShowEvent *event) {
  QMainWindow::showEvent(event);
  if (m_firstShowCompleted) {
    return;
  }
  // Queue the focus pulse for the next event-loop tick so the platform
  // map round-trip and any pending widget rebuilds (e.g. setWindowFlags
  // from the config wizard path) settle before we look up the line
  // edit. The `m_firstShowCompleted` latch is set inside focusInput()
  // *after* it actually focuses, so a transient failed lookup just
  // re-queues on the next show rather than silently dropping.
  QMetaObject::invokeMethod(this, &MainWindow::focusInput,
                            Qt::QueuedConnection);
}

/**
 * @brief MainWindow::initToolBarButtons init main ToolBar and connect actions
 */
void MainWindow::initToolBarButtons() {
  connect(ui->actionAddPassword, &QAction::triggered, this,
          &MainWindow::addPassword);
  connect(ui->actionAddFolder, &QAction::triggered, this,
          &MainWindow::addFolder);
  connect(ui->actionEdit, &QAction::triggered, this, &MainWindow::onEdit);
  connect(ui->actionDelete, &QAction::triggered, this, &MainWindow::onDelete);
  connect(ui->actionPush, &QAction::triggered, this, &MainWindow::onPush);
  connect(ui->actionUpdate, &QAction::triggered, this, &MainWindow::onUpdate);
  connect(ui->actionUsers, &QAction::triggered, this, &MainWindow::onUsers);
  connect(ui->actionConfig, &QAction::triggered, this, &MainWindow::onConfig);
  connect(ui->actionOtp, &QAction::triggered, this, &MainWindow::onOtp);

  ui->actionAddPassword->setIcon(
      QIcon::fromTheme("document-new", QIcon(":/icons/document-new.svg")));
  ui->actionAddFolder->setIcon(
      QIcon::fromTheme("folder-new", QIcon(":/icons/folder-new.svg")));
  ui->actionEdit->setIcon(QIcon::fromTheme(
      "document-properties", QIcon(":/icons/document-properties.svg")));
  ui->actionDelete->setIcon(
      QIcon::fromTheme("edit-delete", QIcon(":/icons/edit-delete.svg")));
  ui->actionPush->setIcon(
      QIcon::fromTheme("go-up", QIcon(":/icons/go-top.svg")));
  ui->actionUpdate->setIcon(
      QIcon::fromTheme("go-down", QIcon(":/icons/go-bottom.svg")));
  ui->actionUsers->setIcon(QIcon::fromTheme(
      "x-office-address-book", QIcon(":/icons/x-office-address-book.svg")));
  ui->actionConfig->setIcon(QIcon::fromTheme(
      "applications-system", QIcon(":/icons/applications-system.svg")));
}

/**
 * @brief MainWindow::initStatusBar init statusBar with default message and logo
 */
void MainWindow::initStatusBar() {
  ui->statusBar->showMessage(tr("Welcome to QtPass %1").arg(VERSION), 2000);

  QPixmap logo = QPixmap::fromImage(QImage(":/artwork/icon.svg"))
                     .scaledToHeight(statusBar()->height());
  auto *logoApp = new QLabel(statusBar());
  logoApp->setPixmap(logo);
  statusBar()->addPermanentWidget(logoApp);
}

/**
 * @brief Build the process-output panel as a bottom QDockWidget.
 *
 * The panel is constructed programmatically rather than declared in
 * mainwindow.ui: uic only places QMainWindow's top-level children into
 * the centralWidget / statusBar / menuBar / toolBars / dock-widget
 * slots, and the previous home (statusBar()->addPermanentWidget()) made
 * an 80–150 px tall QTextEdit sit inside what is otherwise a thin
 * status row. A QDockWidget at the bottom dock area is the conventional
 * place for an IDE-style output console, and it gives users
 * detach/move for free.
 */
void MainWindow::initProcessOutputPanel() {
  m_processOutputWidget = new QWidget;
  m_processOutputWidget->setObjectName(QStringLiteral("processOutputWidget"));
  auto *outputLayout = new QHBoxLayout(m_processOutputWidget);
  outputLayout->setObjectName(QStringLiteral("processOutputLayout"));
  outputLayout->setContentsMargins(0, 0, 0, 0);
  m_clearOutputButton = new QToolButton(m_processOutputWidget);
  m_clearOutputButton->setObjectName(QStringLiteral("clearOutputButton"));
  m_clearOutputButton->setText(tr("Clear"));
  m_clearOutputButton->setToolTip(tr("Clear output"));
  outputLayout->addWidget(m_clearOutputButton);
  m_processOutputEdit = new QTextEdit(m_processOutputWidget);
  m_processOutputEdit->setObjectName(QStringLiteral("processOutputEdit"));
  m_processOutputEdit->setReadOnly(true);
  m_processOutputEdit->setAcceptRichText(false);
  outputLayout->addWidget(m_processOutputEdit);

  m_processOutputDock = new QDockWidget(tr("Process Output"), this);
  m_processOutputDock->setObjectName(QStringLiteral("processOutputDock"));
  m_processOutputDock->setFeatures(QDockWidget::DockWidgetMovable |
                                   QDockWidget::DockWidgetFloatable);
  m_processOutputDock->setAllowedAreas(Qt::BottomDockWidgetArea |
                                       Qt::TopDockWidgetArea);
  m_processOutputDock->setWidget(m_processOutputWidget);
  addDockWidget(Qt::BottomDockWidgetArea, m_processOutputDock);
  // setVisible after addDockWidget so our explicit preference wins
  // even if QMainWindow applies any cached state when the dock is
  // attached. restoreWindow() runs before this method (it's called
  // from the QtPass ctor, which is constructed at the top of the
  // MainWindow ctor), so the saved layout has already been processed
  // by the time we get here.
  m_processOutputDock->setVisible(QtPassSettings::isShowProcessOutput());

  connect(m_clearOutputButton, &QToolButton::clicked, this,
          &MainWindow::on_clearOutputButton_clicked);

  // Hysteresis: while the user is actively dragging the slider, don't
  // touch m_autoScroll on every tick — a brief overshoot at maximum
  // would silently re-arm auto-scroll without an explicit release. Only
  // commit on slider release. Wheel/keyboard scroll never sets
  // isSliderDown(), so they still update immediately.
  connect(m_processOutputEdit->verticalScrollBar(), &QScrollBar::valueChanged,
          this, [this]() {
            auto *sb = m_processOutputEdit->verticalScrollBar();
            if (sb->isSliderDown())
              return;
            m_autoScroll = sb->value() >= sb->maximum();
          });
  connect(m_processOutputEdit->verticalScrollBar(), &QScrollBar::sliderReleased,
          this, [this]() {
            auto *sb = m_processOutputEdit->verticalScrollBar();
            m_autoScroll = sb->value() >= sb->maximum();
          });
}

auto MainWindow::getCurrentTreeViewIndex() -> QModelIndex {
  return ui->treeView->currentIndex();
}

void MainWindow::cleanKeygenDialog() {
  if (m_keyGenDialog != nullptr) {
    m_keyGenDialog->close();
  }
  m_keyGenDialog = nullptr;
}

/**
 * @brief Displays the given text in the main window text browser, optionally
 * marking it as an error and/or rendering it as HTML.
 * @example
 * MainWindow window;
 * window.flashText("Operation completed.", false, false);
 *
 * @param const QString &text - The text content to display.
 * @param const bool isError - If true, sets the text color to red before
 * displaying the text; otherwise any earlier error colour is cleared.
 * @param const bool isHtml - If true, treats the text as HTML and appends it to
 * the existing HTML content.
 * @return void - No return value.
 */
void MainWindow::flashText(const QString &text, const bool isError,
                           const bool isHtml) {
  if (isError) {
    ui->textBrowser->setTextColor(Qt::red);
  } else {
    // setTextColor() merges the red foreground into the browser's current
    // char format, and setText()/setPlainText() re-applies that format to the
    // whole new document. Without clearing it, a plain-text non-error message
    // shown after an error would still be red. Remove the property rather
    // than pinning a palette colour: an explicit foreground would stop the
    // text from following runtime light/dark palette switches (#946).
    QTextCharFormat format = ui->textBrowser->currentCharFormat();
    format.clearForeground();
    ui->textBrowser->setCurrentCharFormat(format);
  }

  if (isHtml) {
    QString _text = text;
    if (!ui->textBrowser->toPlainText().isEmpty()) {
      _text = ui->textBrowser->toHtml() + _text;
    }
    ui->textBrowser->setHtml(_text);
  } else {
    ui->textBrowser->setText(text);
  }
}

/**
 * @brief MainWindow::config pops up the configuration screen and handles all
 * inter-window communication
 */
void MainWindow::applyTextBrowserSettings() {
  const AppSettings s = QtPassSettings::load();
  if (s.useMonospace) {
    QFont monospace("Monospace");
    monospace.setStyleHint(QFont::Monospace);
    ui->textBrowser->setFont(monospace);
  } else {
    ui->textBrowser->setFont(QFont());
  }

  if (s.noLineWrapping) {
    ui->textBrowser->setLineWrapMode(QTextBrowser::NoWrap);
  } else {
    ui->textBrowser->setLineWrapMode(QTextBrowser::WidgetWidth);
  }
}

void MainWindow::applyWindowFlagsSettings() {
  const bool wantOnTop = QtPassSettings::isAlwaysOnTop();
  const Qt::WindowFlags flags = windowFlags();
  if (flags.testFlag(Qt::WindowStaysOnTopHint) == wantOnTop) {
    // Nothing to change. setWindowFlags() on a top-level widget rebuilds the
    // native window (setParent(nullptr, flags)), which used to happen on every
    // Settings OK because the "off" branch never compared equal — the same
    // teardown that left ui->lineEdit dangling (see focusInput()).
    return;
  }
  // Toggle just the one hint; the old "off" branch reset to Qt::Window and
  // discarded every other flag with it.
  const bool wasVisible = isVisible();
  setWindowFlags(wantOnTop ? (flags | Qt::WindowStaysOnTopHint)
                           : (flags & ~Qt::WindowStaysOnTopHint));
  // setWindowFlags() hides a shown window; a window that was not shown yet
  // (first-run wizard from the constructor) is left to its normal show.
  if (wasVisible) {
    show();
  }
}

/**
 * @brief Opens and processes the application configuration dialog, then applies
 * any accepted settings.
 * @example
 * if (!config()) {
 *   // the user cancelled
 * }
 *
 * @return bool - True when the dialog was accepted, false when it was
 * cancelled. An accepted dialog can still leave the configuration invalid
 * (the OK button is not gated on Util::configIsValid()); QtPass::init() keeps
 * asking until the configuration is usable or this returns false.
 */
auto MainWindow::config() -> bool {
  ConfigDialog d(this);
  d.setModal(true);
  // Automatically default to pass if it's available
  if (m_qtPass->isFreshStart() &&
      QFile(QtPassSettings::getPassExecutable()).exists()) {
    QtPassSettings::setUsePass(true);
  }

  if (m_qtPass->isFreshStart()) {
    d.wizard(); // run initial setup wizard for first-time configuration
  }
  if (d.exec() != QDialog::Accepted) {
    return false;
  }

  applyTextBrowserSettings();
  applyWindowFlagsSettings();

  updateProfileBox();
  const AppSettings s = QtPassSettings::load();
  proxyModel.setStore(s.passStore);
  ui->treeView->setRootIndex(proxyModel.rootIndexFor(s.passStore));
  deselect();
  ui->treeView->setCurrentIndex(QModelIndex());

  Pass *activePass = QtPassSettings::getPass();
  activePass->updateEnv();
  proxyModel.setPass(activePass);
  clearPanelTimer.setInterval(MS_PER_SECOND * s.autoclearPanelSeconds);
  m_qtPass->setClipboardTimer();

  updateGitButtonVisibility();
  updateOtpButtonVisibility();
  updateGrepButtonVisibility();
  updateProcessOutputVisibility();
  if (s.useTrayIcon && m_tray == nullptr) {
    initTrayIcon();
  } else if (!s.useTrayIcon && m_tray != nullptr) {
    destroyTrayIcon();
  }

  // Leave the fresh-start state in place while the accepted configuration is
  // still unusable (for example the user declined to create the store), so
  // the next attempt from QtPass::init() runs the first-run wizard again
  // instead of showing the bare dialog. The re-prompt itself lives in init():
  // recursing here re-ran the dialog on this stack frame and never reported
  // a cancel back to the caller.
  if (Util::configIsValid(s)) {
    m_qtPass->setFreshStart(false);
  }
  return true;
}

/**
 * @brief MainWindow::onUpdate do a git pull
 */
void MainWindow::onUpdate(bool block) {
  ui->statusBar->showMessage(tr("Updating password-store"), 2000);
  if (block) {
    QtPassSettings::getPass()->GitPull_b();
  } else {
    QtPassSettings::getPass()->GitPull();
  }
}

/**
 * @brief MainWindow::onPush do a git push
 */
void MainWindow::onPush() {
  if (QtPassSettings::isUseGit()) {
    ui->statusBar->showMessage(tr("Updating password-store"), 2000);
    QtPassSettings::getPass()->GitPush();
  }
}

/**
 * @brief MainWindow::getFile get the selected file path
 * @param index
 * @param forPass returns relative path without '.gpg' extension
 * @return path
 * @return
 */
auto MainWindow::getFile(const QModelIndex &index, bool forPass) -> QString {
  if (!index.isValid() ||
      !model.fileInfo(proxyModel.mapToSource(index)).isFile()) {
    return {};
  }
  QString filePath = model.filePath(proxyModel.mapToSource(index));
  if (forPass) {
    filePath = QDir(QtPassSettings::getPassStore()).relativeFilePath(filePath);
    filePath.replace(Util::endsWithGpg(), "");
  }
  return filePath;
}

/**
 * @brief MainWindow::on_treeView_clicked read the selected password file
 * @param index
 */
void MainWindow::on_treeView_clicked(const QModelIndex &index) {
  bool cleared = ui->treeView->currentIndex().flags() == Qt::NoItemFlags;
  QString file = getFile(index, true);
  ui->passwordName->setText(file);
  if (!file.isEmpty() && !cleared) {
    // Remember what the panel is about to show, so onOtp() can tell whether the
    // code it can see belongs to the entry that is currently selected.
    m_shownFile = file;
    QtPassSettings::getPass()->Show(file);
  } else {
    m_shownFile.clear();
    clearPanel(false);
    ui->actionEdit->setEnabled(false);
    ui->actionDelete->setEnabled(true);
  }
}

/**
 * @brief MainWindow::on_treeView_doubleClicked when doubleclicked on
 * TreeViewItem, open the edit Window
 * @param index
 */
void MainWindow::on_treeView_doubleClicked(const QModelIndex &index) {
  QFileInfo fileOrFolder =
      model.fileInfo(proxyModel.mapToSource(ui->treeView->currentIndex()));

  if (fileOrFolder.isFile()) {
    editPassword(getFile(index, true));
  }
}

/**
 * @brief MainWindow::deselect clear the selection, password and copy buffer
 */
void MainWindow::deselect() {
  m_shownFile.clear();
  cancelOtpRequest();
  m_qtPass->clearClipboard();
  ui->treeView->clearSelection();
  ui->actionEdit->setEnabled(false);
  ui->actionDelete->setEnabled(false);
  ui->passwordName->setText("");
  clearPanel(false);
}

void MainWindow::executeWrapperStarted() {
  m_displayPanel->clear();
  ui->textBrowser->clear();
  setUiElementsEnabled(false);
  clearPanelTimer.stop();
  if (QtPassSettings::isShowProcessOutput()) {
    m_processOutputDock->setVisible(true);
  }
}

/**
 * @brief Handles displaying parsed password entry content in the main window.
 * @example
 * void result = MainWindow::passShowHandler(p_output);
 * // Updates the UI with parsed fields and emits
 * passShowHandlerFinished(output)
 *
 * @param p_output - The raw output text containing the password entry data.
 * @return void - This function does not return a value.
 */
void MainWindow::passShowHandler(const QString &p_output) {
  const AppSettings s = QtPassSettings::load();
  QStringList templ =
      s.useTemplate ? s.passTemplate.split("\n") : QStringList();
  bool allFields = s.useTemplate && s.templateAllFields;
  FileContent fileContent = FileContent::parse(p_output, templ, allFields);
  QString output = p_output;
  // Display variant: empty when the password line is itself an otpauth URI, so
  // the shared secret is neither rendered nor copied to the clipboard.
  QString password = fileContent.getPasswordForDisplay();

  // set clipped text
  //
  // Skipped for an OTP request: the user asked for a one-time code, not the
  // password, and writing both in one event-loop turn can leave the Windows
  // clipboard empty (two OleSetClipboard calls back to back).
  if (!m_otpRequestPending) {
    m_qtPass->setClippedText(password, p_output);
  }

  // first clear the current view:
  m_displayPanel->clear();

  // show what is needed:
  if (s.hideContent) {
    output = "***" + tr("Content hidden") + "***";
  } else if (!s.displayAsIs) {
    m_displayPanel->displayFields(password, fileContent.getNamedValues(), s,
                                  s.useOtp ? fileContent.getOtpUri()
                                           : QString());
    output = fileContent.getRemainingDataForDisplay();
  }

  if (s.useAutoclearPanel) {
    clearPanelTimer.start();
  }

  emit passShowHandlerFinished(output);
  setUiElementsEnabled(true);
}

/**
 * @brief Generates a one-time password from a decrypted entry and copies it.
 *
 * Connected as a one-shot to Pass::finishedShow by onOtp(). passShowHandler is
 * connected first, so by the time this runs the panel has already been
 * repainted and the UI re-enabled.
 *
 * @param p_output - The decrypted entry content.
 * @return void - This function does not return a value.
 */
void MainWindow::otpFromFileToClipboard(const QString &p_output) {
  // A failed decrypt never fires finishedShow, and Qt::SingleShotConnection
  // only self-disconnects when it does fire, so a connection armed by an
  // earlier failed request can still be live here. Ignore it rather than
  // hijacking an unrelated entry's decrypted content.
  if (!m_otpRequestPending) {
    return;
  }
  // finishedShow carries no request identity, so make sure this decrypt is the
  // one we asked for and not a tree click that happened to land first.
  if (m_otpRequestFile != getFile(ui->treeView->currentIndex(), true)) {
    cancelOtpRequest();
    setUiElementsEnabled(true);
    return;
  }
  m_otpRequestPending = false;
  m_otpRequestFile.clear();

  if (p_output.isEmpty()) {
    // Distinguish "could not read the entry" from "entry has no OTP".
    flashText(tr("Could not decrypt this password entry"), true);
    setUiElementsEnabled(true);
    return;
  }

  // passShowHandler is connected first, so it has already repainted the panel
  // for this same finishedShow. When it rendered the OTP row the current code
  // is derived and cached, so reuse it instead of re-loading settings and
  // re-parsing p_output (mirrors onOtp()'s fast path). Falls through to a fresh
  // parse when no OTP row is shown (hideContent / displayAsIs / no OTP field).
  const QString shown = m_displayPanel->currentOtpCode();
  if (!shown.isEmpty()) {
    m_qtPass->copyTextToClipboard(shown);
    showStatusMessage(tr("OTP code copied to clipboard"));
    setUiElementsEnabled(true);
    return;
  }

  const AppSettings s = QtPassSettings::load();
  // Parse with the same template settings passShowHandler uses, so an OTP
  // field is recognised identically in both paths.
  const QStringList templ =
      s.useTemplate ? s.passTemplate.split("\n") : QStringList();
  const bool allFields = s.useTemplate && s.templateAllFields;
  const FileContent fileContent =
      FileContent::parse(p_output, templ, allFields);

  const std::optional<Totp::Settings> settings =
      Totp::parse(fileContent.getOtpUri());
  if (settings.has_value()) {
    m_qtPass->copyTextToClipboard(Totp::generateNow(*settings));
    showStatusMessage(tr("OTP code copied to clipboard"));
  } else {
    flashText(tr("No OTP code found in this password entry"), true);
  }
  setUiElementsEnabled(true);
}

/**
 * @brief MainWindow::clearPanel hide the information from shoulder surfers
 */
void MainWindow::clearPanel(bool notify) {
  m_displayPanel->clear();
  const bool grepWasVisible = ui->grepResultsList->isVisible();
  ui->grepResultsList->clear();
  if (grepWasVisible) {
    ui->grepResultsList->setVisible(false);
    ui->treeView->setVisible(true);
    if (m_grep.inGrepMode()) {
      m_grep.clearGrepMode();
      ui->grepButton->blockSignals(true);
      ui->grepButton->setChecked(false);
      ui->grepButton->blockSignals(false);
      ui->lineEdit->blockSignals(true);
      ui->lineEdit->clear();
      ui->lineEdit->blockSignals(false);
      ui->lineEdit->setPlaceholderText(tr("Search Password"));
    }
  }
  if (notify) {
    QString output = "***" + tr("Password and Content hidden") + "***";
    ui->textBrowser->setHtml(output);
  } else {
    ui->textBrowser->setHtml("");
  }
}

/**
 * @brief MainWindow::setUiElementsEnabled enable or disable the relevant UI
 * elements
 * @param state
 */
void MainWindow::setUiElementsEnabled(bool state) {
  // A running re-encryption owns the UI state: the progress dialog's Cancel is
  // the user's escape hatch and endReencryptPath() releases the interface, so
  // neither an unrelated completion nor the watchdog may re-enable it early.
  if (state && m_reencryptRunning) {
    m_uiWatchdog.stop();
    return;
  }
  // Arm the watchdog while the UI is disabled; disarm once re-enabled.
  if (state) {
    m_uiWatchdog.stop();
  } else {
    m_uiWatchdog.start();
  }
  ui->treeView->setEnabled(state);
  ui->lineEdit->setEnabled(state);
  ui->actionAddPassword->setEnabled(state);
  ui->actionAddFolder->setEnabled(state);
  ui->actionUsers->setEnabled(state);
  ui->actionConfig->setEnabled(state);
  // is a file selected?
  state &= ui->treeView->currentIndex().isValid();
  ui->actionDelete->setEnabled(state);
  ui->actionEdit->setEnabled(state);
  updateGitButtonVisibility();
  // `state` is now "UI enabled AND a file is selected", which is exactly when
  // generating an OTP makes sense.
  updateOtpButtonVisibility(state);
}

/**
 * @brief Restores the main window geometry, state, position, size, and
 * tray/icon settings from saved application settings.
 * @example
 * MainWindow window;
 * window.restoreWindow();
 *
 * @return void - This function does not return a value.
 */
void MainWindow::restoreWindow() {
  // saveGeometry() covers position, size, maximized state and screen; the
  // separate pos/size/maximized keys that used to be applied on top of it
  // are what discarded the restored position, and the unconditional
  // re-centre in main() then discarded the rest. Centre only when nothing
  // was saved.
  const QByteArray geometry = QtPassSettings::getGeometry();
  if (geometry.isEmpty() || !restoreGeometry(geometry)) {
    WindowStateStore::centreOnCursorScreen(*this);
  }
  restoreState(QtPassSettings::getSavestate(saveState()));
  const AppSettings s = QtPassSettings::load();

  applyWindowFlagsSettings();

  if (s.useTrayIcon && m_tray == nullptr) {
    initTrayIcon();
    if (s.startMinimized) {
      // since we are still in constructor, can't directly hide
      QTimer::singleShot(10, this, &MainWindow::hide);
    }
  } else if (!s.useTrayIcon && m_tray != nullptr) {
    destroyTrayIcon();
  }
}

/**
 * @brief MainWindow::on_configButton_clicked run Mainwindow::config
 */
void MainWindow::onConfig() { config(); }

/**
 * @brief Executes when the string in the search box changes, collapses the
 * TreeView
 * @param arg1
 */
void MainWindow::on_lineEdit_textChanged(const QString &arg1) {
  if (m_grep.inGrepMode())
    return;
  ui->statusBar->showMessage(tr("Looking for: %1").arg(arg1), 1000);
  ui->treeView->expandAll();
  clearPanel(false);
  ui->passwordName->setText("");
  ui->actionEdit->setEnabled(false);
  ui->actionDelete->setEnabled(false);
  searchTimer.start();
}

/**
 * @brief MainWindow::onTimeoutSearch Fired when search is finished or too much
 * time from two keypresses is elapsed
 */
void MainWindow::onTimeoutSearch() {
  QString query = ui->lineEdit->text();

  if (query.isEmpty()) {
    ui->treeView->collapseAll();
    deselect();
  }

  query.replace(QStringLiteral(" "), ".*");
  QRegularExpression regExp(query, QRegularExpression::CaseInsensitiveOption);
  if (!regExp.isValid())
    return;
  proxyModel.setFilterRegularExpression(regExp);
  ui->treeView->setRootIndex(
      proxyModel.rootIndexFor(QtPassSettings::getPassStore()));

  if (proxyModel.rowCount() > 0 && !query.isEmpty()) {
    selectFirstFile();
  } else {
    ui->actionEdit->setEnabled(false);
    ui->actionDelete->setEnabled(false);
  }
}

/**
 * @brief MainWindow::on_lineEdit_returnPressed get searching
 *
 * Select the first possible file in the tree
 */
void MainWindow::on_lineEdit_returnPressed() {
#ifdef QT_DEBUG
  dbg() << "on_lineEdit_returnPressed" << proxyModel.rowCount();
#endif

  if (m_grep.inGrepMode()) {
    const QString query = ui->lineEdit->text();
    if (!query.isEmpty()) {
      ui->grepResultsList->clear();
      ui->statusBar->showMessage(tr("Searching…"));
      if (m_grep.beginSearch()) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
      }
      QtPassSettings::getPass()->Grep(query, ui->grepCaseButton->isChecked());
    } else {
      if (m_grep.cancelSearch()) {
        QApplication::restoreOverrideCursor();
      }
      ui->grepResultsList->clear();
      ui->grepResultsList->setVisible(false);
      ui->treeView->setVisible(true);
    }
    return;
  }

  if (proxyModel.rowCount() > 0) {
    selectFirstFile();
    on_treeView_clicked(ui->treeView->currentIndex());
  }
}

/**
 * @brief Toggle grep (content search) mode.
 */
void MainWindow::on_grepButton_toggled(bool checked) {
  const AppSettings s = QtPassSettings::load();
  if (checked) {
    m_grep.enterGrepMode();
    ui->lineEdit->setPlaceholderText(tr("Search content (regex)"));
    // The regex dialect depends on the backend (see Pass::Grep): the pass
    // backend uses POSIX BRE via `pass grep`, the native backend uses PCRE.
    ui->lineEdit->setToolTip(
        s.usePass
            ? tr("Content search uses POSIX basic regular expressions "
                 "(pass grep).")
            : tr("Content search uses Perl-compatible regular expressions "
                 "(PCRE)."));
    ui->lineEdit->clear();
    searchTimer.stop();
    proxyModel.setFilterRegularExpression(QRegularExpression());
    ui->treeView->setRootIndex(proxyModel.rootIndexFor(s.passStore));
    ui->grepResultsList->setVisible(false);
    // Keep treeView visible until results arrive
  } else {
    if (m_grep.leaveGrepMode()) {
      QApplication::restoreOverrideCursor();
    }
    searchTimer.stop();
    ui->lineEdit->blockSignals(true);
    ui->lineEdit->clear();
    ui->lineEdit->blockSignals(false);
    ui->lineEdit->setPlaceholderText(tr("Search Password"));
    ui->lineEdit->setToolTip(QString());
    ui->grepResultsList->clear();
    ui->grepResultsList->setVisible(false);
    ui->treeView->setVisible(true);
    proxyModel.setFilterRegularExpression(QRegularExpression());
    ui->treeView->setRootIndex(proxyModel.rootIndexFor(s.passStore));
  }
}

/**
 * @brief Display grep results in grepResultsList.
 */
void MainWindow::onGrepFinished(
    const QList<QPair<QString, QStringList>> &results) {
  const GrepSearchController::FinishOutcome outcome = m_grep.finishSearch();
  if (outcome.restoreCursor) {
    QApplication::restoreOverrideCursor();
  }
  // Re-enable the UI before the discard check so a cancelled search can never
  // leave controls disabled.
  setUiElementsEnabled(true);
  if (outcome.discard) {
    return;
  }
  if (!m_grep.inGrepMode())
    return;
  ui->grepResultsList->clear();
  if (results.isEmpty()) {
    ui->statusBar->showMessage(tr("No matches found."), 3000);
    ui->grepResultsList->setVisible(false);
    ui->treeView->setVisible(true);
    return;
  }
  const AppSettings s = QtPassSettings::load();
  const bool hideContent = s.hideContent;
  int totalLines = 0;
  for (const auto &pair : results) {
    auto *entryItem = new QTreeWidgetItem(ui->grepResultsList);
    entryItem->setText(0, pair.first);
    entryItem->setData(0, Qt::UserRole, pair.first);
    for (const QString &line : pair.second) {
      auto *lineItem = new QTreeWidgetItem(entryItem);
      lineItem->setText(0, hideContent ? "***" + tr("Content hidden") + "***"
                                       : line);
      lineItem->setData(0, Qt::UserRole, pair.first);
      ++totalLines;
    }
  }
  ui->grepResultsList->expandAll();
  ui->treeView->setVisible(false);
  ui->grepResultsList->setVisible(true);
  ui->statusBar->showMessage(
      tr("Found %n match(es)", nullptr, totalLines) + " " +
          tr("in %n entr(ies).", nullptr, static_cast<int>(results.size())),
      3000);
  if (s.useAutoclearPanel)
    clearPanelTimer.start();
}

/**
 * @brief Navigate to the password entry when a grep result is clicked.
 */
void MainWindow::on_grepResultsList_itemClicked(QTreeWidgetItem *item,
                                                int /*column*/) {
  const AppSettings s = QtPassSettings::load();
  const QString entry = item->data(0, Qt::UserRole).toString();
  if (entry.isEmpty())
    return;
  const QString fullPath =
      QDir::cleanPath(QDir(s.passStore).filePath(entry + ".gpg"));
  QModelIndex srcIndex = model.index(fullPath);
  if (!srcIndex.isValid())
    return;
  QModelIndex proxyIndex = proxyModel.mapFromSource(srcIndex);
  if (!proxyIndex.isValid())
    return;
  ui->treeView->setCurrentIndex(proxyIndex);
  on_treeView_clicked(proxyIndex);
  if (s.hideContent || s.useAutoclearPanel)
    ui->grepResultsList->clear();
  ui->grepResultsList->setVisible(false);
  ui->treeView->setVisible(true);
  ui->treeView->scrollTo(proxyIndex);
  ui->treeView->setFocus();
}

/**
 * @brief MainWindow::selectFirstFile select the first possible file in the
 * tree
 */
void MainWindow::selectFirstFile() {
  QModelIndex index = proxyModel.rootIndexFor(QtPassSettings::getPassStore());
  index = firstFile(index);
  ui->treeView->setCurrentIndex(index);
}

/**
 * @brief MainWindow::firstFile return location of first possible file
 * @param parentIndex
 * @return QModelIndex
 */
auto MainWindow::firstFile(QModelIndex parentIndex) -> QModelIndex {
  int numRows = proxyModel.rowCount(parentIndex);
  for (int row = 0; row < numRows; ++row) {
    QModelIndex index = proxyModel.index(row, 0, parentIndex);
    if (model.fileInfo(proxyModel.mapToSource(index)).isFile()) {
      return index;
    }
    if (proxyModel.hasChildren(index)) {
      QModelIndex childFile = firstFile(index);
      if (childFile.isValid())
        return childFile;
    }
  }
  return QModelIndex();
}

/**
 * @brief MainWindow::confirmPathInStore reject paths that resolve outside
 * the password store and warn the user.
 *
 * Used before file/folder creation, move, and rename to stop user-typed
 * names like "../../etc/passwd" or absolute paths from escaping the
 * configured store root via the input dialogs.
 *
 * @param candidate Absolute candidate path to validate.
 * @return true if the path is inside the password store; false otherwise (a
 * warning dialog is shown in that case).
 */
auto MainWindow::confirmPathInStore(const QString &candidate) -> bool {
  if (PathValidator::isPathInStore(QtPassSettings::getPassStore(), candidate)) {
    return true;
  }
  QMessageBox::warning(this, tr("Invalid name"),
                       tr("That name would resolve outside the password "
                          "store. Please choose a different name."));
  return false;
}

/**
 * @brief MainWindow::setPassword open passworddialog
 * @param file which pgp file
 * @param isNew insert (not update)
 */
void MainWindow::setPassword(const QString &file, bool isNew) {
  const AppSettings s = QtPassSettings::load();
  PasswordDialog d(QtPassSettings::getPass(), s, file, isNew, this);

  if (isNew) {
    const QString storePath = s.passStore;
    QString folder = Util::getDir(ui->treeView->currentIndex(), false, model,
                                  proxyModel, s.passStore);
    if (folder.isEmpty()) {
      folder = storePath;
    }
    QHash<QString, QStringList> templates =
        TemplateIO::readTemplates(storePath);
    if (!templates.isEmpty()) {
      QString defaultTemplate =
          TemplateIO::getFolderTemplate(folder, storePath);
      d.setAvailableTemplates(templates, defaultTemplate);
      new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_T), &d,
                    [&d]() { d.cycleTemplate(); });
    }
  }

  if (!d.exec()) {
    ui->treeView->setFocus();
  }
}

/**
 * @brief MainWindow::addPassword add a new password by showing a
 * number of dialogs.
 */
void MainWindow::addPassword() {
  const QString passStore = QtPassSettings::load().passStore;
  bool ok;
  QString dir = Util::getDir(ui->treeView->currentIndex(), true, model,
                             proxyModel, passStore);
  QString file = QInputDialog::getText(
      this, tr("New file"),
      tr("New password file: \n(Will be placed in %1 )")
          .arg(passStore + Util::getDir(ui->treeView->currentIndex(), true,
                                        model, proxyModel, passStore)),
      QLineEdit::Normal, "", &ok);
  if (!ok || file.isEmpty()) {
    return;
  }
  file = dir + file;
  if (!confirmPathInStore(passStore + file)) {
    return;
  }
  setPassword(file);
}

/**
 * @brief MainWindow::onDelete remove password, if you are
 * sure.
 */
void MainWindow::onDelete() {
  QModelIndex currentIndex = ui->treeView->currentIndex();
  if (!currentIndex.isValid()) {
    // This fixes https://github.com/IJHack/QtPass/issues/556
    // Otherwise the entire password directory would be deleted if
    // nothing is selected in the tree view.
    return;
  }

  QFileInfo fileOrFolder =
      model.fileInfo(proxyModel.mapToSource(ui->treeView->currentIndex()));
  QString file = "";
  bool isDir = false;

  if (fileOrFolder.isFile()) {
    file = getFile(ui->treeView->currentIndex(), true);
  } else {
    file = Util::getDir(ui->treeView->currentIndex(), true, model, proxyModel,
                        QtPassSettings::getPassStore());
    isDir = true;
  }

  QString dirMessage = tr(" and the whole content?");
  if (isDir) {
    QDirIterator it(model.rootPath() + QDir::separator() + file,
                    QDirIterator::Subdirectories);
    bool okDir = true;
    while (it.hasNext() && okDir) {
      it.next();
      if (QFileInfo(it.filePath()).isFile()) {
        if (QFileInfo(it.filePath()).suffix() != "gpg") {
          okDir = false;
          dirMessage = tr(" and the whole content? <br><strong>Attention: "
                          "there are unexpected files in the given folder, "
                          "check them before continue.</strong>");
        }
      }
    }
  }

  if (QMessageBox::question(
          this, isDir ? tr("Delete folder?") : tr("Delete password?"),
          tr("Are you sure you want to delete %1%2?")
              .arg(QDir::separator() + file, isDir ? dirMessage : "?"),
          QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
    return;
  }

  QtPassSettings::getPass()->Remove(file, isDir);
}

/**
 * @brief MainWindow::cancelOtpRequest abandon an in-flight OTP request.
 *
 * Connected to Pass::processErrorExit, and called from deselect(). A failed
 * decrypt never emits finishedShow, and the error handler re-enables the UI —
 * which stops the watchdog that was the only other thing clearing the flag.
 * Left set, it permanently suppressed passShowHandler's copy-on-select and let
 * the still-armed one-shot claim the next unrelated decrypt.
 *
 * The same failed decrypt would otherwise leave m_passwordCopyPending stuck
 * (its only other reset is passwordFromFileToClipboard, which never runs when
 * finishedShow is not emitted), permanently blocking Ctrl+C, so clear it here
 * too. Unlike otpFromFileToClipboard, that slot has no pending-flag guard, so
 * its single-shot connection must be torn down here as well — otherwise a
 * connection left armed by the failed decrypt would claim the next unrelated
 * finishedShow and copy the wrong entry to the clipboard. A never-fired
 * Qt::SingleShotConnection persists until it emits, so disconnect here.
 */
void MainWindow::cancelOtpRequest() {
  m_otpRequestPending = false;
  m_otpRequestFile.clear();
  disconnect(QtPassSettings::getPass(), &Pass::finishedShow, this,
             &MainWindow::passwordFromFileToClipboard);
  m_passwordCopyPending = false;
}

/**
 * @brief MainWindow::onOtp generate the selected entry's OTP code and copy it.
 *
 * Decrypts the entry once and derives the code in-process, so this works with
 * either backend and on every platform. The code itself is produced by
 * otpFromFileToClipboard once Pass::finishedShow arrives.
 */
void MainWindow::onOtp() {
  QString file = getFile(ui->treeView->currentIndex(), true);
  if (file.isEmpty()) {
    flashText(tr("No password selected for OTP generation"), true);
    return;
  }
  if (!QtPassSettings::isUseOtp()) {
    // Normally unreachable (the action is hidden), but never fail silently.
    flashText(tr("No OTP code found in this password entry"), true);
    return;
  }

  // Fast path: the panel already shows a live code for the selected entry, so
  // there is nothing to decrypt. This also keeps the clipboard write single —
  // going through Show() would let passShowHandler copy the password first, and
  // two QClipboard::setMimeData calls in one event-loop turn can leave the
  // Windows clipboard empty.
  //
  // Only when the panel is showing *this* entry: arrow-key navigation and
  // right-clicking move the tree's currentIndex without emitting
  // QTreeView::clicked, so the visible code can belong to a different account.
  const QString shown =
      (m_shownFile == file) ? m_displayPanel->currentOtpCode() : QString();
  if (!shown.isEmpty()) {
    m_qtPass->copyTextToClipboard(shown);
    showStatusMessage(tr("OTP code copied to clipboard"));
    return;
  }

  // Fallback: no OTP row on screen, so decrypt once. The flag stops
  // passShowHandler putting the password on the clipboard for a request that
  // only asked for a code.
  m_otpRequestPending = true;
  m_otpRequestFile = file;
  setUiElementsEnabled(false);
  connectSingleShot(QtPassSettings::getPass(), &Pass::finishedShow, this,
                    &MainWindow::otpFromFileToClipboard);
  // passShowHandler repaints the panel for this Show too, so keep the marker in
  // step or a second request would decrypt again instead of taking the fast
  // path. Safe to set now: executeWrapperStarted() clears the panel on every
  // command, so a failed decrypt leaves currentOtpCode() empty.
  m_shownFile = file;
  QtPassSettings::getPass()->Show(file);
}

/**
 * @brief MainWindow::onEdit try and edit (selected) password.
 */
void MainWindow::onEdit() {
  QString file = getFile(ui->treeView->currentIndex(), true);
  editPassword(file);
}

/**
 * @brief MainWindow::onUsers edit users for the current
 * folder,
 * gets lists and opens UserDialog.
 */
void MainWindow::onUsers() {
  const QString dir = Util::getDir(ui->treeView->currentIndex(), false, model,
                                   proxyModel, QtPassSettings::getPassStore());

  UsersDialog d(QtPassSettings::getPass(), QtPassSettings::load(), dir, this);
  if (!d.exec()) {
    ui->treeView->setFocus();
  }
}

/**
 * @brief MainWindow::messageAvailable we have some text/message/search to do.
 * @param message
 */
void MainWindow::messageAvailable(const QString &message) {
  show();
  raise();
  if (message.isEmpty()) {
    focusInput();
  } else {
    ui->treeView->expandAll();
    ui->lineEdit->setText(message);
    on_lineEdit_returnPressed();
  }
}

/**
 * @brief MainWindow::generateKeyPair internal gpg keypair generator . .
 * @param batch
 * @param keygenWindow
 */
void MainWindow::generateKeyPair(const QString &batch, QDialog *keygenWindow) {
  m_keyGenDialog = keygenWindow;
  emit generateGPGKeyPair(batch);
}

/**
 * @brief MainWindow::updateProfileBox update the list of profiles, optionally
 * select a more appropriate one to view too
 */
void MainWindow::updateProfileBox() {
  QHash<QString, QHash<QString, QString>> profiles =
      QtPassSettings::getProfiles();

  if (profiles.isEmpty()) {
    ui->profileWidget->hide();
  } else {
    ui->profileWidget->show();
    ui->profileBox->setEnabled(profiles.size() > 1);
    ui->profileBox->clear();
    QHashIterator<QString, QHash<QString, QString>> i(profiles);
    while (i.hasNext()) {
      i.next();
      if (!i.key().isEmpty()) {
        ui->profileBox->addItem(i.key());
      }
    }
    ui->profileBox->model()->sort(0);
  }
  int index = ui->profileBox->findText(QtPassSettings::getProfile());
  if (index != -1) { //  -1 for not found
    ui->profileBox->setCurrentIndex(index);
  }
}

/**
 * @brief Handles changes to the selected profile in the profile combo box.
 * @details Ignores the event during a fresh start or when the selected profile
 * matches the current profile. Otherwise, it clears the password field, updates
 * the active profile and related settings, refreshes the environment, and
 * resets the tree view and action states to reflect the newly selected profile.
 *
 * @param name - The newly selected profile name.
 * @return void - This function does not return a value.
 *
 */
void MainWindow::on_profileBox_currentTextChanged(const QString &name) {
  if (m_qtPass->isFreshStart() || name == QtPassSettings::getProfile()) {
    return;
  }

  ui->lineEdit->clear();

  const QHash<QString, QString> prof =
      QtPassSettings::getProfiles().value(name);
  AppSettings s = QtPassSettings::load();
  s.activeProfile = name;
  s.passStore = prof.value("path");
  s.passSigningKey = prof.value("signingKey");
  QtPassSettings::save(s);
  ui->statusBar->showMessage(tr("Profile changed to %1").arg(name), 2000);

  QtPassSettings::getPass()->updateEnv();

  const QString passStore = QtPassSettings::getPassStore();
  proxyModel.setStore(passStore);
  ui->treeView->setRootIndex(proxyModel.rootIndexFor(passStore));
  deselect();
  ui->treeView->setCurrentIndex(QModelIndex());
}

/**
 * @brief MainWindow::initTrayIcon show a nice tray icon on systems that
 * support
 * it
 */
void MainWindow::initTrayIcon() {
  m_tray = new TrayIcon(this);
  if (!m_tray->getIsAllocated()) {
    destroyTrayIcon();
  }
}

/**
 * @brief MainWindow::destroyTrayIcon remove that pesky tray icon
 */
void MainWindow::destroyTrayIcon() {
  delete m_tray;
  m_tray = nullptr;
}

/**
 * @brief MainWindow::closeEvent hide or quit
 * @param event
 */
void MainWindow::closeEvent(QCloseEvent *event) {
  // Save in both branches: a tray user only ever hides the window, and
  // used to quit through the tray menu with nothing ever written.
  saveWindowState();
  if (QtPassSettings::isHideOnClose()) {
    this->hide();
    event->ignore();
  } else {
    m_qtPass->clearClipboard();
    event->accept();
    // A visible QSystemTrayIcon keeps the application alive after the last
    // window closes, so quitOnLastWindowClosed never fires and the window
    // merely vanishes into the tray. Quit explicitly so closing the window
    // actually exits when "hide on close" is disabled.
    QApplication::quit();
  }
}

/**
 * @brief MainWindow::eventFilter filter out some events and focus the
 * treeview
 * @param obj
 * @param event
 * @return
 */
auto MainWindow::eventFilter(QObject *obj, QEvent *event) -> bool {
  if (obj == ui->toolBar && event->type() == QEvent::PaletteChange) {
    // KDE's Breeze style gives top toolbars a "header" palette of its own.
    // After a runtime light/dark switch it recomputes that palette from a
    // cached kdeglobals and stamps the previous theme's colours back on, so
    // the toolbar stays dark on a light window (or vice versa) with matching
    // invisible icons. A legitimate header tint is close to the window
    // colour; a stale theme is not. When the two disagree, drop the imposed
    // palette so the toolbar follows the application palette again. Deferred
    // so we never re-enter the style while it is still applying its palette.
    QTimer::singleShot(0, this, &MainWindow::dropStaleToolBarPalette);
  }
  if (obj == ui->lineEdit && event->type() == QEvent::KeyPress) {
    auto *key = dynamic_cast<QKeyEvent *>(event);
    if (key != nullptr && key->key() == Qt::Key_Down) {
      ui->treeView->setFocus();
    }
  }
  return QObject::eventFilter(obj, event);
}

/**
 * @brief Reset the toolbar palette when a style left it in the wrong theme.
 *
 * Compares the lightness of the toolbar's Window colour with the application
 * palette; a difference above kStaleToolBarLightness means the toolbar shows
 * the other theme. See the PaletteChange branch in eventFilter().
 */
void MainWindow::dropStaleToolBarPalette() {
  if (!ui->toolBar->testAttribute(Qt::WA_SetPalette)) {
    return;
  }
  constexpr int kStaleToolBarLightness = 64;
  const int barLightness =
      ui->toolBar->palette().color(QPalette::Window).lightness();
  const int appLightness =
      QApplication::palette().color(QPalette::Window).lightness();
  if (qAbs(barLightness - appLightness) > kStaleToolBarLightness) {
    ui->toolBar->setPalette(QPalette());
    // The style also paints the tools-area background on the main window
    // from the same stale palette, and the toolbar is transparent by default;
    // paint our own background so the reset is actually visible.
    ui->toolBar->setAutoFillBackground(true);
  }
}

/**
 * @brief MainWindow::keyPressEvent did anyone press return, enter or escape?
 * @param event
 */
void MainWindow::keyPressEvent(QKeyEvent *event) {
  switch (event->key()) {
  case Qt::Key_Delete:
    onDelete();
    break;
  case Qt::Key_Return:
  case Qt::Key_Enter:
    if (proxyModel.rowCount() > 0) {
      on_treeView_clicked(ui->treeView->currentIndex());
    }
    break;
  case Qt::Key_Escape:
    ui->lineEdit->clear();
    break;
  default:
    break;
  }
}

/**
 * @brief MainWindow::showContextMenu show us the (file or folder) context
 * menu
 * @param pos
 */
void MainWindow::showContextMenu(const QPoint &pos) {
  const AppSettings s = QtPassSettings::load();
  QModelIndex index = ui->treeView->indexAt(pos);
  bool selected = true;
  if (!index.isValid()) {
    ui->treeView->clearSelection();
    ui->actionDelete->setEnabled(false);
    ui->actionEdit->setEnabled(false);
    selected = false;
  }

  ui->treeView->setCurrentIndex(index);

  QPoint globalPos = ui->treeView->viewport()->mapToGlobal(pos);

  QFileInfo fileOrFolder =
      model.fileInfo(proxyModel.mapToSource(ui->treeView->currentIndex()));

  QMenu contextMenu;
  if (!selected || fileOrFolder.isDir()) {
    QAction *openFolder =
        contextMenu.addAction(tr("Open folder with file manager"));
    QAction *addFolder = contextMenu.addAction(tr("Add folder"));
    QAction *addPassword = contextMenu.addAction(tr("Add password"));
    QAction *users = contextMenu.addAction(tr("Users"));
    connect(openFolder, &QAction::triggered, this, &MainWindow::openFolder);
    connect(addFolder, &QAction::triggered, this, &MainWindow::addFolder);
    connect(addPassword, &QAction::triggered, this, &MainWindow::addPassword);
    connect(users, &QAction::triggered, this, &MainWindow::onUsers);
  } else if (fileOrFolder.isFile()) {
    QAction *edit = contextMenu.addAction(tr("Edit"));
    connect(edit, &QAction::triggered, this, &MainWindow::onEdit);
  }
  if (selected) {
    contextMenu.addSeparator();
    if (fileOrFolder.isDir()) {
      QAction *renameFolder = contextMenu.addAction(tr("Rename folder"));
      connect(renameFolder, &QAction::triggered, this,
              &MainWindow::renameFolder);
    } else if (fileOrFolder.isFile()) {
      QAction *renamePassword = contextMenu.addAction(tr("Rename password"));
      connect(renamePassword, &QAction::triggered, this,
              &MainWindow::renamePassword);
    }
    QAction *deleteItem = contextMenu.addAction(tr("Delete"));
    connect(deleteItem, &QAction::triggered, this, &MainWindow::onDelete);
    if (fileOrFolder.isDir()) {
      QString dirPath = QDir::cleanPath(Util::getDir(
          ui->treeView->currentIndex(), false, model, proxyModel, s.passStore));

      auto *shareMenu = new QMenu(tr("Share"), &contextMenu);
      contextMenu.addMenu(shareMenu);

      QString gpgIdPath = Pass::getGpgIdPath(dirPath, s.passStore);
      bool gpgIdExists = !gpgIdPath.isEmpty() && QFile(gpgIdPath).exists();

      const QString exePath = s.usePass ? s.passExecutable : s.gpgExecutable;
      bool gpgAvailable = !exePath.isEmpty() && (exePath.startsWith("wsl ") ||
                                                 QFile(exePath).exists());

      QAction *reencrypt = shareMenu->addAction(tr("Re-encrypt all passwords"));
      reencrypt->setEnabled(gpgIdExists && gpgAvailable);
      connect(reencrypt, &QAction::triggered, this,
              [this, dirPath]() { reencryptPath(dirPath); });

      QAction *exportKey = shareMenu->addAction(tr("Export my public key..."));
      exportKey->setEnabled(gpgAvailable);
      connect(exportKey, &QAction::triggered, this,
              &MainWindow::exportPublicKey);

      QAction *addRecipientAction =
          shareMenu->addAction(tr("Add recipient..."));
      addRecipientAction->setEnabled(gpgIdExists && gpgAvailable);
      connect(addRecipientAction, &QAction::triggered, this,
              [this, dirPath]() { addRecipient(dirPath); });

      QAction *shareHelp = shareMenu->addAction(tr("What is this?"));
      connect(shareHelp, &QAction::triggered, this, &MainWindow::showShareHelp);
    }
  }
  contextMenu.exec(globalPos);
}

/**
 * @brief MainWindow::showBrowserContextMenu show us the context menu in
 * password window
 * @param pos
 */
void MainWindow::showBrowserContextMenu(const QPoint &pos) {
  QMenu *contextMenu = ui->textBrowser->createStandardContextMenu(pos);
  // createStandardContextMenu() parents the menu to textBrowser. A stylesheet
  // on the browser (it used to carry "background: palette(base)") cascades to
  // the child QMenu and breaks its opaque native background. Reparent to the
  // main window so the menu paints solid regardless of browser styling.
  contextMenu->setParent(this, contextMenu->windowFlags());
  QPoint globalPos = ui->textBrowser->viewport()->mapToGlobal(pos);

  contextMenu->exec(globalPos);
  delete contextMenu;
}

/**
 * @brief MainWindow::openFolder open the folder in the default file manager
 */
void MainWindow::openFolder() {
  QString dir = Util::getDir(ui->treeView->currentIndex(), false, model,
                             proxyModel, QtPassSettings::getPassStore());

  QString path = QDir::toNativeSeparators(dir);
  QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

/**
 * @brief MainWindow::addFolder add a new folder to store passwords in
 */
void MainWindow::addFolder() {
  const AppSettings s = QtPassSettings::load();
  bool ok;
  QString dir = Util::getDir(ui->treeView->currentIndex(), false, model,
                             proxyModel, s.passStore);
  QString newdir = QInputDialog::getText(
      this, tr("New file"),
      tr("New Folder: \n(Will be placed in %1 )")
          .arg(s.passStore + Util::getDir(ui->treeView->currentIndex(), true,
                                          model, proxyModel, s.passStore)),
      QLineEdit::Normal, "", &ok);
  if (!ok || newdir.isEmpty()) {
    return;
  }
  newdir.prepend(dir);
  if (!confirmPathInStore(newdir)) {
    return;
  }
  if (!QDir().mkdir(newdir)) {
    QMessageBox::warning(this, tr("Error"),
                         tr("Failed to create folder: %1").arg(newdir));
    return;
  }
  // A .gpg-id only counts when it is signed once a signing key is configured:
  // ImitatePass::verifyGpgIdFile and pass (PASSWORD_STORE_SIGNING_KEY) both
  // refuse to encrypt into a folder whose .gpg-id has no matching
  // .gpg-id.sig. Signing here would need the signing secret key and a
  // passphrase prompt just to create a folder, so in that case leave the
  // folder without its own .gpg-id: it inherits the parent's signed list,
  // which encrypts to the same recipients. UsersDialog (Init) is the place to
  // give it a distinct, signed list.
  const bool signingConfigured = !s.passSigningKey.trimmed().isEmpty();
  if (s.addGPGId && !signingConfigured) {
    // Seed the new folder's .gpg-id from the recipients that are currently
    // in effect for its parent. The previous implementation walked
    // listKeys("", true) and wrote every key whose `enabled` flag was set,
    // but that flag is only toggled inside UsersDialog, so the loop wrote
    // nothing and left a zero-byte .gpg-id that shadowed the parent's
    // recipients (see #1682).
    if (!Pass::seedGpgIdFile(newdir, s.passStore)) {
      QMessageBox::warning(
          this, tr("Error"),
          tr("Failed to create .gpg-id file in: %1").arg(newdir));
      return;
    }
  }
}

/**
 * @brief MainWindow::renameFolder rename an existing folder
 */
void MainWindow::renameFolder() {
  bool ok;
  QString srcDir =
      QDir::cleanPath(Util::getDir(ui->treeView->currentIndex(), false, model,
                                   proxyModel, QtPassSettings::getPassStore()));
  QString srcDirName = QDir(srcDir).dirName();
  QString newName =
      QInputDialog::getText(this, tr("Rename file"), tr("Rename Folder To: "),
                            QLineEdit::Normal, srcDirName, &ok);
  if (!ok || newName.isEmpty()) {
    return;
  }
  QString destDir = srcDir;
  destDir.replace(srcDir.lastIndexOf(srcDirName), srcDirName.length(), newName);
  if (!confirmPathInStore(destDir)) {
    return;
  }
  QtPassSettings::getPass()->Move(srcDir, destDir, false);
}

/**
 * @brief MainWindow::editPassword read password and open edit window via
 * MainWindow::onEdit()
 */
void MainWindow::editPassword(const QString &file) {
  if (!file.isEmpty()) {
    const AppSettings s = QtPassSettings::load();
    if (s.useGit && s.autoPull) {
      onUpdate(true);
    }
    setPassword(file, false);
  }
}

/**
 * @brief MainWindow::renamePassword rename an existing password
 */
void MainWindow::renamePassword() {
  bool ok;
  QString file = getFile(ui->treeView->currentIndex(), false);
  QString filePath = QFileInfo(file).path();
  QString fileName = QFileInfo(file).fileName();
  if (fileName.endsWith(".gpg", Qt::CaseInsensitive)) {
    fileName.chop(4);
  }

  QString newName =
      QInputDialog::getText(this, tr("Rename file"), tr("Rename File To: "),
                            QLineEdit::Normal, fileName, &ok);
  if (!ok || newName.isEmpty()) {
    return;
  }
  QString newFile = QDir(filePath).filePath(newName);
  if (!confirmPathInStore(newFile)) {
    return;
  }
  QtPassSettings::getPass()->Move(file, newFile, false);
}

/**
 * @brief Copies the password of the selected file from the tree view to the
 * clipboard.
 * @example
 * MainWindow::copyPasswordFromTreeview();
 *
 * @return void - This function does not return a value.
 */
void MainWindow::copyPasswordFromTreeview() {
  QFileInfo fileOrFolder =
      model.fileInfo(proxyModel.mapToSource(ui->treeView->currentIndex()));

  if (fileOrFolder.isFile()) {
    // finishedShow carries no request identity, so allow only one copy request
    // in flight: otherwise a second Ctrl+C on a different entry would re-arm
    // the single-shot slot and the first decrypt to finish would copy the wrong
    // entry while the later request is silently dropped. See
    // m_passwordCopyPending.
    if (m_passwordCopyPending) {
      return;
    }
    m_passwordCopyPending = true;
    QString file = getFile(ui->treeView->currentIndex(), true);
    connectSingleShot(QtPassSettings::getPass(), &Pass::finishedShow, this,
                      &MainWindow::passwordFromFileToClipboard);
    // This Show repaints the panel as well; see onOtp().
    m_shownFile = file;
    QtPassSettings::getPass()->Show(file);
  }
}

void MainWindow::passwordFromFileToClipboard(const QString &text) {
  m_passwordCopyPending = false;
  const QStringList tokens = text.split('\n');
  if (tokens.isEmpty()) {
    return;
  }
  // An entry created by `pass otp insert` has the otpauth:// URI as its first
  // line. That URI carries the shared TOTP secret (the seed, not a code), so
  // copying it to the clipboard would leak 2FA material. Skip it, matching the
  // display path (FileContent::getPasswordForDisplay / PasswordDisplayPanel).
  if (FileContent::isOtpUriValue(tokens[0])) {
    flashText(tr("This entry holds an OTP secret, not a password"), true);
    return;
  }
  m_qtPass->copyTextToClipboard(tokens[0]);
}

/**
 * @brief Displays message in status bar
 *
 * @param msg     text to be displayed
 * @param timeout time for which msg shall be visible
 */
void MainWindow::showStatusMessage(const QString &msg, int timeout) {
  ui->statusBar->showMessage(msg, timeout);
}

/**
 * @brief MainWindow::reencryptPath re-encrypt all passwords in a directory
 * @param dir Directory path to re-encrypt
 */
void MainWindow::reencryptPath(const QString &dir) {
  QDir checkDir(dir);
  if (!checkDir.exists()) {
    QMessageBox::critical(this, tr("Error"),
                          tr("Directory does not exist: %1").arg(dir));
    return;
  }

  int ret = QMessageBox::question(
      this, tr("Re-encrypt passwords"),
      tr("Re-encrypt all passwords in %1?\n\n"
         "This will re-encrypt ALL password files in this folder "
         "using the current recipients defined in .gpg-id.\n\n"
         "This may rewrite many files and cannot be undone easily.\n\n"
         "Continue?")
          .arg(QDir(dir).dirName()),
      QMessageBox::Yes | QMessageBox::No);

  if (ret != QMessageBox::Yes)
    return;

  // Disable preemptively. ImitatePass::reencryptPath emits
  // startReencryptPath asynchronously and the slot would re-run this,
  // but setEnabled(false) is idempotent so the duplicate is harmless.
  startReencryptPath();

  QtPassSettings::getImitatePass()->reencryptPath(
      QDir::cleanPath(QDir(dir).absolutePath()));
}

/**
 * @brief MainWindow::startReencryptPath disable ui elements and treeview and
 * show the progress dialog. Idempotent: MainWindow::reencryptPath calls it
 * before ImitatePass emits startReencryptPath.
 */
void MainWindow::startReencryptPath() {
  m_reencryptRunning = true;
  setUiElementsEnabled(false);
  // The worker always ends with endReencryptPath(), and Cancel is available
  // throughout, so the generic watchdog is not needed for this operation.
  m_uiWatchdog.stop();
  ui->treeView->setDisabled(true);
  if (m_reencryptProgress)
    return;
  auto *progress = new QProgressDialog(tr("Re-encrypting passwords..."),
                                       tr("Cancel"), 0, 0, this);
  progress->setWindowTitle(tr("Re-encrypt passwords"));
  progress->setWindowModality(Qt::WindowModal);
  progress->setAutoClose(false);
  progress->setAutoReset(false);
  progress->setMinimumDuration(0);
  connect(progress, &QProgressDialog::canceled, this, [this]() {
    QtPassSettings::getImitatePass()->cancelReencryptPath();
    showStatusMessage(tr("Cancelling re-encryption"), 5000);
  });
  m_reencryptProgress = progress;
  progress->show();
}

/**
 * @brief MainWindow::reencryptProgress show how many files were checked
 * @param current files checked so far
 * @param total files found under the folder
 */
void MainWindow::reencryptProgress(int current, int total) {
  if (!m_reencryptProgress)
    return;
  m_reencryptProgress->setMaximum(total);
  m_reencryptProgress->setLabelText(
      tr("Re-encrypting passwords: %1 of %2").arg(current).arg(total));
  // On a modal QProgressDialog setValue() calls processEvents(), which can
  // deliver the queued completion: endReencryptPath() then hides the dialog
  // and resets m_reencryptProgress under our feet. Keep it last and touch
  // nothing afterwards.
  m_reencryptProgress->setValue(current);
}

/**
 * @brief MainWindow::endReencryptPath re-enable ui elements
 */
void MainWindow::endReencryptPath() {
  m_reencryptRunning = false;
  if (m_reencryptProgress) {
    m_reencryptProgress->hide();
    m_reencryptProgress->deleteLater();
    m_reencryptProgress = nullptr;
  }
  setUiElementsEnabled(true);
}

/**
 * @brief MainWindow::exportPublicKey export the configured signing key in
 *        ASCII-armored form via gpg and show it in ExportPublicKeyDialog.
 *
 * Falls back to a help dialog when no signing key is configured or gpg is
 * unavailable, so the user still gets actionable guidance.
 */
void MainWindow::exportPublicKey() {
  const AppSettings s = QtPassSettings::load();
  const QString identity = s.passSigningKey;
  if (identity.isEmpty()) {
    QMessageBox::information(
        this, tr("Export Public Key"),
        tr("<h3>Export Your Public Key</h3>"
           "<p>No signing key is configured. Set one in QtPass Settings "
           "&gt; GPG keys, or run this in a terminal:</p>"
           "<pre>gpg --armor --export --output my_key.asc &lt;your-key-id"
           "&gt;</pre>"
           "<p>Then send the file to your teammates.</p>"));
    return;
  }
  QString gpgExe = s.gpgExecutable;
  if (gpgExe.isEmpty()) {
    gpgExe = QStringLiteral("gpg");
  }
  QStringList args = {"--armor", "--export"};
  args.append(identity.split(' ', Qt::SkipEmptyParts));
  QString stdOut;
  QString stdErr;
  int exitCode = Executor::executeBlocking(gpgExe, args, &stdOut, &stdErr);
  if (exitCode != 0 || stdOut.isEmpty()) {
    QMessageBox::warning(this, tr("Export Public Key"),
                         tr("Could not export public key for %1.\n\n%2")
                             .arg(identity, stdErr.isEmpty()
                                                ? tr("No output from gpg.")
                                                : stdErr));
    return;
  }
  ExportPublicKeyDialog dialog(identity, stdOut, this);
  dialog.exec();
}

/**
 * @brief MainWindow::addRecipient open the recipient management dialog for
 *        the supplied directory.
 * @param dir Folder whose .gpg-id should be edited.
 *
 * Delegates to UsersDialog so users can tick/untick keys from their
 * keyring as recipients of the folder; importing a foreign key into the
 * keyring still has to happen via gpg (or QtPass settings) first.
 */
void MainWindow::addRecipient(const QString &dir) {
  UsersDialog d(QtPassSettings::getPass(), QtPassSettings::load(), dir, this);
  d.exec();
}

/**
 * @brief MainWindow::showShareHelp show help about GPG sharing
 */
void MainWindow::showShareHelp() {
  QMessageBox::information(
      this, tr("Sharing Passwords with GPG"),
      tr("<h3>Sharing Passwords with GPG</h3>"
         "<p>To share passwords with other users:</p>"
         "<ol>"
         "<li><b>Export your public key</b> and send it to teammates</li>"
         "<li><b>Import teammates' public keys</b> into your GPG keyring</li>"
         "<li><b>Re-encrypt passwords</b> so all recipients can decrypt "
         "them</li>"
         "</ol>"
         "<p>Only people who have a matching secret key can decrypt the "
         "passwords.</p>"
         "<p><b>Tip:</b> Use the same GPG key for all shared folders.</p>"
         "<p>See the FAQ for more details.</p>"));
}

void MainWindow::updateGitButtonVisibility() {
  const AppSettings s = QtPassSettings::load();
  if (!s.useGit || (s.gitExecutable.isEmpty() && s.passExecutable.isEmpty())) {
    enableGitButtons(false);
  } else {
    enableGitButtons(true);
  }
}

void MainWindow::updateOtpButtonVisibility(bool uiEnabled) {
  // No platform gating any more: codes are generated in-process, so this works
  // on Windows and macOS and with either backend. It used to be hidden there
  // because the pass-otp extension is Unix-only.
  //
  // Kept visible but disabled when OTP is off, as it was before: hiding it
  // removed the only discoverable trace of the feature, and since an OTP field
  // is suppressed from the panel unconditionally (its value is a secret), a
  // user with the setting off would see nothing at all where their OTP data
  // used to be.
  //
  // Enablement must honour uiEnabled: this is called from
  // setUiElementsEnabled(), and ignoring the argument re-enabled the action
  // while a decrypt was in flight, letting the user stack Show calls.
  ui->actionOtp->setVisible(true);
  ui->actionOtp->setEnabled(QtPassSettings::isUseOtp() && uiEnabled);
}

void MainWindow::updateGrepButtonVisibility() {
  const bool enabled = QtPassSettings::isUseGrepSearch();
  ui->grepButton->setVisible(enabled);
  ui->grepCaseButton->setVisible(enabled);
  if (!enabled && m_grep.inGrepMode()) {
    ui->grepButton->setChecked(false);
  }
}

void MainWindow::enableGitButtons(const bool &state) {
  // Following GNOME guidelines is preferable disable buttons instead of hide
  ui->actionPush->setEnabled(state);
  ui->actionUpdate->setEnabled(state);
}

/**
 * @brief MainWindow::critical critical message popup wrapper.
 * @param title
 * @param msg
 */
void MainWindow::critical(const QString &title, const QString &msg) {
  QMessageBox::critical(this, title, msg);
}

/**
 * @brief Appends processed command output to the output panel.
 *
 * Appends text to the process output text edit, with per-line numbering,
 * optional command prefix, and color coding for errors vs. success.
 * Handles auto-scrolling and line limits.
 *
 * @param output The raw output text from the command.
 * @param isError true if this is error output (stderr).
 * @param linePrefix Optional command name to prefix each line with.
 */
void MainWindow::appendProcessOutput(const QString &output, bool isError,
                                     const QString &linePrefix) {
  if (!QtPassSettings::isShowProcessOutput()) {
    return;
  }

  QStringList lines = output.split('\n', Qt::SkipEmptyParts);
  for (QString &line : lines) {
    // Right-trim only: remove trailing CR and whitespace, preserve leading
    // indentation
    line.remove('\r');
    while (!line.isEmpty() && line.back().isSpace()) {
      line.chop(1);
    }
    if (line.isEmpty()) {
      continue;
    }

    m_outputCounter++;
    QString lineNumber = QString::number(m_outputCounter);

    QColor textColor =
        isError ? QColor(Qt::red)
                : m_processOutputEdit->palette().color(QPalette::Text);
    QString colorHex = textColor.name();
    // Apply the optional prefix per line so multi-line output stays
    // attributed to its command (e.g. all 3 lines of a `git push` show
    // "git push: ..." rather than only the first).
    QString prefixed =
        linePrefix.isEmpty() ? line : linePrefix + QStringLiteral(": ") + line;
    QString coloredOutput =
        QString("<span style=\"color: %1;\">%2: %3</span>")
            .arg(colorHex, lineNumber, prefixed.toHtmlEscaped());

    m_processOutputEdit->append(coloredOutput);
  }

  limitOutputLines();

  if (m_autoScroll) {
    m_processOutputEdit->verticalScrollBar()->setValue(
        m_processOutputEdit->verticalScrollBar()->maximum());
  }
}

/**
 * @brief Handles process output from the Pass executor.
 *
 * Called when any non-sensitive process completes. Filters out password-
 * related commands (pass show, insert, etc.) and delegates to
 * appendProcessOutput.
 *
 * @param output The stdout/stderr text from the process.
 * @param isError true if this is error output (stderr).
 * @param pid The process ID identifying which command ran.
 */
void MainWindow::onProcessOutput(const QString &output, bool isError,
                                 Enums::PROCESS pid) {
  appendProcessOutput(output, isError, getProcessName(pid));
}

/**
 * @brief Maps a process ID to its human-readable command name.
 *
 * Returns static strings for git/pass commands that appear in output.
 * Password-related commands return empty (they are filtered).
 *
 * @param pid The process ID to look up.
 * @return QString with command name, or empty if filtered.
 */
auto MainWindow::getProcessName(Enums::PROCESS pid) -> QString {
  switch (pid) {
  case Enums::GIT_INIT:
    return QStringLiteral("git init"); // no-tr
  case Enums::GIT_ADD:
    return QStringLiteral("git add"); // no-tr
  case Enums::GIT_COMMIT:
    return QStringLiteral("git commit"); // no-tr
  case Enums::GIT_RM:
    return QStringLiteral("git rm"); // no-tr
  case Enums::GIT_PULL:
    return QStringLiteral("git pull"); // no-tr
  case Enums::GIT_PUSH:
    return QStringLiteral("git push"); // no-tr
  case Enums::GIT_MOVE:
    return QStringLiteral("git mv"); // no-tr
  case Enums::GIT_COPY:
    // ImitatePass::Copy literally invokes `git cp` (a git-extras
    // subcommand), so the label matches what's run. Stock-git users
    // without git-extras will see the underlying "'cp' is not a git
    // command" failure surfaced in the process output panel.
    return QStringLiteral("git cp"); // no-tr
  case Enums::PASS_INSERT:
    return QStringLiteral("pass insert"); // no-tr
  case Enums::PASS_REMOVE:
    return QStringLiteral("pass rm"); // no-tr
  case Enums::PASS_INIT:
    return QStringLiteral("pass init"); // no-tr
  case Enums::PASS_MOVE:
    return QStringLiteral("pass mv"); // no-tr
  case Enums::PASS_COPY:
    return QStringLiteral("pass cp"); // no-tr
  case Enums::PASS_GREP:
    return QStringLiteral("pass grep"); // no-tr
  case Enums::GPG_GENKEYS:
    return QStringLiteral("gpg --gen-key"); // no-tr
  case Enums::PASS_SHOW:
  case Enums::PROCESS_COUNT:
  case Enums::INVALID:
    break;
  }
  return {};
}

/**
 * @brief Checks if a process ID represents a sensitive operation whose
 * output should not be shown in the process output panel.
 *
 * Password-related commands (pass show, grep, insert)
 * display their output in other UI areas, so we skip them here.
 *
 * @param pid The process ID to check.
 * @return true if the process is sensitive and should be filtered.
 */
auto MainWindow::isSensitiveProcess(Enums::PROCESS pid) -> bool {
  switch (pid) {
  case Enums::PASS_SHOW:
  case Enums::PASS_GREP:
  case Enums::PASS_INSERT:
    return true;
  case Enums::GIT_INIT:
  case Enums::GIT_ADD:
  case Enums::GIT_COMMIT:
  case Enums::GIT_RM:
  case Enums::GIT_PULL:
  case Enums::GIT_PUSH:
  case Enums::GIT_MOVE:
  case Enums::GIT_COPY:
  case Enums::PASS_REMOVE:
  case Enums::PASS_INIT:
  case Enums::PASS_MOVE:
  case Enums::PASS_COPY:
  case Enums::GPG_GENKEYS:
  case Enums::PROCESS_COUNT:
  case Enums::INVALID:
    break;
  }
  return false;
}

/**
 * @brief Updates the visibility of the process output panel.
 *
 * Shows or hides the process output widget based on the user's
 * showProcessOutput setting.
 */
void MainWindow::updateProcessOutputVisibility() {
  m_processOutputDock->setVisible(QtPassSettings::isShowProcessOutput());
}

/**
 * @brief Limits the output panel to max lines, trimming old excess.
 *
 * Removes the oldest lines when the document exceeds MaxOutputLines (1000).
 * Called after each append to prevent unbounded growth.
 */
void MainWindow::limitOutputLines() {
  QTextDocument *doc = m_processOutputEdit->document();
  int excess = doc->blockCount() - MaxOutputLines;
  if (excess <= 0) {
    return;
  }

  QTextCursor cursor(doc);
  cursor.movePosition(QTextCursor::Start);
  cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, excess);
  cursor.removeSelectedText();
}

/**
 * @brief Clears the process output panel.
 *
 * Clears all output, resets the line counter, and re-enables auto-scroll.
 */
void MainWindow::on_clearOutputButton_clicked() {
  m_processOutputEdit->clear();
  m_outputCounter = 0;
  m_autoScroll = true;
}
