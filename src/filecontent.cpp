// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "filecontent.h"

#include <utility>

/// Shorthand for the public FileContent::isOtpUriValue.
static auto isOtpUri(const QString &value) -> bool {
  return FileContent::isOtpUriValue(value);
}

/**
 * @brief Checks if a line should be hidden from display.
 *
 * Anything that carries a TOTP shared secret must never reach the display: a
 * bare `otpauth://` line, an `OTP:`/`TOTP:` field, or any field whose value is
 * an otpauth URI.
 * @param line The line to check
 * @return true if the line must not be displayed
 */
static auto isLineHidden(const QString &line) -> bool {
  if (isOtpUri(line)) {
    return true;
  }
  const qsizetype colon = line.indexOf(':');
  if (colon < 0) {
    return false;
  }
  return FileContent::isOtpFieldName(line.left(colon)) ||
         isOtpUri(line.right(line.length() - colon - 1));
}

/**
 * @brief Parses password file content into structured data.
 * @param fileContent Raw file content
 * @param templateFields Fields defined in the template
 * @param allFields Whether to include all name:value pairs
 * @return Parsed FileContent object
 */
namespace {

/**
 * @brief Where an OTP configuration was found, in the order getOtpUri()
 * prefers: a named field first, then a bare `otpauth://` line, then an
 * `OTP:` line the template did not promote to a field.
 */
struct OtpCandidates {
  QString field;
  QString bareLine;
  QString remainingField;

  /// The first of the three that was found.
  auto pick() const -> QString {
    if (!field.isEmpty()) {
      return field;
    }
    return bareLine.isEmpty() ? remainingField : bareLine;
  }
};

/**
 * @brief Take @p line as a `name: value` field when the template asks for
 * it, or "Template all fields" is on and the value is not a URL.
 * @param line One line of the entry.
 * @param templateFields The template's field names.
 * @param allFields Whether every `name: value` line becomes a field.
 * @param namedValues Receives the field, if it is one.
 * @param otp Receives the line's OTP configuration, if it carries one.
 * @return Whether the line became a field, so the caller stops with it.
 */
auto takeNamedValue(const QString &line, const QStringList &templateFields,
                    bool allFields, NamedValues &namedValues,
                    OtpCandidates &otp) -> bool {
  const qsizetype colon = line.indexOf(u':');
  if (colon < 0) {
    return false;
  }
  const QString name = line.left(colon);
  const QString value = line.right(line.length() - colon - 1);
  // A value starting with // means the colon is probably from a URL.
  if (!templateFields.contains(name) &&
      !(allFields && !value.startsWith(QStringLiteral("//")))) {
    // An `OTP:` line that was not promoted to a named value, because the
    // template does not list it and allFields is off.
    if (otp.remainingField.isEmpty() && FileContent::isOtpFieldName(name)) {
      otp.remainingField = value.trimmed();
    }
    return false;
  }
  namedValues.append({name.trimmed(), value.trimmed()});
  // Keyed on the value as well as the name: a field called anything at all
  // whose value is an otpauth URI is still a shared secret.
  if (otp.field.isEmpty() && (FileContent::isOtpFieldName(name) ||
                              FileContent::isOtpUriValue(value))) {
    otp.field = value.trimmed();
  }
  return true;
}

} // namespace

auto FileContent::parse(const QString &fileContent,
                        const QStringList &templateFields, bool allFields)
    -> FileContent {
  QStringList lines = fileContent.split("\n");
  QString password;
  if (!lines.isEmpty()) {
    password = lines.takeFirst();
  }
  QStringList remainingData;
  QStringList remainingDataDisplay;
  NamedValues namedValues;
  OtpCandidates otp;
  // The password line itself, for an entry written by `pass otp insert` whose
  // only line is the URI. It is taken off the list above, so the loop below
  // never sees it.
  if (isOtpUri(password)) {
    otp.bareLine = password.trimmed();
  }
  for (const QString &line : std::as_const(lines)) {
    if (takeNamedValue(line, templateFields, allFields, namedValues, otp)) {
      continue;
    }
    remainingData.append(line);
    if (isOtpUri(line) && otp.bareLine.isEmpty()) {
      // A bare otpauth:// line, the convention the pass-otp extension uses.
      otp.bareLine = line.trimmed();
    }
    if (!isLineHidden(line)) {
      remainingDataDisplay.append(line);
    }
  }
  // Note that the OTP value stays in namedValues and remainingData: the edit
  // dialog reads the field from there, and PasswordDialog::getPassword() drops
  // empty fields, so stripping it here would silently delete the user's secret
  // on the next save. Suppression happens in the render paths instead, via
  // getRemainingDataForDisplay() and PasswordDisplayPanel.
  return {password, namedValues, remainingData.join("\n"),
          remainingDataDisplay.join("\n"), otp.pick()};
}

/**
 * @brief Checks whether a field name designates the one-time password field.
 * @param name Field name as written in the file
 * @return true for "OTP" or "TOTP", case-insensitively
 */
auto FileContent::isOtpFieldName(const QString &name) -> bool {
  const QString trimmed = name.trimmed();
  return trimmed.compare(QStringLiteral("OTP"), Qt::CaseInsensitive) == 0 ||
         trimmed.compare(QStringLiteral("TOTP"), Qt::CaseInsensitive) == 0;
}

/**
 * @brief Checks whether a field value is an otpauth URI.
 * @param value The value to check
 * @return true if it starts with otpauth:// (case-insensitive)
 */
auto FileContent::isOtpUriValue(const QString &value) -> bool {
  return value.trimmed().startsWith(QStringLiteral("otpauth://"),
                                    Qt::CaseInsensitive);
}

/**
 * @brief Gets the password from the parsed file.
 * @return The password string
 */
auto FileContent::getPassword() const -> QString { return this->password; }

/**
 * @brief Gets the password unless it is an otpauth URI.
 * @return The password, empty when it is a shared secret rather than a password
 */
auto FileContent::getPasswordForDisplay() const -> QString {
  return isOtpUri(this->password) ? QString() : this->password;
}

/**
 * @brief Gets named value pairs from the parsed file.
 * @return NamedValues list with name:value pairs
 */
auto FileContent::getNamedValues() const -> NamedValues {
  return this->namedValues;
}

/**
 * @brief Gets remaining data not in named values.
 * @return Remaining data as string
 */
auto FileContent::getRemainingData() const -> QString {
  return this->remainingData;
}

/**
 * @brief Gets remaining data for display (excludes hidden fields like OTP).
 * @return Remaining data suitable for display
 */
auto FileContent::getRemainingDataForDisplay() const -> QString {
  return this->remainingDataDisplay;
}

/**
 * @brief Gets the raw OTP configuration found while parsing.
 * @return The otpauth URI or bare base32 secret, empty when there is none
 */
auto FileContent::getOtpUri() const -> QString { return this->otpUri; }

/**
 * @brief Constructs a FileContent with all parsed data.
 * @param password The password
 * @param namedValues Named value pairs
 * @param remainingData Remaining data not in named values
 * @param remainingDataDisplay Remaining data for display
 * @param otpUri Raw OTP configuration, or empty when there is none
 */
FileContent::FileContent(QString password, NamedValues namedValues,
                         QString remainingData, QString remainingDataDisplay,
                         QString otpUri)
    : password(std::move(password)), namedValues(std::move(namedValues)),
      remainingData(std::move(remainingData)),
      remainingDataDisplay(std::move(remainingDataDisplay)),
      otpUri(std::move(otpUri)) {}

/**
 * @brief Default constructor for NamedValues.
 */
NamedValues::NamedValues() = default;

/**
 * @brief Constructs NamedValues from initializer list.
 * @param values Initializer list of NamedValue
 */
NamedValues::NamedValues(std::initializer_list<NamedValue> values)
    : QList(values) {}

/**
 * @brief Finds and removes a named value by name.
 * @param name The name to search for
 * @return The value if found, empty string otherwise
 */
auto NamedValues::takeValue(const QString &name) -> QString {
  for (int i = 0; i < length(); ++i) {
    if (at(i).name == name) {
      return takeAt(i).value;
    }
  }
  return {};
}
