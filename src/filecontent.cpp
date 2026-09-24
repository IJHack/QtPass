// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "filecontent.h"

#include <utility>

/// Shorthand for the public FileContent::isOtpUriValue.
static auto isOtpUri(const QString &value) -> bool {
  return FileContent::isOtpUriValue(value);
}

// The parsed display leaves out anything carrying a TOTP shared secret: a
// bare `otpauth://` line, an `OTP:`/`TOTP:` field, or any field whose value
// is an otpauth URI. "Display the file's content as-is" bypasses this filter
// and shows the entry raw.
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

// Takes `line` as a field when the template lists it, or "Template all fields"
// is on and the value is not a URL; records any OTP configuration in `otp`.
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
    // An `OTP:` line the template did not promote to a field.
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
  // An entry from `pass otp insert` has the URI as its only (password) line,
  // which the loop below never sees.
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
  // The OTP value stays in namedValues/remainingData: the edit dialog reads it
  // from there and PasswordDialog::getPassword() drops empty fields, so
  // stripping it here would delete the secret on the next save. Suppression
  // happens in getRemainingDataForDisplay() and PasswordDisplayPanel.
  return {password, namedValues, remainingData.join("\n"),
          remainingDataDisplay.join("\n"), otp.pick()};
}

auto FileContent::isOtpFieldName(const QString &name) -> bool {
  const QString trimmed = name.trimmed();
  return trimmed.compare(QStringLiteral("OTP"), Qt::CaseInsensitive) == 0 ||
         trimmed.compare(QStringLiteral("TOTP"), Qt::CaseInsensitive) == 0;
}

auto FileContent::isOtpUriValue(const QString &value) -> bool {
  return value.trimmed().startsWith(QStringLiteral("otpauth://"),
                                    Qt::CaseInsensitive);
}

auto FileContent::getPassword() const -> QString { return this->password; }

auto FileContent::getPasswordForDisplay() const -> QString {
  return isOtpUri(this->password) ? QString() : this->password;
}

auto FileContent::getNamedValues() const -> NamedValues {
  return this->namedValues;
}

auto FileContent::getRemainingData() const -> QString {
  return this->remainingData;
}

auto FileContent::getRemainingDataForDisplay() const -> QString {
  return this->remainingDataDisplay;
}

auto FileContent::getOtpUri() const -> QString { return this->otpUri; }

FileContent::FileContent(QString password, NamedValues namedValues,
                         QString remainingData, QString remainingDataDisplay,
                         QString otpUri)
    : password(std::move(password)), namedValues(std::move(namedValues)),
      remainingData(std::move(remainingData)),
      remainingDataDisplay(std::move(remainingDataDisplay)),
      otpUri(std::move(otpUri)) {}

NamedValues::NamedValues() = default;

NamedValues::NamedValues(std::initializer_list<NamedValue> values)
    : QList(values) {}

auto NamedValues::takeValue(const QString &name) -> QString {
  for (int i = 0; i < length(); ++i) {
    if (at(i).name == name) {
      return takeAt(i).value;
    }
  }
  return {};
}
