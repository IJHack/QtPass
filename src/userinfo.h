// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_USERINFO_H_
#define SRC_USERINFO_H_

#include <QDateTime>
#include <QString>

struct UserInfo {
  UserInfo() : validity('-'), have_secret(false), enabled(false) {}

  /**
   * @brief Returns true when validity is f or u.
   * http://git.gnupg.org/cgi-bin/gitweb.cgi?p=gnupg.git;a=blob_plain;f=doc/DETAILS
   */
  auto fullyValid() const -> bool {
    return (validity == 'f') || (validity == 'u');
  }
  /**
   * @brief Returns true when validity is m.
   * http://git.gnupg.org/cgi-bin/gitweb.cgi?p=gnupg.git;a=blob_plain;f=doc/DETAILS
   */
  auto marginallyValid() const -> bool { return validity == 'm'; }
  /**
   * @brief Returns true when fullyValid or marginallyValid.
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
