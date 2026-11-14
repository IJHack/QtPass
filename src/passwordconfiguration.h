// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Value object for the password generator: chosen character set, its
 * alphabets and the length.
 */
#ifndef SRC_PASSWORDCONFIGURATION_H_
#define SRC_PASSWORDCONFIGURATION_H_

#include <QString>

/**
 * @struct PasswordConfiguration
 * @brief Holds the password generator settings: which character set is
 * selected, the alphabet of every set, and the password length.
 *
 * Loaded from and saved to QSettings by QtPassSettings; PasswordDialog reads
 * it when the user presses "Generate" and ConfigDialog edits it.
 */
struct PasswordConfiguration {
  /**
   * @brief Character set options for password generation; also the index
   * into @ref Characters.
   */
  enum CharacterSet {
    ALLCHARS = 0,  /**< Letters, digits and ASCII punctuation. */
    ALPHABETICAL,  /**< Upper- and lower-case ASCII letters only. */
    ALPHANUMERIC,  /**< ASCII letters and digits. */
    CUSTOM,        /**< User-defined alphabet, edited in the settings dialog. */
    CHARSETS_COUNT /**< Sentinel, not a character set: the number of sets,
                      used to size @ref Characters. Must stay last. */
  };
  /**
   * @brief Currently active character set selection.
   */
  CharacterSet selected{ALLCHARS};
  /**
   * @brief Length of the generated password, in characters.
   */
  int length{16};
  /**
   * @brief The alphabet of each character set, indexed by CharacterSet. Only
   * the CUSTOM entry is meant to be changed after construction.
   */
  QString Characters[CHARSETS_COUNT];
  /**
   * @brief Construct a PasswordConfiguration with sensible defaults.
   */
  PasswordConfiguration() {
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
