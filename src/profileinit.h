// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_PROFILEINIT_H_
#define SRC_PROFILEINIT_H_

#include <QString>

/**
 * @file profileinit.h
 * @brief Profile initialization utilities.
 *
 * Handles checking if new password store profiles need initialization.
 */

/**
 * @brief Utility class for profile initialization operations.
 */
class ProfileInit {
public:
  /**
   * @brief Check if a profile path needs initialization.
   * @param path The profile path to check.
   * @return true if the path exists but has no .gpg-id file.
   */
  static auto needsInit(const QString &path) -> bool;

private:
  ProfileInit() = default;
};

#endif // SRC_PROFILEINIT_H_