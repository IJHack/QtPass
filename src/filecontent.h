// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_FILECONTENT_H_
#define SRC_FILECONTENT_H_

#include <QList>
#include <QString>
#include <QStringList>

/**
 * @struct NamedValue
 * @brief A name/value pair parsed from a password file field.
 */
struct NamedValue {
  /**
   * @brief Field name (the part before the colon in "name: value").
   */
  QString name;
  /**
   * @brief Field value (the part after the colon in "name: value").
   */
  QString value;
  /**
   * @brief The field's verbatim source line, colon and spacing intact.
   *
   * The edit dialog writes an untouched field back from this, so saving
   * does not reformat `key:value` to `key: value`, trim the value, or drop
   * a field whose value is empty. Empty when the field has no source line
   * (a template field the file did not fill).
   */
  QString originalLine;
};

inline bool operator==(const NamedValue &a, const NamedValue &b) {
  return a.name == b.name && a.value == b.value;
}

/**
 * @brief The NamedValues class is mostly a list of NamedValue
 * but also has a method to take a specific NamedValue pair out of the list.
 */
class NamedValues : public QList<NamedValue> {
public:
  NamedValues();
  /**
   * @brief Construct a NamedValues list from an initializer list.
   * @param values Initial set of NamedValue pairs.
   */
  NamedValues(std::initializer_list<NamedValue> values);

  /**
   * @brief Remove and return the value for the named field.
   * @param name Field name to look up and remove.
   * @return The value of the removed field, or an empty string if not found.
   */
  auto takeValue(const QString &name) -> QString;

  /**
   * @brief Remove and return the named field, its original line included.
   * @param name Field name to look up and remove.
   * @return The removed NamedValue, or a default-constructed one (empty name)
   *         if not found.
   */
  auto takeNamedValue(const QString &name) -> NamedValue;
};

/**
 * @class FileContent
 * @brief Represents the parsed contents of a password file.
 *
 * Splits the file into a password line, named key/value fields, and
 * remaining data. Use FileContent::parse to construct an instance.
 */
class FileContent {
public:
  /**
   * @brief parse parses the given fileContent in a FileContent object.
   * The password is accessible through getPassword.
   * The named value pairs (name: value) are parsed and depending on the
   * templateFields and allFields parameters accessible through getNamedValues,
   * getRemainingData or getRemainingDataForDisplay.
   *
   * @param fileContent the file content to parse.
   *
   * @param templateFields the fields in the template.
   * Fields in the template will always be in getNamedValues() at the beginning
   * of the list in the same order.
   *
   * @param allFields whether all fields should be considered as named values.
   * If set to false only templateFields are returned in getNamedValues().
   *
   * @return A FileContent instance with password, named values, and remaining
   *         data populated from the parsed input.
   */
  static auto parse(const QString &fileContent,
                    const QStringList &templateFields, bool allFields)
      -> FileContent;

  /**
   * @return the password from the parsed file.
   */
  [[nodiscard]] auto getPassword() const -> QString;

  /**
   * @brief Like getPassword but empty when the password line holds a TOTP
   * shared secret (isOtpPasswordLine()).
   *
   * `pass otp insert` writes the URI as the entry's only line, so it lands in
   * the password position, and an `OTP:`/`TOTP:` field holding a seed written
   * there by hand is the same secret. It is not a password: it must never be
   * rendered or placed on the clipboard. getPassword() still returns it so the
   * edit dialog round-trips the file unchanged.
   * @return The password, or an empty string when it is an OTP line.
   */
  [[nodiscard]] auto getPasswordForDisplay() const -> QString;

  /**
   * @return the named values in the file in the order of appearance, with
   * template values first.
   */
  [[nodiscard]] auto getNamedValues() const -> NamedValues;

  /**
   * @return the data that is not the password and not in getNamedValues.
   */
  [[nodiscard]] auto getRemainingData() const -> QString;

  /**
   * @brief Like getRemainingData but without data that should not be displayed
   * (like a TOTP secret).
   * @return Remaining data string safe for display.
   */
  [[nodiscard]] auto getRemainingDataForDisplay() const -> QString;

  /**
   * @brief Whether a field name designates the one-time password field.
   * @param name Field name as written in the file; surrounding whitespace is
   *        ignored.
   * @return true for "OTP" or "TOTP", compared case-insensitively.
   */
  [[nodiscard]] static auto isOtpFieldName(const QString &name) -> bool;

  /**
   * @brief The password of an entry: its first line, without the carriage
   *        return of a CRLF line end.
   * @param text Decrypted entry text, or just its first line.
   * @return Its first line, a trailing "\r" dropped.
   */
  [[nodiscard]] static auto passwordLine(const QString &text) -> QString;

  /**
   * @brief Whether @p text holds an otpauth URI anywhere: after a list dash,
   *        in quotes, behind other text or a second colon, or split at its
   *        colon as `otpauth: //` (as the edit dialog saved it with otpauth
   *        in the template). What keeps a line off the screen and the
   *        clipboard goes by this; what is read as the OTP configuration
   *        goes by isOtpUriValue().
   * @param text A line or a field value.
   * @return true when "otpauth:" followed by "//" (blanks between allowed)
   *         occurs in it, compared case-insensitively.
   */
  [[nodiscard]] static auto carriesOtpUri(const QString &text) -> bool;

  /**
   * @brief Whether @p line carries a TOTP shared secret: an `OTP:`/`TOTP:`
   *        field, or any line with an otpauth URI in it (carriesOtpUri()).
   *
   * The rule for the lines below the password: such a line is never shown
   * or copied, only the code it yields is. The first line goes by
   * isOtpPasswordLine().
   * @param line One line of the entry.
   * @return true when the line holds a shared secret.
   */
  [[nodiscard]] static auto isOtpLine(const QString &line) -> bool;

  /**
   * @brief Whether an entry's first line holds a TOTP shared secret rather
   *        than a password.
   *
   * Stricter than isOtpLine(), because the first line is the password in the
   * pass format: a line with an otpauth URI in it (carriesOtpUri()), or an
   * `OTP:`/`TOTP:` field whose value is written as a base32 seed (base32
   * characters only, at least 16, and usable by Totp). A password that
   * merely starts with "otp:" or "totp:" (`totp:Secr3t!`) stays the
   * password; a long one of letters and the digits 2 to 7 only reads as a
   * seed and cannot be told apart.
   * @param line The entry's first line.
   * @return true when the line is an OTP secret and not the password.
   */
  [[nodiscard]] static auto isOtpPasswordLine(const QString &line) -> bool;

  /**
   * @brief Whether a field value is an otpauth URI, and therefore a secret.
   *
   * A URI is a shared secret whatever the field is called, so both the
   * display-suppression and the OTP lookup key off the value as well as the
   * name; suppressing only `OTP:`/`TOTP:` leaked `2fa:` and friends.
   * @param value Field value; surrounding whitespace is ignored.
   * @return true when it starts with "otpauth://", compared case-insensitively.
   */
  [[nodiscard]] static auto isOtpUriValue(const QString &value) -> bool;

  /**
   * @brief The entry's raw one-time password configuration, if it has one.
   *
   * Looked up in this order:
   *   1. the value of the first field whose name satisfies isOtpFieldName();
   *   2. the first bare `otpauth://` line in the body, which is how the
   *      `pass-otp` extension stores it;
   *   3. the value of the first `OTP:` line that was not promoted to a named
   *      value, which is what happens when the template does not list `OTP`.
   *
   * The string is returned verbatim and is not validated here: it may be an
   * `otpauth://` URI or a bare base32 secret. Totp::parse() does the
   * validation.
   * @return The OTP configuration string, or an empty string when the entry
   *         has none.
   */
  [[nodiscard]] auto getOtpUri() const -> QString;

private:
  FileContent(QString password, NamedValues namedValues, QString remainingData,
              QString remainingDataDisplay, QString otpUri);

  QString password;
  NamedValues namedValues;
  QString remainingData, remainingDataDisplay;
  QString otpUri;
};

#endif // SRC_FILECONTENT_H_
