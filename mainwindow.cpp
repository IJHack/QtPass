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
#include "ui_mainwindow.h"
#include "usersdialog.h"
#include "util.h"

/**
 * @brief MainWindow::MainWindow handles all of the main functionality and also
 * the main window.
 * @param parent
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), process(new QProcess(this)),
      fusedav(this), keygen(NULL), tray(NULL) {
  // connect(process.data(), SIGNAL(readyReadStandardOutput()), this,
  // SLOT(readyRead()));
  connect(process.data(), SIGNAL(error(QProcess::ProcessError)), this,
          SLOT(processError(QProcess::ProcessError)));
  connect(process.data(), SIGNAL(finished(int, QProcess::ExitStatus)), this,
          SLOT(processFinished(int, QProcess::ExitStatus)));
  ui->setupUi(this);
  enableUiElements(true);
  wrapperRunning = false;
  execQueue = new QQueue<execQueueItem>;
  ui->statusBar->showMessage(tr("Welcome to QtPass %1").arg(VERSION), 2000);
  freshStart = true;
  startupPhase = true;
  autoclearTimer = NULL;
  pwdConfig.selected = 0;
  if (!checkConfig()) {
    // no working config
    QApplication::quit();
  }
  ui->copyPasswordButton->setEnabled(false);
  setClippedPassword("");
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
  qsrand(QDateTime::currentDateTime().toTime_t());

#if QT_VERSION >= 0x050200
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
  if (useWebDav)
    WNetCancelConnection2A(passStore.toUtf8().constData(), 0, 1);
#else
  if (fusedav.state() == QProcess::Running) {
    fusedav.terminate();
    fusedav.waitForFinished(2000);
  }
#endif
}

/**
 * @brief MainWindow::getSettings make sure to only have one set of settings.
 * @return QScopedPointer<QSettings> settings
 */
QSettings &MainWindow::getSettings() {
  if (!settings) {
    QString portable_ini = QCoreApplication::applicationDirPath() +
                           QDir::separator() + "qtpass.ini";
    // qDebug() << "Settings file: " + portable_ini;
    if (QFile(portable_ini).exists()) {
      // qDebug() << "Settings file exists, loading it in";
      settings.reset(new QSettings(portable_ini, QSettings::IniFormat));
    } else {
      // qDebug() << "Settings file does not exist, use defaults";
      settings.reset(new QSettings("IJHack", "QtPass"));
    }
  }
  return *settings;
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
  netres.lpRemoteName = webDavUrl.toUtf8().data();
  DWORD size = sizeof(dst);
  DWORD r = WNetUseConnectionA(
      reinterpret_cast<HWND>(effectiveWinId()), &netres,
      webDavPassword.toUtf8().constData(), webDavUser.toUtf8().constData(),
      CONNECT_TEMPORARY | CONNECT_INTERACTIVE | CONNECT_REDIRECT, dst, &size,
      0);
  if (r == NO_ERROR) {
    passStore = dst;
  } else {
    char message[256] = {0};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, r, 0, message,
                   sizeof(message), 0);
    ui->textBrowser->setTextColor(Qt::red);
    ui->textBrowser->setText(tr("Failed to connect WebDAV:\n") + message +
                             " (0x" + QString::number(r, 16) + ")");
  }
#else
  fusedav.start("fusedav -o nonempty -u \"" + webDavUser + "\" " + webDavUrl +
                " \"" + passStore + '"');
  fusedav.waitForStarted();
  if (fusedav.state() == QProcess::Running) {
    QString pwd = webDavPassword;
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
  QSettings &settings(getSettings());

  QString version = settings.value("version").toString();

  if (freshStart) {
    settings.beginGroup("mainwindow");
    restoreGeometry(settings.value("geometry", saveGeometry()).toByteArray());
    restoreState(settings.value("savestate", saveState()).toByteArray());
    move(settings.value("pos", pos()).toPoint());
    resize(settings.value("size", size()).toSize());
    QList<int> splitter = ui->splitter->sizes();
    int left = settings.value("splitterLeft", splitter[0]).toInt();
    int right = settings.value("splitterRight", splitter[1]).toInt();
    if (left > 0 || right > 0) {
      splitter[0] = left;
      splitter[1] = right;
      ui->splitter->setSizes(splitter);
    }
    if (settings.value("maximized", isMaximized()).toBool())
      showMaximized();
    settings.endGroup();
  }

  usePass = (settings.value("usePass") == "true");

  useClipboard = CLIPBOARD_NEVER;
  if (settings.value("useClipboard") == "true" ||
      settings.value("useClipboard") == "1")
    useClipboard = CLIPBOARD_ALWAYS;
  else if (settings.value("useClipboard") == "2")
    useClipboard = CLIPBOARD_ON_DEMAND;
  useAutoclear = (settings.value("useAutoclear") == "true");
  autoclearSeconds = settings.value("autoclearSeconds").toInt();
  useAutoclearPanel = (settings.value("useAutoclearPanel") == "true");
  autoclearPanelSeconds = settings.value("autoclearPanelSeconds").toInt();
  hidePassword = (settings.value("hidePassword") == "true");
  hideContent = (settings.value("hideContent") == "true");
  addGPGId = (settings.value("addGPGId") != "false");

  passStore = settings.value("passStore").toString();
  if (passStore.isEmpty()) {
    passStore = Util::findPasswordStore();
    settings.setValue("passStore", passStore);
  }
  // ensure directory exists if never used pass or misconfigured.
  // otherwise process->setWorkingDirectory(passStore); will fail on execution.
  QDir().mkdir(passStore);

  passStore = Util::normalizeFolderPath(passStore);

  passExecutable = settings.value("passExecutable").toString();
  if (passExecutable.isEmpty())
    passExecutable = Util::findBinaryInPath("pass");

  gitExecutable = settings.value("gitExecutable").toString();
  if (gitExecutable.isEmpty())
    gitExecutable = Util::findBinaryInPath("git");

  gpgExecutable = settings.value("gpgExecutable").toString();
  if (gpgExecutable.isEmpty())
    gpgExecutable = Util::findBinaryInPath("gpg2");

  pwgenExecutable = settings.value("pwgenExecutable").toString();
  if (pwgenExecutable.isEmpty())
    pwgenExecutable = Util::findBinaryInPath("pwgen");

  gpgHome = settings.value("gpgHome").toString();

  useWebDav = (settings.value("useWebDav") == "true");
  webDavUrl = settings.value("webDavUrl").toString();
  webDavUser = settings.value("webDavUser").toString();
  webDavPassword = settings.value("webDavPassword").toString();

  profile = settings.value("profile").toString();
  settings.beginGroup("profiles");
  QStringList keys = settings.childKeys();
  foreach (QString key, keys)
    profiles[key] = settings.value(key).toString();
  settings.endGroup();

  useGit = (settings.value("useGit") == "true");
  usePwgen = (settings.value("usePwgen") == "true");
  avoidCapitals = settings.value("avoidCapitals").toBool();
  avoidNumbers = settings.value("avoidNumbers").toBool();
  lessRandom = settings.value("lessRandom").toBool();
  useSymbols = (settings.value("useSymbols") == "true");
  pwdConfig.selected = settings.value("passwordCharsSelected").toInt();
  pwdConfig.length = settings.value("passwordLength").toInt();
  pwdConfig.selected = settings.value("passwordCharsselection").toInt();
  pwdConfig.Characters[3] = settings.value("passwordChars").toString();

  useTrayIcon = settings.value("useTrayIcon").toBool();
  hideOnClose = settings.value("hideOnClose").toBool();
  startMinimized = settings.value("startMinimized").toBool();
  alwaysOnTop = settings.value("alwaysOnTop").toBool();

  if (alwaysOnTop) {
    Qt::WindowFlags flags = windowFlags();
    this->setWindowFlags(flags | Qt::WindowStaysOnTopHint);
    this->show();
  }

  autoPull = settings.value("autoPull").toBool();
  autoPush = settings.value("autoPush").toBool();

  if (useTrayIcon && tray == NULL) {
    initTrayIcon();
    if (freshStart && startMinimized) {
      // since we are still in constructor, can't directly hide
      QTimer::singleShot(10, this, SLOT(hide()));
    }
  } else if (!useTrayIcon && tray != NULL) {
    destroyTrayIcon();
  }

  passTemplate = settings.value("passTemplate").toString();
  useTemplate = settings.value("useTemplate").toBool();
  templateAllFields = settings.value("templateAllFields").toBool();

  // qDebug() << version;

  // Config updates
  if (version.isEmpty()) {
    qDebug() << "assuming fresh install";
    if (autoclearSeconds < 5)
      autoclearSeconds = 10;
    if (autoclearPanelSeconds < 5)
      autoclearPanelSeconds = 10;
    if (!pwgenExecutable.isEmpty())
      usePwgen = true;
    else
      usePwgen = false;
    passTemplate = "login\nurl";
  } else {
    // QStringList ver = version.split(".");
    // qDebug() << ver;
    // if (ver[0] == "0" && ver[1] == "8") {
    //// upgrade to 0.9
    // }
    if (passTemplate.isEmpty())
      passTemplate = "login\nurl";
  }

  settings.setValue("version", VERSION);

  if (Util::checkConfig(passStore, passExecutable, gpgExecutable)) {
    config();
    if (freshStart &&
        Util::checkConfig(passStore, passExecutable, gpgExecutable))
      return false;
  }

  freshStart = false;

  // TODO(annejan): this needs to be before we try to access the store,
  // but it would be better to do it after the Window is shown,
  // as the long delay it can cause is irritating otherwise.
  if (useWebDav)
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

  ui->textBrowser->setOpenExternalLinks(true);
  ui->textBrowser->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->textBrowser, SIGNAL(customContextMenuRequested(const QPoint &)),
          this, SLOT(showBrowserContextMenu(const QPoint &)));

  updateProfileBox();

  env = QProcess::systemEnvironment();
  if (!gpgHome.isEmpty()) {
    QDir absHome(gpgHome);
    absHome.makeAbsolute();
    env << "GNUPGHOME=" + absHome.path();
  }
#ifdef __APPLE__
  // If it exists, add the gpgtools to PATH
  if (QFile("/usr/local/MacGPG2/bin").exists())
    env.replaceInStrings("PATH=", "PATH=/usr/local/MacGPG2/bin:");
  // Add missing /usr/local/bin
  if (env.filter("/usr/local/bin").isEmpty())
    env.replaceInStrings("PATH=", "PATH=/usr/local/bin:");

#endif
  // QMessageBox::information(this, "env", env.join("\n"));

  updateEnv();

  if (!useGit || (gitExecutable.isEmpty() && passExecutable.isEmpty())) {
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
  usePass = freshStart ? QFile(passExecutable).exists() : usePass;

  d->setPassPath(passExecutable);
  d->setGitPath(gitExecutable);
  d->setGpgPath(gpgExecutable);
  d->setStorePath(passStore);
  d->usePass(usePass);
  d->useClipboard(useClipboard);
  d->useAutoclear(useAutoclear);
  d->setAutoclear(autoclearSeconds);
  d->useAutoclearPanel(useAutoclearPanel);
  d->setAutoclearPanel(autoclearPanelSeconds);
  d->hidePassword(hidePassword);
  d->hideContent(hideContent);
  d->addGPGId(addGPGId);
  d->useTrayIcon(useTrayIcon);
  d->hideOnClose(hideOnClose);
  d->startMinimized(startMinimized);
  d->setProfiles(profiles, profile);
  d->useGit(useGit);
  d->setPwgenPath(pwgenExecutable);
  d->usePwgen(usePwgen);
  d->avoidCapitals(avoidCapitals);
  d->avoidNumbers(avoidNumbers);
  d->lessRandom(lessRandom);
  d->useSymbols(useSymbols);
  d->setPasswordLength(pwdConfig.length);
  d->setPwdTemplateSelector(pwdConfig.selected);
  if (pwdConfig.selected != 3)
    d->setLineEditEnabled(false);
  d->setPasswordChars(pwdConfig.Characters[pwdConfig.selected]);
  d->useTemplate(useTemplate);
  d->setTemplate(passTemplate);
  d->templateAllFields(templateAllFields);
  d->autoPull(autoPull);
  d->autoPush(autoPush);
  d->alwaysOnTop(alwaysOnTop);
  if (startupPhase)
    d->wizard(); // does shit
  if (d->exec()) {
    if (d->result() == QDialog::Accepted) {
      passExecutable = d->getPassPath();
      gitExecutable = d->getGitPath();
      gpgExecutable = d->getGpgPath();
      passStore = Util::normalizeFolderPath(d->getStorePath());
      usePass = d->usePass();
      useClipboard = d->useClipboard();
      useAutoclear = d->useAutoclear();
      autoclearSeconds = d->getAutoclear();
      useAutoclearPanel = d->useAutoclearPanel();
      autoclearPanelSeconds = d->getAutoclearPanel();
      hidePassword = d->hidePassword();
      hideContent = d->hideContent();
      addGPGId = d->addGPGId();
      useTrayIcon = d->useTrayIcon();
      hideOnClose = d->hideOnClose();
      startMinimized = d->startMinimized();
      profiles = d->getProfiles();
      useGit = d->useGit();
      pwgenExecutable = d->getPwgenPath();
      usePwgen = d->usePwgen();
      avoidCapitals = d->avoidCapitals();
      avoidNumbers = d->avoidNumbers();
      lessRandom = d->lessRandom();
      useSymbols = d->useSymbols();
      pwdConfig.length = d->getPasswordLength();
      pwdConfig.selected = d->getPwdTemplateSelector();
      pwdConfig.Characters[3] = d->getPasswordChars();
      useTemplate = d->useTemplate();
      passTemplate = d->getTemplate();
      templateAllFields = d->templateAllFields();
      autoPush = d->autoPush();
      autoPull = d->autoPull();
      alwaysOnTop = d->alwaysOnTop();

      QSettings &settings(getSettings());

      settings.setValue("version", VERSION);
      settings.setValue("passExecutable", passExecutable);
      settings.setValue("gitExecutable", gitExecutable);
      settings.setValue("gpgExecutable", gpgExecutable);
      settings.setValue("passStore", passStore);
      settings.setValue("usePass", usePass ? "true" : "false");
      switch (useClipboard) {
      case CLIPBOARD_ALWAYS:
        settings.setValue("useClipboard", "true");
        break;
      case CLIPBOARD_ON_DEMAND:
        settings.setValue("useClipboard", "2");
        break;
      default:
        settings.setValue("useClipboard", "false");
        break;
      }
      settings.setValue("useAutoclear", useAutoclear ? "true" : "false");
      settings.setValue("autoclearSeconds", autoclearSeconds);
      settings.setValue("useAutoclearPanel",
                        useAutoclearPanel ? "true" : "false");
      settings.setValue("autoclearPanelSeconds", autoclearPanelSeconds);
      settings.setValue("hidePassword", hidePassword ? "true" : "false");
      settings.setValue("hideContent", hideContent ? "true" : "false");
      settings.setValue("addGPGId", addGPGId ? "true" : "false");
      settings.setValue("useTrayIcon", useTrayIcon ? "true" : "false");
      settings.setValue("hideOnClose", hideOnClose ? "true" : "false");
      settings.setValue("startMinimized", startMinimized ? "true" : "false");
      settings.setValue("useGit", useGit ? "true" : "false");
      settings.setValue("pwgenExecutable", pwgenExecutable);
      settings.setValue("usePwgen", usePwgen ? "true" : "false");
      settings.setValue("avoidCapitals", avoidCapitals ? "true" : "false");
      settings.setValue("avoidNumbers", avoidNumbers ? "true" : "false");
      settings.setValue("lessRandom", lessRandom ? "true" : "false");
      settings.setValue("useSymbols", useSymbols ? "true" : "false");
      settings.setValue("passwordLength", pwdConfig.length);
      settings.setValue("passwordCharsselection", pwdConfig.selected);
      settings.setValue("passwordChars", pwdConfig.Characters[3]);
      settings.setValue("useTemplate", useTemplate);
      settings.setValue("passTemplate", passTemplate);
      settings.setValue("templateAllFields", templateAllFields);
      settings.setValue("autoPull", autoPull ? "true" : "false");
      settings.setValue("autoPush", autoPush ? "true" : "false");
      settings.setValue("alwaysOnTop", alwaysOnTop ? "true" : "false");

      if (alwaysOnTop) {
        Qt::WindowFlags flags = windowFlags();
        this->setWindowFlags(flags | Qt::WindowStaysOnTopHint);
        this->show();
      } else {
        this->setWindowFlags(Qt::Window);
        this->show();
      }

      if (!profiles.isEmpty()) {
        settings.beginGroup("profiles");
        settings.remove("");
        bool profileExists = false;
        QHashIterator<QString, QString> i(profiles);
        while (i.hasNext()) {
          i.next();
          // qDebug() << i.key() + "|" + i.value();
          if (i.key() == profile)
            profileExists = true;
          settings.setValue(i.key(), i.value());
        }
        if (!profileExists) {
          // just take the last one
          profile = i.key();
        }
        settings.endGroup();
      } else {
        settings.remove("profiles");
      }
      updateProfileBox();
      ui->treeView->setRootIndex(
          proxyModel.mapFromSource(model.setRootPath(passStore)));

      if (freshStart &&
          Util::checkConfig(passStore, passExecutable, gpgExecutable))
        config();
      updateEnv();
      if (!useGit || (gitExecutable.isEmpty() && passExecutable.isEmpty())) {
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
      if (useTrayIcon && tray == NULL)
        initTrayIcon();
      else if (!useTrayIcon && tray != NULL)
        destroyTrayIcon();
    }
    freshStart = false;
  }
}

/**
 * @brief MainWindow::on_updateButton_clicked do a git pull
 */
void MainWindow::on_updateButton_clicked() {
  ui->statusBar->showMessage(tr("Updating password-store"), 2000);
  currentAction = GIT;
  if (usePass)
    executePass("git pull");
  else
    executeWrapper(gitExecutable, "pull");
}

/**
 * @brief MainWindow::on_pushButton_clicked do a git push
 */
void MainWindow::on_pushButton_clicked() {
  ui->statusBar->showMessage(tr("Updating password-store"), 2000);
  currentAction = GIT;
  if (usePass)
    executePass("git push");
  else
    executeWrapper(gitExecutable, "push");
}

/**
 * @brief MainWindow::getDir get selectd folder path
 * @param index
 * @param forPass short or full path
 * @return path
 */
QString MainWindow::getDir(const QModelIndex &index, bool forPass) {
  QString abspath = QDir(passStore).absolutePath() + '/';
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
 * @param forPass short or full path
 * @return path
 * @return
 */
QString MainWindow::getFile(const QModelIndex &index, bool forPass) {
  if (!index.isValid() ||
      !model.fileInfo(proxyModel.mapToSource(index)).isFile())
    return QString();
  QString filePath = model.filePath(proxyModel.mapToSource(index));
  if (forPass) {
    filePath = QDir(passStore).relativeFilePath(filePath);
    filePath.replace(QRegExp("\\.gpg$"), "");
  }
  return filePath;
}

/**
 * @brief MainWindow::on_treeView_clicked read the selected password file
 * @param index
 */
void MainWindow::on_treeView_clicked(const QModelIndex &index) {
  currentDir = getDir(ui->treeView->currentIndex(), false);
  lastDecrypt = "Could not decrypt";
  setClippedPassword("");
  QString file = getFile(index, usePass);
  if (!file.isEmpty()) {
    currentAction = GPG;
    if (usePass)
      executePass("show \"" + file + '"');
    else
      executeWrapper(gpgExecutable,
                     "-d --quiet --yes --no-encrypt-to --batch --use-agent \"" +
                         file + '"');
  } else {
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
    QString file = getFile(index, usePass);
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
 * @brief MainWindow::executePass easy wrapper for running pass
 * @param args
 */
void MainWindow::executePass(QString args, QString input) {
  executeWrapper(passExecutable, args, input);
}

/**
 * @brief MainWindow::executePassGitInit git init wrapper
 */
void MainWindow::executePassGitInit() {
  qDebug() << "Pass git init called";
  if (usePass)
    executePass("git init");
  else
    executeWrapper(gitExecutable, "init \"" + passStore + '"');
}

/**
 * @brief MainWindow::executeWrapper run an application, queue when needed.
 * @param app path to application to run
 * @param args required arguements
 * @param input optional input
 */
void MainWindow::executeWrapper(QString app, QString args, QString input) {
  // qDebug() << app + " " + args;
  // Happens a lot if e.g. git binary is not set.
  // This will result in bogus "QProcess::FailedToStart" messages,
  // also hiding legitimate errors from the gpg commands.
  if (app.isEmpty()) {
    qDebug() << "Trying to execute nothing..";
    return;
  }
  // Convert to absolute path, just in case
  app = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(app);
  if (wrapperRunning) {
    execQueueItem item;
    item.app = app;
    item.args = args;
    item.input = input;

    execQueue->enqueue(item);
    qDebug() << item.app + "," + item.args + "," + item.input;
    return;
  }
  wrapperRunning = true;
  process->setWorkingDirectory(passStore);
  process->setEnvironment(env);
  clearTemplateWidgets();
  ui->textBrowser->clear();
  ui->textBrowser->setTextColor(Qt::black);
  enableUiElements(false);
  if (autoclearTimer != NULL) {
    autoclearTimer->stop();
    delete autoclearTimer;
    autoclearTimer = NULL;
  }
  process->start('"' + app + "\" " + args);
  if (!input.isEmpty())
    process->write(input.toUtf8());
  process->closeWriteChannel();
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
    error = process->readAllStandardError();
    QByteArray processOutBytes = process->readAllStandardOutput();
    QTextCodec *codec = QTextCodec::codecForLocale();
    output = codec->toUnicode(processOutBytes);
    if (finished && currentAction == GPG) {
      lastDecrypt = output;
      QStringList tokens = output.split("\n");

      if (useClipboard != CLIPBOARD_NEVER && !output.isEmpty()) {
        setClippedPassword(tokens[0]);
        if (useClipboard == CLIPBOARD_ALWAYS)
          copyPasswordToClipboard();
        if (useAutoclearPanel) {
          QTimer::singleShot(1000 * autoclearPanelSeconds, this,
                             SLOT(clearPanel()));
        }
        if (hidePassword && !useTemplate) {
          tokens[0] = "***" + tr("Password hidden") + "***";
          output = tokens.join("\n");
        }
        if (hideContent)
          output = "***" + tr("Content hidden") + "***";
      }

      if (useTemplate && !hideContent) {
        while (ui->formLayout->count() > 0) {
          QLayoutItem *item = ui->formLayout->takeAt(0);
          delete item->widget();
          delete item;
        }
        QLineEdit *pass = new QLineEdit();
        pass->setText(tokens[0]);
        tokens.pop_front();
        if (hidePassword)
          pass->setEchoMode(QLineEdit::Password);
        pass->setReadOnly(true);
        ui->formLayout->addRow(pass);

        for (int j = 0; j < tokens.length(); ++j) {
          QString token = tokens.at(j);
          if (token.contains(':')) {
            int colon = token.indexOf(':');
            QString field = token.left(colon);
            if (templateAllFields || passTemplate.contains(field)) {
              QString value = token.right(token.length() - colon - 1);
              if (!passTemplate.contains(field) && value.startsWith("//"))
                continue; // colon is probably from a url
              QLineEdit *line = new QLineEdit();
              line->setObjectName(field);
              line->setText(value);
              line->setReadOnly(true);
              ui->formLayout->addRow(new QLabel(field), line);
              tokens.removeAt(j);
              --j; // tokens.length() also got shortened by the remove..
            }
          }
        }
        if (ui->formLayout->count() == 0)
          ui->verticalLayoutPassword->setSpacing(0);
        else
          ui->verticalLayoutPassword->setSpacing(6);
        output = tokens.join("\n");
      } else {
        clearTemplateWidgets();
      }
      if (useAutoclearPanel) {
        autoclearPass = output;
        autoclearTimer = new QTimer(this);
        autoclearTimer->setSingleShot(true);
        autoclearTimer->setInterval(1000 * autoclearPanelSeconds);
        connect(autoclearTimer, SIGNAL(timeout()), this, SLOT(clearPanel()));
        autoclearTimer->start();
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
  if (clipboard->text() == getClippedPassword()) {
    clipboard->clear();
    ui->statusBar->showMessage(tr("Clipboard cleared"), 3000);
  } else {
    ui->statusBar->showMessage(tr("Clipboard not cleared"), 3000);
  }
}

/**
 * @brief MainWindow::clearPanel hide the information from shoulder surfers
 */
void MainWindow::clearPanel() {
  while (ui->formLayout->count() > 0) {
    QLayoutItem *item = ui->formLayout->takeAt(0);
    delete item->widget();
    delete item;
  }
  QString output = "***" + tr("Password and Content hidden") + "***";
  ui->textBrowser->setHtml(output);
}

/**
 * @brief MainWindow::processFinished process is finished, if there is another
 * one queued up to run, start it.
 * @param exitCode
 * @param exitStatus
 */
void MainWindow::processFinished(int exitCode,
                                 QProcess::ExitStatus exitStatus) {
  wrapperRunning = false;
  bool error = exitStatus != QProcess::NormalExit || exitCode > 0;
  readyRead(true);
  enableUiElements(true);
  if (!error && currentAction == EDIT)
    on_treeView_clicked(ui->treeView->currentIndex());
  if (!execQueue->isEmpty()) {
    execQueueItem item = execQueue->dequeue();
    executeWrapper(item.app, item.args, item.input);
  }
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
  if (process->state() == QProcess::NotRunning)
    enableUiElements(true);
}

/**
 * @brief MainWindow::setPassExecutable set the executable for pass eg:
 * /usr/local/bin/pass
 * @param path
 */
void MainWindow::setPassExecutable(QString path) { passExecutable = path; }

/**
 * @brief MainWindow::setGitExecutable set the executable for git eg:
 * /usr/local/bin/git
 * @param path
 */
void MainWindow::setGitExecutable(QString path) { gitExecutable = path; }

/**
 * @brief MainWindow::setGpgExecutable set the executable for gpg eg:
 * /usr/local/bin/gig
 * @param path
 */
void MainWindow::setGpgExecutable(QString path) { gpgExecutable = path; }

/**
 * @brief MainWindow::getGpgExecutable
 * @return path to the gpg executable
 */
QString MainWindow::getGpgExecutable() { return gpgExecutable; }

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
  ui->treeView->setRootIndex(
      proxyModel.mapFromSource(model.setRootPath(passStore)));
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
  QModelIndex index = proxyModel.mapFromSource(model.setRootPath(passStore));
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
 * @brief MainWindow::getRecipientList return list op gpg-id's to encrypt for
 * @param for_file which file (folder) would you like recepients for
 * @return recepients gpg-id contents
 */
QStringList MainWindow::getRecipientList(QString for_file) {
  QDir gpgIdPath(QFileInfo(for_file.startsWith(passStore)
                               ? for_file
                               : passStore + for_file)
                     .absoluteDir());
  bool found = false;
  while (gpgIdPath.exists() && gpgIdPath.absolutePath().startsWith(passStore)) {
    if (QFile(gpgIdPath.absoluteFilePath(".gpg-id")).exists()) {
      found = true;
      break;
    }
    if (!gpgIdPath.cdUp())
      break;
  }
  QFile gpgId(found ? gpgIdPath.absoluteFilePath(".gpg-id")
                    : passStore + ".gpg-id");
  if (!gpgId.open(QIODevice::ReadOnly | QIODevice::Text))
    return QStringList();
  QStringList recipients;
  while (!gpgId.atEnd()) {
    QString recipient(gpgId.readLine());
    recipient = recipient.trimmed();
    if (!recipient.isEmpty())
      recipients += recipient;
  }
  return recipients;
}

/**
 * @brief MainWindow::getRecipientString formated string for use with GPG
 * @param for_file which file (folder) would you like recepients for
 * @param separator formating separator eg: " -r "
 * @param count
 * @return recepient string
 */
QString MainWindow::getRecipientString(QString for_file, QString separator,
                                       int *count) {
  QString recipients_str;
  QStringList recipients_list = getRecipientList(for_file);
  if (count)
    *count = recipients_list.size();
  foreach (const QString recipient, recipients_list)
    recipients_str += separator + '"' + recipient + '"';
  return recipients_str;
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
  PasswordDialog d(this);
  d.setFile(file);
  d.usePwgen(usePwgen);
  d.setTemplate(passTemplate);
  d.useTemplate(useTemplate);
  d.templateAll(templateAllFields);
  d.setPassword(lastDecrypt);
  d.setLength(pwdConfig.length);
  d.setPasswordCharTemplate(pwdConfig.selected);
  if (!d.exec()) {
    d.setPassword(NULL);
    return;
  }
  QString newValue = d.getPassword();
  if (newValue.isEmpty())
    return;

  if (newValue.right(1) != "\n")
    newValue += "\n";

  currentAction = EDIT;
  if (usePass) {
    QString force(overwrite ? " -f " : " ");
    executePass("insert" + force + "-m \"" + file + '"', newValue);
  } else {
    QString recipients = getRecipientString(file, " -r ");
    if (recipients.isEmpty()) {
      QMessageBox::critical(this, tr("Can not edit"),
                            tr("Could not read encryption key to use, .gpg-id "
                               "file missing or invalid."));
      return;
    }
    QString force(overwrite ? " --yes " : " ");
    executeWrapper(gpgExecutable, force + "--batch -eq --output \"" + file +
                                      "\" " + recipients + " -",
                   newValue);
    if (!useWebDav && useGit) {
      if (!overwrite)
        executeWrapper(gitExecutable, "add \"" + file + '"');
      QString path = QDir(passStore).relativeFilePath(file);
      path.replace(QRegExp("\\.gpg$"), "");
      executeWrapper(gitExecutable, "commit \"" + file + "\" -m \"" +
                                        (overwrite ? "Edit" : "Add") + " for " +
                                        path + " using QtPass.\"");
      if (autoPush)
        on_pushButton_clicked();
    }
  }
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
  QString dir = getDir(ui->treeView->currentIndex(), usePass);
  QString file = QInputDialog::getText(
      this, tr("New file"),
      tr("New password file: \n(Will be placed in %1 )")
          .arg(passStore + getDir(ui->treeView->currentIndex(), true)),
      QLineEdit::Normal, "", &ok);
  if (!ok || file.isEmpty())
    return;
  file = dir + file;
  if (!usePass)
    file += ".gpg";
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

  if (fileOrFolder.isFile()) {
    file = getFile(ui->treeView->currentIndex(), usePass);
    if (QMessageBox::question(
            this, tr("Delete password?"),
            tr("Are you sure you want to delete %1?")
                .arg(QDir::separator() +
                     getFile(ui->treeView->currentIndex(), true)),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
      return;
    if (usePass) {
      currentAction = DELETE;
      executePass("rm -f \"" + file + '"');
      if (useGit && autoPush)
        on_pushButton_clicked();
    } else {
      if (useGit) {
        executeWrapper(gitExecutable, "rm -f \"" + file + '"');
        executeWrapper(gitExecutable,
                       "commit \"" + file + "\" -m \"Remove for " +
                           getFile(ui->treeView->currentIndex(), true) +
                           " using QtPass.\"");
        if (autoPush)
          on_pushButton_clicked();
      } else {
        QFile(file).remove();
      }
    }
  } else {
    file = getDir(ui->treeView->currentIndex(), usePass);
    // TODO: message box should accept enter key
    if (QMessageBox::question(
            this, tr("Delete folder?"),
            tr("Are you sure you want to delete %1?")
                .arg(QDir::separator() +
                     getDir(ui->treeView->currentIndex(), true)),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
      return;
    } else {
      if (usePass) {
        currentAction = DELETE;
        executePass("rm -rf \"" + file + '"');
        if (useGit && autoPush) {
          on_pushButton_clicked();
        }
      } else {
        if (useGit) {
          executeWrapper(gitExecutable, "rm -rf \"" + file + '"');
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
          QDir dir(file);
          dir.removeRecursively();
#else
          removeDir(passStore + file);
#endif
          executeWrapper(gitExecutable,
                         "commit \"" + file + "\" -m \"Remove for " +
                             getFile(ui->treeView->currentIndex(), true) +
                             " using QtPass.\"");
          if (autoPush) {
            on_pushButton_clicked();
          }
        } else {
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
          QDir dir(file);
          dir.removeRecursively();
#else
          removeDir(passStore + file);
#endif
        }
      }
    }
  }
  lastDecrypt = "";
}

/**
 * @brief MainWindow::removeDir delete folder recursive.
 * @param dirName which folder.
 * @return was removal succesful?
 */
bool MainWindow::removeDir(const QString &dirName) {
  bool result = true;
  QDir dir(dirName);

  if (dir.exists(dirName)) {
    Q_FOREACH (QFileInfo info,
               dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System |
                                     QDir::Hidden | QDir::AllDirs | QDir::Files,
                                 QDir::DirsFirst)) {
      if (info.isDir())
        result = removeDir(info.absoluteFilePath());
      else
        result = QFile::remove(info.absoluteFilePath());

      if (!result)
        return result;
    }
    result = dir.rmdir(dirName);
  }
  return result;
}

/**
 * @brief MainWindow::on_editButton_clicked try and edit (selected) password.
 */
void MainWindow::on_editButton_clicked() {
  QString file = getFile(ui->treeView->currentIndex(), usePass);
  if (file.isEmpty()) {
    QMessageBox::critical(
        this, tr("Can not edit"),
        tr("Selected password file does not exist, not able to edit"));
    return;
  }
  setPassword(file, true);
}

/**
 * @brief MainWindow::listKeys list users
 * @param keystring
 * @param secret list private keys
 * @return QList<UserInfo> users
 */
QList<UserInfo> MainWindow::listKeys(QString keystring, bool secret) {
  waitFor(5);
  QList<UserInfo> users;
  currentAction = GPG_INTERNAL;
  QString listopt = secret ? "--list-secret-keys " : "--list-keys ";
  executeWrapper(gpgExecutable,
                 "--no-tty --with-colons " + listopt + keystring);
  process->waitForFinished(2000);
  if (process->exitStatus() != QProcess::NormalExit)
    return users;
  QByteArray processOutBytes = process->readAllStandardOutput();
  QTextCodec *codec = QTextCodec::codecForLocale();
  QString processOutString = codec->toUnicode(processOutBytes);
  QStringList keys = QString(processOutString)
                         .split(QRegExp("[\r\n]"), QString::SkipEmptyParts);
  UserInfo current_user;
  foreach (QString key, keys) {
    QStringList props = key.split(':');
    if (props.size() < 10)
      continue;
    if (props[0] == (secret ? "sec" : "pub")) {
      if (!current_user.key_id.isEmpty())
        users.append(current_user);
      current_user = UserInfo();
      current_user.key_id = props[4];
      current_user.name = props[9].toUtf8();
      current_user.validity = props[1][0].toLatin1();
      current_user.created.setTime_t(props[5].toInt());
      current_user.expiry.setTime_t(props[6].toInt());
    } else if (current_user.name.isEmpty() && props[0] == "uid") {
      current_user.name = props[9];
    }
  }
  if (!current_user.key_id.isEmpty())
    users.append(current_user);
  return users;
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
      getRecipientString(dir.isEmpty() ? "" : dir, " ", &count);
  if (!recipients.isEmpty())
    selected_users = listKeys(recipients);
  foreach (const UserInfo &sel, selected_users) {
    for (QList<UserInfo>::iterator it = users.begin(); it != users.end(); ++it)
      if (sel.key_id == it->key_id)
        it->enabled = true;
  }
  if (count > selected_users.size()) {
    // Some keys seem missing from keyring, add them separately
    QStringList recipients = getRecipientList(dir.isEmpty() ? "" : dir);
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

  if (usePass) {
    QString gpgIds = "";
    foreach (const UserInfo &user, users) {
      if (user.enabled) {
        gpgIds += user.key_id + " ";
      }
    }
    executePass("init --path=" + dir + " " + gpgIds);
  } else {
    QString gpgIdFile = dir + ".gpg-id";
    QFile gpgId(gpgIdFile);
    bool addFile = false;
    if (addGPGId) {
      QFileInfo checkFile(gpgIdFile);
      if (!checkFile.exists() || !checkFile.isFile())
        addFile = true;
    }
    if (!gpgId.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QMessageBox::critical(this, tr("Cannot update"),
                            tr("Failed to open .gpg-id for writing."));
      return;
    }
    bool secret_selected = false;
    foreach (const UserInfo &user, users) {
      if (user.enabled) {
        gpgId.write((user.key_id + "\n").toUtf8());
        secret_selected |= user.have_secret;
      }
    }
    gpgId.close();
    if (!secret_selected) {
      QMessageBox::critical(
          this, tr("Check selected users!"),
          tr("None of the selected keys have a secret key available.\n"
             "You will not be able to decrypt any newly added passwords!"));
    }

    if (!useWebDav && useGit && !gitExecutable.isEmpty()) {
      if (addFile)
        executeWrapper(gitExecutable, "add \"" + gpgIdFile + '"');
      QString path = gpgIdFile;
      path.replace(QRegExp("\\.gpg$"), "");
      executeWrapper(gitExecutable, "commit \"" + gpgIdFile + "\" -m \"Added " +
                                        path + " using QtPass.\"");
      if (autoPush)
        on_pushButton_clicked();
    }

    reencryptPath(dir);
  }
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
 * @brief MainWindow::updateEnv update the execution environment (used when
 * switching profiles)
 */
void MainWindow::updateEnv() {
  QStringList store = env.filter("PASSWORD_STORE_DIR");
  // put PASSWORD_STORE_DIR in env
  if (store.isEmpty()) {
    // qDebug() << "Added PASSWORD_STORE_DIR";
    env.append("PASSWORD_STORE_DIR=" + passStore);
  } else {
    // qDebug() << "Update PASSWORD_STORE_DIR with " + passStore;
    env.replaceInStrings(store.first(), "PASSWORD_STORE_DIR=" + passStore);
  }
}

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
  executeWrapper(gpgExecutable, "--gen-key --no-tty --batch", batch);
  // TODO check status / error messages
  // https://github.com/IJHack/QtPass/issues/202#issuecomment-251081688
}

/**
 * @brief MainWindow::updateProfileBox update the list of profiles, optionally
 * select a more appropriate one to view too
 */
void MainWindow::updateProfileBox() {
  // qDebug() << profiles.size();
  if (profiles.isEmpty()) {
    ui->profileBox->hide();
  } else {
    ui->profileBox->show();
    if (profiles.size() < 2)
      ui->profileBox->setEnabled(false);
    else
      ui->profileBox->setEnabled(true);
    ui->profileBox->clear();
    QHashIterator<QString, QString> i(profiles);
    while (i.hasNext()) {
      i.next();
      if (!i.key().isEmpty())
        ui->profileBox->addItem(i.key());
    }
  }
  int index = ui->profileBox->findText(profile);
  if (index != -1) // -1 for not found
    ui->profileBox->setCurrentIndex(index);
}

/**
 * @brief MainWindow::on_profileBox_currentIndexChanged make sure we show the
 * correct "profile"
 * @param name
 */
void MainWindow::on_profileBox_currentIndexChanged(QString name) {
  if (startupPhase || name == profile)
    return;
  profile = name;

  passStore = profiles[name];
  ui->statusBar->showMessage(tr("Profile changed to %1").arg(name), 2000);

  QSettings &settings(getSettings());

  settings.setValue("profile", profile);
  settings.setValue("passStore", passStore);

  // qDebug() << env;
  QStringList store = env.filter("PASSWORD_STORE_DIR");
  // put PASSWORD_STORE_DIR in env
  if (store.isEmpty()) {
    // qDebug() << "Added PASSWORD_STORE_DIR";
    env.append("PASSWORD_STORE_DIR=" + passStore);
  } else {
    // qDebug() << "Update PASSWORD_STORE_DIR";
    env.replaceInStrings(store.first(), "PASSWORD_STORE_DIR=" + passStore);
  }

  ui->treeView->setRootIndex(
      proxyModel.mapFromSource(model.setRootPath(passStore)));
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
  if (hideOnClose) {
    this->hide();
    event->ignore();
  } else {
    clearClipboard();
    settings->beginGroup("mainwindow");
    settings->setValue("geometry", saveGeometry());
    settings->setValue("savestate", saveState());
    settings->setValue("maximized", isMaximized());
    if (!isMaximized()) {
      settings->setValue("pos", pos());
      settings->setValue("size", size());
    }
    settings->setValue("splitterLeft", ui->splitter->sizes()[0]);
    settings->setValue("splitterRight", ui->splitter->sizes()[1]);
    settings->endGroup();
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
 * @brief MainWindow::on_copyPasswordButton_clicked just a launcher for
 * MainWindow::copyPasswordToClipboard()
 */
void MainWindow::on_copyPasswordButton_clicked() { copyPasswordToClipboard(); }

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

  if (useClipboard != CLIPBOARD_NEVER) {
    contextMenu->addSeparator();
    QAction *copyItem = contextMenu->addAction(tr("Copy Password"));
    if (getClippedPassword().length() == 0)
      copyItem->setEnabled(false);
    connect(copyItem, SIGNAL(triggered()), this,
            SLOT(copyPasswordToClipboard()));
  }
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
          .arg(passStore + getDir(ui->treeView->currentIndex(), true)),
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
  if (useGit && autoPull)
    on_updateButton_clicked();
  waitFor(30);
  process->waitForFinished();
  // TODO(annejan) move to editbutton stuff possibly?
  currentDir = getDir(ui->treeView->currentIndex(), false);
  lastDecrypt = "Could not decrypt";
  QString file = getFile(ui->treeView->currentIndex(), usePass);
  if (!file.isEmpty()) {
    currentAction = GPG;
    if (usePass)
      executePass('"' + file + '"');
    else
      executeWrapper(gpgExecutable,
                     "-d --quiet --yes --no-encrypt-to --batch --use-agent \"" +
                         file + '"');
    process->waitForFinished(30000); // long wait (passphrase stuff)
    if (process->exitStatus() == QProcess::NormalExit)
      on_editButton_clicked();
  }
}

/**
 * @brief MainWindow::generatePassword use either pwgen or internal password
 * generator
 * @return the password
 */
// TODO Jounathaen Passwordlength as call parameter
QString MainWindow::generatePassword(int length, clipBoardType selection) {
  QString passwd;
  if (usePwgen) {
    waitFor(2);
    // --secure goes first as it overrides --no-* otherwise
    QString args = QString("-1 ") + (lessRandom ? "" : "--secure ") +
                   (avoidCapitals ? "--no-capitalize " : "--capitalize ") +
                   (avoidNumbers ? "--no-numerals " : "--numerals ") +
                   (useSymbols ? "--symbols " : "") + QString::number(length);
    currentAction = PWGEN;
    executeWrapper(pwgenExecutable, args);
    process->waitForFinished(1000);
    if (process->exitStatus() == QProcess::NormalExit)
      passwd =
          QString(process->readAllStandardOutput()).remove(QRegExp("[\\n\\r]"));
    else
      qDebug() << "pwgen fail";
  } else {
    int charsetLength = pwdConfig.Characters[selection].length();
    if (charsetLength > 0) {
      for (int i = 0; i < length; ++i) {
        int index = qrand() % charsetLength;
        QChar nextChar = pwdConfig.Characters[selection].at(index);
        passwd.append(nextChar);
      }
    } else {
      QMessageBox::critical(
          this, tr("No characters chosen"),
          tr("Can't generate password, there are no characters to choose from "
             "set in the configuration!"));
    }
  }
  return passwd;
}

/**
 * @brief MainWindow::waitFor wait until process->atEnd and execQueue->isEmpty
 * or timeout after x-seconds
 * @param seconds
 */
void MainWindow::waitFor(int seconds) {
  QDateTime current = QDateTime::currentDateTime();
  uint stop = current.toTime_t() + seconds;
  while (!process->atEnd() || !execQueue->isEmpty()) {
    current = QDateTime::currentDateTime();
    if (stop < current.toTime_t()) {
      QMessageBox::critical(
          this, tr("Timed out"),
          tr("Can't start process, previous one is still running!"));
      return;
    }
    Util::qSleep(100);
  }
}

/**
 * @brief MainWindow::clearTemplateWidgets empty the template widget fields in
 * the UI
 */
void MainWindow::clearTemplateWidgets() {
  while (ui->formLayout->count() > 0) {
    QLayoutItem *item = ui->formLayout->takeAt(0);
    delete item->widget();
    delete item;
  }
  ui->verticalLayoutPassword->setSpacing(0);
}

/**
 * @brief Mainwindow::copyPasswordToClipboard - copy the clipped password (if
 * not "") to the clipboard
 */
void MainWindow::copyPasswordToClipboard() {
  if (clippedPass.length() > 0) {
    QClipboard *clip = QApplication::clipboard();
    clip->setText(clippedPass);
    ui->statusBar->showMessage(tr("Password copied to clipboard"), 3000);
    if (useAutoclear) {
      QTimer::singleShot(1000 * autoclearSeconds, this, SLOT(clearClipboard()));
    }
  }
}

/**
 * @brief Mainwindow::setClippedPassword - set the stored clipped password
 */
void MainWindow::setClippedPassword(const QString &pass) {
  clippedPass = pass;
  if (clippedPass.length() == 0)
    ui->copyPasswordButton->setEnabled(false);
  else
    ui->copyPasswordButton->setEnabled(true);
}

const QString &MainWindow::getClippedPassword() { return clippedPass; }

/**
 * @brief MainWindow::reencryptPath reencrypt all files under the chosen
 * directory
 *
 * This is stil quite experimental..s
 * @param dir
 */
void MainWindow::reencryptPath(QString dir) {
  ui->statusBar->showMessage(tr("Re-encrypting from folder %1").arg(dir), 3000);
  enableUiElements(false);
  ui->treeView->setDisabled(true);
  if (autoPull)
    on_updateButton_clicked();
  waitFor(50);
  process->waitForFinished();
  QDir currentDir;
  QDirIterator gpgFiles(dir, QStringList() << "*.gpg", QDir::Files,
                        QDirIterator::Subdirectories);
  QStringList gpgId;
  while (gpgFiles.hasNext()) {
    QString fileName = gpgFiles.next();
    if (gpgFiles.fileInfo().path() != currentDir.path()) {
      gpgId = getRecipientList(fileName);
      gpgId.sort();
    }
    currentAction = GPG_INTERNAL;
    process->waitForFinished();
    executeWrapper(gpgExecutable, "-v --no-secmem-warning "
                                  "--no-permission-warning --list-only "
                                  "--keyid-format long " +
                                      fileName);
    process->waitForFinished(3000);
    QStringList actualKeys;
    QString keys =
        process->readAllStandardOutput() + process->readAllStandardError();
    QStringList key = keys.split("\n");
    QListIterator<QString> itr(key);
    while (itr.hasNext()) {
      QString current = itr.next();
      QStringList cur = current.split(" ");
      if (cur.length() > 4) {
        QString actualKey = cur.takeAt(4);
        if (actualKey.length() == 16) {
          actualKeys << actualKey;
        }
      }
    }
    actualKeys.sort();
    if (actualKeys != gpgId) {
      // qDebug() << actualKeys << gpgId << getRecipientList(fileName);
      qDebug() << "reencrypt " << fileName << " for " << gpgId;
      lastDecrypt = "Could not decrypt";
      currentAction = GPG_INTERNAL;
      executeWrapper(gpgExecutable,
                     "-d --quiet --yes --no-encrypt-to --batch --use-agent \"" +
                         fileName + '"');
      process->waitForFinished(30000); // long wait (passphrase stuff)
      lastDecrypt = process->readAllStandardOutput();

      if (!lastDecrypt.isEmpty() && lastDecrypt != "Could not decrypt") {
        if (lastDecrypt.right(1) != "\n")
          lastDecrypt += "\n";

        QString recipients = getRecipientString(fileName, " -r ");
        if (recipients.isEmpty()) {
          QMessageBox::critical(
              this, tr("Can not edit"),
              tr("Could not read encryption key to use, .gpg-id "
                 "file missing or invalid."));
          return;
        }
        currentAction = EDIT;
        executeWrapper(gpgExecutable, "--yes --batch -eq --output \"" +
                                          fileName + "\" " + recipients + " -",
                       lastDecrypt);
        process->waitForFinished(3000);

        if (!useWebDav && useGit) {
          executeWrapper(gitExecutable, "add \"" + fileName + '"');
          QString path = QDir(passStore).relativeFilePath(fileName);
          path.replace(QRegExp("\\.gpg$"), "");
          executeWrapper(gitExecutable, "commit \"" + fileName + "\" -m \"" +
                                            "Edit for " + path +
                                            " using QtPass.\"");
          process->waitForFinished(3000);
        }

      } else {
        qDebug() << "Decrypt error on re-encrypt";
      }
    }
  }
  if (autoPush)
    on_pushButton_clicked();
  enableUiElements(true);
}
