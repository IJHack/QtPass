// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_PATHVALIDATOR_H_
#define SRC_PATHVALIDATOR_H_

#include <QString>

/**
 * @class PathValidator
 * @brief Password-store path-boundary checks.
 *
 * Extracted from Util. Guards file/folder create, rename, and drag-drop
 * against paths that resolve outside the configured password store.
 */
class PathValidator {
public:
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

private:
  PathValidator() = default;
};

#endif // SRC_PATHVALIDATOR_H_
