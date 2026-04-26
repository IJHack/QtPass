// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file profileinit.cpp
 * @brief Profile initialization utilities implementation.
 */

#include "profileinit.h"

#include <QDir>

/**
 * @brief Check if a profile path is new (doesn't have .gpg-id yet).
 * @param path The profile path to check.
 * @return true if the path needs initialization.
 */
auto ProfileInit::needsInit(const QString &path) -> bool {
  QDir dir(path);
  if (!dir.exists()) {
    return false;
  }
  return !dir.exists(".gpg-id");
}
