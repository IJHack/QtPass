#include "mainwindow.h"
#include <QClipboard>
#include <QCloseEvent>
#include <QDebug>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QQueue>
#include <QTextCodec>
#include <QTimer>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN /*_KILLING_MACHINE*/
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <winnetwk.h>
#undef DELETE
#endif
#include "configdialog.h"
#include "keygendialog.h"
#include "passworddialog.h"
#include "qpushbuttonwithclipboard.h"
#include "qtpasssettings.h"
#include "settingsconstants.h"
#include "ui_mainwindow.h"
#include "usersdialog.h"
#include "util.h"

/**
 * @brief MainWindow::MainWindow handles all of the main functionality and also
 * the main window.
 * @param parent
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), fusedav(this), keygen(NULL),
      tray(NULL), pass(nullptr) {
  // connect(process.data(), SIGNAL(readyReadStandardOutput()), this,
  // SLOT(readyRead()));

  //    TODO(bezet): this should be reconnected dynamically when pass changes
  connect(&rpass, SIGNAL(error(QProcess::ProcessError)), this,
          SLOT(processError(QProcess::ProcessError)));
  connect(&rpass, SIGNAL(finished(int, QProcess::ExitStatus)), this,
          SLOT(processFinished(int, QProcess::ExitStatus)));
  connect(&rpass, SIGNAL(startingExecuteWrapper()), this,
          SLOT(executeWrapperStarted()));
  connect(&rpass, SIGNAL(statusMsg(QString, int)), this,
          SLOT(showStatusMessage(QString, int)));
  connect(&rpass, SIGNAL(critical(QString, QString)), this,
          SLOT(critical(QString, QString)));

  connect(&ipass, SIGNAL(error(QProcess::ProcessError)), this,
          SLOT(processError(QProcess::ProcessError)));
  connect(&ipass, SIGNAL(finished(int, QProcess::ExitStatus)), this,
          SLOT(processFinished(int, QProcess::ExitStatus)));
  connect(&ipass, SIGNAL(startingExecuteWrapper()), this,
          SLOT(executeWrapperStarted()));
  connect(&ipass, SIGNAL(statusMsg(QString, int)), this,
          SLOT(showStatusMessage(QString, int)));
  connect(&ipass, SIGNAL(critical(QString, QString)), this,
          SLOT(critical(QString, QString)));
  //    only for ipass
  connect(&ipass, SIGNAL(startReencryptPath()), this,
          SLOT(startReencryptPath()));
  connect(&ipass, SIGNAL(endReencryptPath()), this, SLOT(endReencryptPath()));
  connect(&ipass, SIGNAL(lastDecrypt(QString)), this,
          SLOT(setLastDecrypt(QString)));

  ui->setupUi(this);
  enableUiElements(true);
  execQueue = new QQueue<execQueueItem>;
  ui->statusBar->showMessage(tr("Welcome to QtPass %1").arg(VERSION), 2000);
  freshStart = true;
  startupPhase = true;
  clearPanelTimer.setSingleShot(true);
  connect(&clearPanelTimer, SIGNAL(timeout()), this, SLOT(clearPanel()));
  clearClipboardTimer.setSingleShot(true);
  connect(&clearClipboardTimer, SIGNAL(timeout()), this,
          SLOT(clearClipboard()));
  pwdConfig.selected = passwordConfiguration::ALLCHARS;
  if (!checkConfig()) {
    // no working config
    QApplication::quit();
  }
  clippedText = "";
  QtPass = NULL;
  QTimer::singleShot(10, this, SLOT(focusInput()));

  // Add a Actions to the Add-Button
  QIcon addFileIcon = QIcon::fromTheme("file_new");
  QIcon addFolderIcon = QIcon::fromTheme("folder_new");
  actionAddPassword = new QAction(addFileIcon, tr("Add Password"), this);
  actionAddFolder = new QAction(addFolderIcon, tr("Add Folder"), this);

  ui->addButton->addAction(actionAddPassword);
  ui->addButton->addAction(actionAddFolder);

  connect(actionAddPassword, SIGNAL(triggered()), this,
          SLOT(on_addButton_clicked()));
  connect(actionAddFolder, SIGNAL(triggered()), this, SLOT(addFolder()));
  qsrand(static_cast<uint>(QTime::currentTime().msec()));

#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
  ui->lineEdit->setClearButtonEnabled(true);
#endif
}

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
 * @brief MainWindow::~MainWindow destroy!
 */
MainWindow::~MainWindow() {
#ifdef Q_OS_WIN
  if (QtPassSettings::isUseWebDav())
    WNetCancelConnection2A(QtPassSettings::getPassStore().toUtf8().constData(),
                           0, 1);
#else
  if (fusedav.state() == QProcess::Running) {
    fusedav.terminate();
    fusedav.waitForFinished(2000);
  }
#endif
}

/**
 * @brief MainWindow::changeEvent sets focus to the search box
 * @param event
 */
void MainWindow::changeEvent(QEvent *event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::ActivationChange) {
    if (this->isActiveWindow()) {
      ui->lineEdit->selectAll();
      ui->lineEdit->setFocus();
    }
  }
}

/**
 * @brief MainWindow::mountWebDav is some scary voodoo magic
 */
void MainWindow::mountWebDav() {
#ifdef Q_OS_WIN
  char dst[20] = {0};
  NETRESOURCEA netres;
  memset(&netres, 0, sizeof(netres));
  netres.dwType = RESOURCETYPE_DISK;
  netres.lpLocalName = 0;
  netres.lpRemoteName = QtPassSettings::getWebDavUrl().toUtf8().data();
  DWORD size = sizeof(dst);
  DWORD r = WNetUseConnectionA(
      reinterpret_cast<HWND>(effectiveWinId()), &netres,
      QtPassSettings::getWebDavPassword().toUtf8().constData(),
      QtPassSettings::getWebDavUser().toUtf8().constData(),
      CONNECT_TEMPORARY | CONNECT_INTERACTIVE | CONNECT_REDIRECT, dst, &size,
      0);
  if (r == NO_ERROR) {
    QtPassSettings::setPassStore(dst);
  } else {
    char message[256] = {0};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, r, 0, message,
                   sizeof(message), 0);
    ui->textBrowser->setTextColor(Qt::red);
    ui->textBrowser->setText(tr("Failed to connect WebDAV:\n") + message +
                             " (0x" + QString::number(r, 16) + ")");
  }
#else
  fusedav.start("fusedav -o nonempty -u \"" + QtPassSettings::getWebDavUser() +
                "\" " + QtPassSettings::getWebDavUrl() + " \"" +
                QtPassSettings::getPassStore() + '"');
  fusedav.waitForStarted();
  if (fusedav.state() == QProcess::Running) {
    QString pwd = QtPassSettings::getWebDavPassword();
    bool ok = true;
    if (pwd.isEmpty()) {
      pwd = QInputDialog::getText(this, tr("QtPass WebDAV password"),
                                  tr("Enter password to connect to WebDAV:"),
                                  QLineEdit::Password, "", &ok);
    }
    if (ok && !pwd.isEmpty()) {
      fusedav.write(pwd.toUtf8() + '\n');
      fusedav.closeWriteChannel();
      fusedav.waitForFinished(2000);
    } else {
      fusedav.terminate();
    }
  }
  QString error = fusedav.readAllStandardError();
  int prompt = error.indexOf("Password:");
  if (prompt >= 0)
    error.remove(0, prompt + 10);
  if (fusedav.state() != QProcess::Running)
    error = tr("fusedav exited unexpectedly\n") + error;
  if (error.size() > 0) {
    ui->textBrowser->setTextColor(Qt::red);
    ui->textBrowser->setText(
        tr("Failed to start fusedav to connect WebDAV:\n") + error);
  }
#endif
}

/**
 * @brief MainWindow::checkConfig make sure we are ready to go as soon as
 * possible
 */
bool MainWindow::checkConfig() {
  QString version = QtPassSettings::getVersion();

  if (freshStart) {
    QByteArray geometry = QtPassSettings::getGeometry(saveGeometry());
    restoreGeometry(geometry);
    QByteArray savestate = QtPassSettings::getSavestate(saveState());
    restoreState(savestate);
    QPoint position = QtPassSettings::getPos(pos());
    move(position);
    QSize newSize = QtPassSettings::getSize(size());
    resize(newSize);
    QList<int> splitter = ui->splitter->sizes();
    int left = QtPassSettings::getSplitterLeft(splitter[0]);
    int right = QtPassSettings::getSplitterRight(splitter[1]);
    if (left > 0 || right > 0) {
      splitter[0] = left;
      splitter[1] = right;
      ui->splitter->setSizes(splitter);
    }
    if (QtPassSettings::isMaximized(isMaximized())) {
      showMaximized();
    }
  }

  QString passStore = QtPassSettings::getPassStore(Util::findPasswordStore());
  QtPassSettings::setPassStore(passStore);

  QString passExecutable =
      QtPassSettings::getPassExecutable(Util::findBinaryInPath("pass"));
  QtPassSettings::setPassExecutable(passExecutable);

  QString gitExecutable =
      QtPassSettings::getGitExecutable(Util::findBinaryInPath("git"));
  QtPassSettings::setGitExecutable(gitExecutable);

  QString gpgExecutable =
      QtPassSettings::getGpgExecutable(Util::findBinaryInPath("gpg2"));
  QtPassSettings::setGpgExecutable(gpgExecutable);

  QString pwgenExecutable =
      QtPassSettings::getPwgenExecutable(Util::findBinaryInPath("pwgen"));
  QtPassSettings::setPwgenExecutable(pwgenExecutable);

  pwdConfig.length = QtPassSettings::getPasswordLength();
  pwdConfig.selected = static_cast<passwordConfiguration::characterSet>(
      QtPassSettings::getPasswordCharsselection());
  pwdConfig.Characters[passwordConfiguration::CUSTOM] =
      QtPassSettings::getPasswordChars();

  if (QtPassSettings::isAlwaysOnTop()) {
    Qt::WindowFlags flags = windowFlags();
    this->setWindowFlags(flags | Qt::WindowStaysOnTopHint);
    this->show();
  }

  if (QtPassSettings::isUseTrayIcon() && tray == NULL) {
    initTrayIcon();
    if (freshStart && QtPassSettings::isStartMinimized()) {
      // since we are still in constructor, can't directly hide
      QTimer::singleShot(10, this, SLOT(hide()));
    }
  } else if (!QtPassSettings::isUseTrayIcon() && tray != NULL) {
    destroyTrayIcon();
  }

  // qDebug() << version;

  // Config updates
  if (version.isEmpty()) {
    qDebug() << "assuming fresh install";
    if (QtPassSettings::getAutoclearSeconds() < 5)
      QtPassSettings::setAutoclearSeconds(10);
    if (QtPassSettings::getAutoclearPanelSeconds() < 5)
      QtPassSettings::setAutoclearPanelSeconds(10);
    if (!QtPassSettings::getPwgenExecutable().isEmpty())
      QtPassSettings::setUsePwgen(true);
    else
      QtPassSettings::setUsePwgen(false);
    QtPassSettings::setPassTemplate("login\nurl");
  } else {
    // QStringList ver = version.split(".");
    // qDebug() << ver;
    // if (ver[0] == "0" && ver[1] == "8") {
    //// upgrade to 0.9
    // }
    if (QtPassSettings::getPassTemplate().isEmpty())
      QtPassSettings::setPassTemplate("login\nurl");
  }

  QtPassSettings::setVersion(VERSION);

  if (Util::checkConfig()) {
    config();
    if (freshStart && Util::checkConfig())
      return false;
  }

  freshStart = false;

  // TODO(annejan): this needs to be before we try to access the store,
  // but it would be better to do it after the Window is shown,
  // as the long delay it can cause is irritating otherwise.
  if (QtPassSettings::isUseWebDav())
    mountWebDav();

  model.setNameFilters(QStringList() << "*.gpg");
  model.setNameFilterDisables(false);

  proxyModel.setSourceModel(&model);
  proxyModel.setModelAndStore(&model, QtPassSettings::getPassStore());
  selectionModel.reset(new QItemSelectionModel(&proxyModel));
  model.fetchMore(model.setRootPath(QtPassSettings::getPassStore()));
  model.sort(0, Qt::AscendingOrder);

  ui->treeView->setModel(&proxyModel);
  ui->treeView->setRootIndex(proxyModel.mapFromSource(
      model.setRootPath(QtPassSettings::getPassStore())));
  ui->treeView->setColumnHidden(1, true);
  ui->treeView->setColumnHidden(2, true);
  ui->treeView->setColumnHidden(3, true);
  ui->treeView->setHeaderHidden(true);
  ui->treeView->setIndentation(15);
  ui->treeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->treeView, SIGNAL(customContextMenuRequested(const QPoint &)),
          this, SLOT(showContextMenu(const QPoint &)));
  connect(ui->treeView, SIGNAL(emptyClicked()), this, SLOT(deselect()));
  ui->textBrowser->setOpenExternalLinks(true);
  ui->textBrowser->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->textBrowser, SIGNAL(customContextMenuRequested(const QPoint &)),
          this, SLOT(showBrowserContextMenu(const QPoint &)));

  updateProfileBox();

  //    TODO(bezet): make this check unnecessary
  if (pass == nullptr) {
    if (QtPassSettings::isUsePass())
      pass = &rpass;
    else
      pass = &ipass;
  }
  pass->updateEnv();

  clearPanelTimer.setInterval(1000 *
                              QtPassSettings::getAutoclearPanelSeconds());
  clearClipboardTimer.setInterval(1000 * QtPassSettings::getAutoclearSeconds());

  if (!QtPassSettings::isUseGit() ||
      (QtPassSettings::getGitExecutable().isEmpty() &&
       QtPassSettings::getPassExecutable().isEmpty())) {
    ui->pushButton->hide();
    ui->updateButton->hide();
    ui->horizontalSpacer->changeSize(0, 20, QSizePolicy::Maximum,
                                     QSizePolicy::Minimum);
  } else {
    ui->pushButton->show();
    ui->updateButton->show();
    ui->horizontalSpacer->changeSize(24, 24, QSizePolicy::Minimum,
                                     QSizePolicy::Minimum);
  }

  startupPhase = false;
  return true;
}

/**
 * @brief MainWindow::config pops up the configuration screen and handles all
 * inter-window communication
 */
void MainWindow::config() {
  QScopedPointer<ConfigDialog> d(new ConfigDialog(this));
  d->setModal(true);
  // Automatically default to pass if it's available
  if (freshStart && QFile(QtPassSettings::getPassExecutable()).exists()) {
    QtPassSettings::setUsePass(true);
    pass = &rpass;
  }

  d->setPassPath(QtPassSettings::getPassExecutable());
  d->setGitPath(QtPassSettings::getGitExecutable());
  d->setGpgPath(QtPassSettings::getGpgExecutable());
  d->setStorePath(QtPassSettings::getPassStore());
  d->usePass(QtPassSettings::isUsePass());
  d->useClipboard(QtPassSettings::getClipBoardType());
  d->useAutoclear(QtPassSettings::isUseAutoclear());
  d->setAutoclear(QtPassSettings::getAutoclearSeconds());
  d->useAutoclearPanel(QtPassSettings::isUseAutoclearPanel());
  d->setAutoclearPanel(QtPassSettings::getAutoclearPanelSeconds());
  d->hidePassword(QtPassSettings::isHidePassword());
  d->hideContent(QtPassSettings::isHideContent());
  d->addGPGId(QtPassSettings::isAddGPGId(true));
  d->useTrayIcon(QtPassSettings::isUseTrayIcon());
  d->hideOnClose(QtPassSettings::isHideOnClose());
  d->startMinimized(QtPassSettings::isStartMinimized());
  d->setProfiles(QtPassSettings::getProfiles(), QtPassSettings::getProfile());
  d->useGit(QtPassSettings::isUseGit());
  d->setPwgenPath(QtPassSettings::getPwgenExecutable());
  d->usePwgen(QtPassSettings::isUsePwgen());
  d->avoidCapitals(QtPassSettings::isAvoidCapitals());
  d->avoidNumbers(QtPassSettings::isAvoidNumbers());
  d->lessRandom(QtPassSettings::isLessRandom());
  d->useSymbols(QtPassSettings::isUseSymbols());
  d->setPasswordLength(pwdConfig.length);
  d->setPwdTemplateSelector(pwdConfig.selected);
  if (pwdConfig.selected != passwordConfiguration::CUSTOM)
    d->setLineEditEnabled(false);
  d->setPasswordChars(pwdConfig.Characters[pwdConfig.selected]);
  d->useTemplate(QtPassSettings::isUseTemplate());
  d->setTemplate(QtPassSettings::getPassTemplate());
  d->templateAllFields(QtPassSettings::isTemplateAllFields());
  d->autoPull(QtPassSettings::isAutoPull());
  d->autoPush(QtPassSettings::isAutoPush());
  d->alwaysOnTop(QtPassSettings::isAlwaysOnTop());
  if (startupPhase)
    d->wizard(); // does shit
  if (d->exec()) {
    if (d->result() == QDialog::Accepted) {
      QtPassSettings::setPassExecutable(d->getPassPath());
      QtPassSettings::setGitExecutable(d->getGitPath());
      QtPassSettings::setGpgExecutable(d->getGpgPath());
      QtPassSettings::setPassStore(
          Util::normalizeFolderPath(d->getStorePath()));
      QtPassSettings::setUsePass(d->usePass());
      if (d->usePass())
        pass = &rpass;
      else
        pass = &ipass;
      QtPassSettings::setClipBoardType(d->useClipboard());
      QtPassSettings::setUseAutoclear(d->useAutoclear());
      QtPassSettings::setAutoclearSeconds(d->getAutoclear());
      QtPassSettings::setUseAutoclearPanel(d->useAutoclearPanel());
      QtPassSettings::setAutoclearPanelSeconds(d->getAutoclearPanel());
      QtPassSettings::setHidePassword(d->hidePassword());
      QtPassSettings::setHideContent(d->hideContent());
      QtPassSettings::setAddGPGId(d->addGPGId());
      QtPassSettings::setUseTrayIcon(d->useTrayIcon());
      QtPassSettings::setHideOnClose(d->hideOnClose());
      QtPassSettings::setStartMinimized(d->startMinimized());
      QtPassSettings::setProfiles(d->getProfiles());
      QtPassSettings::setUseGit(d->useGit());
      QtPassSettings::setPwgenExecutable(d->getPwgenPath());
      QtPassSettings::setUsePwgen(d->usePwgen());
      QtPassSettings::setAvoidCapitals(d->avoidCapitals());
      QtPassSettings::setAvoidNumbers(d->avoidNumbers());
      QtPassSettings::setLessRandom(d->lessRandom());
      QtPassSettings::setUseSymbols(d->useSymbols());
      pwdConfig.length = d->getPasswordLength();
      pwdConfig.selected = static_cast<passwordConfiguration::characterSet>(
          d->getPwdTemplateSelector());
      pwdConfig.Characters[passwordConfiguration::CUSTOM] =
          d->getPasswordChars();
      QtPassSettings::setUseTemplate(d->useTemplate());
      QtPassSettings::setPassTemplate(d->getTemplate());
      QtPassSettings::setTemplateAllFields(d->templateAllFields());
      QtPassSettings::setAutoPush(d->autoPush());
      QtPassSettings::setAutoPull(d->autoPull());
      QtPassSettings::setAlwaysOnTop(d->alwaysOnTop());

      QtPassSettings::setVersion(VERSION);
      QtPassSettings::setPasswordLength(pwdConfig.length);
      QtPassSettings::setPasswordCharsselection(pwdConfig.selected);
      QtPassSettings::setPasswordChars(
          pwdConfig.Characters[passwordConfiguration::CUSTOM]);

      if (QtPassSettings::isAlwaysOnTop()) {
        Qt::WindowFlags flags = windowFlags();
        this->setWindowFlags(flags | Qt::WindowStaysOnTopHint);
        this->show();
      } else {
        this->setWindowFlags(Qt::Window);
        this->show();
      }

      updateProfileBox();
      ui->treeView->setRootIndex(proxyModel.mapFromSource(
          model.setRootPath(QtPassSettings::getPassStore())));

      if (freshStart && Util::checkConfig())
        config();
      pass->updateEnv();
      clearPanelTimer.setInterval(1000 *
                                  QtPassSettings::getAutoclearPanelSeconds());
      clearClipboardTimer.setInterval(1000 *
                                      QtPassSettings::getAutoclearSeconds());

      if (!QtPassSettings::isUseGit() ||
          (QtPassSettings::getGitExecutable().isEmpty() &&
           QtPassSettings::getPassExecutable().isEmpty())) {
        ui->pushButton->hide();
        ui->updateButton->hide();
        ui->horizontalSpacer->changeSize(0, 20, QSizePolicy::Maximum,
                                         QSizePolicy::Minimum);
      } else {
        ui->pushButton->show();
        ui->updateButton->show();
        ui->horizontalSpacer->changeSize(24, 24, QSizePolicy::Minimum,
                                         QSizePolicy::Minimum);
      }
      if (QtPassSettings::isUseTrayIcon() && tray == NULL)
        initTrayIcon();
      else if (!QtPassSettings::isUseTrayIcon() && tray != NULL)
        destroyTrayIcon();
    }
    freshStart = false;
  }
}

/**
 * @brief MainWindow::on_updateButton_clicked do a git pull
 */
//  TODO(bezet): add bool block and wait for process to finish
void MainWindow::on_updateButton_clicked() {
  ui->statusBar->showMessage(tr("Updating password-store"), 2000);
  currentAction = GIT;
  pass->GitPull();
}

/**
 * @brief MainWindow::on_pushButton_clicked do a git push
 */
void MainWindow::on_pushButton_clicked() {
  ui->statusBar->showMessage(tr("Updating password-store"), 2000);
  currentAction = GIT;
  pass->GitPush();
}

/**
 * @brief MainWindow::getDir get selectd folder path
 * @param index
 * @param forPass short or full path
 * @return path
 */
QString MainWindow::getDir(const QModelIndex &index, bool forPass) {
  QString abspath = QDir(QtPassSettings::getPassStore()).absolutePath() + '/';
  if (!index.isValid())
    return forPass ? "" : abspath;
  QFileInfo info = model.fileInfo(proxyModel.mapToSource(index));
  QString filePath =
      (info.isFile() ? info.absolutePath() : info.absoluteFilePath());
  if (forPass) {
    filePath = QDir(abspath).relativeFilePath(filePath);
  }
  filePath += '/';
  return filePath;
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
  currentDir = getDir(ui->treeView->currentIndex(), false);
  lastDecrypt = "Could not decrypt";
  clippedText = "";
  QString file = getFile(index, QtPassSettings::isUsePass());
  QFileInfo fileinfo =
      model.fileInfo(proxyModel.mapToSource(ui->treeView->currentIndex()));
  ui->passwordName->setText(getFile(index, true));
  if (!file.isEmpty() && !cleared) {
    currentAction = GPG;
    pass->Show(file);
  } else {
    clearPanel(false);
    ui->editButton->setEnabled(false);
    ui->deleteButton->setEnabled(true);
  }
}

/**
 * @brief MainWindow::on_treeView_doubleClicked when doubleclicked on
 * TreeViewItem, open the edit Window
 * @param index
 */
void MainWindow::on_treeView_doubleClicked(const QModelIndex &index) {
  // TODO: do nothing when clicked on folder
  QFileInfo fileOrFolder =
      model.fileInfo(proxyModel.mapToSource(ui->treeView->currentIndex()));
  QString file = "";

  if (fileOrFolder.isFile()) {
    QString file = getFile(index, QtPassSettings::isUsePass());
    if (file.isEmpty()) {
      QMessageBox::critical(
          this, tr("Can not edit"),
          tr("Selected password file does not exist, not able to edit"));
      return;
    }
    setPassword(file, true, false);
  }
}

/**
 * @brief MainWindow::deselect clear the selection, password and copy buffer
 */
void MainWindow::deselect() {
  currentDir = "/";
  clearClipboard();
  ui->passwordName->setText("");
  clearPanel(false);
}

/**
 * @brief MainWindow::executePassGitInit git init wrapper
 */
void MainWindow::executePassGitInit() {
  qDebug() << "Pass git init called";
  pass->GitInit();
}

void MainWindow::executeWrapperStarted() {
  clearTemplateWidgets();
  ui->textBrowser->clear();
  ui->textBrowser->setTextColor(Qt::black);
  enableUiElements(false);
  clearPanelTimer.stop();
}

/**
 * @brief MainWindow::readyRead we have data
 */
void MainWindow::readyRead(bool finished = false) {
  if (currentAction == PWGEN)
    return;
  QString output = "";
  QString error = "";
  if (currentAction != GPG_INTERNAL) {
    error = pass->readAllStandardError();
    QByteArray processOutBytes = pass->readAllStandardOutput();
    QTextCodec *codec = QTextCodec::codecForLocale();
    output = codec->toUnicode(processOutBytes);
    if (finished && currentAction == GPG) {
      lastDecrypt = output;
      QStringList tokens = output.split("\n");
      QString password = tokens.at(0);

      if (QtPassSettings::getClipBoardType() != Enums::CLIPBOARD_NEVER &&
          !output.isEmpty()) {
        clippedText = tokens[0];
        if (QtPassSettings::getClipBoardType() == Enums::CLIPBOARD_ALWAYS)
          copyTextToClipboard(tokens[0]);
        if (QtPassSettings::isUseAutoclearPanel()) {
          clearPanelTimer.start();
        }
        if (QtPassSettings::isHidePassword() &&
            !QtPassSettings::isUseTemplate()) {
          tokens[0] = "***" + tr("Password hidden") + "***";
          output = tokens.join("\n");
        }
        if (QtPassSettings::isHideContent())
          output = "***" + tr("Content hidden") + "***";
      }

      if (QtPassSettings::isUseTemplate() && !QtPassSettings::isHideContent()) {
        while (ui->gridLayout->count() > 0) {
          QLayoutItem *item = ui->gridLayout->takeAt(0);
          delete item->widget();
          delete item;
        }
        QStringList remainingTokens;
        for (int j = 1; j < tokens.length(); ++j) {
          QString token = tokens.at(j);
          if (token.contains(':')) {
            int colon = token.indexOf(':');
            QString field = token.left(colon);
            if (QtPassSettings::isTemplateAllFields() ||
                QtPassSettings::getPassTemplate().contains(field)) {
              QString value = token.right(token.length() - colon - 1);
              if (!QtPassSettings::getPassTemplate().contains(field) &&
                  value.startsWith("//")) {
                remainingTokens.append(token);
                continue; // colon is probably from a url
              }
              addToGridLayout(j, field, value);
            }
          } else {
            remainingTokens.append(token);
          }
        }
        if (ui->gridLayout->count() == 0)
          ui->verticalLayoutPassword->setSpacing(0);
        else
          ui->verticalLayoutPassword->setSpacing(6);
        output = remainingTokens.join("\n");
      } else {
        clearTemplateWidgets();
      }
      if (!QtPassSettings::isHideContent() && !password.isEmpty()) {
        // now set the password. If we set it earlier, the layout will be
        // cleared
        addToGridLayout(0, tr("Password"), password);
      }
      if (QtPassSettings::isUseAutoclearPanel()) {
        clearPanelTimer.start();
      }
    }
    output.replace(QRegExp("<"), "&lt;");
    output.replace(QRegExp(">"), "&gt;");
    output.replace(QRegExp(" "), "&nbsp;");
  } else {
    // qDebug() << process->readAllStandardOutput();
    // qDebug() << process->readAllStandardError();
    if (finished && 0 != keygen) {
      qDebug() << "Keygen Done";
      keygen->close();
      keygen = 0;
      // TODO(annejan) some sanity checking ?
    }
  }

  if (!error.isEmpty()) {
    if (currentAction == GIT) {
      // https://github.com/IJHack/qtpass/issues/111
      output = "<span style=\"color: darkgray;\">" + error + "</span><br />" +
               output;
    } else {
      output =
          "<span style=\"color: red;\">" + error + "</span><br />" + output;
    }
  }

  output.replace(QRegExp("((?:https?|ftp|ssh)://\\S+)"),
                 "<a href=\"\\1\">\\1</a>");
  output.replace(QRegExp("\n"), "<br />");
  if (!ui->textBrowser->toPlainText().isEmpty())
    output = ui->textBrowser->toHtml() + output;
  ui->textBrowser->setHtml(output);
}

/**
 * @brief MainWindow::clearClipboard remove clipboard contents.
 */
void MainWindow::clearClipboard() {
  QClipboard *clipboard = QApplication::clipboard();
  QString clippedText = clipboard->text();
  if (clippedText == clippedText) {
    clipboard->clear();
    ui->statusBar->showMessage(tr("Clipboard cleared"), 2000);
  } else {
    ui->statusBar->showMessage(tr("Clipboard not cleared"), 2000);
  }
}

/**
 * @brief MainWindow::clearPanel hide the information from shoulder surfers
 */
void MainWindow::clearPanel(bool notify = true) {
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
 * @brief MainWindow::clearPanel because slots needs the same amout of params as
 * signals
 */
void MainWindow::clearPanel() { clearPanel(true); }

/**
 * @brief MainWindow::processFinished process is finished, if there is another
 * one queued up to run, start it.
 * @param exitCode
 * @param exitStatus
 */
void MainWindow::processFinished(int exitCode,
                                 QProcess::ExitStatus exitStatus) {
  bool error = exitStatus != QProcess::NormalExit || exitCode > 0;
  readyRead(true);
  enableUiElements(true);
  if (!error && currentAction == EDIT)
    on_treeView_clicked(ui->treeView->currentIndex());
}

/**
 * @brief MainWindow::enableUiElements enable or disable the relevant UI
 * elements
 * @param state
 */
void MainWindow::enableUiElements(bool state) {
  ui->updateButton->setEnabled(state);
  ui->treeView->setEnabled(state);
  ui->lineEdit->setEnabled(state);
  ui->lineEdit->installEventFilter(this);
  ui->addButton->setEnabled(state);
  ui->usersButton->setEnabled(state);
  ui->configButton->setEnabled(state);
  // is a file selected?
  state &= ui->treeView->currentIndex().isValid();
  ui->deleteButton->setEnabled(state);
  ui->editButton->setEnabled(state);
  ui->pushButton->setEnabled(state);
}

/**
 * @brief MainWindow::processError something went wrong
 * @param error
 */
void MainWindow::processError(QProcess::ProcessError error) {
  QString errorString;
  switch (error) {
  case QProcess::FailedToStart:
    errorString = tr("QProcess::FailedToStart");
    break;
  case QProcess::Crashed:
    errorString = tr("QProcess::Crashed");
    break;
  case QProcess::Timedout:
    errorString = tr("QProcess::Timedout");
    break;
  case QProcess::ReadError:
    errorString = tr("QProcess::ReadError");
    break;
  case QProcess::WriteError:
    errorString = tr("QProcess::WriteError");
    break;
  case QProcess::UnknownError:
    errorString = tr("QProcess::UnknownError");
    break;
  }
  ui->textBrowser->setTextColor(Qt::red);
  ui->textBrowser->setText(errorString);
  //    TODO(bezet): this probably shall be done in finished handler(I guess it
  //    finishes even on error)
  if (pass->state() == QProcess::NotRunning)
    enableUiElements(true);
}

/**
 * @brief MainWindow::on_configButton_clicked run Mainwindow::config
 */
void MainWindow::on_configButton_clicked() { config(); }

/**
 * @brief Executes when the string in the search box changes, collapses the
 * TreeView
 * @param arg1
 */
void MainWindow::on_lineEdit_textChanged(const QString &arg1) {
  ui->treeView->expandAll();
  ui->statusBar->showMessage(tr("Looking for: %1").arg(arg1), 1000);
  QString query = arg1;
  query.replace(QRegExp(" "), ".*");
  QRegExp regExp(query, Qt::CaseInsensitive);
  proxyModel.setFilterRegExp(regExp);
  ui->treeView->setRootIndex(proxyModel.mapFromSource(
      model.setRootPath(QtPassSettings::getPassStore())));
  selectFirstFile();
}

/**
 * @brief MainWindow::on_lineEdit_returnPressed get searching
 *
 * Select the first possible file in the tree
 */
void MainWindow::on_lineEdit_returnPressed() {
  qDebug() << "on_lineEdit_returnPressed";
  selectFirstFile();
  on_treeView_clicked(ui->treeView->currentIndex());
}

/**
 * @brief MainWindow::selectFirstFile select the first possible file in the tree
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
 * @brief MainWindow::setPassword open passworddialog and save file (if not
 * canceled)
 * @param file which pgp file
 * @param overwrite update file (not insert)
 * @param isNew insert (not update)
 */
void MainWindow::setPassword(QString file, bool overwrite, bool isNew = false) {
  if (!isNew && lastDecrypt.isEmpty()) {
    // warn?
    return;
  }
  PasswordDialog d(pwdConfig, *pass, this);
  d.setFile(file);
  d.usePwgen(QtPassSettings::isUsePwgen());
  d.setTemplate(QtPassSettings::getPassTemplate());
  d.useTemplate(QtPassSettings::isUseTemplate());
  d.templateAll(QtPassSettings::isTemplateAllFields());
  d.setPassword(lastDecrypt);
  currentAction = PWGEN;
  if (!d.exec()) {
    d.setPassword(QString());
    return;
  }
  QString newValue = d.getPassword();
  if (newValue.isEmpty())
    return;

  if (newValue.right(1) != "\n")
    newValue += "\n";

  currentAction = EDIT;
  pass->Insert(file, newValue, overwrite);

  if (QtPassSettings::isUseGit() && QtPassSettings::isAutoPush())
    on_pushButton_clicked();
}

/**
 * @brief MainWindow::on_addButton_clicked add a new password by showing a
 * number of dialogs.
 */
void MainWindow::on_addButton_clicked() {
  // Check for active and selected encryption key
  //  QList<UserInfo> users=listKeys();
  //  UserInfo testuser;
  //  bool noUserEnabled = false;
  //  // Check if at least one active user is selected
  //  for (int i = 0; i< users.length();i++) {
  //    testuser = users[i];
  //    noUserEnabled = users[i].enabled | noUserEnabled;
  //  }
  //  // Error if no user is enabled, so a password doesn't get saved
  //  if (noUserEnabled==false) {
  //    QMessageBox::critical(this, tr("Can not get key list"),
  //                          tr("No Key for encryption selected! \nPlease
  //                          select a valid key pair in the users dialouge"));
  //    return;
  //  }
  bool ok;
  QString dir =
      getDir(ui->treeView->currentIndex(), QtPassSettings::isUsePass());
  QString file = QInputDialog::getText(
      this, tr("New file"),
      tr("New password file: \n(Will be placed in %1 )")
          .arg(QtPassSettings::getPassStore() +
               getDir(ui->treeView->currentIndex(), true)),
      QLineEdit::Normal, "", &ok);
  if (!ok || file.isEmpty())
    return;
  file = dir + file;
  lastDecrypt = "";
  setPassword(file, false, true);
}

/**
 * @brief MainWindow::on_deleteButton_clicked remove password, if you are sure.
 */
void MainWindow::on_deleteButton_clicked() {
  QFileInfo fileOrFolder =
      model.fileInfo(proxyModel.mapToSource(ui->treeView->currentIndex()));
  QString file = "";
  bool isDir = false;

  if (fileOrFolder.isFile()) {
    file = getFile(ui->treeView->currentIndex(), QtPassSettings::isUsePass());
  } else {
    file = getDir(ui->treeView->currentIndex(), QtPassSettings::isUsePass());
    isDir = true;
  }

  if (QMessageBox::question(
          this, isDir ? tr("Delete folder?") : tr("Delete password?"),
          tr("Are you sure you want to delete %1?")
              .arg(QDir::separator() +
                   getFile(ui->treeView->currentIndex(), true)),
          QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    return;

  currentAction = REMOVE;
  pass->Remove(file, isDir);
  //  TODO(bezet): hide inside interface?
  if (QtPassSettings::isUseGit() && QtPassSettings::isAutoPush())
    on_pushButton_clicked();

  lastDecrypt = "";
}

/**
 * @brief MainWindow::on_editButton_clicked try and edit (selected) password.
 */
void MainWindow::on_editButton_clicked() {
  QString file =
      getFile(ui->treeView->currentIndex(), QtPassSettings::isUsePass());
  if (file.isEmpty()) {
    QMessageBox::critical(
        this, tr("Can not edit"),
        tr("Selected password file does not exist, not able to edit"));
    return;
  }
  setPassword(file, true);
}

/**
 * @brief MainWindow::userDialog see MainWindow::on_usersButton_clicked()
 * @param dir folder to edit users for.
 */
void MainWindow::userDialog(QString dir) {
  if (!dir.isEmpty())
    currentDir = dir;
  on_usersButton_clicked();
}

//  TODO(bezet): temporary wrapper
QList<UserInfo> MainWindow::listKeys(QString keystring, bool secret) {
  currentAction = GPG_INTERNAL;
  return pass->listKeys(keystring, secret);
}

/**
 * @brief MainWindow::on_usersButton_clicked edit users for the current folder,
 * gets lists and opens UserDialog.
 */
void MainWindow::on_usersButton_clicked() {
  QList<UserInfo> users = listKeys();
  if (users.size() == 0) {
    QMessageBox::critical(this, tr("Can not get key list"),
                          tr("Unable to get list of available gpg keys"));
    return;
  }
  QList<UserInfo> secret_keys = listKeys("", true);
  foreach (const UserInfo &sec, secret_keys) {
    for (QList<UserInfo>::iterator it = users.begin(); it != users.end(); ++it)
      if (sec.key_id == it->key_id)
        it->have_secret = true;
  }
  QList<UserInfo> selected_users;
  QString dir = currentDir.isEmpty()
                    ? getDir(ui->treeView->currentIndex(), false)
                    : currentDir;
  int count = 0;
  QString recipients =
      pass->getRecipientString(dir.isEmpty() ? "" : dir, " ", &count);
  if (!recipients.isEmpty())
    selected_users = listKeys(recipients);
  foreach (const UserInfo &sel, selected_users) {
    for (QList<UserInfo>::iterator it = users.begin(); it != users.end(); ++it)
      if (sel.key_id == it->key_id)
        it->enabled = true;
  }
  if (count > selected_users.size()) {
    // Some keys seem missing from keyring, add them separately
    QStringList recipients = pass->getRecipientList(dir.isEmpty() ? "" : dir);
    foreach (const QString recipient, recipients) {
      if (listKeys(recipient).size() < 1) {
        UserInfo i;
        i.enabled = true;
        i.key_id = recipient;
        i.name = " ?? " + tr("Key not found in keyring");
        users.append(i);
      }
    }
  }
  UsersDialog d(this);
  d.setUsers(&users);
  if (!d.exec()) {
    d.setUsers(NULL);
    return;
  }
  d.setUsers(NULL);

  pass->Init(dir, users);

  if (QtPassSettings::isAutoPush())
    on_pushButton_clicked();
}

/**
 * @brief MainWindow::setApp make sure we know what/who/where we are
 * @param app
 */
void MainWindow::setApp(SingleApplication *app) {
#if SINGLE_APP
  connect(app, SIGNAL(messageAvailable(QString)), this,
          SLOT(messageAvailable(QString)));
#endif
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
 * @brief MainWindow::setText do a search from an external source
 * (eg: commandline)
 * @param text
 */
void MainWindow::setText(QString text) { ui->lineEdit->setText(text); }

/**
 * @brief MainWindow::getSecretKeys get list of secret/private keys
 * @return QStringList keys
 */
QStringList MainWindow::getSecretKeys() {
  QList<UserInfo> keys = listKeys("", true);
  QStringList names;

  if (keys.size() == 0)
    return names;

  foreach (const UserInfo &sec, keys)
    names << sec.name;

  return names;
}

/**
 * @brief MainWindow::generateKeyPair internal gpg keypair generator . .
 * @param batch
 * @param keygenWindow
 */
void MainWindow::generateKeyPair(QString batch, QDialog *keygenWindow) {
  keygen = keygenWindow;
  ui->statusBar->showMessage(tr("Generating GPG key pair"), 60000);
  currentAction = GPG_INTERNAL;
  pass->GenerateGPGKeys(batch);
}

/**
 * @brief MainWindow::updateProfileBox update the list of profiles, optionally
 * select a more appropriate one to view too
 */
void MainWindow::updateProfileBox() {
  // qDebug() << profiles.size();
  if (QtPassSettings::getProfiles().isEmpty()) {
    ui->profileBox->hide();
  } else {
    ui->profileBox->show();
    if (QtPassSettings::getProfiles().size() < 2)
      ui->profileBox->setEnabled(false);
    else
      ui->profileBox->setEnabled(true);
    ui->profileBox->clear();
    QHashIterator<QString, QString> i(QtPassSettings::getProfiles());
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
  if (startupPhase || name == QtPassSettings::getProfile())
    return;
  QtPassSettings::setProfile(name);

  QtPassSettings::setPassStore(QtPassSettings::getProfiles()[name]);
  ui->statusBar->showMessage(tr("Profile changed to %1").arg(name), 2000);

  pass->resetPasswordStoreDir();

  ui->treeView->setRootIndex(proxyModel.mapFromSource(
      model.setRootPath(QtPassSettings::getPassStore())));
}

/**
 * @brief MainWindow::initTrayIcon show a nice tray icon on systems that support
 * it
 */
void MainWindow::initTrayIcon() {
  if (tray != NULL) {
    qDebug() << "Creating tray icon again?";
    return;
  }
  if (QSystemTrayIcon::isSystemTrayAvailable() == true) {
    // Setup tray icon
    this->tray = new TrayIcon(this);
    if (tray == NULL)
      qDebug() << "Allocating tray icon failed.";
  } else {
    qDebug() << "No tray icon for this OS possibly also not show options?";
  }
}

/**
 * @brief MainWindow::destroyTrayIcon remove that pesky tray icon
 */
void MainWindow::destroyTrayIcon() {
  if (tray == NULL) {
    qDebug() << "Destroy non existing tray icon?";
    return;
  }
  delete this->tray;
  tray = NULL;
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
    clearClipboard();
    QtPassSettings::setGeometry(saveGeometry());
    QtPassSettings::setSavestate(saveState());
    QtPassSettings::setMaximized(isMaximized());
    if (!isMaximized()) {
      QtPassSettings::setPos(pos());
      QtPassSettings::setSize(size());
    }
    QtPassSettings::setSplitterLeft(ui->splitter->sizes()[0]);
    QtPassSettings::setSplitterRight(ui->splitter->sizes()[1]);
    event->accept();
  }
}

/**
 * @brief MainWindow::eventFilter filter out some events and focus the treeview
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
    on_deleteButton_clicked();
    break;
  case Qt::Key_Return:
  case Qt::Key_Enter:
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
 * @brief MainWindow::showContextMenu show us the (file or folder) context menu
 * @param pos
 */
void MainWindow::showContextMenu(const QPoint &pos) {
  QModelIndex index = ui->treeView->indexAt(pos);
  bool selected = true;
  if (!index.isValid()) {
    ui->treeView->clearSelection();
    ui->deleteButton->setEnabled(false);
    ui->editButton->setEnabled(false);
    currentDir = "";
    selected = false;
  }

  ui->treeView->setCurrentIndex(index);

  QPoint globalPos = ui->treeView->viewport()->mapToGlobal(pos);

  QFileInfo fileOrFolder =
      model.fileInfo(proxyModel.mapToSource(ui->treeView->currentIndex()));

  QMenu contextMenu;
  if (!selected || fileOrFolder.isDir()) {
    QAction *addFolder = contextMenu.addAction(tr("Add folder"));
    QAction *addPassword = contextMenu.addAction(tr("Add password"));
    QAction *users = contextMenu.addAction(tr("Users"));
    connect(addFolder, SIGNAL(triggered()), this, SLOT(addFolder()));
    connect(addPassword, SIGNAL(triggered()), this,
            SLOT(on_addButton_clicked()));
    connect(users, SIGNAL(triggered()), this, SLOT(on_usersButton_clicked()));
  } else if (fileOrFolder.isFile()) {
    QAction *edit = contextMenu.addAction(tr("Edit"));
    connect(edit, SIGNAL(triggered()), this, SLOT(editPassword()));
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
    connect(deleteItem, SIGNAL(triggered()), this,
            SLOT(on_deleteButton_clicked()));
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
 * @brief MainWindow::addFolder add a new folder to store passwords in
 */
void MainWindow::addFolder() {
  bool ok;
  QString dir = getDir(ui->treeView->currentIndex(), false);
  QString newdir = QInputDialog::getText(
      this, tr("New file"),
      tr("New Folder: \n(Will be placed in %1 )")
          .arg(QtPassSettings::getPassStore() +
               getDir(ui->treeView->currentIndex(), true)),
      QLineEdit::Normal, "", &ok);
  if (!ok || newdir.isEmpty())
    return;
  newdir.prepend(dir);
  // qDebug() << newdir;
  QDir().mkdir(newdir);
  // TODO(annejan) add to git?
}

/**
 * @brief MainWindow::editPassword read password and open edit window via
 * MainWindow::on_editButton_clicked()
 */
void MainWindow::editPassword() {
  if (QtPassSettings::isUseGit() && QtPassSettings::isAutoPull())
    on_updateButton_clicked();
  pass->waitFor(30);
  pass->waitForProcess();
  // TODO(annejan) move to editbutton stuff possibly?
  currentDir = getDir(ui->treeView->currentIndex(), false);
  lastDecrypt = "Could not decrypt";
  QString file =
      getFile(ui->treeView->currentIndex(), QtPassSettings::isUsePass());
  if (!file.isEmpty()) {
    currentAction = GPG;
    if (pass->Show(file, true) == QProcess::NormalExit)
      on_editButton_clicked();
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

/**
 * @brief MainWindow::copyTextToClipboard copies text to your clipboard
 * @param text
 */
void MainWindow::copyTextToClipboard(const QString &text) {
  QClipboard *clip = QApplication::clipboard();
  clip->setText(text);
  clippedText = text;
  ui->statusBar->showMessage(tr("Copied to clipboard"), 2000);
  if (QtPassSettings::isUseAutoclear()) {
    clearClipboardTimer.start();
  }
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
    connect(fieldLabel, SIGNAL(clicked(QString)), this,
            SLOT(copyTextToClipboard(QString)));

    fieldLabel->setStyleSheet("border-style: none ; background: transparent;");
    // fieldLabel->setContentsMargins(0,5,5,0);
    frame->layout()->addWidget(fieldLabel);
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
    trimmedValue.replace(QRegExp("((?:https?|ftp|ssh)://\\S+)"),
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
  * @params msg     text to be displayed
  * @params timeout time for which msg shall be visible
  */
void MainWindow::showStatusMessage(QString msg, int timeout) {
  ui->statusBar->showMessage(msg, timeout);
}

void MainWindow::startReencryptPath() {
  enableUiElements(false);
  ui->treeView->setDisabled(true);
}

void MainWindow::endReencryptPath() { enableUiElements(true); }

void MainWindow::critical(QString title, QString msg) {
  QMessageBox::critical(this, title, msg);
}

void MainWindow::setLastDecrypt(QString msg) { lastDecrypt = msg; }
