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
#include "executor.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>
#include <QTextStream>
#include <algorithm>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif
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

namespace {
/**
 * @brief Verify that a candidate SSH_AUTH_SOCK socket has a live agent
 *        listening, before letting QtPass switch the environment to it.
 *
 * Why: a user may have a working external SSH agent (OpenSSH ssh-agent,
 * KeePassXC / 1Password / yubikey-agent / gnome-keyring) configured in
 * their shell, but its env didn't propagate to the GUI launcher. If we
 * blindly adopt whatever `gpgconf --list-dirs agent-ssh-socket` reports —
 * even when gpg-agent isn't running with SSH support, or has no keys —
 * we'd be silently switching them away from their real agent.
 *
 * Approach: spawn `ssh-add -l` with SSH_AUTH_SOCK pointed at the candidate
 * (via Executor's env-aware overload, so the parent env isn't disturbed).
 * Treat exit codes 0 (keys present) and 1 (agent alive but key list empty —
 * legitimate for YubiKey-backed setups that enumerate on tap) as
 * "reachable". Anything else, including ssh-add not being on PATH, means
 * we don't adopt the candidate.
 *
 * @param candidate Path to validate.
 * @return true if the socket has a reachable agent; false otherwise.
 */
auto isSshAgentReachable(const QString &candidate) -> bool {
  if (candidate.isEmpty()) {
    return false;
  }
  // Build a minimal env that overrides SSH_AUTH_SOCK without polluting the
  // parent process. systemEnvironment() captures whatever was set when
  // QtPass started, then we override the one var of interest.
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert(QStringLiteral("SSH_AUTH_SOCK"), candidate);
  QString out;
  QString err;
  const int exitCode =
      Executor::executeBlocking(env.toStringList(), QStringLiteral("ssh-add"),
                                {QStringLiteral("-l")}, &out, &err);
  // OpenSSH ssh-add(1) exit codes: 0 = success / has keys, 1 = no
  // identities present, 2 = couldn't open a connection. Anything else is
  // also treated as unreachable (e.g. ssh-add not on PATH).
  return exitCode == 0 || exitCode == 1;
}
} // namespace

/**
 * @brief Probe and set SSH_AUTH_SOCK if missing — see header for full rules.
 *
 * Implementation notes:
 * - Reads QtPassSettings::getSshAuthSockOverride() for the manual override
 *   path; if non-empty, uses it verbatim (no validation — explicit override).
 * - Otherwise runs `gpgconf --list-dirs agent-ssh-socket` (gpg-agent's
 *   canonical socket reporter). On macOS, additionally tries `launchctl
 *   getenv SSH_AUTH_SOCK`.
 * - Auto-probed candidates are validated via `ssh-add -l` before adoption,
 *   so users with a different external SSH agent aren't silently switched
 *   to an empty gpg-agent SSH socket.
 * - All probes go through Executor::executeBlocking with short, bounded
 *   subprocess executions; any failure is silently absorbed (the user just
 *   doesn't get the auto-fix).
 */
void Util::initialiseSshAuthSock() {
  // Honour any value already in the environment — terminal launches,
  // explicit `.desktop` Exec= overrides, and parent-process exports must win.
  if (!qgetenv("SSH_AUTH_SOCK").isEmpty()) {
    return;
  }

  // Manual override from settings takes precedence over auto-probe.
  const QString override = QtPassSettings::getSshAuthSockOverride();
  if (!override.isEmpty()) {
    qputenv("SSH_AUTH_SOCK", override.toUtf8());
#ifdef QT_DEBUG
    dbg() << "Util::initialiseSshAuthSock(): set from settings override:"
          << override;
#endif
    return;
  }

  // Auto-probe via gpgconf (canonical for gpg-agent's SSH support).
  QString out;
  QString err;
  if (Executor::executeBlocking(
          QStringLiteral("gpgconf"),
          {QStringLiteral("--list-dirs"), QStringLiteral("agent-ssh-socket")},
          &out, &err) == 0) {
    const QString socket = out.trimmed();
    if (!socket.isEmpty() && isSshAgentReachable(socket)) {
      qputenv("SSH_AUTH_SOCK", socket.toUtf8());
#ifdef QT_DEBUG
      dbg() << "Util::initialiseSshAuthSock(): set from gpgconf:" << socket;
#endif
      return;
    }
#ifdef QT_DEBUG
    if (!socket.isEmpty()) {
      dbg() << "Util::initialiseSshAuthSock(): gpgconf reported" << socket
            << "but ssh-add -l rejected it; not adopting";
    }
#endif
  }

#ifdef Q_OS_MACOS
  // On macOS, GUI-launched apps may have SSH_AUTH_SOCK in launchd's
  // per-session environment but not in the inherited process env.
  out.clear();
  err.clear();
  if (Executor::executeBlocking(
          QStringLiteral("launchctl"),
          {QStringLiteral("getenv"), QStringLiteral("SSH_AUTH_SOCK")}, &out,
          &err) == 0) {
    const QString socket = out.trimmed();
    if (!socket.isEmpty() && isSshAgentReachable(socket)) {
      qputenv("SSH_AUTH_SOCK", socket.toUtf8());
#ifdef QT_DEBUG
      dbg() << "Util::initialiseSshAuthSock(): set from launchctl:" << socket;
#endif
    }
#ifdef QT_DEBUG
    else if (!socket.isEmpty()) {
      dbg() << "Util::initialiseSshAuthSock(): launchctl reported" << socket
            << "but ssh-add -l rejected it; not adopting";
    }
#endif
  }
#endif
}

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
    // Expand current-user tilde forms ("~" and "~/...") — env vars set in
    // non-shell contexts (systemd units, .desktop entries, quoted shell
    // assignments) skip shell tilde expansion, leaving "~" literal.
    // Note: "~username" forms are intentionally not resolved here.
    if (path == "~") {
      path = QDir::homePath();
    } else if (path.startsWith("~/")) {
      path = QDir::homePath() + path.mid(1);
    }
  } else {
#ifdef Q_OS_WIN
    path = QDir(QDir::homePath()).filePath("password-store");
#else
    path = QDir(QDir::homePath()).filePath(".password-store");
#endif
  }
  return Util::normalizeFolderPath(QDir::cleanPath(path));
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
  if (binary.isEmpty()) {
    return {};
  }

  initialiseEnvironment();

  QString ret;

  const QString binaryWithSep = QDir::separator() + binary;

  if (_env.contains("PATH")) {
    QString path = _env.value("PATH");
#ifdef Q_OS_WIN
    const QChar delimiter = ';';
#else
    const QChar delimiter = ':';
#endif
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
    // Cache per-binary WSL lookup result — the wsl --version probe is a
    // blocking subprocess that can run several times per session for
    // missing binaries; once decided, the answer doesn't change at runtime.
    static QHash<QString, QString> wslBinaryCache;
    const bool hasWhitespace =
        std::any_of(binary.cbegin(), binary.cend(),
                    [](const QChar ch) { return ch.isSpace(); });
    if (!hasWhitespace) {
      auto cached = wslBinaryCache.constFind(binary);
      if (cached != wslBinaryCache.constEnd()) {
        ret = cached.value();
      } else {
        QString wslCommand = QStringLiteral("wsl ") + binary;
#ifdef QT_DEBUG
        dbg() << "Util::findBinaryInPath(): falling back to WSL for binary"
              << binary;
#endif
        QString out, err;
        QString cachedResult;
        if (Executor::executeBlocking(wslCommand, {"--version"}, &out, &err) ==
                0 &&
            !out.isEmpty() && err.isEmpty()) {
#ifdef QT_DEBUG
          dbg() << "Util::findBinaryInPath(): using WSL binary" << wslCommand;
#endif
          cachedResult = wslCommand;
        }
        wslBinaryCache.insert(binary, cachedResult);
        ret = cachedResult;
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
    // Probe WSL once per session — availability doesn't change at runtime
    // and the executeBlocking call is a blocking subprocess.
    static const bool wslAvailable = []() {
      QString out;
      QString err;
      return Executor::executeBlocking(QStringLiteral("wsl"),
                                       {QStringLiteral("--version")}, &out,
                                       &err) == 0 &&
             !out.isEmpty() && err.isEmpty();
    }();
    if (wslAvailable) {
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

/**
 * @brief Returns a regex matching strings that end with the .gpg extension.
 *
 * @return QRegularExpression reference
 */
auto Util::endsWithGpg() -> const QRegularExpression & {
  static const QRegularExpression expr{R"(\.gpg$)"};
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
      R"(((?:https?|ftp|ssh|sftp|ftps|webdav|webdavs)://[^" <>\)\]\[]+))"};
  return regex;
}

/**
 * @brief Returns a regex matching newline characters (CR or LF).
 *
 * Useful for detecting or sanitising line breaks in text content.
 *
 * @return QRegularExpression reference
 */
auto Util::newLinesRegex() -> const QRegularExpression & {
  static const QRegularExpression regex{"[\r\n]"};
  return regex;
}

/**
 * @brief Validate whether a string is an accepted GPG key identifier.
 *
 * Accepted formats:
 * - Hexadecimal key IDs / fingerprints, length 8 to 40 hex characters,
 *   optionally prefixed with `0x` or `0X`.
 * - Identifiers wrapped in angle brackets (`<...>`); brackets are stripped
 *   before validation.
 * - Special routing prefixes: `@`, `/`, `#`, `&` (used by GnuPG to look up
 *   keys via mail-server / keyring / fingerprint substring matchers).
 * - Email-style user IDs (any value containing `@`).
 *
 * Empty input is invalid.
 *
 * @param keyId Input key identifier string to validate.
 * @return true if the input matches any accepted format; false otherwise.
 */
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  in.setEncoding(QStringConverter::Utf8);
#else
  in.setCodec("UTF-8");
#endif
  QString currentSection;
  QStringList currentFields;
  bool skipInvalidSection = false;
  while (!in.atEnd()) {
    QString line = in.readLine().trimmed();
    if (line.startsWith('[') && line.endsWith(']')) {
      if (!currentSection.isEmpty() && !skipInvalidSection) {
        result.insert(currentSection, currentFields);
      }
      currentSection = line.mid(1, line.length() - 2).trimmed();
      if (currentSection.isEmpty()) {
        qWarning()
            << "Empty template section in .templates file, ignoring fields";
        skipInvalidSection = true;
        currentFields.clear();
      } else {
        skipInvalidSection = false;
        currentFields.clear();
      }
    } else if (!line.isEmpty() && !line.startsWith('#') &&
               !skipInvalidSection) {
      currentFields.append(line);
    }
  }
  if (!currentSection.isEmpty() && !skipInvalidSection) {
    result.insert(currentSection, currentFields);
  }
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
  QSaveFile saveFile(QDir(storePath).filePath(".templates"));
  if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }
  QTextStream out(&saveFile);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  out.setEncoding(QStringConverter::Utf8);
#else
  out.setCodec("UTF-8");
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
  out.flush();
  if (out.status() != QTextStream::Ok) {
    return false;
  }
  return saveFile.commit();
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
  QDir storeDir(storePath);
  QString cleanStoreAbs = QDir::cleanPath(storeDir.absolutePath());
  QString sep = QDir::separator();
  QDir dir(folderPath);
  while (true) {
    if (dir.exists(".default_template")) {
      QFile file(dir.filePath(".default_template"));
      if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        in.setEncoding(QStringConverter::Utf8);
#else
        in.setCodec("UTF-8");
#endif
        QString templateName = in.readLine().trimmed();
        file.close();
        if (!templateName.isEmpty() && !templateName.startsWith('#')) {
          return templateName;
        }
      }
    }
    QString currentPath = QDir::cleanPath(dir.absolutePath());
    if (currentPath == cleanStoreAbs) {
      break;
    }
    if (!currentPath.startsWith(cleanStoreAbs + sep)) {
      break;
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return {};
}
