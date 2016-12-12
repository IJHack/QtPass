#ifndef DATAHELPERS_H
#define DATAHELPERS_H

#include <QDateTime>
#include <QString>

/*!
    \struct passwordConfiguration
    \brief  holds the Password configuration settings
 */
struct passwordConfiguration {
  /**
   * @brief passwordConfiguration::selected character set.
   */
  enum characterSet {
    ALLCHARS = 0,
    ALPHABETICAL,
    ALPHANUMERIC,
    CUSTOM,
    CHARSETS_COUNT //  have to be last, for easier initialization of arrays
  } selected;
  /**
   * @brief passwordConfiguration::length of password.
   */
  int length;
  /**
   * @brief passwordConfiguration::Characters the different character sets.
   */
  QString Characters[CHARSETS_COUNT];
  passwordConfiguration() : selected(ALLCHARS), length(16) {
    Characters[ALLCHARS] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890~!@#$%^&"
        "*()_-+={}[]|:;<>,.?"; /*AllChars*/
    Characters[ALPHABETICAL] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstu"
                               "vwxyz"; /*Only Alphabetical*/
    Characters[ALPHANUMERIC] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstu"
                               "vwxyz1234567890"; /*Alphabetical and Numerical*/
    Characters[CUSTOM] = Characters[ALLCHARS]; //  this may be redefined by user
  }
};

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
  bool fullyValid() { return validity == 'f' || validity == 'u'; }
  /**
   * @brief UserInfo::marginallyValid when validity is m.
   * http://git.gnupg.org/cgi-bin/gitweb.cgi?p=gnupg.git;a=blob_plain;f=doc/DETAILS
   */
  bool marginallyValid() { return validity == 'm'; }
  /**
   * @brief UserInfo::isValid when fullyValid or marginallyValid.
   */
  bool isValid() { return fullyValid() || marginallyValid(); }

  /**
   * @brief UserInfo::name full name
   */
  QString name;
  /**
   * @brief UserInfo::key_id hexadecimal representation
   */
  QString key_id;
  /**
   * @brief UserInfo::validity GnuPG representation of validity
   * http://git.gnupg.org/cgi-bin/gitweb.cgi?p=gnupg.git;a=blob_plain;f=doc/DETAILS
   */
  char validity;
  /**
   * @brief UserInfo::have_secret secret key is available
   * (can decrypt with this key)
   */
  bool have_secret;
  /**
   * @brief UserInfo::enabled
   */
  bool enabled;
  /**
   * @brief UserInfo::expiry date/time key expires
   */
  QDateTime expiry;
  /**
   * @brief UserInfo::created date/time key was created
   */
  QDateTime created;
};

#endif // DATAHELPERS_H
