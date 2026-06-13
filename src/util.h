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
   * @brief Check that a candidate path resolves inside the password store
   * root, after following symlinks and resolving `..` components.
   *
   * For candidates that already exist on disk the path is canonicalised via
   * `QFileInfo::canonicalFilePath()` (which resolves symlinks). For
   * candidates that do not yet exist (typical "create new file" flow) we
   * walk up to the nearest existing ancestor, canonicalise that, then
   * re-append the leaf components. This catches both user-typed `..`
   * escapes and symlinks pointing outside the store.
   *
   * @param storeRoot Password store root (typically
   * `QtPassSettings::getPassStore()`).
   * @param candidate Path to validate. May be absolute or relative; relative
   * paths are interpreted against the current working directory before
   * resolution.
   * @return true if `candidate` resolves to a path equal to or strictly
   * inside `storeRoot`; false otherwise (including when `storeRoot` itself
   * does not exist).
   */
  static auto isPathInStore(const QString &storeRoot, const QString &candidate)
      -> bool;
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
   * @brief Check whether a value is a safe, launchable web URL.
   *
   * Stricter than protocolRegex(): intended to gate an "open in browser"
   * action, where launching a non-web scheme would be a security risk.
   * Returns true only when the trimmed value:
   * - contains no control characters (CR, LF, NUL),
   * - parses to a valid QUrl,
   * - has scheme exactly "http" or "https" (case-insensitive),
   * - has a non-empty host,
   * - carries no embedded userinfo (user:password\@host).
   *
   * Deliberately rejects file://, javascript:, data:, ftp/ssh/webdav and
   * scheme-less inputs (e.g. "www.example.com").
   *
   * @param value Candidate URL string (typically a password-file field).
   * @return true if the value is a launchable http(s) URL, false otherwise.
   */
  static auto isLaunchableWebUrl(const QString &value) -> bool;
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

private:
  static void initialiseEnvironment();
  static QProcessEnvironment _env;
  static bool _envInitialised;
};

#endif // SRC_UTIL_H_
