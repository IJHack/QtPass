#include "qtpass.h"
#include "mainwindow.h"
#include "qtpasssettings.h"
#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QPixmap>

#ifndef Q_OS_WIN
#include <QInputDialog>
#include <QLineEdit>
#include <utility>
#else
#define WIN32_LEAN_AND_MEAN /*_KILLING_MACHINE*/
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <winnetwk.h>
#undef DELETE
#endif

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

QtPass::QtPass(MainWindow *mainWindow)
    : m_mainWindow(mainWindow), clippedText(QString()), freshStart(true) {
  setClipboardTimer();
  clearClipboardTimer.setSingleShot(true);
  connect(&clearClipboardTimer, SIGNAL(timeout()), this,
          SLOT(clearClipboard()));

  QObject::connect(qApp, &QApplication::aboutToQuit, this,
                   &QtPass::clearClipboard);

  setMainWindow();
}

/**
 * @brief QtPass::~QtPass destroy!
 */
QtPass::~QtPass() {
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
 * @brief QtPass::init make sure we are ready to go as soon as
 * possible
 */
bool QtPass::init() {
  QString passStore = QtPassSettings::getPassStore(Util::findPasswordStore());
  QtPassSettings::setPassStore(passStore);

  QtPassSettings::initExecutables();

  QString version = QtPassSettings::getVersion();
  // dbg()<< version;

  // Config updates
  if (version.isEmpty()) {
#ifdef QT_DEBUG
    dbg() << "assuming fresh install";
#endif

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
    m_mainWindow->config();
    if (freshStart && Util::checkConfig())
      return false;
  }

  // TODO(annejan): this needs to be before we try to access the store,
  // but it would be better to do it after the Window is shown,
  // as the long delay it can cause is irritating otherwise.
  if (QtPassSettings::isUseWebDav())
    mountWebDav();

  freshStart = false;
  // startupPhase = false;
  return true;
}

void QtPass::setMainWindow(void) {
  m_mainWindow->restoreWindow();

  fusedav.setParent(m_mainWindow);

  //  TODO(bezet): this should be reconnected dynamically when pass changes
  connectPassSignalHandlers(QtPassSettings::getRealPass());
  connectPassSignalHandlers(QtPassSettings::getImitatePass());

  connect(m_mainWindow, &MainWindow::passShowHandlerFinished, this,
          &QtPass::passShowHandlerFinished);

  // only for ipass
  connect(QtPassSettings::getImitatePass(), &ImitatePass::startReencryptPath,
          m_mainWindow, &MainWindow::startReencryptPath);
  connect(QtPassSettings::getImitatePass(), &ImitatePass::endReencryptPath,
          m_mainWindow, &MainWindow::endReencryptPath);

  connect(m_mainWindow, &MainWindow::passGitInitNeeded, [=]() {
#ifdef QT_DEBUG
    dbg() << "Pass git init called";
#endif
    QtPassSettings::getPass()->GitInit();
  });

  connect(
      m_mainWindow, &MainWindow::generateGPGKeyPair, [=](const QString &batch) {
        QtPassSettings::getPass()->GenerateGPGKeys(batch);
        m_mainWindow->showStatusMessage(tr("Generating GPG key pair"), 60000);
      });
}

void QtPass::connectPassSignalHandlers(Pass *pass) {
  connect(pass, &Pass::error, this, &QtPass::processError);
  connect(pass, &Pass::processErrorExit, this, &QtPass::processErrorExit);
  connect(pass, &Pass::critical, m_mainWindow, &MainWindow::critical);
  connect(pass, &Pass::startingExecuteWrapper, m_mainWindow,
          &MainWindow::executeWrapperStarted);
  connect(pass, &Pass::statusMsg, m_mainWindow, &MainWindow::showStatusMessage);
  connect(pass, &Pass::finishedShow, m_mainWindow,
          &MainWindow::passShowHandler);
  connect(pass, &Pass::finishedOtpGenerate, m_mainWindow,
          &MainWindow::passOtpHandler);

  connect(pass, &Pass::finishedGitInit, this, &QtPass::passStoreChanged);
  connect(pass, &Pass::finishedGitPull, this, &QtPass::processFinished);
  connect(pass, &Pass::finishedGitPush, this, &QtPass::processFinished);
  connect(pass, &Pass::finishedInsert, this, &QtPass::finishedInsert);
  connect(pass, &Pass::finishedRemove, this, &QtPass::passStoreChanged);
  connect(pass, &Pass::finishedInit, this, &QtPass::passStoreChanged);
  connect(pass, &Pass::finishedMove, this, &QtPass::passStoreChanged);
  connect(pass, &Pass::finishedCopy, this, &QtPass::passStoreChanged);
  connect(pass, &Pass::finishedGenerateGPGKeys, this,
          &QtPass::onKeyGenerationComplete);
}

/**
 * @brief QtPass::mountWebDav is some scary voodoo magic
 */
void QtPass::mountWebDav() {
#ifdef Q_OS_WIN
  char dst[20] = {0};
  NETRESOURCEA netres;
  memset(&netres, 0, sizeof(netres));
  netres.dwType = RESOURCETYPE_DISK;
  netres.lpLocalName = 0;
  netres.lpRemoteName = QtPassSettings::getWebDavUrl().toUtf8().data();
  DWORD size = sizeof(dst);
  DWORD r = WNetUseConnectionA(
      reinterpret_cast<HWND>(m_mainWindow->effectiveWinId()), &netres,
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
    m_mainWindow->flashText(tr("Failed to connect WebDAV:\n") + message +
                                " (0x" + QString::number(r, 16) + ")",
                            true);
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
      pwd = QInputDialog::getText(m_mainWindow, tr("QtPass WebDAV password"),
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
    m_mainWindow->flashText(
        tr("Failed to start fusedav to connect WebDAV:\n") + error, true);
  }
#endif
}

/**
 * @brief QtPass::processError something went wrong
 * @param error
 */
void QtPass::processError(QProcess::ProcessError error) {
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

  m_mainWindow->flashText(errorString, true);
  m_mainWindow->setUiElementsEnabled(true);
}

void QtPass::processErrorExit(int exitCode, const QString &p_error) {
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
        R"(<a href="\1">\1</a>)");
    output.replace(QRegExp("\n"), "<br />");

    m_mainWindow->flashText(output, false, true);
  }

  m_mainWindow->setUiElementsEnabled(true);
}

/**
 * @brief QtPass::processFinished background process has finished
 * @param exitCode
 * @param exitStatus
 * @param output    stdout from a process
 * @param errout    stderr from a process
 */
void QtPass::processFinished(const QString &p_output, const QString &p_errout) {
  showInTextBrowser(p_output);
  //    Sometimes there is error output even with 0 exit code, which is
  //    assumed in this function
  processErrorExit(0, p_errout);

  m_mainWindow->setUiElementsEnabled(true);
}

void QtPass::passStoreChanged(const QString &p_out, const QString &p_err) {
  processFinished(p_out, p_err);
  doGitPush();
}

void QtPass::finishedInsert(const QString &p_output, const QString &p_errout) {
  processFinished(p_output, p_errout);
  doGitPush();
  m_mainWindow->on_treeView_clicked(m_mainWindow->getCurrentTreeViewIndex());
}

void QtPass::onKeyGenerationComplete(const QString &p_output,
                                     const QString &p_errout) {
  if (nullptr != m_mainWindow->getKeygenDialog()) {
#ifdef QT_DEBUG
    qDebug() << "Keygen Done";
#endif

    m_mainWindow->cleanKeygenDialog();
    // TODO(annejan) some sanity checking ?
  }

  processFinished(p_output, p_errout);
}

void QtPass::passShowHandlerFinished(QString output) {
  showInTextBrowser(std::move(output));
}

void QtPass::showInTextBrowser(QString output, QString prefix,
                               QString postfix) {
  output.replace(QRegExp("<"), "&lt;");
  output.replace(QRegExp(">"), "&gt;");
  output.replace(QRegExp(" "), "&nbsp;");

  output.replace(
      QRegExp("((?:https?|ftp|ssh|sftp|ftps|webdav|webdavs)://\\S+)"),
      R"(<a href="\1">\1</a>)");
  output.replace(QRegExp("\n"), "<br />");
  output = prefix + output + postfix;

  m_mainWindow->flashText(output, false, true);
}

void QtPass::doGitPush() {
  if (QtPassSettings::isAutoPush())
    m_mainWindow->onPush();
}

void QtPass::setClippedText(const QString &password, const QString &p_output) {
  if (QtPassSettings::getClipBoardType() != Enums::CLIPBOARD_NEVER &&
      !p_output.isEmpty()) {
    clippedText = password;
    if (QtPassSettings::getClipBoardType() == Enums::CLIPBOARD_ALWAYS)
      copyTextToClipboard(password);
  }
}
void QtPass::clearClippedText() { clippedText = ""; }

void QtPass::setClipboardTimer() {
  clearClipboardTimer.setInterval(1000 * QtPassSettings::getAutoclearSeconds());
}

/**
 * @brief MainWindow::clearClipboard remove clipboard contents.
 */
void QtPass::clearClipboard() {
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
    m_mainWindow->showStatusMessage(tr("Clipboard cleared"));
  } else {
    m_mainWindow->showStatusMessage(tr("Clipboard not cleared"));
  }

  clippedText.clear();
}

/**
 * @brief MainWindow::copyTextToClipboard copies text to your clipboard
 * @param text
 */
void QtPass::copyTextToClipboard(const QString &text) {
  QClipboard *clip = QApplication::clipboard();
  if (!QtPassSettings::isUseSelection()) {
    clip->setText(text, QClipboard::Clipboard);
  } else {
    clip->setText(text, QClipboard::Selection);
  }

  clippedText = text;
  m_mainWindow->showStatusMessage(tr("Copied to clipboard"));
  if (QtPassSettings::isUseAutoclear()) {
    clearClipboardTimer.start();
  }
}

/**
 * @brief displays the text as qrcode
 * @param text
 */
void QtPass::showTextAsQRCode(const QString &text) {
  QProcess qrencode;
  qrencode.start("/usr/bin/qrencode", QStringList() << "-o-"
                                                    << "-tPNG");
  qrencode.write(text.toUtf8());
  qrencode.closeWriteChannel();
  qrencode.waitForFinished();
  QByteArray output(qrencode.readAllStandardOutput());

  if (qrencode.exitStatus() || qrencode.exitCode()) {
    QString error(qrencode.readAllStandardError());
    m_mainWindow->showStatusMessage(error);
  } else {
    QPixmap image;
    image.loadFromData(output, "PNG");

    QLabel *label = new QLabel();
    label->setPixmap(image);
    label->setScaledContents(true);
    label->show();
  }
}
