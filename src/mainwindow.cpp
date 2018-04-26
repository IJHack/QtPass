#include "mainwindow.h"
#include "debughelper.h"
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QQueue>
#include <QShortcut>
#include <QSystemTrayIcon>
#include <QTextCodec>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN /*_KILLING_MACHINE*/
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <winnetwk.h>
#undef DELETE
#endif
#include "configdialog.h"
#include "filecontent.h"
#include "keygendialog.h"
#include "passworddialog.h"
#include "qpushbuttonwithclipboard.h"
#include "qtpasssettings.h"
#include "settingsconstants.h"
#include "trayicon.h"
#include "ui_mainwindow.h"
#include "usersdialog.h"
#include "util.h"

#if SINGLE_APP
#include "singleapplication.h"
#endif

/**
 * @brief MainWindow::MainWindow handles all of the main functionality and also
 * the main window.
 * @param parent
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), fusedav(this), keygen(NULL),
      tray(NULL) {
#ifdef __APPLE__
  // extra treatment for mac os
  // see http://doc.qt.io/qt-5/qkeysequence.html#qt_set_sequence_auto_mnemonic
  qt_set_sequence_auto_mnemonic(true);
#endif
  // register shortcut ctrl/cmd + Q to close the main window
  new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q), this, SLOT(close()));
  // register shortcut ctrl/cmd + C to copy the currently selected password
  new QShortcut(QKeySequence(QKeySequence::StandardKey::Copy), this,
                SLOT(copyPasswordFromTreeview()));

  //    TODO(bezet): this should be reconnected dynamically when pass changes
  connectPassSignalHandlers(QtPassSettings::getRealPass());
  connectPassSignalHandlers(QtPassSettings::getImitatePass());

  //    only for ipass
  connect(QtPassSettings::getImitatePass(), SIGNAL(startReencryptPath()), this,
          SLOT(startReencryptPath()));
  connect(QtPassSettings::getImitatePass(), SIGNAL(endReencryptPath()), this,
          SLOT(endReencryptPath()));

  ui->setupUi(this);

  enableUiElements(true);
  ui->statusBar->showMessage(tr("Welcome to QtPass %1").arg(VERSION), 2000);

  QPixmap logo = QPixmap::fromImage(QImage(":/artwork/icon.svg"))
                     .scaledToHeight(statusBar()->height());
  QLabel *logoApp = new QLabel(statusBar());
  logoApp->setPixmap(logo);
  statusBar()->addPermanentWidget(logoApp);

  freshStart = true;
  startupPhase = true;
  clearPanelTimer.setSingleShot(true);
  connect(&clearPanelTimer, SIGNAL(timeout()), this, SLOT(clearPanel()));
  clearClipboardTimer.setSingleShot(true);
  connect(&clearClipboardTimer, SIGNAL(timeout()), this,
          SLOT(clearClipboard()));
  if (!checkConfig()) {
    // no working config
    QApplication::quit();
  }
  clippedText = "";
  QTimer::singleShot(10, this, SLOT(focusInput()));

  initToolBarButtons();

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
 * @brief MainWindow::connectPassSignalHandlers this method connects Pass
 *                                              signals to approprite MainWindow
 *                                              slots
 *
 * @param pass        pointer to pass instance
 */
void MainWindow::connectPassSignalHandlers(Pass *pass) {

  //    TODO(bezet): this is never emitted(should be), also naming(see
  //    critical())
  connect(pass, &Pass::error, this, &MainWindow::processError);
  connect(pass, &Pass::startingExecuteWrapper, this,
          &MainWindow::executeWrapperStarted);
  connect(pass, &Pass::critical, this, &MainWindow::critical);
  connect(pass, &Pass::statusMsg, this, &MainWindow::showStatusMessage);
  connect(pass, &Pass::processErrorExit, this, &MainWindow::processErrorExit);

  connect(pass, &Pass::finishedGitInit, this, &MainWindow::passStoreChanged);
  connect(pass, &Pass::finishedGitPull, this, &MainWindow::processFinished);
  connect(pass, &Pass::finishedGitPush, this, &MainWindow::processFinished);
  connect(pass, &Pass::finishedShow, this, &MainWindow::passShowHandler);
  connect(pass, &Pass::finishedInsert, this, &MainWindow::finishedInsert);
  connect(pass, &Pass::finishedRemove, this, &MainWindow::passStoreChanged);
  connect(pass, &Pass::finishedInit, this, &MainWindow::passStoreChanged);
  connect(pass, &Pass::finishedMove, this, &MainWindow::passStoreChanged);
  connect(pass, &Pass::finishedCopy, this, &MainWindow::passStoreChanged);

  connect(pass, &Pass::finishedGenerateGPGKeys, this,
          &MainWindow::keyGenerationComplete);
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
    ui->textBrowser->setTextColor(Qt::black);
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
    ui->textBrowser->setTextColor(Qt::black);
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
    restoreWindow();
  }

  QString passStore = QtPassSettings::getPassStore(Util::findPasswordStore());
  QtPassSettings::setPassStore(passStore);

  QtPassSettings::initExecutables();

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

  // dbg()<< version;

  // Config updates
  if (version.isEmpty()) {
    dbg() << "assuming fresh install";
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
    // dbg()<< ver;
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
  proxyModel.setModelAndStore(&model, passStore);
  selectionModel.reset(new QItemSelectionModel(&proxyModel));
  model.fetchMore(model.setRootPath(passStore));
  model.sort(0, Qt::AscendingOrder);

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
  connect(ui->treeView, SIGNAL(customContextMenuRequested(const QPoint &)),
          this, SLOT(showContextMenu(const QPoint &)));
  connect(ui->treeView, SIGNAL(emptyClicked()), this, SLOT(deselect()));
  ui->textBrowser->setOpenExternalLinks(true);
  ui->textBrowser->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->textBrowser, SIGNAL(customContextMenuRequested(const QPoint &)),
          this, SLOT(showBrowserContextMenu(const QPoint &)));

  updateProfileBox();
  QtPassSettings::getPass()->updateEnv();
  clearPanelTimer.setInterval(1000 *
                              QtPassSettings::getAutoclearPanelSeconds());
  clearClipboardTimer.setInterval(1000 * QtPassSettings::getAutoclearSeconds());
  updateGitButtonVisibility();

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
  }

  d->setPassPath(QtPassSettings::getPassExecutable());
  d->setGitPath(QtPassSettings::getGitExecutable());
  d->setGpgPath(QtPassSettings::getGpgExecutable());
  d->setStorePath(QtPassSettings::getPassStore());
  d->usePass(QtPassSettings::isUsePass());
  d->useClipboard(QtPassSettings::getClipBoardType());
  d->useSelection(QtPassSettings::isUseSelection());
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
  d->setPasswordConfiguration(QtPassSettings::getPasswordConfiguration());
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
      QtPassSettings::setClipBoardType(d->useClipboard());
      QtPassSettings::setUseSelection(d->useSelection());
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
      QtPassSettings::setPasswordConfiguration(d->getPasswordConfiguration());
      QtPassSettings::setUseTemplate(d->useTemplate());
      QtPassSettings::setPassTemplate(d->getTemplate());
      QtPassSettings::setTemplateAllFields(d->templateAllFields());
      QtPassSettings::setAutoPush(d->autoPush());
      QtPassSettings::setAutoPull(d->autoPull());
      QtPassSettings::setAlwaysOnTop(d->alwaysOnTop());

      QtPassSettings::setVersion(VERSION);

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
      QtPassSettings::getPass()->updateEnv();
      clearPanelTimer.setInterval(1000 *
                                  QtPassSettings::getAutoclearPanelSeconds());
      clearClipboardTimer.setInterval(1000 *
                                      QtPassSettings::getAutoclearSeconds());

      updateGitButtonVisibility();
      if (QtPassSettings::isUseTrayIcon() && tray == NULL)
        initTrayIcon();
      else if (!QtPassSettings::isUseTrayIcon() && tray != NULL)
        destroyTrayIcon();
    }
    freshStart = false;
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
  clippedText = "";
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
  currentDir = "/";
  clearClipboard();
  ui->passwordName->setText("");
  clearPanel(false);
}

/**
 * @brief MainWindow::executePassGitInit git init wrapper
 */
void MainWindow::executePassGitInit() {
  dbg() << "Pass git init called";
  QtPassSettings::getPass()->GitInit();
}

void MainWindow::executeWrapperStarted() {
  clearTemplateWidgets();
  ui->textBrowser->clear();
  enableUiElements(false);
  clearPanelTimer.stop();
}

void MainWindow::keyGenerationComplete(const QString &p_output,
                                       const QString &p_errout) {
  // qDebug() << p_output;
  // qDebug() << p_errout;
  if (0 != keygen) {
    qDebug() << "Keygen Done";
    keygen->close();
    keygen = 0;
    // TODO(annejan) some sanity checking ?
  }
  processFinished(p_output, p_errout);
}

void MainWindow::initToolBarButtons() {
  connect(ui->actionAddPassword, SIGNAL(triggered()), this,
          SLOT(addPassword()));
  connect(ui->actionAddFolder, SIGNAL(triggered()), this, SLOT(addFolder()));
  connect(ui->actionEdit, SIGNAL(triggered()), this, SLOT(onEdit()));
  connect(ui->actionDelete, SIGNAL(triggered()), this, SLOT(onDelete()));
  connect(ui->actionPush, SIGNAL(triggered()), this, SLOT(onPush()));
  connect(ui->actionUpdate, SIGNAL(triggered()), this, SLOT(onUpdate()));
  connect(ui->actionUsers, SIGNAL(triggered()), this, SLOT(onUsers()));
  connect(ui->actionConfig, SIGNAL(triggered()), this, SLOT(onConfig()));
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

  // handle clipboard
  if (QtPassSettings::getClipBoardType() != Enums::CLIPBOARD_NEVER &&
      !p_output.isEmpty()) {
    clippedText = password;
    if (QtPassSettings::getClipBoardType() == Enums::CLIPBOARD_ALWAYS)
      copyTextToClipboard(password);
  }

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

  DisplayInTextBrowser(output);
  enableUiElements(true);
}

void MainWindow::passStoreChanged(const QString &p_out, const QString &p_err) {
  processFinished(p_out, p_err);
  doGitPush();
}

void MainWindow::doGitPush() {
  if (QtPassSettings::isAutoPush())
    onPush();
}

void MainWindow::finishedInsert(const QString &p_output,
                                const QString &p_errout) {
  processFinished(p_output, p_errout);
  doGitPush();
  on_treeView_clicked(ui->treeView->currentIndex());
}

void MainWindow::DisplayInTextBrowser(QString output, QString prefix,
                                      QString postfix) {

  output.replace(QRegExp("<"), "&lt;");
  output.replace(QRegExp(">"), "&gt;");
  output.replace(QRegExp(" "), "&nbsp;");

  output.replace(
      QRegExp("((?:https?|ftp|ssh|sftp|ftps|webdav|webdavs)://\\S+)"),
      "<a href=\"\\1\">\\1</a>");
  output.replace(QRegExp("\n"), "<br />");
  output = prefix + output + postfix;
  if (!ui->textBrowser->toPlainText().isEmpty())
    output = ui->textBrowser->toHtml() + output;
  ui->textBrowser->setHtml(output);
}

void MainWindow::processErrorExit(int exitCode, const QString &p_error) {
  if (!p_error.isEmpty()) {
    QString output;
    QString error = p_error;
    error.replace(QRegExp("<"), "&lt;");
    error.replace(QRegExp(">"), "&gt;");
    error.replace(QRegExp(" "), "&nbsp;");
    if (exitCode == 0) {
      //  https://github.com/IJHack/qtpass/issues/111
      output = "<span style=\"color: darkgray;\">" + error + "</span><br />";
    } else {
      output = "<span style=\"color: red;\">" + error + "</span><br />";
    }

    output.replace(
        QRegExp("((?:https?|ftp|ssh|sftp|ftps|webdav|webdavs)://\\S+)"),
        "<a href=\"\\1\">\\1</a>");
    output.replace(QRegExp("\n"), "<br />");
    if (!ui->textBrowser->toPlainText().isEmpty())
      output = ui->textBrowser->toHtml() + output;
    ui->textBrowser->setHtml(output);
  }
  enableUiElements(true);
}

/**
 * @brief MainWindow::clearClipboard remove clipboard contents.
 */
void MainWindow::clearClipboard() {
  QClipboard *clipboard = QApplication::clipboard();
  bool cleared = false;
  if (this->clippedText == clipboard->text(QClipboard::Selection)) {
    clipboard->clear(QClipboard::Clipboard);
    cleared = true;
  }
  if (this->clippedText == clipboard->text(QClipboard::Clipboard)) {
    clipboard->clear(QClipboard::Clipboard);
    cleared = true;
  }
  if (cleared) {
    ui->statusBar->showMessage(tr("Clipboard cleared"), 2000);
  } else {
    ui->statusBar->showMessage(tr("Clipboard not cleared"), 2000);
  }
  this->clippedText.clear();
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
 * @brief MainWindow::processFinished background process has finished
 * @param exitCode
 * @param exitStatus
 * @param output    stdout from a process
 * @param errout    stderr from a process
 */
void MainWindow::processFinished(const QString &p_output,
                                 const QString &p_errout) {
  DisplayInTextBrowser(p_output);
  //    Sometimes there is error output even with 0 exit code, which is
  //    assumed in this function
  processErrorExit(0, p_errout);
  enableUiElements(true);
}

/**
 * @brief MainWindow::enableUiElements enable or disable the relevant UI
 * elements
 * @param state
 */
void MainWindow::enableUiElements(bool state) {
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
  //  QList<int> splitter = ui->splitter->sizes();
  //  int left = QtPassSettings::getSplitterLeft(splitter[0]);
  //  int right = QtPassSettings::getSplitterRight(splitter[1]);
  //  if (left > 0 || right > 0) {
  //    splitter[0] = left;
  //    splitter[1] = right;
  //    ui->splitter->setSizes(splitter);
  //  }
  if (QtPassSettings::isMaximized(isMaximized())) {
    showMaximized();
  }
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
  ui->textBrowser->setTextColor(Qt::black);
  enableUiElements(true);
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
  dbg() << "on_lineEdit_returnPressed";
  selectFirstFile();
  on_treeView_clicked(ui->treeView->currentIndex());
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
  connect(QtPassSettings::getPass(), &Pass::finishedShow, &d,
          &PasswordDialog::setPass);

  if (!d.exec()) {
    this->ui->treeView->setFocus();
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
    QDirIterator it(model.rootPath() + "/" + file,
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
  QList<UserInfo> users = QtPassSettings::getPass()->listKeys();
  if (users.size() == 0) {
    QMessageBox::critical(this, tr("Can not get key list"),
                          tr("Unable to get list of available gpg keys"));
    return;
  }
  QList<UserInfo> secret_keys = QtPassSettings::getPass()->listKeys("", true);
  foreach (const UserInfo &sec, secret_keys) {
    for (QList<UserInfo>::iterator it = users.begin(); it != users.end(); ++it)
      if (sec.key_id == it->key_id)
        it->have_secret = true;
  }
  QList<UserInfo> selected_users;
  QString dir =
      currentDir.isEmpty()
          ? Util::getDir(ui->treeView->currentIndex(), false, model, proxyModel)
          : currentDir;
  int count = 0;
  QString recipients = QtPassSettings::getPass()->getRecipientString(
      dir.isEmpty() ? "" : dir, " ", &count);
  if (!recipients.isEmpty())
    selected_users = QtPassSettings::getPass()->listKeys(recipients);
  foreach (const UserInfo &sel, selected_users) {
    for (QList<UserInfo>::iterator it = users.begin(); it != users.end(); ++it)
      if (sel.key_id == it->key_id)
        it->enabled = true;
  }
  if (count > selected_users.size()) {
    // Some keys seem missing from keyring, add them separately
    QStringList recipients =
        QtPassSettings::getPass()->getRecipientList(dir.isEmpty() ? "" : dir);
    foreach (const QString recipient, recipients) {
      if (QtPassSettings::getPass()->listKeys(recipient).size() < 1) {
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

  QtPassSettings::getPass()->Init(dir, users);
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
  QList<UserInfo> keys = QtPassSettings::getPass()->listKeys("", true);
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
  QtPassSettings::getPass()->GenerateGPGKeys(batch);
}

/**
 * @brief MainWindow::updateProfileBox update the list of profiles, optionally
 * select a more appropriate one to view too
 */
void MainWindow::updateProfileBox() {
  QHash<QString, QString> profiles = QtPassSettings::getProfiles();

  if (profiles.isEmpty()) {
    ui->profileBox->hide();
  } else {
    ui->profileBox->show();
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
  if (startupPhase || name == QtPassSettings::getProfile())
    return;
  QtPassSettings::setProfile(name);

  QtPassSettings::setPassStore(QtPassSettings::getProfiles()[name]);
  ui->statusBar->showMessage(tr("Profile changed to %1").arg(name), 2000);

  QtPassSettings::getPass()->updateEnv();

  ui->treeView->setRootIndex(proxyModel.mapFromSource(
      model.setRootPath(QtPassSettings::getPassStore())));
}

/**
 * @brief MainWindow::initTrayIcon show a nice tray icon on systems that
 * support
 * it
 */
void MainWindow::initTrayIcon() {
  if (tray != NULL) {
    dbg() << "Creating tray icon again?";
    return;
  }
  if (QSystemTrayIcon::isSystemTrayAvailable() == true) {
    // Setup tray icon
    this->tray = new TrayIcon(this);
    if (tray == NULL)
      dbg() << "Allocating tray icon failed.";
  } else {
    dbg() << "No tray icon for this OS possibly also not show options?";
  }
}

/**
 * @brief MainWindow::destroyTrayIcon remove that pesky tray icon
 */
void MainWindow::destroyTrayIcon() {
  if (tray == NULL) {
    dbg() << "Destroy non existing tray icon?";
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
    //    QtPassSettings::setSplitterLeft(ui->splitter->sizes()[0]);
    //    QtPassSettings::setSplitterRight(ui->splitter->sizes()[1]);
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
    connect(openFolder, SIGNAL(triggered()), this, SLOT(openFolder()));
    connect(addFolder, SIGNAL(triggered()), this, SLOT(addFolder()));
    connect(addPassword, SIGNAL(triggered()), this, SLOT(addPassword()));
    connect(users, SIGNAL(triggered()), this, SLOT(onUsers()));
  } else if (fileOrFolder.isFile()) {
    QAction *edit = contextMenu.addAction(tr("Edit"));
    connect(edit, SIGNAL(triggered()), this, SLOT(onEdit()));
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
    connect(deleteItem, SIGNAL(triggered()), this, SLOT(onDelete()));
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

/**
 * @brief MainWindow::copyTextToClipboard copies text to your clipboard
 * @param text
 */
void MainWindow::copyTextToClipboard(const QString &text) {
  QClipboard *clip = QApplication::clipboard();
  if (!QtPassSettings::isUseSelection()) {
    clip->setText(text, QClipboard::Clipboard);
  } else {
    clip->setText(text, QClipboard::Selection);
  }
  clippedText = text;
  ui->statusBar->showMessage(tr("Copied to clipboard"), 2000);
  if (QtPassSettings::isUseAutoclear()) {
    clearClipboardTimer.start();
  }
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
  copyTextToClipboard(tokens[0]);
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
  enableUiElements(false);
  ui->treeView->setDisabled(true);
}

/**
 * @brief MainWindow::endReencryptPath re-enable ui elements
 */
void MainWindow::endReencryptPath() { enableUiElements(true); }

/**
 * @brief MainWindow::critical critical message popup wrapper.
 * @param title
 * @param msg
 */
void MainWindow::critical(QString title, QString msg) {
  QMessageBox::critical(this, title, msg);
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

void MainWindow::enableGitButtons(const bool &state) {
  // Following GNOME guidelines is preferable disable buttons instead of hide
  ui->actionPush->setEnabled(state);
  ui->actionUpdate->setEnabled(state);
}
