#include "mainwindow.h"

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

#include "configdialog.h"
#include "filecontent.h"
#include "keygendialog.h"
#include "passworddialog.h"
#include "qpushbuttonwithclipboard.h"
#include "qpushbuttonasqrcode.h"
#include "qtpass.h"
#include "qtpasssettings.h"
#include "settingsconstants.h"
#include "trayicon.h"
#include "ui_mainwindow.h"
#include "usersdialog.h"
#include "util.h"
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QShortcut>
#include <QTimer>

/**
 * @brief MainWindow::MainWindow handles all of the main functionality and also
 * the main window.
 * @param searchText for searching from cli
 * @param parent pointer
 */
MainWindow::MainWindow(const QString &searchText, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), keygen(nullptr),
      tray(nullptr) {
#ifdef __APPLE__
  // extra treatment for mac os
  // see http://doc.qt.io/qt-5/qkeysequence.html#qt_set_sequence_auto_mnemonic
  qt_set_sequence_auto_mnemonic(true);
#endif
  ui->setupUi(this);

  m_qtPass = new QtPass();
  m_qtPass->setMainWindow(this);

  // register shortcut ctrl/cmd + Q to close the main window
  new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q), this, SLOT(close()));
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

  proxyModel.setSourceModel(&model);
  proxyModel.setModelAndStore(&model, passStore);
  // proxyModel.sort(0, Qt::AscendingOrder);
  selectionModel.reset(new QItemSelectionModel(&proxyModel));
  model.fetchMore(model.setRootPath(passStore));
  // model.sort(0, Qt::AscendingOrder);

  ui->treeView->setModel(&proxyModel);
  ui->treeView->setRootIndex(
      proxyModel.mapFromSource(model.setRootPath(passStore)));
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

  ui->textBrowser->setOpenExternalLinks(true);
  ui->textBrowser->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->textBrowser, &QWidget::customContextMenuRequested, this,
          &MainWindow::showBrowserContextMenu);

  updateProfileBox();

  QtPassSettings::getPass()->updateEnv();
  clearPanelTimer.setInterval(1000 *
                              QtPassSettings::getAutoclearPanelSeconds());
  clearPanelTimer.setSingleShot(true);
  connect(&clearPanelTimer, SIGNAL(timeout()), this, SLOT(clearPanel()));

  searchTimer.setInterval(350);
  searchTimer.setSingleShot(true);

  connect(&searchTimer, &QTimer::timeout, this, &MainWindow::onTimeoutSearch);

  initToolBarButtons();
  initStatusBar();

#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
  ui->lineEdit->setClearButtonEnabled(true);
#endif

  setUiElementsEnabled(true);

  qsrand(static_cast<uint>(QTime::currentTime().msec()));
  QTimer::singleShot(10, this, SLOT(focusInput()));

  ui->lineEdit->setText(searchText);
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
  QLabel *logoApp = new QLabel(statusBar());
  logoApp->setPixmap(logo);
  statusBar()->addPermanentWidget(logoApp);
}

const QModelIndex MainWindow::getCurrentTreeViewIndex() {
  return ui->treeView->currentIndex();
}

void MainWindow::cleanKeygenDialog() {
  this->keygen->close();
  this->keygen = nullptr;
}

void MainWindow::setTextTextBrowser(const QString &text) {
  ui->textBrowser->setText(text);
}

void MainWindow::flashText(const QString &text, const bool isError,
                           const bool isHtml) {
  if (isError)
    ui->textBrowser->setTextColor(Qt::red);

  if (isHtml) {
    QString _text = text;
    if (!ui->textBrowser->toPlainText().isEmpty())
      _text = ui->textBrowser->toHtml() + _text;
    ui->textBrowser->setHtml(_text);
  } else {
    ui->textBrowser->setText(text);
    ui->textBrowser->setTextColor(Qt::black);
  }
}

/**
 * @brief MainWindow::config pops up the configuration screen and handles all
 * inter-window communication
 */
void MainWindow::config() {
  QScopedPointer<ConfigDialog> d(new ConfigDialog(this));
  d->setModal(true);
  // Automatically default to pass if it's available
  if (m_qtPass->isFreshStart() &&
      QFile(QtPassSettings::getPassExecutable()).exists()) {
    QtPassSettings::setUsePass(true);
  }

  if (m_qtPass->isFreshStart())
    d->wizard(); // does shit
  if (d->exec()) {
    if (d->result() == QDialog::Accepted) {
      if (QtPassSettings::isAlwaysOnTop()) {
        Qt::WindowFlags flags = windowFlags();
        this->setWindowFlags(flags | Qt::WindowStaysOnTopHint);
      } else {
        this->setWindowFlags(Qt::Window);
      }
      this->show();

      updateProfileBox();
      ui->treeView->setRootIndex(proxyModel.mapFromSource(
          model.setRootPath(QtPassSettings::getPassStore())));

      if (m_qtPass->isFreshStart() && Util::checkConfig())
        config();
      QtPassSettings::getPass()->updateEnv();
      clearPanelTimer.setInterval(1000 *
                                  QtPassSettings::getAutoclearPanelSeconds());
      m_qtPass->setClipboardTimer();

      updateGitButtonVisibility();
      updateOtpButtonVisibility();
      if (QtPassSettings::isUseTrayIcon() && tray == nullptr)
        initTrayIcon();
      else if (!QtPassSettings::isUseTrayIcon() && tray != nullptr) {
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
  if (block)
    QtPassSettings::getPass()->GitPull_b();
  else
    QtPassSettings::getPass()->GitPull();
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
QString MainWindow::getFile(const QModelIndex &index, bool forPass) {
  if (!index.isValid() ||
      !model.fileInfo(proxyModel.mapToSource(index)).isFile())
    return QString();
  QString filePath = model.filePath(proxyModel.mapToSource(index));
  if (forPass) {
    filePath = QDir(QtPassSettings::getPassStore()).relativeFilePath(filePath);
    filePath.replace(QRegExp("\\.gpg$"), "");
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
  //    TODO(bezet): "Could not decrypt";
  m_qtPass->clearClippedText();
  QString file = getFile(index, true);
  ui->passwordName->setText(getFile(index, true));
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
  ui->actionDelete->setEnabled(false);
  ui->actionEdit->setEnabled(false);
  clearPanel(false);
}

void MainWindow::executeWrapperStarted() {
  clearTemplateWidgets();
  ui->textBrowser->clear();
  setUiElementsEnabled(false);
  clearPanelTimer.stop();
}

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
  } else {
    if (!password.isEmpty()) {
      // set the password, it is hidden if needed in addToGridLayout
      addToGridLayout(0, tr("Password"), password);
    }

    NamedValues namedValues = fileContent.getNamedValues();
    for (int j = 0; j < namedValues.length(); ++j) {
      NamedValue nv = namedValues.at(j);
      addToGridLayout(j + 1, nv.name, nv.value);
    }
    if (ui->gridLayout->count() == 0)
      ui->verticalLayoutPassword->setSpacing(0);
    else
      ui->verticalLayoutPassword->setSpacing(6);
    output = fileContent.getRemainingData();
  }

  if (QtPassSettings::isUseAutoclearPanel()) {
    clearPanelTimer.start();
  }

  setUiElementsEnabled(true);
}

void MainWindow::passOtpHandler(const QString &p_output) {
  if (!p_output.isEmpty()) {
    addToGridLayout(ui->gridLayout->count() + 1, tr("OTP Code"), p_output);
    m_qtPass->copyTextToClipboard(p_output);
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
    if (m_qtPass->isFreshStart() && QtPassSettings::isStartMinimized()) {
      // since we are still in constructor, can't directly hide
      QTimer::singleShot(10, this, SLOT(hide()));
    }
  } /*else if (!QtPassSettings::isUseTrayIcon() && tray != NULL) {
    destroyTrayIcon();
  }*/
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
  ui->statusBar->showMessage(tr("Looking for: %1").arg(arg1), 1000);
  ui->treeView->expandAll();

  searchTimer.start();
}

/**
 * @brief MainWindow::onTimeoutSearch Fired when search is finished or too much
 * time from two keypresses is elapsed
 */
void MainWindow::onTimeoutSearch() {
  QString query = ui->lineEdit->text();

  if (query.isEmpty())
    ui->treeView->collapseAll();

  query.replace(QRegExp(" "), ".*");
  QRegExp regExp(query, Qt::CaseInsensitive);
  proxyModel.setFilterRegExp(regExp);
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

  if (proxyModel.rowCount() > 0) {
    selectFirstFile();
    on_treeView_clicked(ui->treeView->currentIndex());
  }
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
QModelIndex MainWindow::firstFile(QModelIndex parentIndex) {
  QModelIndex index = parentIndex;
  int numRows = proxyModel.rowCount(parentIndex);
  for (int row = 0; row < numRows; ++row) {
    index = proxyModel.index(row, 0, parentIndex);
    if (model.fileInfo(proxyModel.mapToSource(index)).isFile())
      return index;
    if (proxyModel.hasChildren(index))
      return firstFile(index);
  }
  return index;
}

/**
 * @brief MainWindow::setPassword open passworddialog
 * @param file which pgp file
 * @param isNew insert (not update)
 */
void MainWindow::setPassword(QString file, bool isNew) {
  PasswordDialog d(file, isNew, this);

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
  if (!ok || file.isEmpty())
    return;
  file = dir + file;
  setPassword(file);
}

/**
 * @brief MainWindow::onDelete remove password, if you are
 * sure.
 */
void MainWindow::onDelete() {
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
          tr("Are you sure you want to delete %1%2")
              .arg(QDir::separator() + file)
              .arg(isDir ? dirMessage : "?"),
          QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    return;

  QtPassSettings::getPass()->Remove(file, isDir);
}

/**
 * @brief MainWindow::onOTP try and generate (selected) OTP code.
 */
void MainWindow::onOtp() {
  QString file = getFile(ui->treeView->currentIndex(), true);
  if (!file.isEmpty()) {
    if (QtPassSettings::isUseOtp())
      QtPassSettings::getPass()->OtpGenerate(file);
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
void MainWindow::userDialog(QString dir) {
  if (!dir.isEmpty())
    currentDir = dir;
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
  d.exec();
}

/**
 * @brief MainWindow::messageAvailable we have some text/message/search to do.
 * @param message
 */
void MainWindow::messageAvailable(QString message) {
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
void MainWindow::generateKeyPair(QString batch, QDialog *keygenWindow) {
  keygen = keygenWindow;
  emit generateGPGKeyPair(batch);
}

/**
 * @brief MainWindow::updateProfileBox update the list of profiles, optionally
 * select a more appropriate one to view too
 */
void MainWindow::updateProfileBox() {
  QHash<QString, QString> profiles = QtPassSettings::getProfiles();

  if (profiles.isEmpty()) {
    ui->profileWidget->hide();
  } else {
    ui->profileWidget->show();
    ui->profileBox->setEnabled(profiles.size() > 1);
    ui->profileBox->clear();
    QHashIterator<QString, QString> i(profiles);
    while (i.hasNext()) {
      i.next();
      if (!i.key().isEmpty())
        ui->profileBox->addItem(i.key());
    }
  }
  int index = ui->profileBox->findText(QtPassSettings::getProfile());
  if (index != -1) // -1 for not found
    ui->profileBox->setCurrentIndex(index);
}

/**
 * @brief MainWindow::on_profileBox_currentIndexChanged make sure we show the
 * correct "profile"
 * @param name
 */
void MainWindow::on_profileBox_currentIndexChanged(QString name) {
  if (m_qtPass->isFreshStart() || name == QtPassSettings::getProfile())
    return;
  QtPassSettings::setProfile(name);

  QtPassSettings::setPassStore(QtPassSettings::getProfiles()[name]);
  ui->statusBar->showMessage(tr("Profile changed to %1").arg(name), 2000);

  QtPassSettings::getPass()->updateEnv();

  ui->treeView->selectionModel()->clear();
  ui->treeView->setRootIndex(proxyModel.mapFromSource(
      model.setRootPath(QtPassSettings::getPassStore())));

  ui->actionEdit->setEnabled(false);
  ui->actionDelete->setEnabled(false);
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
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
  if (obj == ui->lineEdit && event->type() == QEvent::KeyPress) {
    QKeyEvent *key = static_cast<QKeyEvent *>(event);
    if (key->key() == Qt::Key_Down) {
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
    if (proxyModel.rowCount() > 0)
      on_treeView_clicked(ui->treeView->currentIndex());
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
    // if (useClipboard != CLIPBOARD_NEVER) {
    // contextMenu.addSeparator();
    // QAction* copyItem = contextMenu.addAction(tr("Copy Password"));
    // if (getClippedPassword().length() == 0) copyItem->setEnabled(false);
    // connect(copyItem, SIGNAL(triggered()), this,
    // SLOT(copyPasswordToClipboard()));
    // }
    contextMenu.addSeparator();
    QAction *deleteItem = contextMenu.addAction(tr("Delete"));
    connect(deleteItem, &QAction::triggered, this, &MainWindow::onDelete);
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
  if (!ok || newdir.isEmpty())
    return;
  newdir.prepend(dir);
  // dbg()<< newdir;
  QDir().mkdir(newdir);
}

/**
 * @brief MainWindow::editPassword read password and open edit window via
 * MainWindow::onEdit()
 */
void MainWindow::editPassword(const QString &file) {
  if (!file.isEmpty()) {
    if (QtPassSettings::isUseGit() && QtPassSettings::isAutoPull())
      onUpdate(true);
    setPassword(file, false);
  }
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

void MainWindow::copyPasswordFromTreeview() {
  QFileInfo fileOrFolder =
      model.fileInfo(proxyModel.mapToSource(ui->treeView->currentIndex()));

  if (fileOrFolder.isFile()) {
    QString file = getFile(ui->treeView->currentIndex(), true);
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

  // Combine the Copy button and the line edit in one widget
  QFrame *frame = new QFrame();
  QLayout *ly = new QHBoxLayout();
  ly->setContentsMargins(5, 2, 2, 2);
  frame->setLayout(ly);
  if (QtPassSettings::getClipBoardType() != Enums::CLIPBOARD_NEVER) {
    QPushButtonWithClipboard *fieldLabel =
        new QPushButtonWithClipboard(trimmedValue, this);
    connect(fieldLabel, &QPushButtonWithClipboard::clicked, m_qtPass,
            &QtPass::copyTextToClipboard);

    fieldLabel->setStyleSheet("border-style: none ; background: transparent;");
    // fieldLabel->setContentsMargins(0,5,5,0);
    frame->layout()->addWidget(fieldLabel);
  }

  if (QtPassSettings::isUseQrencode()) {
    QPushButtonAsQRCode *qrbutton =
        new QPushButtonAsQRCode(trimmedValue, this);
    connect(qrbutton, &QPushButtonAsQRCode::clicked, m_qtPass,
            &QtPass::showTextAsQRCode);
    qrbutton->setStyleSheet("border-style: none ; background: transparent;");

    frame->layout()->addWidget(qrbutton);
  }

  // set the echo mode to password, if the field is "password"
  if (QtPassSettings::isHidePassword() && trimmedField == tr("Password")) {
    QLineEdit *line = new QLineEdit();
    line->setObjectName(trimmedField);
    line->setText(trimmedValue);
    line->setReadOnly(true);
    line->setStyleSheet("border-style: none ; background: transparent;");
    line->setContentsMargins(0, 0, 0, 0);
    line->setEchoMode(QLineEdit::Password);
    frame->layout()->addWidget(line);
  } else {
    QTextBrowser *line = new QTextBrowser();
    line->setOpenExternalLinks(true);
    line->setOpenLinks(true);
    line->setMaximumHeight(26);
    line->setMinimumHeight(26);
    line->setSizePolicy(
        QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum));
    line->setObjectName(trimmedField);
    trimmedValue.replace(
        QRegExp("((?:https?|ftp|ssh|sftp|ftps|webdav|webdavs)://\\S+)"),
        "<a href=\"\\1\">\\1</a>");
    line->setText(trimmedValue);
    line->setReadOnly(true);
    line->setStyleSheet("border-style: none ; background: transparent;");
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
void MainWindow::showStatusMessage(QString msg, int timeout) {
  ui->statusBar->showMessage(msg, timeout);
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
  if (!QtPassSettings::isUseOtp())
    ui->actionOtp->setEnabled(false);
  else
    ui->actionOtp->setEnabled(true);
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
void MainWindow::critical(QString title, QString msg) {
  QMessageBox::critical(this, title, msg);
}
