// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
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
  static auto findBinaryInPath(QString binary) -> QString;
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
  static auto normalizeFolderPath(QString path) -> QString;
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

private:
  static void initialiseEnvironment();
  static QProcessEnvironment _env;
  static bool _envInitialised;
};

#endif // SRC_UTIL_H_
