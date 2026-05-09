// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include "storemodel.h"
#include <QFileSystemModel>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QString>

constexpr int MS_PER_SECOND = 1000;

class StoreModel;

/*!
    \class Util
    \brief Some static utilities to be used elsewhere.
 */
class Util {
public:
  /**
   * @brief Locate an executable by searching the process PATH and (on Windows)
   * falling back to WSL.
   * @param binary Executable name or relative path to locate (e.g., "gpg" or
   * "pass").
   * @return QString Absolute path to the executable if found, empty QString
   * otherwise.
   */
  static auto findBinaryInPath(const QString &binary) -> QString;
  /**
   * @brief Locate the password store directory.
   * @return QString Path to the password store, always ends with native
   * directory separator.
   */
  static auto findPasswordStore() -> QString;
  /**
   * @brief Ensure a folder path always ends with the native directory
   * separator.
   * @param path The folder path to normalize.
   * @return QString Path with trailing separator added if missing.
   */
  static auto normalizeFolderPath(const QString &path) -> QString;
  /**
   * @brief Verify that the required configuration is complete.
   * @return bool `true` if the password store's `.gpg-id` exists AND the
   * configured executable (pass or gpg, depending on settings) exists or is a
   * WSL wrapper; `false` otherwise.
   */
  static auto configIsValid() -> bool;
  /**
   * @brief Get the selected folder path, either relative to the configured pass
   * store or absolute.
   * @param index Model index selecting the file or folder.
   * @param forPass If true, return the path relative to the pass store;
   * otherwise return an absolute path.
   * @param model Filesystem model used to resolve the index.
   * @param storeModel StoreModel used to map view indexes to the filesystem
   * model.
   * @return QString Folder path that always ends with the native directory
   * separator. Returns an empty string when `index` is invalid and `forPass` is
   * true; otherwise returns the pass store root.
   */
  static auto getDir(const QModelIndex &index, bool forPass,
                     const QFileSystemModel &model,
                     const StoreModel &storeModel) -> QString;
  /**
   * @brief Returns a regex to match .gpg file extensions.
   * @return Reference to static regex
   */
  static auto endsWithGpg() -> const QRegularExpression &;
  /**
   * @brief Returns a regex to match URL protocols.
   * @return Reference to static regex
   */
  static auto protocolRegex() -> const QRegularExpression &;
  /**
   * @brief Returns a regex to match newline characters.
   * @return Reference to static regex
   */
  static auto newLinesRegex() -> const QRegularExpression &;
  /**
   * @brief Check if a string looks like a valid GPG key ID.
   * Accepts:
   * - Key IDs: 8-40 hex characters (0-9, A-F, a-f), optional 0x prefix
   * - Emails: any string containing @ (e.g., user@domain.org,
   * <user@domain.org>)
   * - Special prefixes: leading @, /, #, or & (any content after prefix)
   * @param keyId The string to validate.
   * @return true if the key ID format is valid, false otherwise.
   */
  static auto isValidKeyId(const QString &keyId) -> bool;
  /**
   * @brief Read templates from .templates file in password store.
   * @param storePath Path to password store root.
   * @return Hash of template name to field list.
   */
  static auto readTemplates(const QString &storePath)
      -> QHash<QString, QStringList>;
  /**
   * @brief Write templates to .templates file in password store.
   * @param storePath Path to password store root.
   * @param templates Hash of template name to field list.
   * @return true if write succeeded.
   */
  static auto writeTemplates(const QString &storePath,
                             const QHash<QString, QStringList> &templates)
      -> bool;
  /**
   * @brief Get default template for a folder.
   * Looks in folder, then parent folders up to root.
   * @param folderPath Path to folder.
   * @param storePath Path to password store root.
   * @return Template name or empty if none found.
   */
  static auto getFolderTemplate(const QString &folderPath,
                                const QString &storePath) -> QString;

  /**
   * @brief Ensure SSH_AUTH_SOCK is set for child processes.
   *
   * GUI-launched applications don't inherit shell-set environment variables
   * (`.bashrc`/`.zshrc`/etc.), so users with gpg-agent's SSH support or other
   * external SSH agents see `git push`/`git pull` fail when QtPass launches
   * from a desktop launcher rather than a terminal. Issue #543.
   *
   * Resolution order:
   * 1. If `SSH_AUTH_SOCK` is already set (terminal launch, .desktop override,
   *    parent process), do nothing.
   * 2. If a `sshAuthSockOverride` setting is configured in QtPass, use it
   *    verbatim — the override is an explicit user choice, no validation.
   * 3. Probe `gpgconf --list-dirs agent-ssh-socket` (canonical for gpg-agent),
   *    then validate the candidate with `ssh-add -l` (must exit 0 or 1) before
   *    adopting. Validation prevents silently switching users from a working
   *    external SSH agent to an empty gpg-agent SSH socket.
   * 4. On macOS, fall back to `launchctl getenv SSH_AUTH_SOCK`, with the same
   *    `ssh-add -l` validation.
   *
   * Sets the variable via qputenv so child processes inherit it.
   */
  static void initialiseSshAuthSock();

private:
  static void initialiseEnvironment();
  static QProcessEnvironment _env;
  static bool _envInitialised;
};

#endif // SRC_UTIL_H_
