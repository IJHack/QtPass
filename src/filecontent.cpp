// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "filecontent.h"

#include <utility>

/**
 * @brief Checks whether a value is an otpauth URI.
 * @param value The value to check
 * @return true if it starts with otpauth:// (case-insensitive)
 */
static auto isOtpUri(const QString &value) -> bool {
  return value.trimmed().startsWith("otpauth://", Qt::CaseInsensitive);
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
  // The OTP configuration can arrive from three places; remember the first hit
  // for each so getOtpUri() can apply a fixed precedence afterwards.
  QString otpFromField;
  QString otpFromBareLine;
  QString otpFromRemainingField;
  for (const QString &line : std::as_const(lines)) {
    if (line.contains(":")) {
      qsizetype colon = line.indexOf(':');
      QString name = line.left(colon);
      QString value = line.right(line.length() - colon - 1);
      if ((allFields &&
           !value.startsWith(
               "//")) // if value startswith  // colon is probably from a url
          || templateFields.contains(name)) {
        namedValues.append({name.trimmed(), value.trimmed()});
        if (otpFromField.isEmpty() && FileContent::isOtpFieldName(name)) {
          otpFromField = value.trimmed();
        }
        continue;
      }
      // An `OTP:` line that was not promoted to a named value, because the
      // template does not list it and allFields is off.
      if (otpFromRemainingField.isEmpty() &&
          FileContent::isOtpFieldName(name)) {
        otpFromRemainingField = value.trimmed();
      }
    }

    remainingData.append(line);
    if (isOtpUri(line) && otpFromBareLine.isEmpty()) {
      // A bare otpauth:// line, the convention the pass-otp extension uses.
      otpFromBareLine = line.trimmed();
    }
    if (!isLineHidden(line)) {
      remainingDataDisplay.append(line);
    }
  }
  QString otpUri = otpFromField;
  if (otpUri.isEmpty()) {
    otpUri = otpFromBareLine;
  }
  if (otpUri.isEmpty()) {
    otpUri = otpFromRemainingField;
  }
  // Note that the OTP value stays in namedValues and remainingData: the edit
  // dialog reads the field from there, and PasswordDialog::getPassword() drops
  // empty fields, so stripping it here would silently delete the user's secret
  // on the next save. Suppression happens in the render paths instead, via
  // getRemainingDataForDisplay() and PasswordDisplayPanel.
  return {password, namedValues, remainingData.join("\n"),
          remainingDataDisplay.join("\n"), otpUri};
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
 * @brief Gets the password from the parsed file.
 * @return The password string
 */
auto FileContent::getPassword() const -> QString { return this->password; }

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
