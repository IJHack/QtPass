// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_USERINFO_H_
#define SRC_USERINFO_H_

#include <QDateTime>
#include <QString>

/*!
    \struct UserInfo
    \brief Stores key info lines including validity, creation date and more.
 */
struct UserInfo {
  UserInfo() : validity('-'), have_secret(false), enabled(false) {}

  /**
   * @brief UserInfo::fullyValid when validity is f or u.
   * http://git.gnupg.org/cgi-bin/gitweb.cgi?p=gnupg.git;a=blob_plain;f=doc/DETAILS
   */
  auto fullyValid() const -> bool {
    return (validity == 'f') || (validity == 'u');
  }
  /**
   * @brief UserInfo::marginallyValid when validity is m.
   * http://git.gnupg.org/cgi-bin/gitweb.cgi?p=gnupg.git;a=blob_plain;f=doc/DETAILS
   */
  auto marginallyValid() const -> bool { return validity == 'm'; }
  /**
   * @brief UserInfo::isValid when fullyValid or marginallyValid.
   */
  auto isValid() const -> bool { return fullyValid() || marginallyValid(); }

  /**
   * @brief UserInfo::name GPG user ID / full name
   */
  QString name;
  /**
   * @brief UserInfo::key_id hexadecimal representation of the GnuPG key
   * identifier
   */
  QString key_id;
  /**
   * @brief UserInfo::validity GnuPG representation of validity
   * http://git.gnupg.org/cgi-bin/gitweb.cgi?p=gnupg.git;a=blob_plain;f=doc/DETAILS
   */
  char validity;
  /**
   * @brief UserInfo::have_secret whether secret key is available
   * (can decrypt with this key)
   */
  bool have_secret;
  /**
   * @brief UserInfo::enabled
   * Whether this user/key is enabled for normal use.
   * True when the key should be treated as active/usable; false when it is
   * disabled.
   */
  bool enabled;
  /**
   * @brief UserInfo::expiry date/time when key expires
   */
  QDateTime expiry;
  /**
   * @brief UserInfo::created date/time when key was created
   */
  QDateTime created;
};

#endif // SRC_USERINFO_H_
