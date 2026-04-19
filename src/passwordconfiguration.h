// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_PASSWORDCONFIGURATION_H_
#define SRC_PASSWORDCONFIGURATION_H_

#include <QString>

/**
 * @struct PasswordConfiguration
 * @brief Holds the password configuration settings.
 */
struct PasswordConfiguration {
  /**
   * @brief Character set options for password generation.
   */
  enum characterSet {
    ALLCHARS = 0,
    ALPHABETICAL,
    ALPHANUMERIC,
    CUSTOM,
    CHARSETS_COUNT //   have to be last, for easier initialization of arrays
  };
  /**
   * @brief Currently active character set selection.
   */
  characterSet selected;
  /**
   * @brief Length of the password.
   */
  int length;
  /**
   * @brief The different character sets.
   */
  QString Characters[CHARSETS_COUNT];
  /**
   * @brief Construct a PasswordConfiguration with sensible defaults.
   */
  PasswordConfiguration() : selected(ALLCHARS), length(16) {
    Characters[ALLCHARS] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890~!@#$%^&"
        "*()_-+={}[]|:;<>,.?"; /*AllChars*/
    Characters[ALPHABETICAL] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstu"
                               "vwxyz"; /*Only Alphabetical*/
    Characters[ALPHANUMERIC] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstu"
                               "vwxyz1234567890"; /*Alphabetical and Numerical*/
    Characters[CUSTOM] = Characters[ALLCHARS];    // may be redefined by user
  }
};

#endif // SRC_PASSWORDCONFIGURATION_H_
