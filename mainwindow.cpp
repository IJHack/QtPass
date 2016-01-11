#include "mainwindow.h"
#include <QClipboard>
#include <QDebug>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>
#include <QFileInfo>
#include <QQueue>
#include <QCloseEvent>
#include <QLabel>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN /*_KILLING_MACHINE*/
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <winnetwk.h>
#undef DELETE
#endif
#include "ui_mainwindow.h"
#include "configdialog.h"
#include "usersdialog.h"
#include "keygendialog.h"
#include "passworddialog.h"
#include "util.h"

/**
 * @brief MainWindow::MainWindow
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
  if (!checkConfig()) {
    // no working config
    QApplication::quit();
  }
  ui->copyPasswordButton->setEnabled(false);
  setClippedPassword("");
  QtPass = NULL;
  QTimer::singleShot(10, this, SLOT(focusInput()));
  qsrand(QDateTime::currentDateTime().toTime_t());
}

void MainWindow::focusInput() {
  ui->lineEdit->selectAll();
  ui->lineEdit->setFocus();
}

/**
 * @brief MainWindow::~MainWindow
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
 * @brief MainWindow::checkConfig
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
  foreach(QString key, keys)
    profiles[key] = settings.value(key).toString();
  settings.endGroup();

  useGit = (settings.value("useGit") == "true");
  usePwgen = (settings.value("usePwgen") == "true");
  useSymbols = (settings.value("useSymbols") == "true");
  passwordLength = settings.value("passwordLength").toInt();
  passwordChars = settings.value("passwordChars").toString();

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
    passwordLength = 16;
    passwordChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz123456"
                    "7890~!@#$%^&*()_-+={}[]|:;<>,.?";
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
    if (passwordChars.isEmpty())
      passwordChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234"
                      "567890~!@#$%^&*()_-+={}[]|:;<>,.?";
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
  } else {
    ui->pushButton->show();
    ui->updateButton->show();
  }

  startupPhase = false;
  return true;
}

/**
 * @brief MainWindow::config
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
  d->useSymbols(useSymbols);
  d->setPasswordLength(passwordLength);
  d->setPasswordChars(passwordChars);
  d->useTemplate(useTemplate);
  d->setTemplate(passTemplate);
  d->templateAllFields(templateAllFields);
  d->autoPull(autoPull);
  d->autoPush(autoPush);
  d->alwaysOnTop(alwaysOnTop);
  if (startupPhase)
    d->wizard();  // does shit
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
      useSymbols = d->useSymbols();
      passwordLength = d->getPasswordLength();
      passwordChars = d->getPasswordChars();
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
      settings.setValue("useSymbols", useSymbols ? "true" : "false");
      settings.setValue("passwordLength", passwordLength);
      settings.setValue("passwordChars", passwordChars);
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
      } else {
        ui->pushButton->show();
        ui->updateButton->show();
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
 * @brief MainWindow::on_updateButton_clicked
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
 * @brief MainWindow::on_pushButton_clicked
 */
void MainWindow::on_pushButton_clicked() {
  ui->statusBar->showMessage(tr("Updating password-store"), 2000);
  currentAction = GIT;
  if (usePass)
    executePass("git push");
  else
    executeWrapper(gitExecutable, "push");
}

QString MainWindow::getDir(const QModelIndex &index, bool forPass) {
  QString abspath = QDir(passStore).absolutePath() + '/';
  if (!index.isValid())
    return forPass ? "" : abspath;
  QFileInfo info = model.fileInfo(proxyModel.mapToSource(index));
  QString filePath =
      (info.isFile() ? info.absolutePath() : info.absoluteFilePath()) + '/';
  if (forPass) {
    filePath.replace(QRegExp("^" + passStore), "");
    filePath.replace(QRegExp("^" + abspath), "");
  }
  return filePath;
}

QString MainWindow::getFile(const QModelIndex &index, bool forPass) {
  if (!index.isValid() ||
      !model.fileInfo(proxyModel.mapToSource(index)).isFile())
    return QString();
  QString filePath = model.filePath(proxyModel.mapToSource(index));
  if (forPass) {
    filePath.replace(QRegExp("\\.gpg$"), "");
    filePath.replace(QRegExp("^" + passStore), "");
  }
  return filePath;
}

/**
 * @brief MainWindow::on_treeView_clicked
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
 * @brief MainWindow::executePass
 * @param args
 */
void MainWindow::executePass(QString args, QString input) {
  executeWrapper(passExecutable, args, input);
}

/**
 * @brief MainWindow::executePassGitInit
 */
void MainWindow::executePassGitInit() {
  qDebug() << "Pass git init called";
  if (usePass)
    executePass("git init");
  else
    executeWrapper("git", "init \"" + passStore + '"');
}

/**
 * @brief MainWindow::executeWrapper
 * @param app
 * @param args
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
 * @brief MainWindow::readyRead
 */
void MainWindow::readyRead(bool finished = false) {
  if (currentAction == PWGEN)
    return;
  QString output = "";
  QString error = process->readAllStandardError();
  if (currentAction != GPG_INTERNAL) {
    output = process->readAllStandardOutput();
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

      if (useTemplate) {
        while (ui->formLayout->count() > 0) {
          QLayoutItem *item = ui->formLayout->takeAt(0);
          delete item->widget();
          delete item;
        }
        QLineEdit *pass = new QLineEdit();  // создает строку для вывода пароля
        pass->setText(tokens[0]);
        tokens.pop_front();
        if (hidePassword)
          pass->setEchoMode(QLineEdit::Password);   // ставим отображение пароля спрятанное звездочками
        pass->setReadOnly(true);
        ui->formLayout->addRow(pass); // добавляет новую строку с паролем в слой

        for (int j = 0; j < tokens.length(); ++j) { // цикл для создания строк, так как их надо две: для логина и урла
          QString token = tokens.at(j);
          if (token.contains(':')) {
            int colon = token.indexOf(':');
            QString field = token.left(colon);
            if (templateAllFields || passTemplate.contains(field)) {
              QString value = token.right(token.length() - colon - 1);
              if (!passTemplate.contains(field) && value.startsWith("//"))
                continue;  // colon is probably from a url
              QLineEdit *line = new QLineEdit(); // создаем строку для вывода логина и урла (по-одной за итерацию)
              line->setObjectName(field);
              line->setText(value); // заполняет созданные строки данными из токена
              line->setReadOnly(true);
              ui->formLayout->addRow(new QLabel(field), line);  // создает лейбаки в отдельных столбцах слоя
              tokens.removeAt(j);
              --j;  // tokens.length() also got shortened by the remove..
            }
          }
        }
        if (ui->formLayout->count() == 0)
          ui->verticalLayoutPassword->setSpacing(0); // если данные скрыты, то отступ слоя 0
        else {
          ui->verticalLayoutPassword->setSpacing(3); // если видны, то отступ слоя сверху и снизу такой-то
          ui->formLayout->setVerticalSpacing(3); // отступ между элементами слоя
        }

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

  output.replace(QRegExp("((?:https?|ftp)://\\S+)"), "<a href=\"\\1\">\\1</a>");
  output.replace(QRegExp("\n"), "<br />");
  if (!ui->textBrowser->toPlainText().isEmpty())
    output = ui->textBrowser->toHtml() + output;
  ui->textBrowser->setHtml(output);
}

/**
 * @brief MainWindow::clearClipboard
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
 * @brief MainWindow::clearPanel
 */
void MainWindow::clearPanel() {
  while (ui->formLayout->count() > 0) {
    QLayoutItem *item = ui->formLayout->takeAt(0);
    delete item->widget();
    delete item;
    ui->verticalLayoutPassword->setSpacing(0);   // возвращаем отступ лэйера, иначе перекосит окно вывода, когда   спрячутся строки с логином и паролем
  }
  QString output = "***" + tr("Password and Content hidden") + "***";
  ui->textBrowser->setHtml(output);
  ui->textBrowser->setAlignment(Qt::AlignCenter); // выравниваем выводимое сообщение по центру
}

/**
 * @brief MainWindow::processFinished
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

/**                QStringList tokens =  output.split("\n");

 * @brief MainWindow::enableUiElements
 * @param state
 */
void MainWindow::enableUiElements(bool state) {
  ui->updateButton->setEnabled(state);
  ui->treeView->setEnabled(state);
  ui->lineEdit->setEnabled(state);
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
 * @brief MainWindow::processError
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
 * @brief MainWindow::setPassExecutable
 * @param path
 */
void MainWindow::setPassExecutable(QString path) { passExecutable = path; }

/**
 * @brief MainWindow::setGitExecutable
 * @param path
 */
void MainWindow::setGitExecutable(QString path) { gitExecutable = path; }

/**
 * @brief MainWindow::setGpgExecutable
 * @param path
 */
void MainWindow::setGpgExecutable(QString path) { gpgExecutable = path; }

/**
 * @brief MainWindow::getGpgExecutable
 * @return
 */
QString MainWindow::getGpgExecutable() { return gpgExecutable; }

/**
 * @brief MainWindow::on_configButton_clicked
 */
void MainWindow::on_configButton_clicked() { config(); }

/**
 * @brief MainWindow::on_lineEdit_textChanged
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
 * @brief MainWindow::on_lineEdit_returnPressed
 */
void MainWindow::on_lineEdit_returnPressed() {
  selectFirstFile();
  on_treeView_clicked(ui->treeView->currentIndex());
}

/**
 * @brief MainWindow::selectFirstFile
 */
void MainWindow::selectFirstFile() {
  QModelIndex index = proxyModel.mapFromSource(model.setRootPath(passStore));
  index = firstFile(index);
  ui->treeView->setCurrentIndex(index);
}

/**
 * @brief MainWindow::firstFile
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
 * @brief MainWindow::on_clearButton_clicked
 */
void MainWindow::on_clearButton_clicked() { ui->lineEdit->clear(); }

/**
 * @brief MainWindow::getRecipientList
 * @param for_file
 * @return
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
 * @brief MainWindow::getRecipientString
 * @param for_file
 * @param separator
 * @param count
 * @return
 */
QString MainWindow::getRecipientString(QString for_file, QString separator,
                                       int *count) {
  QString recipients_str;
  QStringList recipients_list = getRecipientList(for_file);
  if (count)
    *count = recipients_list.size();
  foreach(const QString recipient, recipients_list)
    recipients_str += separator + '"' + recipient + '"';
  return recipients_str;
}

/**
 * @brief MainWindow::setPassword
 * @param file
 * @param overwrite
 */
void MainWindow::setPassword(QString file, bool overwrite, bool isNew = false) {
  if (!isNew && lastDecrypt.isEmpty()) {
    // warn?
    return;
  }
  PasswordDialog d(this);
  d.setFile(file);
  d.setTemplate(passTemplate);
  d.useTemplate(useTemplate);
  d.templateAll(templateAllFields);
  d.setPassword(lastDecrypt);
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
      QString path = file;
      path.replace(QRegExp("\\.gpg$"), "");
      path.replace(QRegExp("^" + passStore), "");
      executeWrapper(gitExecutable, "commit \"" + file + "\" -m \"" +
                                        (overwrite ? "Edit" : "Add") + " for " +
                                        path + " using QtPass.\"");
      if (autoPush)
        on_pushButton_clicked();
    }
  }
}

/**
 * @brief MainWindow::on_addButton_clicked
 */
void MainWindow::on_addButton_clicked() {
  bool ok;
  QString dir = getDir(ui->treeView->currentIndex(), usePass);
  QString file = QInputDialog::getText(
      this, tr("New file"),
      tr("New password file, will be placed in folder %1:")
          .arg(QDir::separator() + getDir(ui->treeView->currentIndex(), true)),
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
 * @brief MainWindow::on_deleteButton_clicked
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
                       "commit \"" + file + "\" -m \" Remove for " +
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
    if (QMessageBox::question(
            this, tr("Delete folder?"),
            tr("Are you sure you want to delete %1?")
                .arg(QDir::separator() +
                     getDir(ui->treeView->currentIndex(), true)),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
      return;
    if (usePass) {
      currentAction = DELETE;
      executePass("rm -r \"" + file + '"');
      if (useGit && autoPush)
        on_pushButton_clicked();
    } else {
      if (useGit) {
        executeWrapper(gitExecutable, "rm -rf \"" + file + '"');
        executeWrapper(gitExecutable,
                       "commit \"" + file + "\" -m \" Remove for " +
                           getFile(ui->treeView->currentIndex(), true) +
                           " using QtPass.\"");
        if (autoPush)
          on_pushButton_clicked();
      } else {
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        QDir dir(file);
        dir.removeRecursively();
#else
        removeDir(file);
#endif
      }
    }
  }
  lastDecrypt = "";
}

/**
 * @brief MainWindow::removeDir
 * @param dirName
 * @return
 */
bool MainWindow::removeDir(const QString &dirName) {
  bool result = true;
  QDir dir(dirName);

  if (dir.exists(dirName)) {
    Q_FOREACH(QFileInfo info,
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
 * @brief MainWindow::on_editButton_clicked
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
 * @brief MainWindow::listKeys
 * @param keystring
 * @param secret
 * @return
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
  QStringList keys = QString(process->readAllStandardOutput())
                         .split(QRegExp("[\r\n]"), QString::SkipEmptyParts);
  UserInfo current_user;
  foreach(QString key, keys) {
    QStringList props = key.split(':');
    if (props.size() < 10)
      continue;
    if (props[0] == (secret ? "sec" : "pub")) {
      if (!current_user.key_id.isEmpty())
        users.append(current_user);
      current_user = UserInfo();
      current_user.key_id = props[4];
      current_user.name = props[9].toUtf8();
      current_user.validity = props[8][0].toLatin1();
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

void MainWindow::userDialog(QString dir) {
  if (!dir.isEmpty())
    currentDir = dir;
  on_usersButton_clicked();
}

void MainWindow::on_usersButton_clicked() {
  QList<UserInfo> users = listKeys();
  if (users.size() == 0) {
    QMessageBox::critical(this, tr("Can not get key list"),
                          tr("Unable to get list of available gpg keys"));
    return;
  }
  QList<UserInfo> secret_keys = listKeys("", true);
  foreach(const UserInfo &sec, secret_keys) {
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
  foreach(const UserInfo &sel, selected_users) {
    for (QList<UserInfo>::iterator it = users.begin(); it != users.end(); ++it)
      if (sel.key_id == it->key_id)
        it->enabled = true;
  }
  if (count > selected_users.size()) {
    // Some keys seem missing from keyring, add them separately
    QStringList recipients = getRecipientList(dir.isEmpty() ? "" : dir);
    foreach(const QString recipient, recipients) {
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
  foreach(const UserInfo &user, users) {
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
}

/**
 * @brief MainWindow::setApp
 * @param app
 */
void MainWindow::setApp(SingleApplication *app) {
#if SINGLE_APP
  connect(app, SIGNAL(messageAvailable(QString)), this,
          SLOT(messageAvailable(QString)));
#endif
}

/**
 * @brief MainWindow::messageAvailable
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
 * @brief MainWindow::setText
 * @param message
 */
void MainWindow::setText(QString text) { ui->lineEdit->setText(text); }

/**
 * @brief MainWindow::updateEnv
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
 * @brief MainWindow::getSecretKeys
 * @return QStringList keys
 */
QStringList MainWindow::getSecretKeys() {
  QList<UserInfo> keys = listKeys("", true);
  QStringList names;

  if (keys.size() == 0)
    return names;

  foreach(const UserInfo &sec, keys)
    names << sec.name;

  return names;
}

/**
 * @brief Dialog::genKey
 * @param QString batch
 */
void MainWindow::generateKeyPair(QString batch, QDialog *keygenWindow) {
  keygen = keygenWindow;
  ui->statusBar->showMessage(tr("Generating GPG key pair"), 60000);
  currentAction = GPG_INTERNAL;
  executeWrapper(gpgExecutable, "--gen-key --no-tty --batch", batch);
}

/**
 * @brief MainWindow::updateProfileBox
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
  if (index != -1)  // -1 for not found
    ui->profileBox->setCurrentIndex(index);
}

/**
 * @brief MainWindow::on_profileBox_currentIndexChanged
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
 * @brief MainWindow::initTrayIcon
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
 * @brief MainWindow::destroyTrayIcon
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
 * @brief MainWindow::closeEvent
 * @param event
 */
void MainWindow::closeEvent(QCloseEvent *event) {
  if (hideOnClose) {
    this->hide();
    event->ignore();
  } else {
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

void MainWindow::on_copyPasswordButton_clicked() { copyPasswordToClipboard(); }

/**
 * @brief MainWindow::showContextMenu
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
 * @brief MainWindow::showContextMenu
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
 * @brief MainWindow::addFolder
 */
void MainWindow::addFolder() {
  bool ok;
  QString dir = getDir(ui->treeView->currentIndex(), false);
  QString newdir = QInputDialog::getText(
      this, tr("New file"),
      tr("New folder, will be placed in folder %1:")
          .arg(QDir::separator() + getDir(ui->treeView->currentIndex(), true)),
      QLineEdit::Normal, "", &ok);
  if (!ok || newdir.isEmpty())
    return;
  newdir.prepend(dir);
  // qDebug() << newdir;
  QDir().mkdir(newdir);
  // TODO(annejan) add to git?
}

/**
 * @brief MainWindow::editPassword
 */
void MainWindow::editPassword() {
  if (useGit && autoPull)
    on_updateButton_clicked();
  waitFor(30);
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
    process->waitForFinished(30000);  // long wait (passphrase stuff)
    if (process->exitStatus() == QProcess::NormalExit)
      on_editButton_clicked();
  }
}

/**
 * @brief MainWindow::generatePassword
 * @return
 */
QString MainWindow::generatePassword() {
  QString passwd;
  if (usePwgen) {
    waitFor(2);
    QString args = (useSymbols ? "--symbols -1 " : "-1 ") +
                   QString::number(passwordLength);
    currentAction = PWGEN;
    executeWrapper(pwgenExecutable, args);
    process->waitForFinished(1000);
    if (process->exitStatus() == QProcess::NormalExit)
      passwd =
          QString(process->readAllStandardOutput()).remove(QRegExp("[\\n\\r]"));
    else
      qDebug() << "pwgen fail";
  } else {
    int length = passwordChars.length();
    if (length > 0) {
      for (int i = 0; i < passwordLength; ++i) {
        int index = qrand() % length;
        QChar nextChar = passwordChars.at(index);
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
 * @brief MainWindow::waitFor
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
 * @brief MainWindow::clearTemplateWidgets
 */
void MainWindow::clearTemplateWidgets() {
  while (ui->formLayout->count() > 0) {
    QLayoutItem *item = ui->formLayout->takeAt(0);
    delete item->widget();
    delete item;
  }
  ui->verticalLayoutPassword->setSpacing(0);
}

/*
 * @brief Mainwindow::copyPasswordToClipboard - copy the clipped password (if
 * not "") to the clipboard
 * @return
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
 * @return none
 */
void MainWindow::setClippedPassword(const QString &pass) {
  clippedPass = pass;
  if (clippedPass.length() == 0)
    ui->copyPasswordButton->setEnabled(false);
  else
    ui->copyPasswordButton->setEnabled(true);
}

const QString &MainWindow::getClippedPassword() { return clippedPass; }
