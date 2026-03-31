// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
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

/**
 * @brief Util::initialiseEnvironment set the correct PATH for use with gpg, git
 * etc.
 */
void Util::initialiseEnvironment() {
  if (!_envInitialised) {
    _env = QProcessEnvironment::systemEnvironment();
#ifdef __APPLE__
    QString path = _env.value("PATH");
    if (!path.contains("/usr/local/MacGPG2/bin") &&
        QFile("/usr/local/MacGPG2/bin").exists())
      path += ":/usr/local/MacGPG2/bin";
    if (!path.contains("/usr/local/bin"))
      path += ":/usr/local/bin";
    _env.insert("PATH", path);
#endif
#ifdef Q_OS_WIN
    QString path = _env.value("PATH");
    if (!path.contains("C:\\Program Files\\WinGPG\\x86") &&
        QFile("C:\\Program Files\\WinGPG\\x86").exists())
      path += ";C:\\Program Files\\WinGPG\\x86";
    if (!path.contains("C:\\Program Files\\GnuPG\\bin") &&
        QFile("C:\\Program Files\\GnuPG\\bin").exists())
      path += ";C:\\Program Files\\GnuPG\\bin";
    _env.insert("PATH", path);
#endif
#ifdef QT_DEBUG
    dbg() << _env.value("PATH");
#endif
    _envInitialised = true;
  }
}

/**
 * @brief Util::findPasswordStore look for common .password-store folder
 * location.
 * @return
 */
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

/**
 * @brief Util::normalizeFolderPath let's always end folders with a
 * QDir::separator()
 * @param path
 * @return
 */
auto Util::normalizeFolderPath(QString path) -> QString {
  if (!path.endsWith("/") && !path.endsWith(QDir::separator())) {
    path += QDir::separator();
  }
  return QDir::toNativeSeparators(path);
}

/**
 * @brief Locate an executable by searching the process PATH and (on Windows) falling back to WSL.
 *
 * @param binary Executable name or relative path to locate (e.g., "gpg" or "pass").
 * @return QString Absolute path to the executable if found, empty QString otherwise.
 */

/**
 * @brief Determine whether required configuration or executables are missing.
 *
 * @return bool `true` if the password store's `.gpg-id` is missing or the configured executable
 *              (pass or gpg, depending on settings) does not exist; `false` otherwise.
 */

/**
 * @brief Get the selected folder path, either relative to the configured pass store or absolute.
 *
 * @param index Model index selecting the file or folder.
 * @param forPass If true, return the path relative to the pass store; otherwise return an absolute path.
 * @param model Filesystem model used to resolve the index.
 * @param storeModel StoreModel used to map view indexes to the filesystem model.
 * @return QString Folder path that always ends with the native directory separator. Returns an empty
 *                 string when `index` is invalid and `forPass` is true; otherwise returns the pass store root.
 */

/**
 * @brief Returns a regex to match file names that end with ".gpg".
 *
 * @return const QRegularExpression& Reference to a static regex matching "\.gpg$".
 */

/**
 * @brief Returns a regex to match URL-like protocols and their following path.
 *
 * @return const QRegularExpression& Reference to a static regex matching protocols like
 *                                  http/https/ftp/ssh/sftp/ftps/webdav/webdavs followed by a path.
 */

/**
 * @brief Returns a regex to match carriage return or newline characters.
 *
 * @return const QRegularExpression& Reference to a static regex matching "\r" or "\n".
 */
auto Util::findBinaryInPath(QString binary) -> QString {
  initialiseEnvironment();

  QString ret;

  binary.prepend(QDir::separator());

  if (_env.contains("PATH")) {
    QString path = _env.value("PATH");

    QStringList entries;
#ifndef Q_OS_WIN
    entries = path.split(':');
    if (entries.length() < 2) {
#endif
      entries = path.split(';');
#ifndef Q_OS_WIN
    }
#endif

    for (const QString &entryConst : entries) {
      QString fullPath = entryConst + binary;
      QScopedPointer<QFileInfo> qfi(new QFileInfo(fullPath));
#ifdef Q_OS_WIN
      if (!qfi->exists()) {
        QString fullPathExe = fullPath + ".exe";
        qfi.reset(new QFileInfo(fullPathExe));
      }

#endif
      if (!qfi->isExecutable()) {
        continue;
      }

      ret = qfi->absoluteFilePath();
      break;
    }
  }
#ifdef Q_OS_WIN
  if (ret.isEmpty()) {
    binary.remove(0, 1);
    binary.prepend("wsl ");
    QString out, err;
    if (Executor::executeBlocking(binary, {"--version"}, &out, &err) == 0 &&
        !out.isEmpty() && err.isEmpty())
      ret = binary;
  }
#endif

  return ret;
}

/**
 * @brief Util::checkConfig do we have prerequisite settings?
 * @return
 */
auto Util::checkConfig() -> bool {
  return !QFile(QDir(QtPassSettings::getPassStore()).filePath(".gpg-id"))
              .exists() ||
         (QtPassSettings::isUsePass()
              ? !QtPassSettings::getPassExecutable().startsWith("wsl ") &&
                    !QFile(QtPassSettings::getPassExecutable()).exists()
              : !QtPassSettings::getGpgExecutable().startsWith("wsl ") &&
                    !QFile(QtPassSettings::getGpgExecutable()).exists());
}

/**
 * @brief Util::getDir get selected folder path
 * @param index
 * @param forPass short or full path
 * @param model the filesystem model to operate on
 * @param storeModel our storemodel to operate on
 * @return path
 */
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

/**
 * @brief Returns a regex to match .gpg file extensions.
 * @return Reference to static regex
 */
auto Util::endsWithGpg() -> const QRegularExpression & {
  static const QRegularExpression expr{"\\.gpg$"};
  return expr;
}

/**
 * @brief Returns a regex to match URL protocols.
 * @return Reference to static regex
 */
auto Util::protocolRegex() -> const QRegularExpression & {
  static const QRegularExpression regex{
      "((?:https?|ftp|ssh|sftp|ftps|webdav|webdavs)://[^\" <>\\)\\]\\[]+)"};
  return regex;
}

/**
 * @brief Returns a regex to match newline characters.
 * @return Reference to static regex
 */
auto Util::newLinesRegex() -> const QRegularExpression & {
  static const QRegularExpression regex{"[\r\n]"};
  return regex;
}
