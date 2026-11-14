// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Implements QtPass; init() holds the settings migrations run on every
 *        start-up and showTextAsQRCode() drives the qrencode process.
 */
#include "qtpass.h"
#include "executor.h"
#include "imitatepass.h"
#include "qtpasssettings.h"
#include "settingsconstants.h"
#include "util.h"
#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>

#include "qtpasslogging.h"

QtPass::QtPass(QObject *parent) : QObject(parent), m_clipboard(this) {
  // Both implementations are wired up: the active one can change through the
  // configuration dialog, and reconnecting on every switch is not worth it.
  connectPassSignalHandlers(QtPassSettings::getRealPass());
  connectPassSignalHandlers(QtPassSettings::getImitatePass());
}

QtPass::~QtPass() = default;

void QtPass::init() {
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
}

void QtPass::connectPassSignalHandlers(Pass *pass) {
  connect(pass, &Pass::processErrorExit, this, &QtPass::processErrorExit);
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

void QtPass::reportError(int exitCode, const QString &error) {
  if (error.isEmpty()) {
    return;
  }
  const QString colour =
      exitCode == 0 ? QStringLiteral("darkgray") : QStringLiteral("red");
  emit outputReady(formatOutput(
      error, QStringLiteral("<span style=\"color: %1;\">").arg(colour),
      QStringLiteral("</span><br />")));
}

void QtPass::processErrorExit(int exitCode, const QString &error) {
  reportError(exitCode, error);
  emit operationFinished();
}

void QtPass::processFinished(const QString &output, const QString &errout) {
  // A silent command (git push with nothing to push, say) has nothing to
  // show; re-setting the browser's HTML for it would only reset its scroll.
  if (!output.isEmpty()) {
    emit outputReady(formatOutput(output));
  }
  reportError(0, errout);
  emit operationFinished();
}

void QtPass::passStoreChanged(const QString &output, const QString &errout) {
  processFinished(output, errout);
  doGitPush();
}

void QtPass::finishedInsert(const QString &output, const QString &errout) {
  processFinished(output, errout);
  doGitPush();
  emit entryInserted();
}

void QtPass::onKeyGenerationComplete(const QString &output,
                                     const QString &errout) {
  // The KeygenDialog listens to the same signal and closes itself.
  emit statusMessage(tr("GPG key pair generated successfully"), 10000);
  processFinished(output, errout);
}

auto QtPass::formatOutput(QString output, const QString &prefix,
                          const QString &postfix) -> QString {
  // Escapes and links only launchable http(s) URLs; anything else stays
  // plain text (the text browser opens external links on click).
  output = Util::linkifyUrls(output);
  output.replace(QStringLiteral("\n"), "<br />");
  return prefix + output + postfix;
}

void QtPass::doGitPush() {
  if (QtPassSettings::isAutoPush()) {
    emit pushRequested();
  }
}

void QtPass::showTextAsQRCode(const QString &text) {
  const AppSettings s = QtPassSettings::load();
  const QString qrExe = s.qrencodeExecutable.isEmpty()
                            ? QStringLiteral("/usr/bin/qrencode")
                            : s.qrencodeExecutable;
  QProcess qrencode;
  Executor::detachFromTerminal(qrencode);
  qrencode.start(qrExe, QStringList() << "-o-"
                                      << "-tPNG");
  // A missing or non-executable binary never "finishes": exitStatus() stays
  // NormalExit and exitCode() 0, which used to fall through to an empty popup.
  if (!qrencode.waitForStarted()) {
    emit statusMessage(tr("Could not start qrencode: %1").arg(qrExe), 2000);
    return;
  }
  qrencode.write(text.toUtf8());
  qrencode.closeWriteChannel();
  // A hung qrencode also leaves exitStatus()/exitCode() at their defaults.
  if (!qrencode.waitForFinished()) {
    qrencode.kill();
    emit statusMessage(tr("qrencode did not finish in time"), 2000);
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
    emit statusMessage(error, 2000);
  } else {
    QPixmap image;
    image.loadFromData(output, "PNG");
    QDialog *popup = createQRCodePopup(image);
    popup->exec();
  }
}

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
