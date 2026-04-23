// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
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
#include <QFile>
#include <QFileInfo>
#include <QStringConverter>
#include <QTextStream>
#include <algorithm>
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
 * @brief Initializes the process environment and augments PATH with
 * platform-specific GPG locations.
 * @example
 * Util::initialiseEnvironment();
 *
 * @note On macOS, appends common MacGPG2 and /usr/local/bin paths if available.
 * @note On Windows, appends common WinGPG and GnuPG installation paths if
 * available.
 */
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

/**
 * @brief Resolves the path to the password store directory.
 * @details Initializes the environment, checks for the {@code
 * PASSWORD_STORE_DIR} variable, and falls back to a platform-specific default
 * location under the user's home directory.
 * @return QString - Normalized path to the password store folder.
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

auto Util::normalizeFolderPath(const QString &path) -> QString {
  QString normalizedPath = path;
  if (!normalizedPath.endsWith("/") &&
      !normalizedPath.endsWith(QDir::separator())) {
    normalizedPath += QDir::separator();
  }
  return QDir::toNativeSeparators(normalizedPath);
}

/**
 * @brief Finds the absolute path of a binary by searching the PATH environment
 * variable.
 *
 * Iterates through each PATH entry, checks whether the binary exists and is
 * executable, and returns the first matching absolute file path. On Windows, if
 * no local match is found, it may fall back to a WSL invocation when the binary
 * name is valid and WSL appears to support it.
 *
 * @example
 * QString result = Util::findBinaryInPath("git");
 * // Expected output sample: "/usr/bin/git" or "wsl git"
 *
 * @param QString binary - The name of the binary to locate.
 * @return QString - The absolute path to the binary, or an empty string if not
 * found.
 */
auto Util::findBinaryInPath(const QString &binary) -> QString {
  if (binary.isEmpty())
    return QString();

  initialiseEnvironment();

  QString ret;

  const QString binaryWithSep = QDir::separator() + binary;

  if (_env.contains("PATH")) {
    QString path = _env.value("PATH");
    const QChar delimiter = QDir::separator() == '\\' ? ';' : ':';
    QStringList entries = path.split(delimiter);

    for (const QString &entryConst : entries) {
      QString fullPath = entryConst + binaryWithSep;
      QFileInfo qfi(fullPath);
#ifdef Q_OS_WIN
      if (!qfi.exists()) {
        QString fullPathExe = fullPath + ".exe";
        qfi = QFileInfo(fullPathExe);
      }
#endif
      if (!qfi.exists()) {
        continue;
      }
      if (!qfi.isExecutable()) {
        continue;
      }

      ret = qfi.absoluteFilePath();
      break;
    }
  }
#ifdef Q_OS_WIN
  if (ret.isEmpty()) {
    static const QRegularExpression whitespaceRegex(QStringLiteral("\\s"));
    const bool hasWhitespace = binary.contains(whitespaceRegex);
    if (!binary.isEmpty() && !hasWhitespace) {
      QString wslCommand = QStringLiteral("wsl ") + binary;
#ifdef QT_DEBUG
      dbg() << "Util::findBinaryInPath(): falling back to WSL for binary"
            << binary;
#endif
      QString out, err;
      if (Executor::executeBlocking(wslCommand, {"--version"}, &out, &err) ==
              0 &&
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

/**
 * @brief Checks whether the current QtPass configuration is valid.
 * @example
 * bool result = Util::configIsValid();
 * std::cout << std::boolalpha << result << std::endl; // Expected output: true
 * or false
 *
 * @return bool - True if the configuration file exists and the required
 * executable is available; otherwise false.
 */
auto Util::configIsValid() -> bool {
  const QString configFilePath =
      QDir(QtPassSettings::getPassStore()).filePath(".gpg-id");
  if (!QFile(configFilePath).exists()) {
    return false;
  }

  const QString executable = QtPassSettings::isUsePass()
                                 ? QtPassSettings::getPassExecutable()
                                 : QtPassSettings::getGpgExecutable();

  if (executable.startsWith(QStringLiteral("wsl "))) {
    QString out;
    QString err;
    if (Executor::executeBlocking(QStringLiteral("wsl"),
                                  {QStringLiteral("--version")}, &out,
                                  &err) == 0 &&
        !out.isEmpty() && err.isEmpty()) {
      return true;
    }
  }
  return QFile(executable).exists();
}

/**
 * @brief Returns a directory path derived from a model index, optionally
 * relative to the pass store.
 * @example
 * QString result = Util::getDir(index, true, model, storeModel);
 * std::cout << result.toStdString() << std::endl; // Expected output: relative
 * directory path with trailing separator
 *
 * @param QModelIndex &index - Source index used to resolve the file or
 * directory path.
 * @param bool forPass - If true, returns a path relative to the pass store;
 * otherwise returns an absolute path.
 * @param QFileSystemModel &model - File system model used to obtain file
 * information.
 * @param StoreModel &storeModel - Proxy model used to map the provided index to
 * the source model.
 * @return QString - The resolved directory path, always ending with the
 * platform's directory separator.
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

auto Util::endsWithGpg() -> const QRegularExpression & {
  static const QRegularExpression expr{"\\.gpg$"};
  return expr;
}

/**
 * @brief Returns a regex matching common remote/network protocol schemes.
 *
 * Matches http://, https://, ftp://, ftps://, ssh://, sftp://, webdav://,
 * webdavs://
 *
 * Note: Local file URLs (file:///) are intentionally excluded by design, as
 * they represent local paths rather than network protocols. If this behavior
 * needs to change, update both this function and the corresponding test.
 *
 * @return QRegularExpression reference
 */
auto Util::protocolRegex() -> const QRegularExpression & {
  static const QRegularExpression regex{
      "((?:https?|ftp|ssh|sftp|ftps|webdav|webdavs)://[^\" <>\\)\\]\\[]+)"};
  return regex;
}

auto Util::newLinesRegex() -> const QRegularExpression & {
  static const QRegularExpression regex{"[\r\n]"};
  return regex;
}

auto Util::isValidKeyId(const QString &keyId) -> bool {
  static const QRegularExpression hexPrefixRegex{"^0[xX]"};
  static const QRegularExpression specialPrefixRegex{"^[@/#&]"};
  static const QRegularExpression hexKeyIdRegex{"^[0-9A-Fa-f]{8,40}$"};

  if (keyId.isEmpty()) {
    return false;
  }

  QString normalized = keyId;
  if (normalized.startsWith('<') && normalized.endsWith('>')) {
    normalized = normalized.mid(1, normalized.length() - 2);
  }
  normalized.remove(hexPrefixRegex);

  if (specialPrefixRegex.match(normalized).hasMatch() ||
      normalized.contains('@')) {
    return true;
  }

  return hexKeyIdRegex.match(normalized).hasMatch();
}

/**
 * @brief Read templates from .templates file in password store.
 * @param storePath Path to password store root.
 * @return Hash of template name to field list.
 */
auto Util::readTemplates(const QString &storePath)
    -> QHash<QString, QStringList> {
  QHash<QString, QStringList> result;
  QFile file(QDir(storePath).filePath(".templates"));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return result;
  }
  QTextStream in(&file);
#ifdef QT_VERSION_LT_6_0
  in.setCodec("UTF-8");
#else
  in.setEncoding(QStringConverter::Utf8);
#endif
  QString currentSection;
  QStringList currentFields;
  while (!in.atEnd()) {
    QString line = in.readLine().trimmed();
    if (line.startsWith('[') && line.endsWith(']')) {
      if (!currentSection.isEmpty()) {
        result.insert(currentSection, currentFields);
      }
      currentSection = line.mid(1, line.length() - 2);
      if (currentSection.isEmpty()) {
        qWarning() << "Empty template section in .templates file";
        currentSection.clear();
        currentFields.clear();
      }
      currentFields.clear();
    } else if (!line.isEmpty() && !line.startsWith('#')) {
      currentFields.append(line);
    }
  }
  if (!currentSection.isEmpty()) {
    result.insert(currentSection, currentFields);
  }
  file.close();
  return result;
}

/**
 * @brief Write templates to .templates file in password store.
 * @param storePath Path to password store root.
 * @param templates Hash of template name to field list.
 * @return true if write succeeded.
 */
auto Util::writeTemplates(const QString &storePath,
                          const QHash<QString, QStringList> &templates)
    -> bool {
  QFile file(QDir(storePath).filePath(".templates"));
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }
  QTextStream out(&file);
#ifdef QT_VERSION_LT_6_0
  out.setCodec("UTF-8");
#else
  out.setEncoding(QStringConverter::Utf8);
#endif
  out << "# QtPass templates configuration\n";
  out << "# Format: INI-style with [template_name] sections,\n";
  out << "# followed by field names (one per line)\n\n";

  QStringList sortedKeys = templates.keys();
  std::sort(sortedKeys.begin(), sortedKeys.end());
  for (const QString &key : sortedKeys) {
    out << "[" << key << "]\n";
    for (const QString &field : templates.value(key)) {
      out << field << "\n";
    }
    out << "\n";
  }
  file.close();
  return true;
}

/**
 * @brief Get default template for a folder.
 * Looks in folder, then parent folders up to root.
 * @param folderPath Path to folder.
 * @param storePath Path to password store root.
 * @return Template name or empty if none found.
 */
auto Util::getFolderTemplate(const QString &folderPath,
                             const QString &storePath) -> QString {
  QDir dir(folderPath);
  QString cleanStore = QDir::cleanPath(storePath);
  while (true) {
    if (dir.exists(".default_template")) {
      QFile file(dir.filePath(".default_template"));
      if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
#ifdef QT_VERSION_LT_6_0
        in.setCodec("UTF-8");
#else
        in.setEncoding(QStringConverter::Utf8);
#endif
        QString templateName = in.readLine().trimmed();
        file.close();
        if (!templateName.isEmpty() && !templateName.startsWith('#')) {
          return templateName;
        }
      }
    }
    QString currentPath = QDir::cleanPath(dir.absolutePath());
    if (currentPath == cleanStore) {
      break;
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return QString();
}