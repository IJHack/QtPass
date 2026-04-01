// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @class Util
 * @brief Static utility functions implementation.
 *
 * Implementation of utility functions for path handling, binary discovery,
 * and configuration validation.
 *
 * @see util.h
 */

#include "util.h"
#include <QDir>
#include <QFileInfo>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/time.h>
#endif
#include "qtpasssettings.h"

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

QProcessEnvironment Util::_env;
bool Util::_envInitialised = false;

void Util::initialiseEnvironment() {
  if (!_envInitialised) {
    _env = QProcessEnvironment::systemEnvironment();
#ifdef __APPLE__
    QString path = _env.value("PATH");
    if (!path.contains("/usr/local/MacGPG2/bin") &&
        QDir("/usr/local/MacGPG2/bin").exists())
      path += ":/usr/local/MacGPG2/bin";
    if (!path.contains("/usr/local/bin"))
      path += ":/usr/local/bin";
    _env.insert("PATH", path);
#endif
#ifdef Q_OS_WIN
    QString path = _env.value("PATH");
    if (!path.contains("C:\\Program Files\\WinGPG\\x86") &&
        QDir("C:\\Program Files\\WinGPG\\x86").exists())
      path += ";C:\\Program Files\\WinGPG\\x86";
    if (!path.contains("C:\\Program Files\\GnuPG\\bin") &&
        QDir("C:\\Program Files\\GnuPG\\bin").exists())
      path += ";C:\\Program Files\\GnuPG\\bin";
    _env.insert("PATH", path);
#endif
#ifdef QT_DEBUG
    dbg() << _env.value("PATH");
#endif
    _envInitialised = true;
  }
}

auto Util::findPasswordStore() -> QString {
  QString path;
  initialiseEnvironment();
  if (_env.contains("PASSWORD_STORE_DIR")) {
    path = _env.value("PASSWORD_STORE_DIR");
  } else {
#ifdef Q_OS_WIN
    path = QDir::homePath() + QDir::separator() + "password-store" +
           QDir::separator();
#else
    path = QDir::homePath() + QDir::separator() + ".password-store" +
           QDir::separator();
#endif
  }
  return Util::normalizeFolderPath(path);
}

auto Util::normalizeFolderPath(QString path) -> QString {
  if (!path.endsWith("/") && !path.endsWith(QDir::separator())) {
    path += QDir::separator();
  }
  return QDir::toNativeSeparators(path);
}

auto Util::findBinaryInPath(QString binary) -> QString {
  initialiseEnvironment();

  QString ret;

  binary.prepend(QDir::separator());

  if (_env.contains("PATH")) {
    QString path = _env.value("PATH");
    const QChar delimiter = QDir::separator() == '\\' ? ';' : ':';
    QStringList entries = path.split(delimiter);

    for (const QString &entryConst : entries) {
      QString fullPath = entryConst + binary;
      QFileInfo qfi(fullPath);
#ifdef Q_OS_WIN
      if (!qfi.exists()) {
        QString fullPathExe = fullPath + ".exe";
        qfi = QFileInfo(fullPathExe);
      }

#endif
      if (!qfi.isExecutable()) {
        continue;
      }

      ret = qfi.absoluteFilePath();
      break;
    }
  }
#ifdef Q_OS_WIN
  if (ret.isEmpty()) {
    // Remove leading path separator added earlier when searching PATH entries.
    if (!binary.isEmpty() && (binary.at(0) == QDir::separator() || binary.at(0) == '/')) {
      binary.remove(0, 1);
    }

    // Validate binary name before attempting WSL fallback: require a non-empty,
    // whitespace-free program name to avoid confusing WSL invocations.
    const bool hasWhitespace = binary.contains(QRegularExpression(QStringLiteral("\\s")));
    if (!binary.isEmpty() && !hasWhitespace) {
      QString wslCommand = QStringLiteral("wsl ") + binary;
#ifdef QT_DEBUG
      dbg() << "Util::findBinaryInPath(): falling back to WSL for binary" << binary;
#endif
      QString out, err;
      if (Executor::executeBlocking(wslCommand, {"--version"}, &out, &err) == 0 &&
          !out.isEmpty() && err.isEmpty()) {
#ifdef QT_DEBUG
        dbg() << "Util::findBinaryInPath(): using WSL binary" << wslCommand;
#endif
        ret = wslCommand;
      }
    }
  }
#endif

  return ret;
}

auto Util::configIsValid() -> bool {
  const QString configFilePath =
      QDir(QtPassSettings::getPassStore()).filePath(".gpg-id");
  if (!QFile(configFilePath).exists()) {
    return false;
  }

  const QString executable = QtPassSettings::isUsePass()
                                 ? QtPassSettings::getPassExecutable()
                                 : QtPassSettings::getGpgExecutable();

  if (executable.startsWith("wsl ")) {
    return true;
  }
  return QFile(executable).exists();
}

auto Util::getDir(const QModelIndex &index, bool forPass,
                  const QFileSystemModel &model, const StoreModel &storeModel)
    -> QString {
  QString abspath =
      QDir(QtPassSettings::getPassStore()).absolutePath() + QDir::separator();
  if (!index.isValid()) {
    return forPass ? "" : abspath;
  }
  QFileInfo info = model.fileInfo(storeModel.mapToSource(index));
  QString filePath =
      (info.isFile() ? info.absolutePath() : info.absoluteFilePath());
  if (forPass) {
    filePath = QDir(abspath).relativeFilePath(filePath);
  }
  filePath += QDir::separator();
  return filePath;
}

auto Util::endsWithGpg() -> const QRegularExpression & {
  static const QRegularExpression expr{"\\.gpg$"};
  return expr;
}

auto Util::protocolRegex() -> const QRegularExpression & {
  static const QRegularExpression regex{
      "((?:https?|ftp|ssh|sftp|ftps|webdav|webdavs)://[^\" <>\\)\\]\\[]+)"};
  return regex;
}

auto Util::newLinesRegex() -> const QRegularExpression & {
  static const QRegularExpression regex{"[\r\n]"};
  return regex;
}