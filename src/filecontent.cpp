// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "filecontent.h"
#include "totp.h"

#include <utility>

/// Shorthand for the public FileContent::isOtpUriValue.
static auto isOtpUri(const QString &value) -> bool {
  return FileContent::isOtpUriValue(value);
}

/**
 * @brief The otpauth URI a line is written as: a bare `otpauth://` line, or
 * the split `otpauth: //...` that 1.8 to 2.0-rc2 saved (a template field
 * "otpauth" with the rest of the URI as its value), its blanks dropped.
 * @param line One line of the entry.
 * @return The URI, trimmed, or an empty string when the line is not one.
 */
static auto bareOtpUriOf(const QString &line) -> QString {
  const QString trimmed = line.trimmed();
  if (isOtpUri(trimmed)) {
    return trimmed;
  }
  static const QString scheme = QStringLiteral("otpauth:");
  if (!trimmed.startsWith(scheme, Qt::CaseInsensitive)) {
    return {};
  }
  qsizetype i = scheme.size();
  while (i < trimmed.size() && trimmed.at(i).isSpace()) {
    ++i;
  }
  if (trimmed.mid(i, 2) != QLatin1String("//")) {
    return {};
  }
  return trimmed.left(scheme.size()) + trimmed.mid(i);
}

/**
 * @brief Checks if a line should be hidden from display: it carries a TOTP
 * shared secret (FileContent::isOtpLine()).
 * @param line The line to check
 * @return true if the line must not be displayed
 */
static auto isLineHidden(const QString &line) -> bool {
  return FileContent::isOtpLine(line);
}

/**
 * @brief Whether @p value is written as a TOTP seed: base32 letters and the
 * digits 2 to 7 only (either case; white space, dashes, dots or underscores
 * between groups and "=" padding allowed), at least 16 of them (80 bits, the
 * shortest seed in common use), that Totp::isValid() takes.
 *
 * Strict where Totp::parse() is lenient (it drops punctuation and reads 0, 1
 * and 8 as letters, which almost any password survives): this decides
 * whether a first line is a seed or the password.
 * @param value The value of an `OTP:`/`TOTP:` field.
 * @return true when it can only be read as a seed.
 */
static auto isBase32Seed(const QString &value) -> bool {
  qsizetype letters = 0;
  for (const QChar c : value) {
    const char16_t u = c.unicode();
    // Grouping and padding, as seeds are written and pasted: spaces, tabs, a
    // no-break space, dashes, dots, underscores, "=". Base32::sanitizeInput()
    // drops them too, and Totp::parse() accepts them.
    if (c.isSpace() || u == u'-' || u == u'.' || u == u'_' || u == u'=') {
      continue;
    }
    if (!((u >= u'A' && u <= u'Z') || (u >= u'a' && u <= u'z') ||
          (u >= u'2' && u <= u'7'))) {
      return false;
    }
    ++letters;
  }
  constexpr qsizetype kMinSeedLetters = 16;
  return letters >= kMinSeedLetters && Totp::isValid(value);
}

/**
 * @brief What follows a line's first colon; empty without one.
 * @param line One line of the entry.
 * @return The text after the first ':', or an empty string.
 */
static auto afterFirstColon(const QString &line) -> QString {
  const qsizetype colon = line.indexOf(QLatin1Char(':'));
  return colon < 0 ? QString() : line.mid(colon + 1);
}

/**
 * @brief Whether a first line is written as an OTP configuration that
 * otpConfigOf() can read: a bare otpauth URI (split or not, see
 * bareOtpUriOf()), a field whose value is one, or
 * an `OTP:`/`TOTP:` field whose value is written as a base32 seed.
 * @param line The entry's first line.
 * @return true when the line is read as the OTP configuration.
 */
static auto holdsOtpConfig(const QString &line) -> bool {
  if (!bareOtpUriOf(line).isEmpty()) {
    return true;
  }
  const qsizetype colon = line.indexOf(QLatin1Char(':'));
  if (colon < 0) {
    return false;
  }
  const QString value = line.mid(colon + 1);
  // Any field holding an otpauth URI is a shared secret; an OTP-named field
  // only when what it holds would give a code: the line is the password
  // otherwise, as the pass format has it.
  return isOtpUri(value) ||
         (FileContent::isOtpFieldName(line.left(colon)) && isBase32Seed(value));
}

/**
 * @brief The OTP configuration an OTP line holds: the URI for a bare
 * `otpauth://` line (bareOtpUriOf()), the value for a field.
 * @param line A line for which holdsOtpConfig() is true.
 * @return The configuration, trimmed.
 */
static auto otpConfigOf(const QString &line) -> QString {
  if (const QString uri = bareOtpUriOf(line); !uri.isEmpty()) {
    return uri;
  }
  return afterFirstColon(line).trimmed();
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
    password = passwordLine(lines.takeFirst());
  }
  QStringList remainingData;
  QStringList remainingDataDisplay;
  NamedValues namedValues;
  // The OTP configuration can arrive from several places; remember the first
  // hit for each so getOtpUri() can apply a fixed precedence afterwards.
  QString otpFromField;
  QString otpFromBareLine;
  QString otpFromRemainingField;
  // The password line itself, for an entry written by `pass otp insert` whose
  // only line is the URI. It is taken off the list above, so the loop below
  // never sees it. An `OTP:`/`TOTP:` field written there by hand holding a
  // usable secret is the same, and yields a code as it does further down.
  // Read only from what is written as a configuration (holdsOtpConfig()); a
  // first line with a URI elsewhere in it is kept off the screen all the
  // same (isOtpPasswordLine()).
  if (holdsOtpConfig(password)) {
    otpFromBareLine = otpConfigOf(password);
  }
  for (const QString &line : std::as_const(lines)) {
    // A line with an otpauth URI in it is never a field unless the URI is
    // its value ("2fa: otpauth://..."), also not when the template lists
    // "otpauth": split at a colon, the value would lose the scheme that
    // marks it as a shared secret and be shown, seed and all
    // ("otpauth://...", "AWS: backup phone: otpauth://...").
    if (line.contains(":") &&
        (!carriesOtpUri(line) || isOtpUri(afterFirstColon(line)))) {
      qsizetype colon = line.indexOf(':');
      QString name = line.left(colon);
      QString value = line.right(line.length() - colon - 1);
      if ((allFields &&
           !value.startsWith(
               "//")) // if value startswith  // colon is probably from a url
          || templateFields.contains(name)) {
        namedValues.append({name.trimmed(), value.trimmed(), line});
        // Keyed on the value as well as the name: a field called anything at
        // all whose value is an otpauth URI is still a shared secret.
        if (otpFromField.isEmpty() &&
            (FileContent::isOtpFieldName(name) || isOtpUri(value))) {
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
    if (otpFromBareLine.isEmpty()) {
      // A bare otpauth:// line, the convention the pass-otp extension uses,
      // or the split form older versions saved.
      otpFromBareLine = bareOtpUriOf(line);
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
 * The one rule for every path that takes the password from decrypted text:
 * parse() (so the password row and "Always copy") and the Ctrl+C copy. A
 * "\r" before the line feed is the line end, not part of the secret.
 */
auto FileContent::passwordLine(const QString &text) -> QString {
  // left(-1) is the whole text when there is no line feed.
  QString line = text.left(text.indexOf(QLatin1Char('\n')));
  if (line.endsWith(QLatin1Char('\r'))) {
    line.chop(1);
  }
  return line;
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
 * @brief Gets the password unless the line holds a TOTP shared secret.
 * @return The password, empty when it is a shared secret rather than a password
 */
auto FileContent::getPasswordForDisplay() const -> QString {
  return isOtpPasswordLine(this->password) ? QString() : this->password;
}

/**
 * Not only at the start: "- otpauth://", "\"otpauth://...\"", "old token
 * otpauth://", "AWS: backup phone: otpauth://" and the split "otpauth:
 * //totp/" all showed the seed, being neither a bare line nor the value after
 * the first colon. By hand, not a regular expression: parse() asks this for
 * every line of an entry.
 */
auto FileContent::carriesOtpUri(const QString &text) -> bool {
  static const QString scheme = QStringLiteral("otpauth:");
  for (qsizetype at = text.indexOf(scheme, 0, Qt::CaseInsensitive); at >= 0;
       at = text.indexOf(scheme, at + 1, Qt::CaseInsensitive)) {
    qsizetype i = at + scheme.size();
    while (i < text.size() && text.at(i).isSpace()) {
      ++i;
    }
    if (text.mid(i, 2) == QLatin1String("//")) {
      return true;
    }
  }
  return false;
}

/**
 * The rule for the lines below the password.
 */
auto FileContent::isOtpLine(const QString &line) -> bool {
  if (carriesOtpUri(line)) {
    return true;
  }
  const qsizetype colon = line.indexOf(QLatin1Char(':'));
  return colon >= 0 && isOtpFieldName(line.left(colon));
}

/**
 * The rule for the first line, stricter than isOtpLine().
 */
auto FileContent::isOtpPasswordLine(const QString &line) -> bool {
  return carriesOtpUri(line) || holdsOtpConfig(line);
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
  return takeNamedValue(name).value;
}

auto NamedValues::takeNamedValue(const QString &name) -> NamedValue {
  for (int i = 0; i < length(); ++i) {
    if (at(i).name == name) {
      return takeAt(i);
    }
  }
  return {};
}
