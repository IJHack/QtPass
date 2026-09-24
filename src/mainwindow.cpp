// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"

#include "qtpasslogging.h"

#include "configdialog.h"
#include "enums.h"
#include "executor.h"
#include "exportpublickeydialog.h"
#include "filecontent.h"
#include "firstrunwizard.h"
#include "passworddialog.h"
#include "passworddisplaypanel.h"
#include "pathvalidator.h"
#include "processoutputpanel.h"
#include "qpushbuttonasqrcode.h"
#include "qpushbuttonshowpassword.h"
#include "qpushbuttonwithclipboard.h"
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
#include <QDate>
#include <QDesktopServices>
#include <QDialog>
#include <QDockWidget>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QShortcut>
#include <QTextCharFormat>
#include <QTextStream>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>
#include <algorithm>
#include <array>
#include <utility>

MainWindow::MainWindow(const QString &searchText, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
#ifdef __APPLE__
  // extra treatment for mac os
  // see https://doc.qt.io/qt-6/qkeysequence.html#qt_set_sequence_auto_mnemonic
  qt_set_sequence_auto_mnemonic(true);
#endif
  ui->setupUi(this);

  m_qtPass = new QtPass(this);
  restoreWindow();
  connect(m_qtPass, &QtPass::outputReady, this,
          [this](const QString &html) { flashText(html, false, true); });
  connect(m_qtPass, &QtPass::operationFinished, this,
          [this]() { setUiElementsEnabled(true); });
  connect(m_qtPass, &QtPass::statusMessage, this,
          &MainWindow::showStatusMessage);
  connect(m_qtPass, &QtPass::pushRequested, this, &MainWindow::onPush);
  connect(m_qtPass, &QtPass::entryInserted, this,
          [this]() { on_treeView_clicked(getCurrentTreeViewIndex()); });
  connect(&m_qtPass->clipboard(), &ClipboardManager::statusMessage, this,
          [this](const QString &message) { showStatusMessage(message); });
  // Both backends: the active one can change through the configuration
  // dialog without a restart.
  connectPassSignals(QtPassSettings::getRealPass());
  connectPassSignals(QtPassSettings::getImitatePass());
  ImitatePass *ipass = QtPassSettings::getImitatePass();
  connect(ipass, &ImitatePass::startReencryptPath, this,
          &MainWindow::startReencryptPath);
  connect(ipass, &ImitatePass::reencryptProgress, this,
          &MainWindow::reencryptProgress);
  connect(ipass, &ImitatePass::endReencryptPath, this,
          &MainWindow::endReencryptPath);

  new QShortcut(QKeySequence(QKeySequence::StandardKey::Copy), this, this,
                &MainWindow::copyPasswordFromTreeview);

  m_tree = new StoreTree(ui->treeView, this);
  m_tree->setStore(QtPassSettings::getPassStore(Util::findPasswordStore()));
  m_tree->setPass(QtPassSettings::getPass());
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
  connect(m_displayPanel, &PasswordDisplayPanel::copyRequested,
          &m_qtPass->clipboard(), &ClipboardManager::copyText);
  connect(m_displayPanel, &PasswordDisplayPanel::qrRequested, m_qtPass,
          &QtPass::showTextAsQRCode);
  // Double-click on what is shown edits it, as in the tree.
  connect(m_displayPanel, &PasswordDisplayPanel::editRequested, this, [this] {
    if (ui->actionEdit->isEnabled()) {
      onEdit();
    }
  });

  QtPassSettings::getPass()->updateEnv();
  clearPanelTimer.setSingleShot(true);
  connect(&clearPanelTimer, &QTimer::timeout, this, [this]() { clearPanel(); });

  searchTimer.setInterval(350);
  searchTimer.setSingleShot(true);

  connect(&searchTimer, &QTimer::timeout, this, &MainWindow::onTimeoutSearch);

  // Install the search-box key filter once, not on every setUiElementsEnabled
  // call.
  ui->lineEdit->installEventFilter(this);
  // Both live in Breeze's "tools area" and get its header palette; see the
  // PaletteChange branch in eventFilter().
  ui->toolBar->installEventFilter(this);
  menuBar()->installEventFilter(this);

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
  m_processOutput = new ProcessOutputPanel(this);
  addDockWidget(Qt::BottomDockWidgetArea, m_processOutput);
  // After addDockWidget so our preference wins over any cached state
  // QMainWindow applies; restoreWindow() already processed the saved layout.
  m_processOutput->setVisible(QtPassSettings::isShowProcessOutput());
  // From the setting, not isVisible(): the window is not shown yet, so no
  // child of it reports visible.
  ui->actionShowProcessOutput->setChecked(
      QtPassSettings::isShowProcessOutput());
  connect(ui->actionShowProcessOutput, &QAction::toggled, this,
          [this](bool show) {
            m_processOutput->setVisible(show);
            AppSettings s = QtPassSettings::load();
            if (s.showProcessOutput != show) {
              s.showProcessOutput = show;
              QtPassSettings::save(s);
            }
          });

  connect(QtPassSettings::getPass(), &Pass::finishedAnyWithPid, this,
          [this](const QString &out, const QString &err, Enums::PROCESS pid) {
            // Never route potentially-secret output through the panel:
            // PASS_SHOW goes to the text browser (cleared on a timer when
            // "Autoclear panel" is set),
            // PASS_GREP returns password-file lines (#252), and PASS_INSERT
            // is excluded defensively in case a future path uses --echo.
            if (ProcessOutputPanel::isSensitiveProcess(pid)) {
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

  // No QApplication::quit() here: before exec() it is a documented no-op.
  // main() checks initSucceeded() and exits before show() instead.
  m_qtPass->init();
  // OK is not gated on Util::configIsValid(): an accepted configuration can
  // still lack a .gpg-id, so ask again; only a cancel ends the loop.
  while (!Util::configIsValid(QtPassSettings::load())) {
    if (!config()) {
      return;
    }
  }
  m_freshStart = false;
  m_initSucceeded = true;

  // Initial focus is set in showEvent(): a 10 ms QTimer here could fire
  // before the window was realised (queued ActivationChange, nested exec() in
  // init()) and selectAll() segfaulted inside Qt (#1187, #1188).
}

MainWindow::~MainWindow() {
  // Quit from the tray menu or a SIGTERM never delivers closeEvent(); the
  // window still knows its last geometry here.
  saveWindowState();
  delete m_qtPass;
}

// The signals of one backend that the window answers itself.
void MainWindow::connectPassSignals(Pass *pass) {
  // A failed decrypt never emits finishedShow; drop the pending OTP/copy
  // request so a later decrypt of the same entry is not taken as its answer.
  connect(pass, &Pass::processErrorExit, this, &MainWindow::onOperationError);
  connect(pass, &Pass::critical, this, &MainWindow::critical);
  connect(pass, &Pass::startingExecuteWrapper, this,
          &MainWindow::executeWrapperStarted);
  connect(pass, &Pass::statusMsg, this, &MainWindow::showStatusMessage);
  connect(pass, &Pass::finishedShow, this, &MainWindow::passShowHandler);
  // Both check the file against their own pending request and ignore the
  // rest, so they can stay connected for good.
  connect(pass, &Pass::finishedShow, this, &MainWindow::otpFromFileToClipboard);
  connect(pass, &Pass::finishedShow, this,
          &MainWindow::passwordFromFileToClipboard);
  connect(pass, &Pass::finishedGrep, this, &MainWindow::onGrepFinished);
}

// Idempotent; called from closeEvent() and the destructor.
void MainWindow::saveWindowState() {
  QtPassSettings::setGeometry(saveGeometry());
  QtPassSettings::setSavestate(saveState());
}

void MainWindow::focusInput() {
  // findChild() rather than ui->lineEdit: on a fresh-config first launch
  // config() calls setWindowFlags(), which rebuilds the native window, and
  // under Qt 6.11 the QLineEdit was destroyed while ui->lineEdit kept its old
  // address (SIGSEGV in QWidget::testAttribute). findChild() returns null.
  if (!isVisible()) {
    return;
  }
  auto *lineEdit = findChild<QLineEdit *>(QStringLiteral("lineEdit"));
  if (lineEdit == nullptr || !lineEdit->isVisible()) {
    return;
  }
  lineEdit->selectAll();
  lineEdit->setFocus();
  // Only once focus has landed: set in showEvent(), an early return above
  // would consume the one-shot and never retry.
  m_firstShowCompleted = true;
}

auto MainWindow::event(QEvent *event) -> bool {
  if (event->type() == QEvent::ApplicationPaletteChange) {
    m_themeSwitched = true;
  }
  return QMainWindow::event(event);
}

void MainWindow::changeEvent(QEvent *event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::PaletteChange && m_displayPanel != nullptr) {
    // Desktop switched light/dark. Top-level widgets get PaletteChange
    // (ApplicationPaletteChange goes to QApplication); stylesheets with
    // baked-in palette colours must be re-derived by hand.
    m_displayPanel->refreshPalette();
  }
  if (event->type() == QEvent::ActivationChange && isActiveWindow() &&
      isVisible()) {
    // Defer a tick so the activation dispatch chain unwinds first: calling
    // focusInput() inline has segfaulted on half-rebuilt ui state.
    QMetaObject::invokeMethod(this, &MainWindow::focusInput,
                              Qt::QueuedConnection);
  }
}

void MainWindow::showEvent(QShowEvent *event) {
  QMainWindow::showEvent(event);
  if (m_firstShowCompleted) {
    return;
  }
  // Queued so the platform map and pending rebuilds (setWindowFlags on the
  // wizard path) settle first. focusInput() sets the latch only on success,
  // so a failed lookup retries on the next show.
  QMetaObject::invokeMethod(this, &MainWindow::focusInput,
                            Qt::QueuedConnection);
}

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

  // Menu-only. Quit ends the application, tray icon or not, as in KeePassXC,
  // Telegram and the KDE, GNOME and macOS guidelines (#1788). Close is
  // close(), a hide when "hide on close" is set, like the X button and Alt+F4.
  connect(ui->actionQuit, &QAction::triggered, qApp, &QApplication::quit);
  connect(ui->actionClose, &QAction::triggered, this, &MainWindow::close);
#ifdef Q_OS_MACOS
  // The menu bar is the system's up there; nothing to hide.
  ui->actionShowMenuBar->setVisible(false);
#else
  // The action lives in the menu it hides, so it has to be on the window as
  // well for Ctrl+M to keep working while the bar is gone.
  addAction(ui->actionShowMenuBar);
  ui->actionShowMenuBar->setChecked(QtPassSettings::load().showMenuBar);
  menuBar()->setVisible(ui->actionShowMenuBar->isChecked());
  connect(ui->actionShowMenuBar, &QAction::toggled, this, [this](bool show) {
    menuBar()->setVisible(show);
    AppSettings s = QtPassSettings::load();
    s.showMenuBar = show;
    QtPassSettings::save(s);
  });
#endif
  connect(ui->actionFaq, &QAction::triggered, this, [] {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://qtpass.org/faq")));
  });
  connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::showAbout);
  connect(ui->actionAboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);

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

void MainWindow::initStatusBar() {
  ui->statusBar->showMessage(tr("Welcome to QtPass %1").arg(VERSION), 2000);

  QPixmap logo = QPixmap::fromImage(QImage(":/artwork/icon.svg"))
                     .scaledToHeight(statusBar()->height());
  auto *logoApp = new QLabel(statusBar());
  logoApp->setPixmap(logo);
  statusBar()->addPermanentWidget(logoApp);
}

auto MainWindow::getCurrentTreeViewIndex() -> QModelIndex {
  return ui->treeView->currentIndex();
}

void MainWindow::flashText(const QString &text, const bool isError,
                           const bool isHtml) {
  if (isError) {
    ui->textBrowser->setTextColor(Qt::red);
  } else {
    // setText() re-applies the current char format, red after an error, to
    // the whole document. Clear the foreground rather than pin a palette
    // colour, so text follows runtime light/dark switches (#946).
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
    // setWindowFlags() rebuilds the native window, the teardown that left
    // ui->lineEdit dangling (see focusInput()); skip it when nothing changes.
    return;
  }
  // Toggle just this hint; resetting to Qt::Window discarded every other flag.
  const bool wasVisible = isVisible();
  setWindowFlags(wantOnTop ? (flags | Qt::WindowStaysOnTopHint)
                           : (flags & ~Qt::WindowStaysOnTopHint));
  // setWindowFlags() hides a shown window; a window that was not shown yet
  // (first-run wizard from the constructor) is left to its normal show.
  if (wasVisible) {
    show();
  }
}

auto MainWindow::config() -> bool {
  if (m_freshStart) {
    // No usable configuration yet: the wizard finds the programs, the key
    // and the store, and creates the store when it has to.
    FirstRunWizard wizard(this);
    if (wizard.exec() != QDialog::Accepted) {
      return false;
    }
  } else {
    ConfigDialog d(this);
    d.setModal(true);
    if (d.exec() != QDialog::Accepted) {
      return false;
    }
  }

  applyTextBrowserSettings();
  applyWindowFlagsSettings();

  updateProfileBox();
  const AppSettings s = QtPassSettings::load();
  m_tree->setStore(s.passStore);
  deselect();
  ui->treeView->setCurrentIndex(QModelIndex());

  Pass *activePass = QtPassSettings::getPass();
  activePass->updateEnv();
  m_tree->setPass(activePass);
  clearPanelTimer.setInterval(MS_PER_SECOND * s.autoclearPanelSeconds);
  m_qtPass->clipboard().setAutoclearTimer();

  updateGitButtonVisibility();
  updateOtpButtonVisibility();
  updateGrepButtonVisibility();
  updateProcessOutputVisibility();
#ifndef Q_OS_MACOS
  // The action's toggled handler shows or hides the bar (and saves again).
  ui->actionShowMenuBar->setChecked(s.showMenuBar);
#endif
  if (s.useTrayIcon && m_tray == nullptr) {
    initTrayIcon();
  } else if (!s.useTrayIcon && m_tray != nullptr) {
    destroyTrayIcon();
  }

  // Stay fresh-start while the configuration is unusable, so the
  // constructor's loop re-runs the wizard. The re-prompt lives there:
  // recursing here never reported a cancel back to the caller.
  if (Util::configIsValid(s)) {
    m_freshStart = false;
  }
  return true;
}

void MainWindow::onUpdate(bool block) {
  ui->statusBar->showMessage(tr("Updating password-store"), 2000);
  if (block) {
    QtPassSettings::getPass()->GitPull_b();
  } else {
    QtPassSettings::getPass()->GitPull();
  }
}

void MainWindow::onPush() {
  if (QtPassSettings::isUseGit()) {
    ui->statusBar->showMessage(tr("Updating password-store"), 2000);
    QtPassSettings::getPass()->GitPush();
  }
}

auto MainWindow::getFile(const QModelIndex &index, bool forPass) -> QString {
  return m_tree->fileFor(index, forPass);
}

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

void MainWindow::on_treeView_doubleClicked(const QModelIndex &index) {
  QFileInfo fileOrFolder = m_tree->fileInfo(ui->treeView->currentIndex());

  if (fileOrFolder.isFile()) {
    editPassword(getFile(index, true));
  }
}

void MainWindow::deselect() {
  m_shownFile.clear();
  cancelOtpRequest();
  m_qtPass->clipboard().clear();
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
    m_processOutput->setVisible(true);
  }
}

void MainWindow::passShowHandler(const QString &p_output, const QString &file) {
  // Only the entry we most recently asked to show may repaint the panel; a
  // slower decrypt of an entry the user has since left is dropped.
  if (!file.isEmpty() && !m_shownFile.isEmpty() && file != m_shownFile) {
    return;
  }
  const AppSettings s = QtPassSettings::load();
  QStringList templ =
      s.useTemplate ? s.passTemplate.split("\n") : QStringList();
  bool allFields = s.useTemplate && s.templateAllFields;
  FileContent fileContent = FileContent::parse(p_output, templ, allFields);
  QString output = p_output;
  // Display variant: empty when the password line is itself an otpauth URI, so
  // the shared secret is neither rendered nor copied to the clipboard.
  QString password = fileContent.getPasswordForDisplay();

  // Skipped for an OTP request: the user asked for a code, and two
  // OleSetClipboard calls in one turn can leave the Windows clipboard empty.
  if (m_otpRequestFile.isEmpty() || m_otpRequestFile != file) {
    m_qtPass->clipboard().copyIfAlways(password, p_output);
  }

  m_displayPanel->clear();

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

  flashText(QtPass::formatOutput(output), false, true);
  setUiElementsEnabled(true);
}

void MainWindow::otpFromFileToClipboard(const QString &p_output,
                                        const QString &file) {
  // Permanently connected: only the decrypt of the entry onOtp() asked for
  // is ours, everything else belongs to a tree click or a Ctrl+C.
  if (m_otpRequestFile.isEmpty() || file != m_otpRequestFile) {
    return;
  }
  m_otpRequestFile.clear();

  if (p_output.isEmpty()) {
    // Distinguish "could not read the entry" from "entry has no OTP".
    flashText(tr("Could not decrypt this password entry"), true);
    setUiElementsEnabled(true);
    return;
  }

  // passShowHandler ran first for this finishedShow; if it rendered the OTP
  // row, reuse its code (as onOtp()'s fast path does). Otherwise parse:
  // hideContent, displayAsIs or no OTP field.
  const QString shown = m_displayPanel->currentOtpCode();
  if (!shown.isEmpty()) {
    m_qtPass->clipboard().copyText(shown);
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
    m_qtPass->clipboard().copyText(Totp::generateNow(*settings));
    showStatusMessage(tr("OTP code copied to clipboard"));
  } else {
    flashText(tr("No OTP code found in this password entry"), true);
  }
  setUiElementsEnabled(true);
}

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
      ui->lineEdit->setPlaceholderText(tr("Search password"));
    }
  }
  if (notify) {
    QString output = "***" + tr("Password and content hidden") + "***";
    ui->textBrowser->setHtml(output);
  } else {
    ui->textBrowser->setHtml("");
  }
}

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
  const bool uiEnabled = state;
  // is a file selected?
  state &= ui->treeView->currentIndex().isValid();
  ui->actionDelete->setEnabled(state);
  ui->actionEdit->setEnabled(state);
  updateGitButtonVisibility(uiEnabled);
  // `state` is now "UI enabled AND a file is selected", which is exactly when
  // generating an OTP makes sense.
  updateOtpButtonVisibility(state);
}

void MainWindow::restoreWindow() {
  // saveGeometry() covers position, size, maximized state and screen;
  // anything applied on top discards it. Centre only when nothing was saved.
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

void MainWindow::onConfig() { config(); }

void MainWindow::showAbout() {
  QMessageBox::about(
      this, tr("About QtPass"),
      tr("<h3>QtPass %1</h3>"
         "<p>A multi-platform GUI for <a "
         "href=\"https://www.passwordstore.org/\">"
         "pass</a>, the standard Unix password manager.</p>"
         "<p><a href=\"https://qtpass.org/\">qtpass.org</a> &middot; "
         "<a href=\"https://github.com/IJHack/QtPass\">Source and "
         "issues</a></p>"
         "<p>Copyright &copy; 2014&ndash;%2 IJhack. Licensed under the "
         "<a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">GNU GPL "
         "version 3</a> or later.</p>")
          .arg(QStringLiteral(VERSION),
               QString::number(QDate::currentDate().year())));
}

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

void MainWindow::onTimeoutSearch() {
  QString query = ui->lineEdit->text();

  if (query.isEmpty()) {
    ui->treeView->collapseAll();
    deselect();
  }

  // The words are matched literally, in order, with anything in between:
  // "work vpn" finds work/acme/vpn. Regular expressions belong to the
  // content search; here a typed "[" or "(" must not switch filtering off.
  QStringList words;
  for (const QString &word :
       query.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
    words << QRegularExpression::escape(word);
  }
  QRegularExpression regExp(words.join(QStringLiteral(".*")),
                            QRegularExpression::CaseInsensitiveOption);
  m_tree->setFilter(regExp);

  if (m_tree->proxy().rowCount() > 0 && !query.isEmpty()) {
    selectFirstFile();
  } else {
    ui->actionEdit->setEnabled(false);
    ui->actionDelete->setEnabled(false);
  }
}

void MainWindow::on_lineEdit_returnPressed() {
  qCDebug(lcQtPass) << "on_lineEdit_returnPressed"
                    << m_tree->proxy().rowCount();

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

  if (m_tree->proxy().rowCount() > 0) {
    selectFirstFile();
    on_treeView_clicked(ui->treeView->currentIndex());
  }
}

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
    m_tree->setFilter(QRegularExpression());
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
    ui->lineEdit->setPlaceholderText(tr("Search password"));
    ui->lineEdit->setToolTip(QString());
    ui->grepResultsList->clear();
    ui->grepResultsList->setVisible(false);
    ui->treeView->setVisible(true);
    m_tree->setFilter(QRegularExpression());
  }
}

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

void MainWindow::on_grepResultsList_itemClicked(QTreeWidgetItem *item,
                                                int /*column*/) {
  const AppSettings s = QtPassSettings::load();
  const QString entry = item->data(0, Qt::UserRole).toString();
  if (entry.isEmpty())
    return;
  const QString fullPath =
      QDir::cleanPath(QDir(s.passStore).filePath(entry + ".gpg"));
  const QModelIndex proxyIndex = m_tree->indexFor(fullPath);
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

void MainWindow::selectFirstFile() {
  QModelIndex index = firstFile(m_tree->rootIndex());
  ui->treeView->setCurrentIndex(index);
}

auto MainWindow::firstFile(QModelIndex parentIndex) -> QModelIndex {
  StoreModel &proxy = m_tree->proxy();
  int numRows = proxy.rowCount(parentIndex);
  for (int row = 0; row < numRows; ++row) {
    QModelIndex index = proxy.index(row, 0, parentIndex);
    if (m_tree->fileInfo(index).isFile()) {
      return index;
    }
    if (proxy.hasChildren(index)) {
      QModelIndex childFile = firstFile(index);
      if (childFile.isValid())
        return childFile;
    }
  }
  return QModelIndex();
}

// Guards the input dialogs (create, move, rename) against names like
// "../../etc/passwd" or absolute paths escaping the store root.
auto MainWindow::confirmPathInStore(const QString &candidate) -> bool {
  if (PathValidator::isPathInStore(QtPassSettings::getPassStore(), candidate)) {
    return true;
  }
  QMessageBox::warning(this, tr("Invalid name"),
                       tr("That name would resolve outside the password "
                          "store. Please choose a different name."));
  return false;
}

void MainWindow::setPassword(const QString &file, bool isNew) {
  const AppSettings s = QtPassSettings::load();
  PasswordDialog d(QtPassSettings::getPass(), s, file, isNew, this);

  if (isNew) {
    const QString storePath = s.passStore;
    QString folder = m_tree->currentDir(false);
    if (folder.isEmpty()) {
      folder = storePath;
    }
    // Folders to pick from, the tree's current one preselected. Real folders
    // only: a linked or junctioned one would write the entry outside the store.
    QStringList folders{QString()};
    const QStringList found = Util::directoriesUnder(storePath);
    for (const QString &path : found) {
      const QString rel = QDir(storePath).relativeFilePath(path);
      if (!rel.startsWith(u'.') && !rel.contains(QStringLiteral("/."))) {
        folders << rel;
      }
    }
    std::sort(folders.begin() + 1, folders.end());
    d.setNewEntryLocation(storePath, folders,
                          QDir(storePath).relativeFilePath(folder));
    QHash<QString, QStringList> templates =
        TemplateIO::readTemplates(storePath);
    if (!templates.isEmpty()) {
      QString defaultTemplate =
          TemplateIO::getFolderTemplate(folder, storePath);
      d.setAvailableTemplates(templates, defaultTemplate);
    }
  }

  if (!d.exec()) {
    ui->treeView->setFocus();
  }
}

void MainWindow::addPassword() { setPassword(QString()); }

auto MainWindow::confirmDeletion(const QString &file, bool isDir) -> bool {
  const QString path = QDir::separator() + file;
  if (!isDir) {
    return QMessageBox::question(
               this, tr("Delete password?"),
               tr("Are you sure you want to delete %1?").arg(path),
               QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
  }
  // Whole sentences, not a question stitched from pieces: a verb-final
  // language cannot put the rest of the question after the path.
  QString question =
      tr("Are you sure you want to delete %1 and the whole content?").arg(path);
  // A link, junction or special file inside is as unexpected as a stray
  // plain file: the walker leaves them out of content and reports them.
  QStringList skipped;
  const QStringList content = Util::regularFilesUnder(
      m_tree->fileSystem().rootPath() + path, {QStringLiteral("*")}, &skipped);
  const bool unexpected =
      !skipped.isEmpty() ||
      std::any_of(content.cbegin(), content.cend(), [](const QString &entry) {
        return QFileInfo(entry).suffix() != QLatin1String("gpg");
      });
  if (unexpected) {
    question += QStringLiteral("<br><strong>") +
                tr("Attention: there are unexpected files in the given "
                   "folder, check them before continuing.") +
                QStringLiteral("</strong>");
  }
  return QMessageBox::question(this, tr("Delete folder?"), question,
                               QMessageBox::Yes | QMessageBox::No) ==
         QMessageBox::Yes;
}

auto MainWindow::confirmLinkRemoval(const QString &file) -> bool {
  // Only the link goes (ImitatePass::Remove unlinks it); nothing behind it
  // is looked at or mentioned.
  return QMessageBox::question(
             this, tr("Delete link?"),
             tr("%1 is a symbolic link or junction. Remove the link? What it "
                "points to is left alone.")
                 .arg(QDir::separator() + file),
             QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}

void MainWindow::onDelete() {
  const QModelIndex currentIndex = ui->treeView->currentIndex();
  if (!currentIndex.isValid()) {
    // Otherwise the whole password directory would be deleted (#556).
    return;
  }

  const bool isDir = !m_tree->fileInfo(currentIndex).isFile();
  const QString file =
      isDir ? m_tree->currentDir(true) : getFile(currentIndex, true);
  const QString folder =
      m_tree->fileSystem().rootPath() + QDir::separator() + file;
  const bool linkedFolder = isDir && Util::isLinkedFolder(folder);

  if (!linkedFolder && refuseLinkedFolder(folder)) {
    // Behind a link: deleting would reach outside the store.
    return;
  }
  const bool confirmed =
      linkedFolder ? confirmLinkRemoval(file) : confirmDeletion(file, isDir);
  if (!confirmed) {
    return;
  }
  QtPassSettings::getPass()->Remove(file, isDir);
}

void MainWindow::cancelOtpRequest() {
  m_otpRequestFile.clear();
  m_copyRequestFile.clear();
}

void MainWindow::onOperationError() {
  cancelOtpRequest();
  // executeWrapperStarted() emptied the panel and the browser when the
  // operation started (a refusal says so too); what is left to forget is
  // which entry the panel was about to show.
  m_shownFile.clear();
}

// Derives the code in-process from one decrypt, so it works with either
// backend on every platform; otpFromFileToClipboard() finishes the job.
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

  // Fast path: the panel shows a live code, so no decrypt, and one clipboard
  // write (two setMimeData calls in one turn can empty the Windows
  // clipboard). Only for *this* entry: arrow keys and right-click move
  // currentIndex without QTreeView::clicked, so the code may be another's.
  const QString shown =
      (m_shownFile == file) ? m_displayPanel->currentOtpCode() : QString();
  if (!shown.isEmpty()) {
    m_qtPass->clipboard().copyText(shown);
    showStatusMessage(tr("OTP code copied to clipboard"));
    return;
  }

  // Fallback: decrypt once. The request file keeps passShowHandler from
  // copying the password and tells otpFromFileToClipboard its answer.
  m_otpRequestFile = file;
  setUiElementsEnabled(false);
  // Keep the marker in step so a second request takes the fast path; safe as
  // executeWrapperStarted() clears the panel, so a failed decrypt shows none.
  m_shownFile = file;
  QtPassSettings::getPass()->Show(file);
}

void MainWindow::onEdit() {
  QString file = getFile(ui->treeView->currentIndex(), true);
  editPassword(file);
}

void MainWindow::onUsers() {
  const QString dir = m_tree->currentDir(false);
  if (refuseLinkedFolder(dir)) {
    return;
  }

  UsersDialog d(QtPassSettings::getPass(), QtPassSettings::load(), dir, this);
  if (!d.exec()) {
    ui->treeView->setFocus();
  }
}

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

void MainWindow::updateProfileBox() {
  const Profiles profiles = QtPassSettings::getProfiles();

  if (profiles.isEmpty()) {
    ui->profileWidget->hide();
  } else {
    ui->profileWidget->show();
    ui->profileBox->setEnabled(profiles.size() > 1);
    ui->profileBox->clear();
    for (auto i = profiles.cbegin(); i != profiles.cend(); ++i) {
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

void MainWindow::on_profileBox_currentTextChanged(const QString &name) {
  if (m_freshStart || name == QtPassSettings::getProfile()) {
    return;
  }

  ui->lineEdit->clear();

  const Profile prof = QtPassSettings::getProfiles().value(name);
  AppSettings s = QtPassSettings::load();
  s.activeProfile = name;
  s.passStore = prof.path;
  s.passSigningKey = prof.signingKey;
  // A profile without its own Git flags keeps the global values.
  s.useGit = prof.useGit.value_or(s.useGit);
  s.autoPush = prof.autoPush.value_or(s.autoPush);
  s.autoPull = prof.autoPull.value_or(s.autoPull);
  QtPassSettings::save(s);
  ui->statusBar->showMessage(tr("Profile changed to %1").arg(name), 2000);

  QtPassSettings::getPass()->updateEnv();

  m_tree->setStore(QtPassSettings::getPassStore());
  deselect();
  ui->treeView->setCurrentIndex(QModelIndex());
}

void MainWindow::initTrayIcon() {
  m_tray = new TrayIcon(this);
  if (!m_tray->getIsAllocated()) {
    destroyTrayIcon();
  }
}

void MainWindow::destroyTrayIcon() {
  delete m_tray;
  m_tray = nullptr;
}

void MainWindow::closeEvent(QCloseEvent *event) {
  // Save in both branches: a tray user only ever hides the window, and
  // used to quit through the tray menu with nothing ever written.
  saveWindowState();
  if (QtPassSettings::isHideOnClose()) {
    this->hide();
    event->ignore();
  } else {
    m_qtPass->clipboard().clear();
    event->accept();
    // A visible QSystemTrayIcon keeps the application alive, so
    // quitOnLastWindowClosed never fires; quit explicitly.
    QApplication::quit();
  }
}

auto MainWindow::eventFilter(QObject *obj, QEvent *event) -> bool {
  if ((obj == ui->toolBar || obj == menuBar()) &&
      event->type() == QEvent::PaletteChange) {
    // Breeze gives these bars a "header" palette and, after a runtime
    // light/dark switch, stamps the previous theme's colours back from a
    // cached kdeglobals, hiding the toolbar icons. Deferred so we never
    // re-enter the style while it is still applying its palette.
    QTimer::singleShot(0, this, &MainWindow::dropStaleToolsAreaPalettes);
  }
  if (obj == ui->lineEdit && event->type() == QEvent::KeyPress) {
    auto *key = dynamic_cast<QKeyEvent *>(event);
    if (key != nullptr && key->key() == Qt::Key_Down) {
      ui->treeView->setFocus();
    }
  }
  return QObject::eventFilter(obj, event);
}

// Only once the application palette changed (see m_themeSwitched): a bar
// whose active Window lightness is far from the application's shows the
// other theme.
void MainWindow::dropStaleToolsAreaPalettes() {
  if (!m_themeSwitched) {
    return;
  }
  constexpr int kStaleToolsAreaLightness = 64;
  // The active group on both sides: a bar's palette() reads the inactive
  // group while another window has focus (the settings app, during a theme
  // switch), and Breeze gives the header set different inactive colours.
  const int appLightness = QApplication::palette()
                               .color(QPalette::Active, QPalette::Window)
                               .lightness();
  for (QWidget *bar : {static_cast<QWidget *>(ui->toolBar),
                       static_cast<QWidget *>(menuBar())}) {
    if (!bar->testAttribute(Qt::WA_SetPalette)) {
      continue;
    }
    const int barLightness =
        bar->palette().color(QPalette::Active, QPalette::Window).lightness();
    if (qAbs(barLightness - appLightness) <= kStaleToolsAreaLightness) {
      continue;
    }
    bar->setPalette(QPalette());
    // Both bars are transparent and the style paints the tools area from the
    // stale palette, so fill our own. Window, not their Button role, which is
    // not the window colour in either theme.
    bar->setBackgroundRole(QPalette::Window);
    bar->setAutoFillBackground(true);
  }
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
  switch (event->key()) {
  case Qt::Key_Delete:
    onDelete();
    break;
  case Qt::Key_Return:
  case Qt::Key_Enter:
    if (m_tree->proxy().rowCount() > 0) {
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

void MainWindow::addShareMenu(QMenu &contextMenu, const AppSettings &s) {
  const QString dirPath = QDir::cleanPath(m_tree->currentDir(false));
  auto *shareMenu = new QMenu(tr("Share"), &contextMenu);
  contextMenu.addMenu(shareMenu);

  const QString gpgIdPath = Pass::getGpgIdPath(dirPath, s.passStore);
  const bool gpgIdExists = !gpgIdPath.isEmpty() && QFile(gpgIdPath).exists();
  const QString exePath = s.usePass ? s.passExecutable : s.gpgExecutable;
  const bool gpgAvailable =
      !exePath.isEmpty() &&
      (Executor::parseWslCommand(exePath) || QFile(exePath).exists());

  QAction *reencrypt = shareMenu->addAction(tr("Re-encrypt all passwords"));
  reencrypt->setEnabled(gpgIdExists && gpgAvailable);
  connect(reencrypt, &QAction::triggered, this,
          [this, dirPath]() { reencryptPath(dirPath); });

  QAction *exportKey = shareMenu->addAction(tr("Export my public key..."));
  exportKey->setEnabled(gpgAvailable);
  connect(exportKey, &QAction::triggered, this, &MainWindow::exportPublicKey);

  QAction *addRecipientAction = shareMenu->addAction(tr("Add recipient..."));
  addRecipientAction->setEnabled(gpgIdExists && gpgAvailable);
  connect(addRecipientAction, &QAction::triggered, this,
          [this, dirPath]() { addRecipient(dirPath); });

  QAction *shareHelp = shareMenu->addAction(tr("What is this?"));
  connect(shareHelp, &QAction::triggered, this, &MainWindow::showShareHelp);
}

void MainWindow::showContextMenu(const QPoint &pos) {
  const AppSettings s = QtPassSettings::load();
  const QModelIndex index = ui->treeView->indexAt(pos);
  const bool selected = index.isValid();
  if (!selected) {
    ui->treeView->clearSelection();
    ui->actionDelete->setEnabled(false);
    ui->actionEdit->setEnabled(false);
  }
  ui->treeView->setCurrentIndex(index);
  const QPoint globalPos = ui->treeView->viewport()->mapToGlobal(pos);
  const QFileInfo fileOrFolder = m_tree->fileInfo(ui->treeView->currentIndex());
  const bool isDir = fileOrFolder.isDir();

  QMenu contextMenu;
  if (!selected || isDir) {
    const std::array<std::pair<QString, void (MainWindow::*)()>, 4> onFolder{{
        {tr("Open folder with file manager"), &MainWindow::openFolder},
        {tr("Add folder"), &MainWindow::addFolder},
        {tr("Add password"), &MainWindow::addPassword},
        {tr("Users"), &MainWindow::onUsers},
    }};
    for (const auto &[text, slot] : onFolder) {
      connect(contextMenu.addAction(text), &QAction::triggered, this, slot);
    }
  } else if (fileOrFolder.isFile()) {
    connect(contextMenu.addAction(tr("Edit")), &QAction::triggered, this,
            &MainWindow::onEdit);
  }
  if (selected) {
    contextMenu.addSeparator();
    if (isDir) {
      connect(contextMenu.addAction(tr("Rename folder")), &QAction::triggered,
              this, &MainWindow::renameFolder);
    } else if (fileOrFolder.isFile()) {
      connect(contextMenu.addAction(tr("Rename password")), &QAction::triggered,
              this, &MainWindow::renamePassword);
    }
    connect(contextMenu.addAction(tr("Delete")), &QAction::triggered, this,
            &MainWindow::onDelete);
    if (isDir) {
      addShareMenu(contextMenu, s);
    }
  }
  contextMenu.exec(globalPos);
}

void MainWindow::showBrowserContextMenu(const QPoint &pos) {
  QMenu *contextMenu = ui->textBrowser->createStandardContextMenu(pos);
  // Reparent from textBrowser: a browser stylesheet cascades to the child
  // QMenu and breaks its opaque native background.
  contextMenu->setParent(this, contextMenu->windowFlags());
  QPoint globalPos = ui->textBrowser->viewport()->mapToGlobal(pos);

  contextMenu->exec(globalPos);
  delete contextMenu;
}

void MainWindow::openFolder() {
  QString dir = m_tree->currentDir(false);

  QString path = QDir::toNativeSeparators(dir);
  QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::addFolder() {
  const AppSettings s = QtPassSettings::load();
  bool ok;
  QString dir = m_tree->currentDir(false);
  QString newdir =
      QInputDialog::getText(this, tr("New folder"),
                            tr("New folder: \n(Will be placed in %1 )")
                                .arg(s.passStore + m_tree->currentDir(true)),
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
  // With a signing key, ImitatePass::loadVerifiedRecipients and pass
  // (PASSWORD_STORE_SIGNING_KEY) refuse a .gpg-id without .gpg-id.sig, and
  // signing needs a passphrase just to make a folder. So it inherits the
  // parent's signed list; UsersDialog (Init) gives it its own.
  const bool signingConfigured = !s.passSigningKey.trimmed().isEmpty();
  if (s.addGPGId && !signingConfigured) {
    // Seed from the parent's recipients in effect, not from `enabled` keys:
    // that flag is only set in UsersDialog, so it left a zero-byte .gpg-id
    // shadowing the parent's recipients (#1682).
    if (!Pass::seedGpgIdFile(newdir, s.passStore)) {
      QMessageBox::warning(
          this, tr("Error"),
          tr("Failed to create .gpg-id file in: %1").arg(newdir));
      return;
    }
  }
}

void MainWindow::renameFolder() {
  bool ok;
  QString srcDir = QDir::cleanPath(m_tree->currentDir(false));
  QString srcDirName = QDir(srcDir).dirName();
  QString newName =
      QInputDialog::getText(this, tr("Rename folder"), tr("Rename folder to: "),
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

void MainWindow::editPassword(const QString &file) {
  if (!file.isEmpty()) {
    const AppSettings s = QtPassSettings::load();
    if (s.useGit && s.autoPull) {
      onUpdate(true);
    }
    setPassword(file, false);
  }
}

void MainWindow::renamePassword() {
  bool ok;
  QString file = getFile(ui->treeView->currentIndex(), false);
  QString filePath = QFileInfo(file).path();
  QString fileName = QFileInfo(file).fileName();
  if (fileName.endsWith(".gpg", Qt::CaseInsensitive)) {
    fileName.chop(4);
  }

  QString newName =
      QInputDialog::getText(this, tr("Rename file"), tr("Rename file to: "),
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

void MainWindow::copyPasswordFromTreeview() {
  QFileInfo fileOrFolder = m_tree->fileInfo(ui->treeView->currentIndex());

  if (fileOrFolder.isFile()) {
    QString file = getFile(ui->treeView->currentIndex(), true);
    // The newest Ctrl+C wins: finishedShow names its file, so the decrypt of
    // an earlier request is simply not this one's.
    m_copyRequestFile = file;
    // This Show repaints the panel as well; see onOtp().
    m_shownFile = file;
    QtPassSettings::getPass()->Show(file);
  }
}

void MainWindow::passwordFromFileToClipboard(const QString &text,
                                             const QString &file) {
  if (m_copyRequestFile.isEmpty() || file != m_copyRequestFile) {
    return;
  }
  m_copyRequestFile.clear();
  const QStringList tokens = text.split('\n');
  if (tokens.isEmpty()) {
    return;
  }
  // A `pass otp insert` entry's first line is the otpauth:// URI carrying the
  // TOTP seed; copying it would leak 2FA material. Matches the display path
  // (FileContent::getPasswordForDisplay / PasswordDisplayPanel).
  if (FileContent::isOtpUriValue(tokens[0])) {
    flashText(tr("This entry holds an OTP secret, not a password"), true);
    return;
  }
  m_qtPass->clipboard().copyText(tokens[0]);
}

void MainWindow::showStatusMessage(const QString &msg, int timeout) {
  ui->statusBar->showMessage(msg, timeout);
}

void MainWindow::reencryptPath(const QString &dir) {
  QDir checkDir(dir);
  if (!checkDir.exists()) {
    QMessageBox::critical(this, tr("Error"),
                          tr("Directory does not exist: %1").arg(dir));
    return;
  }
  if (refuseLinkedFolder(dir)) {
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

  // Disable now; the asynchronous startReencryptPath signal re-runs this
  // harmlessly.
  startReencryptPath();

  QtPassSettings::getImitatePass()->reencryptPath(
      QDir::cleanPath(QDir(dir).absolutePath()));
}

// Idempotent: reencryptPath() calls it before ImitatePass emits the signal.
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

void MainWindow::reencryptProgress(int current, int total) {
  if (!m_reencryptProgress)
    return;
  m_reencryptProgress->setMaximum(total);
  m_reencryptProgress->setLabelText(
      tr("Re-encrypting passwords: %1 of %2").arg(current).arg(total));
  // On a modal dialog setValue() runs processEvents(), which can deliver
  // endReencryptPath() and reset m_reencryptProgress: keep this last.
  m_reencryptProgress->setValue(current);
}

void MainWindow::endReencryptPath() {
  m_reencryptRunning = false;
  if (m_reencryptProgress) {
    m_reencryptProgress->hide();
    m_reencryptProgress->deleteLater();
    m_reencryptProgress = nullptr;
  }
  setUiElementsEnabled(true);
}

void MainWindow::exportPublicKey() {
  const AppSettings s = QtPassSettings::load();
  const QString identity = s.passSigningKey;
  if (identity.isEmpty()) {
    QMessageBox::information(
        this, tr("Export public key"),
        tr("<h3>Export your public key</h3>"
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
    QMessageBox::warning(this, tr("Export public key"),
                         tr("Could not export public key for %1.\n\n%2")
                             .arg(identity, stdErr.isEmpty()
                                                ? tr("No output from gpg.")
                                                : stdErr));
    return;
  }
  ExportPublicKeyDialog dialog(identity, stdOut, this);
  dialog.exec();
}

// UsersDialog only ticks keys already in the keyring; importing a foreign
// key still has to happen via gpg (or QtPass settings) first.
void MainWindow::addRecipient(const QString &dir) {
  if (refuseLinkedFolder(dir)) {
    return;
  }
  UsersDialog d(QtPassSettings::getPass(), QtPassSettings::load(), dir, this);
  d.exec();
}

// A tree-picked folder that is, or lies behind, a symlink or junction is not
// a folder of the store. The store root itself may be a link (a synced
// folder); that is configuration, not a tree pick.
auto MainWindow::refuseLinkedFolder(const QString &dir) -> bool {
  if (!Util::isUnderLink(dir, QtPassSettings::load().passStore)) {
    return false;
  }
  QMessageBox::critical(
      this, tr("Not a folder of the store"),
      tr("%1 is, or lies behind, a symbolic link or junction. What that "
         "points to is not part of the password store and is left alone.")
          .arg(QDir::toNativeSeparators(QDir::cleanPath(dir))));
  return true;
}

void MainWindow::showShareHelp() {
  QMessageBox::information(
      this, tr("Sharing passwords with GPG"),
      tr("<h3>Sharing passwords with GPG</h3>"
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

void MainWindow::updateGitButtonVisibility(bool uiEnabled) {
  const AppSettings s = QtPassSettings::load();
  // Without Git there is nothing to push or pull, ever: the buttons go, not
  // grey out. While an operation runs they stay and are disabled.
  const bool usable =
      s.useGit && !(s.gitExecutable.isEmpty() && s.passExecutable.isEmpty());
  ui->actionPush->setVisible(usable);
  ui->actionUpdate->setVisible(usable);
  // The separator in front of them would otherwise sit next to the next one.
  const QList<QAction *> actions = ui->toolBar->actions();
  const qsizetype at = actions.indexOf(ui->actionPush);
  if (at > 0 && actions.at(at - 1)->isSeparator()) {
    actions.at(at - 1)->setVisible(usable);
  }
  enableGitButtons(usable && uiEnabled);
}

void MainWindow::updateOtpButtonVisibility(bool uiEnabled) {
  // No platform gating: codes are generated in-process. Visible but disabled
  // when OTP is off: the panel never shows an OTP field (a secret), so hiding
  // this too would leave no trace of the feature. Honour uiEnabled, or the
  // user can stack Show calls while a decrypt is in flight.
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
  ui->actionPush->setEnabled(state);
  ui->actionUpdate->setEnabled(state);
}

void MainWindow::critical(const QString &title, const QString &msg) {
  QMessageBox::critical(this, title, msg);
}

void MainWindow::onProcessOutput(const QString &output, bool isError,
                                 Enums::PROCESS pid) {
  if (!QtPassSettings::isShowProcessOutput()) {
    return;
  }
  m_processOutput->append(output, isError,
                          ProcessOutputPanel::processName(pid));
}

void MainWindow::updateProcessOutputVisibility() {
  // The action's toggled handler shows or hides the dock.
  ui->actionShowProcessOutput->setChecked(
      QtPassSettings::isShowProcessOutput());
  m_processOutput->setVisible(QtPassSettings::isShowProcessOutput());
}
