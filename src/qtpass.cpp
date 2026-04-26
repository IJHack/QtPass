// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "qtpass.h"
#include "mainwindow.h"
#include "qtpasssettings.h"
#include "util.h"
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>

#ifndef Q_OS_WIN
#include <QInputDialog>
#include <QLineEdit>
#include <QMimeData>
#include <utility>
#else
#define WIN32_LEAN_AND_MEAN /*_KILLING_MACHINE*/
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <winnetwk.h>
#undef DELETE
#include <QMimeData>
#endif

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

/**
 * @brief Constructs a QtPass instance.
 * @param mainWindow The main window reference
 */
QtPass::QtPass(MainWindow *mainWindow)
    : m_mainWindow(mainWindow), freshStart(true) {
  setClipboardTimer();
  clearClipboardTimer.setSingleShot(true);
  connect(&clearClipboardTimer, &QTimer::timeout, this,
          &QtPass::clearClipboard);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
  QObject::connect(qApp, &QApplication::aboutToQuit, this,
                   &QtPass::clearClipboard);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#pragma GCC diagnostic pop
#endif

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
auto QtPass::init() -> bool {
  QString passStore = QtPassSettings::getPassStore(Util::findPasswordStore());
  QtPassSettings::setPassStore(passStore);

  QtPassSettings::initExecutables();

  QString version = QtPassSettings::getVersion();

  // Config updates
  if (version.isEmpty()) {
#ifdef QT_DEBUG
    dbg() << "assuming fresh install";
#endif

    if (QtPassSettings::getAutoclearSeconds() < 5) {
      QtPassSettings::setAutoclearSeconds(10);
    }
    if (QtPassSettings::getAutoclearPanelSeconds() < 5) {
      QtPassSettings::setAutoclearPanelSeconds(10);
    }
    if (!QtPassSettings::getPwgenExecutable().isEmpty()) {
      QtPassSettings::setUsePwgen(true);
    } else {
      QtPassSettings::setUsePwgen(false);
    }
    QtPassSettings::setPassTemplate("login\nurl");
  } else {
    if (QtPassSettings::getPassTemplate().isEmpty()) {
      QtPassSettings::setPassTemplate("login\nurl");
    }
  }

  QtPassSettings::setVersion(VERSION);

  if (!Util::configIsValid()) {
    m_mainWindow->config();
    if (freshStart && !Util::configIsValid()) {
      return false;
    }
  }

  // Note: WebDAV mount needs to happen before accessing the store,
  // but ideally should be done after Window is shown to avoid long delay.
  if (QtPassSettings::isUseWebDav()) {
    mountWebDav();
  }

  freshStart = false;
  return true;
}

/**
 * @brief Sets up the main window and connects signal handlers.
 */
void QtPass::setMainWindow() {
  m_mainWindow->restoreWindow();

  fusedav.setParent(m_mainWindow);

  // Signal handlers are connected for both pass implementations
  // Note: When pass binary changes, QtPass restart is required to reconnect
  // This is acceptable as pass binary change is infrequent
  connectPassSignalHandlers(QtPassSettings::getRealPass());
  connectPassSignalHandlers(QtPassSettings::getImitatePass());

  connect(m_mainWindow, &MainWindow::passShowHandlerFinished, this,
          &QtPass::passShowHandlerFinished);

  // only for ipass
  connect(QtPassSettings::getImitatePass(), &ImitatePass::startReencryptPath,
          m_mainWindow, &MainWindow::startReencryptPath);
  connect(QtPassSettings::getImitatePass(), &ImitatePass::endReencryptPath,
          m_mainWindow, &MainWindow::endReencryptPath);

  connect(m_mainWindow, &MainWindow::passGitInitNeeded, []() {
#ifdef QT_DEBUG
    dbg() << "Pass git init called";
#endif
    QtPassSettings::getPass()->GitInit();
  });

  connect(m_mainWindow, &MainWindow::generateGPGKeyPair, m_mainWindow,
          [this](const QString &batch) {
            QtPassSettings::getPass()->GenerateGPGKeys(batch);
            m_mainWindow->showStatusMessage(tr("Generating GPG key pair"),
                                            60000);
          });
}

/**
 * @brief Connects pass signal handlers to QtPass slots.
 * @param pass The pass instance to connect
 */
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
  connect(pass, &Pass::finishedGrep, m_mainWindow, &MainWindow::onGrepFinished);
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
  netres.lpLocalName = nullptr;
  // Store QByteArray in variables to ensure lifetime during WNetUseConnectionA
  // call
  QByteArray webDavUrlUtf8 = QtPassSettings::getWebDavUrl().toUtf8();
  QByteArray webDavPasswordUtf8 = QtPassSettings::getWebDavPassword().toUtf8();
  QByteArray webDavUserUtf8 = QtPassSettings::getWebDavUser().toUtf8();
  netres.lpRemoteName = const_cast<char *>(webDavUrlUtf8.constData());
  DWORD size = sizeof(dst);
  DWORD r = WNetUseConnectionA(
      reinterpret_cast<HWND>(m_mainWindow->effectiveWinId()), &netres,
      const_cast<char *>(webDavPasswordUtf8.constData()),
      const_cast<char *>(webDavUserUtf8.constData()),
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
  fusedav.start("fusedav", QStringList()
                               << "-o"
                               << "nonempty"
                               << "-u"
                               << "\"" + QtPassSettings::getWebDavUser() + "\""
                               << QtPassSettings::getWebDavUrl()
                               << "\"" + QtPassSettings::getPassStore() + "\"");
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
  if (prompt >= 0) {
    error.remove(0, prompt + 10);
  }
  if (fusedav.state() != QProcess::Running) {
    error = tr("fusedav exited unexpectedly\n") + error;
  }
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

/**
 * @brief Handles process error exit.
 * @param exitCode The exit code
 * @param p_error The error message
 */
void QtPass::processErrorExit(int exitCode, const QString &p_error) {
  if (nullptr != m_mainWindow->getKeygenDialog()) {
    m_mainWindow->cleanKeygenDialog();
    if (exitCode != 0) {
      m_mainWindow->showStatusMessage(tr("GPG key pair generation failed"),
                                      10000);
    }
  }

  if (!p_error.isEmpty()) {
    QString output;
    QString error = p_error.toHtmlEscaped();
    if (exitCode == 0) {
      //  https://github.com/IJHack/qtpass/issues/111
      output = "<span style=\"color: darkgray;\">" + error + "</span><br />";
    } else {
      output = "<span style=\"color: red;\">" + error + "</span><br />";
    }

    output.replace(Util::protocolRegex(), R"(<a href="\1">\1</a>)");
    output.replace(QStringLiteral("\n"), "<br />");

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

/**
 * @brief Called when pass store has changed.
 * @param p_out Output from the process
 * @param p_err Error output
 */
void QtPass::passStoreChanged(const QString &p_out, const QString &p_err) {
  processFinished(p_out, p_err);
  doGitPush();
}

/**
 * @brief Called when an insert operation has finished.
 * @param p_output Output from the process
 * @param p_errout Error output
 */
void QtPass::finishedInsert(const QString &p_output, const QString &p_errout) {
  processFinished(p_output, p_errout);
  doGitPush();
  m_mainWindow->on_treeView_clicked(m_mainWindow->getCurrentTreeViewIndex());
}

/**
 * @brief Called when GPG key generation is complete.
 * @param p_output Standard output from the key generation process
 * @param p_errout Standard error output from the key generation process
 */
void QtPass::onKeyGenerationComplete(const QString &p_output,
                                     const QString &p_errout) {
  if (nullptr != m_mainWindow->getKeygenDialog()) {
#ifdef QT_DEBUG
    qDebug() << "Keygen Done";
#endif

    m_mainWindow->cleanKeygenDialog();
    m_mainWindow->showStatusMessage(tr("GPG key pair generated successfully"),
                                    10000);
  }

  processFinished(p_output, p_errout);
}

/**
 * @brief Called when the password show handler has finished.
 * @param output The password content to display
 */
void QtPass::passShowHandlerFinished(QString output) {
  showInTextBrowser(std::move(output));
}

/**
 * @brief Displays output text in the main window's text browser.
 * @param output The text to display
 * @param prefix Optional prefix to prepend to the output
 * @param postfix Optional postfix to append to the output
 */
void QtPass::showInTextBrowser(QString output, const QString &prefix,
                               const QString &postfix) {
  output = output.toHtmlEscaped();

  output.replace(Util::protocolRegex(), R"(<a href="\1">\1</a>)");
  output.replace(QStringLiteral("\n"), "<br />");
  output = prefix + output + postfix;

  m_mainWindow->flashText(output, false, true);
}

/**
 * @brief Performs automatic git push if enabled in settings.
 */
void QtPass::doGitPush() {
  if (QtPassSettings::isAutoPush()) {
    m_mainWindow->onPush();
  }
}

/**
 * @brief Sets the text to be stored in clipboard and handles clipboard
 * operations.
 * @param password The password or text to store
 * @param p_output Additional output text
 */
void QtPass::setClippedText(const QString &password, const QString &p_output) {
  if (QtPassSettings::getClipBoardType() != Enums::CLIPBOARD_NEVER &&
      !p_output.isEmpty()) {
    clippedText = password;
    if (QtPassSettings::getClipBoardType() == Enums::CLIPBOARD_ALWAYS) {
      copyTextToClipboard(password);
    }
  }
}
/**
 * @brief Clears the stored clipped text.
 */
void QtPass::clearClippedText() { clippedText = ""; }

/**
 * @brief Sets the clipboard clear timer based on autoclear settings.
 */
void QtPass::setClipboardTimer() {
  clearClipboardTimer.setInterval(MS_PER_SECOND *
                                  QtPassSettings::getAutoclearSeconds());
}

/**
 * @brief MainWindow::clearClipboard remove clipboard contents.
 */
void QtPass::clearClipboard() {
  QClipboard *clipboard = QApplication::clipboard();
  bool cleared = false;
  if (this->clippedText == clipboard->text(QClipboard::Selection)) {
    clipboard->clear(QClipboard::Selection);
    clipboard->setText(QString(""), QClipboard::Selection);
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
 * @brief Build clipboard MIME data with platform-specific security hints.
 * @param text - Plain text to copy
 * @return QMimeData with text and security hints
 */
auto buildClipboardMimeData(const QString &text) -> QMimeData * {
  auto *mimeData = new QMimeData();
  mimeData->setText(text);
#ifdef Q_OS_LINUX
  mimeData->setData("x-kde-passwordManagerHint", QByteArray("secret"));
#endif
#ifdef Q_OS_MAC
  mimeData->setData("application/x-nspasteboard-concealed-type", QByteArray());
#endif
#ifdef Q_OS_WIN
  mimeData->setData("ExcludeClipboardContentFromMonitorProcessing",
                    dwordBytes(1));
  mimeData->setData("CanIncludeInClipboardHistory", dwordBytes(0));
  mimeData->setData("CanUploadToCloudClipboard", dwordBytes(0));
#endif
  return mimeData;
}

/**
 * @brief MainWindow::copyTextToClipboard copies text to your clipboard
 * @param text
 */
void QtPass::copyTextToClipboard(const QString &text) {
  QClipboard *clip = QApplication::clipboard();

  QClipboard::Mode mode = QClipboard::Clipboard;
  if (QtPassSettings::isUseSelection() && clip->supportsSelection()) {
    mode = QClipboard::Selection;
  }

  auto *mimeData = buildClipboardMimeData(text);
  clip->setMimeData(mimeData, mode);

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
  qrencode.start(QtPassSettings::getQrencodeExecutable("/usr/bin/qrencode"),
                 QStringList() << "-o-"
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
    QDialog *popup = createQRCodePopup(image);
    popup->exec();
  }
}

/**
 * @brief QtPass::createQRCodePopup creates a popup dialog with the given QR
 * code image. This is extracted for testability. The caller is responsible
 * for showing and managing the popup lifecycle.
 * @param image The QR code pixmap to display
 * @return The created popup dialog
 */
QDialog *QtPass::createQRCodePopup(const QPixmap &image) {
  auto *popup = new QDialog(nullptr, Qt::Popup | Qt::FramelessWindowHint);
  popup->setAttribute(Qt::WA_DeleteOnClose);
  auto *layout = new QVBoxLayout;
  auto *popupLabel = new QLabel();
  layout->addWidget(popupLabel);
  popupLabel->setPixmap(image);
  popupLabel->setScaledContents(true);
  popupLabel->show();
  popup->setLayout(layout);
  popup->move(QCursor::pos());
  return popup;
}
