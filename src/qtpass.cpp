// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "qtpass.h"
#include "mainwindow.h"
#include "qtpasssettings.h"
#include "settingsconstants.h"
#include "util.h"
#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>

#include <utility>

#include "qtpasslogging.h"

/**
 * @brief Constructs a QtPass instance.
 * @param mainWindow The main window reference
 */
QtPass::QtPass(MainWindow *mainWindow)
    : QObject(mainWindow), m_mainWindow(mainWindow), m_clipboard(this) {
  connect(&m_clipboard, &ClipboardManager::statusMessage, m_mainWindow,
          [this](const QString &message) {
            m_mainWindow->showStatusMessage(message);
          });

  setMainWindow();
}

/**
 * @brief QtPass::~QtPass destroy!
 */
QtPass::~QtPass() = default;

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
    qCDebug(lcQtPass) << "assuming fresh install";
    AppSettings s = QtPassSettings::load();
    if (s.autoclearSeconds < 5)
      s.autoclearSeconds = 10;
    if (s.autoclearPanelSeconds < 5)
      s.autoclearPanelSeconds = 10;
    s.usePwgen = !s.pwgenExecutable.isEmpty();
    s.passTemplate = QStringLiteral("login\nurl\nOTP");
    QtPassSettings::save(s);
    // A fresh profile already has the new useOtp default, so record the
    // migration as done. Without this, turning OTP off during the first session
    // would be undone by the migration branch on the next launch.
    QtPassSettings::getInstance()->setValue(
        SettingsConstants::otpMigratedToNative, true);
  } else {
    AppSettings s = QtPassSettings::load();
    bool changed = false;
    if (s.passTemplate.isEmpty()) {
      s.passTemplate = QStringLiteral("login\nurl\nOTP");
      changed = true;
    }
    // `useOtp` used to gate the Unix-only pass-otp extension and now gates
    // built-in TOTP, so an existing profile's stored false is stale rather than
    // a preference. The new default of true never reaches these users: the
    // fresh-install branch above already persisted the key, and the serializer
    // writes it unconditionally. Enable it once, remembered by its own key so a
    // deliberate opt-out is not overridden on the next launch.
    if (!QtPassSettings::getInstance()
             ->value(SettingsConstants::otpMigratedToNative, false)
             .toBool()) {
      s.useOtp = true;
      QtPassSettings::getInstance()->setValue(
          SettingsConstants::otpMigratedToNative, true);
      changed = true;
    }
    if (changed) {
      QtPassSettings::save(s);
    }
  }

  QtPassSettings::setVersion(VERSION);

  // Ask again until the configuration is usable or the user gives up. The
  // dialog's OK button is not gated on Util::configIsValid(), so an accepted
  // first-run configuration can still point at a store without a .gpg-id
  // (the user declined to create it); only a cancel ends the loop, which
  // main() turns into an exit before the window is shown.
  while (!Util::configIsValid(QtPassSettings::load())) {
    if (!m_mainWindow->config()) {
      return false;
    }
  }

  freshStart = false;
  return true;
}

/**
 * @brief Sets up the main window and connects signal handlers.
 */
void QtPass::setMainWindow() {
  m_mainWindow->restoreWindow();

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
  connect(QtPassSettings::getImitatePass(), &ImitatePass::reencryptProgress,
          m_mainWindow, &MainWindow::reencryptProgress);
  connect(QtPassSettings::getImitatePass(), &ImitatePass::endReencryptPath,
          m_mainWindow, &MainWindow::endReencryptPath);
}

/**
 * @brief Connects pass signal handlers to QtPass slots.
 * @param pass The pass instance to connect
 */
void QtPass::connectPassSignalHandlers(Pass *pass) {
  connect(pass, &Pass::processErrorExit, this, &QtPass::processErrorExit);
  // A failed decrypt never emits finishedShow, so an OTP request would
  // otherwise stay pending for the rest of the session.
  connect(pass, &Pass::processErrorExit, m_mainWindow,
          &MainWindow::cancelOtpRequest);
  connect(pass, &Pass::critical, m_mainWindow, &MainWindow::critical);
  connect(pass, &Pass::startingExecuteWrapper, m_mainWindow,
          &MainWindow::executeWrapperStarted);
  connect(pass, &Pass::statusMsg, m_mainWindow, &MainWindow::showStatusMessage);
  connect(pass, &Pass::finishedShow, m_mainWindow,
          &MainWindow::passShowHandler);
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
 * @brief Handles process error exit.
 * @param exitCode The exit code
 * @param p_error The error message
 */
void QtPass::processErrorExit(int exitCode, const QString &p_error) {
  if (!p_error.isEmpty()) {
    QString output;
    // Escapes and links only launchable http(s) URLs; anything else stays
    // plain text (the text browser opens external links on click).
    const QString error = Util::linkifyUrls(p_error);
    if (exitCode == 0) {
      //  https://github.com/IJHack/qtpass/issues/111
      output = "<span style=\"color: darkgray;\">" + error + "</span><br />";
    } else {
      output = "<span style=\"color: red;\">" + error + "</span><br />";
    }

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
  // The KeygenDialog listens to the same signal and closes itself.
  m_mainWindow->showStatusMessage(tr("GPG key pair generated successfully"),
                                  10000);
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
  // Escapes and links only launchable http(s) URLs; anything else stays
  // plain text (the text browser opens external links on click).
  output = Util::linkifyUrls(output);
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
 * @brief displays the text as qrcode
 * @param text
 */
void QtPass::showTextAsQRCode(const QString &text) {
  const AppSettings s = QtPassSettings::load();
  const QString qrExe = s.qrencodeExecutable.isEmpty()
                            ? QStringLiteral("/usr/bin/qrencode")
                            : s.qrencodeExecutable;
  QProcess qrencode;
  qrencode.start(qrExe, QStringList() << "-o-"
                                      << "-tPNG");
  // A missing or non-executable binary never "finishes": exitStatus() stays
  // NormalExit and exitCode() 0, which used to fall through to an empty popup.
  if (!qrencode.waitForStarted()) {
    m_mainWindow->showStatusMessage(
        tr("Could not start qrencode: %1").arg(qrExe));
    return;
  }
  qrencode.write(text.toUtf8());
  qrencode.closeWriteChannel();
  // A hung qrencode also leaves exitStatus()/exitCode() at their defaults.
  if (!qrencode.waitForFinished()) {
    qrencode.kill();
    m_mainWindow->showStatusMessage(tr("qrencode did not finish in time"));
    return;
  }
  QByteArray output(qrencode.readAllStandardOutput());

  const bool crashed = qrencode.exitStatus() != QProcess::NormalExit;
  // exitCode() is meaningless after a crash, so only consult it otherwise.
  const bool failed = !crashed && qrencode.exitCode() != 0;
  if (crashed || failed) {
    QString error(qrencode.readAllStandardError());
    if (error.trimmed().isEmpty()) {
      error = crashed
                  ? tr("qrencode crashed")
                  : tr("qrencode exited with code %1").arg(qrencode.exitCode());
    }
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
