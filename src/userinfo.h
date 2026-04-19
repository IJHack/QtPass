// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_USERINFO_H_
#define SRC_USERINFO_H_

#include <QDateTime>
#include <QString>

/**
 * @struct UserInfo
 * @brief Stores GPG key info including validity, creation date, and more.
 */
struct UserInfo {
  /**
   * @brief Construct a UserInfo with default (invalid/disabled) state.
   */
  UserInfo() : validity('-'), have_secret(false), enabled(false) {}

  /**
   * @brief Check full validity per GnuPG validity codes.
   * @return true when validity is 'f' or 'u'.
   * http://git.gnupg.org/cgi-bin/gitweb.cgi?p=gnupg.git;a=blob_plain;f=doc/DETAILS
   */
  auto fullyValid() const -> bool {
    return (validity == 'f') || (validity == 'u');
  }
  /**
   * @brief Check marginal validity per GnuPG validity codes.
   * @return true when validity is 'm'.
   * http://git.gnupg.org/cgi-bin/gitweb.cgi?p=gnupg.git;a=blob_plain;f=doc/DETAILS
   */
  auto marginallyValid() const -> bool { return validity == 'm'; }
  /**
   * @brief Check whether the key has any usable validity level.
   * @return true when fullyValid() or marginallyValid() is true.
   */
  auto isValid() const -> bool { return fullyValid() || marginallyValid(); }

  /**
   * @brief GPG user ID / full name.
   */
  QString name;
  /**
   * @brief Hexadecimal representation of the GnuPG key identifier.
   */
  QString key_id;
  /**
   * @brief GnuPG representation of validity.
   * http://git.gnupg.org/cgi-bin/gitweb.cgi?p=gnupg.git;a=blob_plain;f=doc/DETAILS
   */
  char validity;
  /**
   * @brief Whether secret key is available (can decrypt with this key).
   */
  bool have_secret;
  /**
   * @brief Whether this user/key is enabled for normal use.
   */
  bool enabled;
  /**
   * @brief Date/time when key expires.
   */
  QDateTime expiry;
  /**
   * @brief Date/time when key was created.
   */
  QDateTime created;
};

#endif // SRC_USERINFO_H_
