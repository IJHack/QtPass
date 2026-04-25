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
#include "qpushbuttonasqrcode.h"
#include "qpushbuttonshowpassword.h"
#include "qpushbuttonwithclipboard.h"
#include "qtpass.h"
#include "qtpasssettings.h"
#include "trayicon.h"
#include "ui_mainwindow.h"
#include "usersdialog.h"
#include "util.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDialog>
#include <QDirIterator>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QShortcut>
#include <QTextCursor>
#include <QTimer>
#include <QTreeWidget>
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
  // see http://doc.qt.io/qt-5/qkeysequence.html#qt_set_sequence_auto_mnemonic
  qt_set_sequence_auto_mnemonic(true);
#endif
  ui->setupUi(this);

  m_qtPass = new QtPass(this);

  // register shortcut ctrl/cmd + Q to close the main window
  new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q), this, SLOT(close()));
  // register shortcut ctrl/cmd + C to copy the currently selected password
  new QShortcut(QKeySequence(QKeySequence::StandardKey::Copy), this,
                SLOT(copyPasswordFromTreeview()));

  model.setNameFilters(QStringList() << "*.gpg");
  model.setNameFilterDisables(false);

  /*
   * I added this to solve Windows bug but now on GNU/Linux the main folder,
   * if hidden, disappear
   *
   * model.setFilter(QDir::NoDot);
   */

  QString passStore = QtPassSettings::getPassStore(Util::findPasswordStore());

  QModelIndex rootDir = model.setRootPath(passStore);
  model.fetchMore(rootDir);

  proxyModel.setModelAndStore(&model, passStore);
  selectionModel.reset(new QItemSelectionModel(&proxyModel));

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

  if (QtPassSettings::isUseMonospace()) {
    QFont monospace("Monospace");
    monospace.setStyleHint(QFont::Monospace);
    ui->textBrowser->setFont(monospace);
  }
  if (QtPassSettings::isNoLineWrapping()) {
    ui->textBrowser->setLineWrapMode(QTextBrowser::NoWrap);
  }
  ui->textBrowser->setOpenExternalLinks(true);
  ui->textBrowser->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->textBrowser, &QWidget::customContextMenuRequested, this,
          &MainWindow::showBrowserContextMenu);

  updateProfileBox();

  QtPassSettings::getPass()->updateEnv();
  clearPanelTimer.setInterval(MS_PER_SECOND *
                              QtPassSettings::getAutoclearPanelSeconds());
  clearPanelTimer.setSingleShot(true);
  connect(&clearPanelTimer, &QTimer::timeout, this, [this]() { clearPanel(); });

  searchTimer.setInterval(350);
  searchTimer.setSingleShot(true);

  connect(&searchTimer, &QTimer::timeout, this, &MainWindow::onTimeoutSearch);

  initToolBarButtons();
  initStatusBar();

  connect(QtPassSettings::getPass(), &Pass::finishedAnyWithPid, this,
          [this](const QString &out, const QString &err, int pid) {
            if (pid == Enums::PASS_SHOW || pid == Enums::PASS_OTP_GENERATE) {
              return;
            }
            if (!out.isEmpty()) {
              onProcessOutput(out, false, pid);
            }
            if (!err.isEmpty()) {
              onProcessOutput(err, true, pid);
            }
          });

  connect(ui->processOutputEdit->verticalScrollBar(),
          &QScrollBar::sliderPressed, this, [this]() { m_autoScroll = false; });
  connect(ui->processOutputEdit->verticalScrollBar(), &QScrollBar::valueChanged,
          this, [this]() {
            auto *sb = ui->processOutputEdit->verticalScrollBar();
            m_autoScroll = sb->value() >= sb->maximum();
          });

  ui->lineEdit->setClearButtonEnabled(true);
  updateGrepButtonVisibility();

  setUiElementsEnabled(true);

  QTimer::singleShot(10, this, SLOT(focusInput()));

  ui->lineEdit->setText(searchText);

  if (!m_qtPass->init()) {
    // no working config so this should just quit
    QApplication::quit();
  }
}

MainWindow::~MainWindow() { delete m_qtPass; }

/**
 * @brief MainWindow::focusInput selects any text (if applicable) in the search
 * box and sets focus to it. Allows for easy searching, called at application
 * start and when receiving empty message in MainWindow::messageAvailable when
 * compiled with SINGLE_APP=1 (default).
 */
void MainWindow::focusInput() {
  ui->lineEdit->selectAll();
  ui->lineEdit->setFocus();
}

/**
 * @brief MainWindow::changeEvent sets focus to the search box
 * @param event
 */
void MainWindow::changeEvent(QEvent *event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::ActivationChange) {
    if (isActiveWindow()) {
      focusInput();
    }
  }
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

  statusBar()->addPermanentWidget(ui->processOutputWidget);

  updateProcessOutputVisibility();
}

auto MainWindow::getCurrentTreeViewIndex() -> QModelIndex {
  return ui->treeView->currentIndex();
}

void MainWindow::cleanKeygenDialog() {
  if (this->keygenDialog != nullptr) {
    this->keygenDialog->close();
  }
  this->keygenDialog = nullptr;
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
 * displaying the text.
 * @param const bool isHtml - If true, treats the text as HTML and appends it to
 * the existing HTML content.
 * @return void - No return value.
 */
void MainWindow::flashText(const QString &text, const bool isError,
                           const bool isHtml) {
  if (isError) {
    ui->textBrowser->setTextColor(Qt::red);
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
  if (QtPassSettings::isUseMonospace()) {
    QFont monospace("Monospace");
    monospace.setStyleHint(QFont::Monospace);
    ui->textBrowser->setFont(monospace);
  } else {
    ui->textBrowser->setFont(QFont());
  }

  if (QtPassSettings::isNoLineWrapping()) {
    ui->textBrowser->setLineWrapMode(QTextBrowser::NoWrap);
  } else {
    ui->textBrowser->setLineWrapMode(QTextBrowser::WidgetWidth);
  }
}

void MainWindow::applyWindowFlagsSettings() {
  if (QtPassSettings::isAlwaysOnTop()) {
    Qt::WindowFlags flags = windowFlags();
    this->setWindowFlags(flags | Qt::WindowStaysOnTopHint);
  } else {
    this->setWindowFlags(Qt::Window);
  }
  this->show();
}

/**
 * @brief Opens and processes the application configuration dialog, then applies
 * any accepted settings.
 * @example
 * config();
 *
 * @return void - This function does not return a value.
 */
void MainWindow::config() {
  QScopedPointer<ConfigDialog> d(new ConfigDialog(this));
  d->setModal(true);
  // Automatically default to pass if it's available
  if (m_qtPass->isFreshStart() &&
      QFile(QtPassSettings::getPassExecutable()).exists()) {
    QtPassSettings::setUsePass(true);
  }

  if (m_qtPass->isFreshStart()) {
    d->wizard(); // run initial setup wizard for first-time configuration
  }
  if (d->exec()) {
    if (d->result() == QDialog::Accepted) {
      applyTextBrowserSettings();
      applyWindowFlagsSettings();

      updateProfileBox();
      const QString passStore = QtPassSettings::getPassStore();
      proxyModel.setStore(passStore);
      ui->treeView->setRootIndex(
          proxyModel.mapFromSource(model.setRootPath(passStore)));
      deselect();
      ui->treeView->setCurrentIndex(QModelIndex());

      if (m_qtPass->isFreshStart() && !Util::configIsValid()) {
        config();
      }
      QtPassSettings::getPass()->updateEnv();
      clearPanelTimer.setInterval(MS_PER_SECOND *
                                  QtPassSettings::getAutoclearPanelSeconds());
      m_qtPass->setClipboardTimer();

      updateGitButtonVisibility();
      updateOtpButtonVisibility();
      updateGrepButtonVisibility();
      updateProcessOutputVisibility();
      if (QtPassSettings::isUseTrayIcon() && tray == nullptr) {
        initTrayIcon();
      } else if (!QtPassSettings::isUseTrayIcon() && tray != nullptr) {
        destroyTrayIcon();
      }
    }

    m_qtPass->setFreshStart(false);
  }
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
  currentDir =
      Util::getDir(ui->treeView->currentIndex(), false, model, proxyModel);
  // Clear any previously cached clipped text before showing new password
  m_qtPass->clearClippedText();
  QString file = getFile(index, true);
  ui->passwordName->setText(file);
  if (!file.isEmpty() && !cleared) {
    QtPassSettings::getPass()->Show(file);
  } else {
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
  currentDir = "";
  m_qtPass->clearClipboard();
  ui->treeView->clearSelection();
  ui->actionEdit->setEnabled(false);
  ui->actionDelete->setEnabled(false);
  ui->passwordName->setText("");
  clearPanel(false);
}

void MainWindow::executeWrapperStarted() {
  clearTemplateWidgets();
  ui->textBrowser->clear();
  setUiElementsEnabled(false);
  clearPanelTimer.stop();
  m_processOutputShownForCurrentCommand = false;
  if (QtPassSettings::isShowProcessOutput()) {
    ui->processOutputWidget->setVisible(true);
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
  QStringList templ = QtPassSettings::isUseTemplate()
                          ? QtPassSettings::getPassTemplate().split("\n")
                          : QStringList();
  bool allFields =
      QtPassSettings::isUseTemplate() && QtPassSettings::isTemplateAllFields();
  FileContent fileContent = FileContent::parse(p_output, templ, allFields);
  QString output = p_output;
  QString password = fileContent.getPassword();

  // set clipped text
  m_qtPass->setClippedText(password, p_output);

  // first clear the current view:
  clearTemplateWidgets();

  // show what is needed:
  if (QtPassSettings::isHideContent()) {
    output = "***" + tr("Content hidden") + "***";
  } else if (!QtPassSettings::isDisplayAsIs()) {
    if (!password.isEmpty()) {
      // set the password, it is hidden if needed in addToGridLayout
      addToGridLayout(0, tr("Password"), password);
    }

    NamedValues namedValues = fileContent.getNamedValues();
    for (int j = 0; j < namedValues.length(); ++j) {
      const NamedValue &nv = namedValues.at(j);
      addToGridLayout(j + 1, nv.name, nv.value);
    }
    if (ui->gridLayout->count() == 0) {
      ui->verticalLayoutPassword->setSpacing(0);
    } else {
      ui->verticalLayoutPassword->setSpacing(6);
    }

    output = fileContent.getRemainingDataForDisplay();
  }

  if (QtPassSettings::isUseAutoclearPanel()) {
    clearPanelTimer.start();
  }

  emit passShowHandlerFinished(output);
  setUiElementsEnabled(true);
}

/**
 * @brief Handles the OTP output by displaying it, copying it to the clipboard,
 * and updating the UI state.
 * @example
 * void MainWindow::passOtpHandler(const QString &p_output);
 *
 * @param const QString &p_output - The OTP code text to process; if empty, an
 * error message is shown instead.
 * @return void - This function does not return a value.
 */
void MainWindow::passOtpHandler(const QString &p_output) {
  if (!p_output.isEmpty()) {
    addToGridLayout(ui->gridLayout->count() + 1, tr("OTP Code"), p_output);
    m_qtPass->copyTextToClipboard(p_output);
    showStatusMessage(tr("OTP code copied to clipboard"));
  } else {
    flashText(tr("No OTP code found in this password entry"), true);
  }
  if (QtPassSettings::isUseAutoclearPanel()) {
    clearPanelTimer.start();
  }
  setUiElementsEnabled(true);
}

/**
 * @brief MainWindow::clearPanel hide the information from shoulder surfers
 */
void MainWindow::clearPanel(bool notify) {
  while (ui->gridLayout->count() > 0) {
    QLayoutItem *item = ui->gridLayout->takeAt(0);
    delete item->widget();
    delete item;
  }
  const bool grepWasVisible = ui->grepResultsList->isVisible();
  ui->grepResultsList->clear();
  if (grepWasVisible) {
    ui->grepResultsList->setVisible(false);
    ui->treeView->setVisible(true);
    if (m_grepMode) {
      m_grepMode = false;
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
  ui->treeView->setEnabled(state);
  ui->lineEdit->setEnabled(state);
  ui->lineEdit->installEventFilter(this);
  ui->actionAddPassword->setEnabled(state);
  ui->actionAddFolder->setEnabled(state);
  ui->actionUsers->setEnabled(state);
  ui->actionConfig->setEnabled(state);
  // is a file selected?
  state &= ui->treeView->currentIndex().isValid();
  ui->actionDelete->setEnabled(state);
  ui->actionEdit->setEnabled(state);
  updateGitButtonVisibility();
  updateOtpButtonVisibility();
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
  QByteArray geometry = QtPassSettings::getGeometry(saveGeometry());
  restoreGeometry(geometry);
  QByteArray savestate = QtPassSettings::getSavestate(saveState());
  restoreState(savestate);
  QPoint position = QtPassSettings::getPos(pos());
  move(position);
  QSize newSize = QtPassSettings::getSize(size());
  resize(newSize);
  if (QtPassSettings::isMaximized(isMaximized())) {
    showMaximized();
  }

  if (QtPassSettings::isAlwaysOnTop()) {
    Qt::WindowFlags flags = windowFlags();
    setWindowFlags(flags | Qt::WindowStaysOnTopHint);
    show();
  }

  if (QtPassSettings::isUseTrayIcon() && tray == nullptr) {
    initTrayIcon();
    if (QtPassSettings::isStartMinimized()) {
      // since we are still in constructor, can't directly hide
      QTimer::singleShot(10, this, SLOT(hide()));
    }
  } else if (!QtPassSettings::isUseTrayIcon() && tray != nullptr) {
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
  if (m_grepMode)
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
  proxyModel.setFilterRegularExpression(regExp);
  ui->treeView->setRootIndex(proxyModel.mapFromSource(
      model.setRootPath(QtPassSettings::getPassStore())));

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

  if (m_grepMode) {
    const QString query = ui->lineEdit->text();
    if (!query.isEmpty()) {
      m_grepCancelled = false;
      ui->grepResultsList->clear();
      ui->statusBar->showMessage(tr("Searching…"));
      if (!m_grepBusy) {
        m_grepBusy = true;
        QApplication::setOverrideCursor(Qt::WaitCursor);
      }
      QtPassSettings::getPass()->Grep(query, ui->grepCaseButton->isChecked());
    } else {
      m_grepCancelled = true;
      if (m_grepBusy) {
        m_grepBusy = false;
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
  m_grepMode = checked;
  if (checked) {
    ui->lineEdit->setPlaceholderText(tr("Search content (regex)"));
    ui->lineEdit->clear();
    searchTimer.stop();
    proxyModel.setFilterRegularExpression(QRegularExpression());
    ui->treeView->setRootIndex(proxyModel.mapFromSource(
        model.setRootPath(QtPassSettings::getPassStore())));
    ui->grepResultsList->setVisible(false);
    // Keep treeView visible until results arrive
  } else {
    if (m_grepBusy) {
      m_grepBusy = false;
      m_grepCancelled = true;
      QApplication::restoreOverrideCursor();
    }
    searchTimer.stop();
    ui->lineEdit->blockSignals(true);
    ui->lineEdit->clear();
    ui->lineEdit->blockSignals(false);
    ui->lineEdit->setPlaceholderText(tr("Search Password"));
    ui->grepResultsList->clear();
    ui->grepResultsList->setVisible(false);
    ui->treeView->setVisible(true);
    proxyModel.setFilterRegularExpression(QRegularExpression());
    ui->treeView->setRootIndex(proxyModel.mapFromSource(
        model.setRootPath(QtPassSettings::getPassStore())));
  }
}

/**
 * @brief Display grep results in grepResultsList.
 */
void MainWindow::onGrepFinished(
    const QList<QPair<QString, QStringList>> &results) {
  if (m_grepBusy) {
    m_grepBusy = false;
    QApplication::restoreOverrideCursor();
  }
  if (m_grepCancelled) {
    m_grepCancelled = false;
    return;
  }
  setUiElementsEnabled(true);
  if (!m_grepMode)
    return;
  ui->grepResultsList->clear();
  if (results.isEmpty()) {
    ui->statusBar->showMessage(tr("No matches found."), 3000);
    ui->grepResultsList->setVisible(false);
    ui->treeView->setVisible(true);
    return;
  }
  const bool hideContent = QtPassSettings::isHideContent();
  int totalLines = 0;
  for (const auto &pair : results) {
    QTreeWidgetItem *entryItem = new QTreeWidgetItem(ui->grepResultsList);
    entryItem->setText(0, pair.first);
    entryItem->setData(0, Qt::UserRole, pair.first);
    for (const QString &line : pair.second) {
      QTreeWidgetItem *lineItem = new QTreeWidgetItem(entryItem);
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
          tr("in %n entr(ies).", nullptr, results.size()),
      3000);
  if (QtPassSettings::isUseAutoclearPanel())
    clearPanelTimer.start();
}

/**
 * @brief Navigate to the password entry when a grep result is clicked.
 */
void MainWindow::on_grepResultsList_itemClicked(QTreeWidgetItem *item,
                                                int /*column*/) {
  const QString entry = item->data(0, Qt::UserRole).toString();
  if (entry.isEmpty())
    return;
  const QString fullPath = QDir::cleanPath(
      QDir(QtPassSettings::getPassStore()).filePath(entry + ".gpg"));
  QModelIndex srcIndex = model.index(fullPath);
  if (!srcIndex.isValid())
    return;
  QModelIndex proxyIndex = proxyModel.mapFromSource(srcIndex);
  if (!proxyIndex.isValid())
    return;
  ui->treeView->setCurrentIndex(proxyIndex);
  on_treeView_clicked(proxyIndex);
  if (QtPassSettings::isHideContent() || QtPassSettings::isUseAutoclearPanel())
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
  QModelIndex index = proxyModel.mapFromSource(
      model.setRootPath(QtPassSettings::getPassStore()));
  index = firstFile(index);
  ui->treeView->setCurrentIndex(index);
}

/**
 * @brief MainWindow::firstFile return location of first possible file
 * @param parentIndex
 * @return QModelIndex
 */
auto MainWindow::firstFile(QModelIndex parentIndex) -> QModelIndex {
  QModelIndex index = parentIndex;
  int numRows = proxyModel.rowCount(parentIndex);
  for (int row = 0; row < numRows; ++row) {
    index = proxyModel.index(row, 0, parentIndex);
    if (model.fileInfo(proxyModel.mapToSource(index)).isFile()) {
      return index;
    }
    if (proxyModel.hasChildren(index)) {
      return firstFile(index);
    }
  }
  return index;
}

/**
 * @brief MainWindow::setPassword open passworddialog
 * @param file which pgp file
 * @param isNew insert (not update)
 */
void MainWindow::setPassword(const QString &file, bool isNew) {
  PasswordDialog d(file, isNew, this);

  if (isNew) {
    QString storePath = QtPassSettings::getPassStore();
    QString folder =
        Util::getDir(ui->treeView->currentIndex(), false, model, proxyModel);
    if (folder.isEmpty()) {
      folder = storePath;
    }
    QHash<QString, QStringList> templates = Util::readTemplates(storePath);
    if (!templates.isEmpty()) {
      QString defaultTemplate = Util::getFolderTemplate(folder, storePath);
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
  bool ok;
  QString dir =
      Util::getDir(ui->treeView->currentIndex(), true, model, proxyModel);
  QString file =
      QInputDialog::getText(this, tr("New file"),
                            tr("New password file: \n(Will be placed in %1 )")
                                .arg(QtPassSettings::getPassStore() +
                                     Util::getDir(ui->treeView->currentIndex(),
                                                  true, model, proxyModel)),
                            QLineEdit::Normal, "", &ok);
  if (!ok || file.isEmpty()) {
    return;
  }
  file = dir + file;
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
    file = Util::getDir(ui->treeView->currentIndex(), true, model, proxyModel);
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
 * @brief MainWindow::onOTP try and generate (selected) OTP code.
 */
void MainWindow::onOtp() {
  QString file = getFile(ui->treeView->currentIndex(), true);
  if (!file.isEmpty()) {
    if (QtPassSettings::isUseOtp()) {
      setUiElementsEnabled(false);
      QtPassSettings::getPass()->OtpGenerate(file);
    }
  } else {
    flashText(tr("No password selected for OTP generation"), true);
  }
}

/**
 * @brief MainWindow::onEdit try and edit (selected) password.
 */
void MainWindow::onEdit() {
  QString file = getFile(ui->treeView->currentIndex(), true);
  editPassword(file);
}

/**
 * @brief MainWindow::userDialog see MainWindow::onUsers()
 * @param dir folder to edit users for.
 */
void MainWindow::userDialog(const QString &dir) {
  if (!dir.isEmpty()) {
    currentDir = dir;
  }
  onUsers();
}

/**
 * @brief MainWindow::onUsers edit users for the current
 * folder,
 * gets lists and opens UserDialog.
 */
void MainWindow::onUsers() {
  QString dir =
      currentDir.isEmpty()
          ? Util::getDir(ui->treeView->currentIndex(), false, model, proxyModel)
          : currentDir;

  UsersDialog d(dir, this);
  if (!d.exec()) {
    ui->treeView->setFocus();
  }
}

/**
 * @brief MainWindow::messageAvailable we have some text/message/search to do.
 * @param message
 */
void MainWindow::messageAvailable(const QString &message) {
  if (message.isEmpty()) {
    focusInput();
  } else {
    ui->treeView->expandAll();
    ui->lineEdit->setText(message);
    on_lineEdit_returnPressed();
  }
  show();
  raise();
}

/**
 * @brief MainWindow::generateKeyPair internal gpg keypair generator . .
 * @param batch
 * @param keygenWindow
 */
void MainWindow::generateKeyPair(const QString &batch, QDialog *keygenWindow) {
  keygenDialog = keygenWindow;
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
 * @brief MainWindow::on_profileBox_currentIndexChanged make sure we show the
 * correct "profile"
 * @param name
 */
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void MainWindow::on_profileBox_currentIndexChanged(QString name) {
#else
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
#endif
  if (m_qtPass->isFreshStart() || name == QtPassSettings::getProfile()) {
    return;
  }

  ui->lineEdit->clear();

  QtPassSettings::setProfile(name);

  QtPassSettings::setPassStore(
      QtPassSettings::getProfiles().value(name).value("path"));
  QtPassSettings::setPassSigningKey(
      QtPassSettings::getProfiles().value(name).value("signingKey"));
  ui->statusBar->showMessage(tr("Profile changed to %1").arg(name), 2000);

  QtPassSettings::getPass()->updateEnv();

  const QString passStore = QtPassSettings::getPassStore();
  proxyModel.setStore(passStore);
  ui->treeView->setRootIndex(
      proxyModel.mapFromSource(model.setRootPath(passStore)));
  deselect();
  ui->treeView->setCurrentIndex(QModelIndex());
}

/**
 * @brief MainWindow::initTrayIcon show a nice tray icon on systems that
 * support
 * it
 */
void MainWindow::initTrayIcon() {
  this->tray = new TrayIcon(this);
  // Setup tray icon

  if (tray == nullptr) {
#ifdef QT_DEBUG
    dbg() << "Allocating tray icon failed.";
#endif
    return;
  }

  if (!tray->getIsAllocated()) {
    destroyTrayIcon();
  }
}

/**
 * @brief MainWindow::destroyTrayIcon remove that pesky tray icon
 */
void MainWindow::destroyTrayIcon() {
  delete this->tray;
  tray = nullptr;
}

/**
 * @brief MainWindow::closeEvent hide or quit
 * @param event
 */
void MainWindow::closeEvent(QCloseEvent *event) {
  if (QtPassSettings::isHideOnClose()) {
    this->hide();
    event->ignore();
  } else {
    m_qtPass->clearClipboard();

    QtPassSettings::setGeometry(saveGeometry());
    QtPassSettings::setSavestate(saveState());
    QtPassSettings::setMaximized(isMaximized());
    if (!isMaximized()) {
      QtPassSettings::setPos(pos());
      QtPassSettings::setSize(size());
    }
    event->accept();
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
  if (obj == ui->lineEdit && event->type() == QEvent::KeyPress) {
    auto *key = dynamic_cast<QKeyEvent *>(event);
    if (key != nullptr && key->key() == Qt::Key_Down) {
      ui->treeView->setFocus();
    }
  }
  return QObject::eventFilter(obj, event);
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
  QModelIndex index = ui->treeView->indexAt(pos);
  bool selected = true;
  if (!index.isValid()) {
    ui->treeView->clearSelection();
    ui->actionDelete->setEnabled(false);
    ui->actionEdit->setEnabled(false);
    currentDir = "";
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
      QString dirPath = QDir::cleanPath(
          Util::getDir(ui->treeView->currentIndex(), false, model, proxyModel));

      QMenu *shareMenu = new QMenu(tr("Share"), &contextMenu);
      contextMenu.addMenu(shareMenu);

      QString gpgIdPath = Pass::getGpgIdPath(dirPath);
      bool gpgIdExists = !gpgIdPath.isEmpty() && QFile(gpgIdPath).exists();

      QString exePath = QtPassSettings::isUsePass()
                            ? QtPassSettings::getPassExecutable()
                            : QtPassSettings::getGpgExecutable();
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
  QPoint globalPos = ui->textBrowser->viewport()->mapToGlobal(pos);

  contextMenu->exec(globalPos);
  delete contextMenu;
}

/**
 * @brief MainWindow::openFolder open the folder in the default file manager
 */
void MainWindow::openFolder() {
  QString dir =
      Util::getDir(ui->treeView->currentIndex(), false, model, proxyModel);

  QString path = QDir::toNativeSeparators(dir);
  QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

/**
 * @brief MainWindow::addFolder add a new folder to store passwords in
 */
void MainWindow::addFolder() {
  bool ok;
  QString dir =
      Util::getDir(ui->treeView->currentIndex(), false, model, proxyModel);
  QString newdir =
      QInputDialog::getText(this, tr("New file"),
                            tr("New Folder: \n(Will be placed in %1 )")
                                .arg(QtPassSettings::getPassStore() +
                                     Util::getDir(ui->treeView->currentIndex(),
                                                  true, model, proxyModel)),
                            QLineEdit::Normal, "", &ok);
  if (!ok || newdir.isEmpty()) {
    return;
  }
  newdir.prepend(dir);
  if (!QDir().mkdir(newdir)) {
    QMessageBox::warning(this, tr("Error"),
                         tr("Failed to create folder: %1").arg(newdir));
    return;
  }
  if (QtPassSettings::isAddGPGId(true)) {
    QString gpgIdFile = newdir + "/.gpg-id";
    QFile gpgId(gpgIdFile);
    if (!gpgId.open(QIODevice::WriteOnly)) {
      QMessageBox::warning(
          this, tr("Error"),
          tr("Failed to create .gpg-id file in: %1").arg(newdir));
      return;
    }
    QList<UserInfo> users = QtPassSettings::getPass()->listKeys("", true);
    for (const UserInfo &user : users) {
      if (user.enabled) {
        gpgId.write((user.key_id + "\n").toUtf8());
      }
    }
    gpgId.close();
  }
}

/**
 * @brief MainWindow::renameFolder rename an existing folder
 */
void MainWindow::renameFolder() {
  bool ok;
  QString srcDir = QDir::cleanPath(
      Util::getDir(ui->treeView->currentIndex(), false, model, proxyModel));
  QString srcDirName = QDir(srcDir).dirName();
  QString newName =
      QInputDialog::getText(this, tr("Rename file"), tr("Rename Folder To: "),
                            QLineEdit::Normal, srcDirName, &ok);
  if (!ok || newName.isEmpty()) {
    return;
  }
  QString destDir = srcDir;
  destDir.replace(srcDir.lastIndexOf(srcDirName), srcDirName.length(), newName);
  QtPassSettings::getPass()->Move(srcDir, destDir);
}

/**
 * @brief MainWindow::editPassword read password and open edit window via
 * MainWindow::onEdit()
 */
void MainWindow::editPassword(const QString &file) {
  if (!file.isEmpty()) {
    if (QtPassSettings::isUseGit() && QtPassSettings::isAutoPull()) {
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
  QtPassSettings::getPass()->Move(file, newFile);
}

/**
 * @brief MainWindow::clearTemplateWidgets empty the template widget fields in
 * the UI
 */
void MainWindow::clearTemplateWidgets() {
  while (ui->gridLayout->count() > 0) {
    QLayoutItem *item = ui->gridLayout->takeAt(0);
    delete item->widget();
    delete item;
  }
  ui->verticalLayoutPassword->setSpacing(0);
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
    QString file = getFile(ui->treeView->currentIndex(), true);
    // Disconnect any previous connection to avoid accumulation
    disconnect(QtPassSettings::getPass(), &Pass::finishedShow, this,
               &MainWindow::passwordFromFileToClipboard);
    connect(QtPassSettings::getPass(), &Pass::finishedShow, this,
            &MainWindow::passwordFromFileToClipboard);
    QtPassSettings::getPass()->Show(file);
  }
}

void MainWindow::passwordFromFileToClipboard(const QString &text) {
  QStringList tokens = text.split('\n');
  m_qtPass->copyTextToClipboard(tokens[0]);
}

/**
 * @brief MainWindow::addToGridLayout add a field to the template grid
 * @param position
 * @param field
 * @param value
 */
void MainWindow::addToGridLayout(int position, const QString &field,
                                 const QString &value) {
  QString trimmedField = field.trimmed();
  QString trimmedValue = value.trimmed();

  const QString buttonStyle =
      "border-style: none; background: transparent; padding: 0; margin: 0; "
      "icon-size: 16px; color: inherit;";

  // Combine the Copy button and the line edit in one widget
  auto *frame = new QFrame();
  QLayout *ly = new QHBoxLayout();
  ly->setContentsMargins(5, 2, 2, 2);
  ly->setSpacing(0);
  frame->setLayout(ly);
  if (QtPassSettings::getClipBoardType() != Enums::CLIPBOARD_NEVER) {
    auto *fieldLabel = new QPushButtonWithClipboard(trimmedValue, this);
    connect(fieldLabel, &QPushButtonWithClipboard::clicked, m_qtPass,
            &QtPass::copyTextToClipboard);

    fieldLabel->setStyleSheet(buttonStyle);
    frame->layout()->addWidget(fieldLabel);
  }

  if (QtPassSettings::isUseQrencode()) {
    auto *qrbutton = new QPushButtonAsQRCode(trimmedValue, this);
    connect(qrbutton, &QPushButtonAsQRCode::clicked, m_qtPass,
            &QtPass::showTextAsQRCode);
    qrbutton->setStyleSheet(buttonStyle);
    frame->layout()->addWidget(qrbutton);
  }

  // set the echo mode to password, if the field is "password"
  const QString lineStyle =
      QtPassSettings::isUseMonospace()
          ? "border-style: none; background: transparent; font-family: "
            "monospace;"
          : "border-style: none; background: transparent;";

  if (QtPassSettings::isHidePassword() && trimmedField == tr("Password")) {
    auto *line = new QLineEdit();
    line->setObjectName(trimmedField);
    line->setText(trimmedValue);
    line->setReadOnly(true);
    line->setStyleSheet(lineStyle);
    line->setContentsMargins(0, 0, 0, 0);
    line->setEchoMode(QLineEdit::Password);
    auto *showButton = new QPushButtonShowPassword(line, this);
    showButton->setStyleSheet(buttonStyle);
    showButton->setContentsMargins(0, 0, 0, 0);
    frame->layout()->addWidget(showButton);
    frame->layout()->addWidget(line);
  } else {
    auto *line = new QTextBrowser();
    line->setOpenExternalLinks(true);
    line->setOpenLinks(true);
    line->setMaximumHeight(26);
    line->setMinimumHeight(26);
    line->setSizePolicy(
        QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum));
    line->setObjectName(trimmedField);
    trimmedValue.replace(Util::protocolRegex(), R"(<a href="\1">\1</a>)");
    line->setText(trimmedValue);
    line->setReadOnly(true);
    line->setStyleSheet(lineStyle);
    line->setContentsMargins(0, 0, 0, 0);
    frame->layout()->addWidget(line);
  }

  frame->setStyleSheet(
      ".QFrame{border: 1px solid lightgrey; border-radius: 5px;}");

  // set into the layout
  ui->gridLayout->addWidget(new QLabel(trimmedField), position, 0);
  ui->gridLayout->addWidget(frame, position, 1);
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

  // Prevent double execution - use same method as startReencryptPath
  setUiElementsEnabled(false);
  ui->treeView->setDisabled(true);

  QtPassSettings::getImitatePass()->reencryptPath(
      QDir::cleanPath(QDir(dir).absolutePath()));
}

/**
 * @brief MainWindow::startReencryptPath disable ui elements and treeview
 */
void MainWindow::startReencryptPath() {
  setUiElementsEnabled(false);
  ui->treeView->setDisabled(true);
}

/**
 * @brief MainWindow::endReencryptPath re-enable ui elements
 */
void MainWindow::endReencryptPath() { setUiElementsEnabled(true); }

/**
 * @brief MainWindow::exportPublicKey export the configured signing key in
 *        ASCII-armored form via gpg and show it in ExportPublicKeyDialog.
 *
 * Falls back to a help dialog when no signing key is configured or gpg is
 * unavailable, so the user still gets actionable guidance.
 */
void MainWindow::exportPublicKey() {
  QString identity = QtPassSettings::getPassSigningKey();
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
  QString gpgExe = QtPassSettings::getGpgExecutable();
  if (gpgExe.isEmpty()) {
    gpgExe = QStringLiteral("gpg");
  }
  QStringList args = {"--armor", "--export"};
  args.append(identity.split(' ', Qt::SkipEmptyParts));
  QString stdOut;
  QString stdErr;
  int exitCode =
      Executor::executeBlocking(gpgExe, args, QString(), &stdOut, &stdErr);
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
  UsersDialog d(dir, this);
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
         "<li><b>Import teammates' public keys</b> to their own folders</li>"
         "<li><b>Re-encrypt passwords</b> so all recipients can decrypt "
         "them</li>"
         "</ol>"
         "<p>Only people who have a matching secret key can decrypt the "
         "passwords.</p>"
         "<p><b>Tip:</b> Use the same GPG key for all shared folders.</p>"
         "<p>See the FAQ for more details.</p>"));
}

void MainWindow::updateGitButtonVisibility() {
  if (!QtPassSettings::isUseGit() ||
      (QtPassSettings::getGitExecutable().isEmpty() &&
       QtPassSettings::getPassExecutable().isEmpty())) {
    enableGitButtons(false);
  } else {
    enableGitButtons(true);
  }
}

void MainWindow::updateOtpButtonVisibility() {
#if defined(Q_OS_WIN) || defined(__APPLE__)
  ui->actionOtp->setVisible(false);
#endif
  if (!QtPassSettings::isUseOtp()) {
    ui->actionOtp->setEnabled(false);
  } else {
    ui->actionOtp->setEnabled(true);
  }
}

void MainWindow::updateGrepButtonVisibility() {
  const bool enabled = QtPassSettings::isUseGrepSearch();
  ui->grepButton->setVisible(enabled);
  ui->grepCaseButton->setVisible(enabled);
  if (!enabled && m_grepMode) {
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

void MainWindow::appendProcessOutput(const QString &output, bool isError,
                                     int pid) {
  if (!QtPassSettings::isShowProcessOutput()) {
    return;
  }

  QStringList lines = output.split('\n', Qt::SkipEmptyParts);
  for (QString &line : lines) {
    line = line.trimmed();
    if (line.isEmpty()) {
      continue;
    }

    m_outputCounter++;
    QString lineNumber = QString::number(m_outputCounter);

    QColor textColor =
        isError ? QColor(Qt::red)
                : ui->processOutputEdit->palette().color(QPalette::Text);
    QString colorHex = textColor.name();
    QString coloredOutput =
        QString("<span style=\"color: %1;\">%2: %3</span>")
            .arg(colorHex, lineNumber, line.toHtmlEscaped());

    ui->processOutputEdit->append(coloredOutput);
  }

  limitOutputLines();

  if (m_autoScroll) {
    ui->processOutputEdit->moveCursor(QTextCursor::End);
  }

  if (!m_processOutputShownForCurrentCommand) {
    ui->processOutputWidget->setVisible(true);
    m_processOutputShownForCurrentCommand = true;
  }
}

void MainWindow::onProcessOutput(const QString &output, bool isError, int pid) {
  QString cmdName = getProcessName(pid);
  appendProcessOutput(cmdName.isEmpty() ? output : cmdName + ": " + output,
                      isError);
}

auto MainWindow::getProcessName(int pid) -> QString {
  switch (pid) {
  case Enums::GIT_INIT:
    return "git init";
  case Enums::GIT_ADD:
    return "git add";
  case Enums::GIT_COMMIT:
    return "git commit";
  case Enums::GIT_RM:
    return "git rm";
  case Enums::GIT_PULL:
    return "git pull";
  case Enums::GIT_PUSH:
    return "git push";
  case Enums::PASS_INSERT:
    return "pass insert";
  case Enums::PASS_REMOVE:
    return "pass rm";
  case Enums::PASS_INIT:
    return "pass init";
  case Enums::PASS_MOVE:
    return "pass mv";
  case Enums::PASS_COPY:
    return "pass cp";
  case Enums::PASS_GREP:
    return "pass grep";
  case Enums::GPG_GENKEYS:
    return "gpg --gen-key";
  default:
    return QString();
  }
}

void MainWindow::updateProcessOutputVisibility() {
  ui->processOutputWidget->setVisible(QtPassSettings::isShowProcessOutput());
}

void MainWindow::limitOutputLines() {
  QTextDocument *doc = ui->processOutputEdit->document();
  int excess = doc->blockCount() - MaxOutputLines;
  if (excess <= 0) {
    return;
  }

  QTextCursor cursor(doc);
  cursor.movePosition(QTextCursor::Start);
  cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, excess);
  cursor.removeSelectedText();
}

void MainWindow::on_clearOutputButton_clicked() {
  ui->processOutputEdit->clear();
  m_outputCounter = 0;
  m_autoScroll = true;
  ui->processOutputEdit->moveCursor(QTextCursor::End);
}