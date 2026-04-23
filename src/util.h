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

private:
  static void initialiseEnvironment();
  static QProcessEnvironment _env;
  static bool _envInitialised;
};

#endif // SRC_UTIL_H_
