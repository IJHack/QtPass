// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_PROFILEINIT_H_
#define SRC_PROFILEINIT_H_

#include <QString>
#include <QStringList>

/**
 * @file profileinit.h
 * @brief Profile initialization utilities.
 *
 * Handles initialization of new password store profiles:
 * - Creates .gpg-id file (pass initialization)
 * - Initializes git repository if enabled
 */

/**
 * @brief Initialize a new profile's password store.
 *
 * Checks if the path is new, and if so, optionally initializes
 * pass (.gpg-id) and git repository.
 */
class ProfileInit {
public:
  /**
   * @brief Check if a profile path is new (doesn't have .gpg-id yet).
   * @param path The profile path to check.
   * @return true if the path needs initialization.
   */
  static auto needsInit(const QString &path) -> bool;

  /**
   * @brief Initialize a new profile's password store.
   *
   * Creates .gpg-id file with the given recipients, and optionally
   * initializes git repository.
   *
   * @param path The profile path to initialize.
   * @param recipients List of GPG key IDs to encrypt for.
   * @param useGit Whether to also initialize git.
   * @return true if initialization was successful.
   */
  static auto init(const QString &path, const QStringList &recipients,
                   bool useGit) -> bool;

private:
  ProfileInit() = default;
};

#endif // SRC_PROFILEINIT_H_