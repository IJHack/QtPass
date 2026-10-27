// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "filecontent.h"

#include <QCoreApplication>
#include <QStringTokenizer>

#include <algorithm>
#include <limits>

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

/// The line ending a text cut at FileContent::kMaxDisplayLines.
auto moreLinesNotShown(qsizetype count) -> QString {
  // The count in a variable of its own: lupdate up to 6.8 loses a
  // translate() call with template arguments among its arguments and would
  // mark the string vanished.
  const int n = static_cast<int>(
      std::min<qsizetype>(count, std::numeric_limits<int>::max()));
  return QCoreApplication::translate("FileContent",
                                     "… %n more line(s) not shown", nullptr, n);
}

} // namespace

auto FileContent::parse(const QString &fileContent,
                        const QStringList &templateFields, bool allFields)
    -> FileContent {
  // Line by line over the text, never a list of every line: a compressed
  // entry can expand into tens of millions of lines within the plaintext
  // limit, and a QString each (empty ones too) would take gigabytes.
  QStringTokenizer tokens = QStringView(fileContent).tokenize(u'\n');
  auto it = tokens.begin();
  QString password;
  if (it != tokens.end()) {
    password = it->toString();
    ++it;
  }
  QString remainingData;
  QString remainingDataDisplay;
  qsizetype remainingLines = 0;
  qsizetype displayedLines = 0;
  qsizetype notShown = 0;
  // Past kMaxNamedFields a line is still offered to takeNamedValue(), into
  // this throwaway list, for what it notes about OTP: the line stays text.
  NamedValues pastTheCap;
  NamedValues namedValues;
  OtpCandidates otp;
  // An entry from `pass otp insert` has the URI as its only (password) line,
  // which the loop below never sees.
  if (isOtpUri(password)) {
    otp.bareLine = password.trimmed();
  }
  for (; it != tokens.end(); ++it) {
    const QString line = it->toString();
    // A bare otpauth:// line is never a field, also not when the template
    // lists "otpauth": split at its colon, the value would lose the scheme
    // that marks it as a shared secret and be shown seed and all. Past
    // kMaxNamedFields, lines stay text.
    if (!isOtpUri(line)) {
      if (namedValues.size() < kMaxNamedFields) {
        if (takeNamedValue(line, templateFields, allFields, namedValues, otp)) {
          continue;
        }
      } else {
        takeNamedValue(line, templateFields, allFields, pastTheCap, otp);
        pastTheCap.clear();
      }
    }
    // Joined with "\n" as they go, as split() and join() did.
    if (remainingLines > 0) {
      remainingData += QLatin1Char('\n');
    }
    remainingData += line;
    ++remainingLines;
    if (isOtpUri(line) && otp.bareLine.isEmpty()) {
      // A bare otpauth:// line, the convention the pass-otp extension uses.
      otp.bareLine = line.trimmed();
    }
    if (!isLineHidden(line)) {
      if (displayedLines == kMaxDisplayLines) {
        ++notShown;
        continue;
      }
      if (displayedLines > 0) {
        remainingDataDisplay += QLatin1Char('\n');
      }
      remainingDataDisplay += line;
      ++displayedLines;
    }
  }
  if (notShown > 0) {
    remainingDataDisplay += QLatin1Char('\n') + moreLinesNotShown(notShown);
  }
  // The OTP value stays in namedValues/remainingData: the edit dialog reads it
  // from there and PasswordDialog::getPassword() drops empty fields, so
  // stripping it here would delete the secret on the next save. Suppression
  // happens in getRemainingDataForDisplay() and PasswordDisplayPanel.
  return {password,   namedValues,   remainingData, remainingDataDisplay,
          otp.pick(), remainingLines};
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
                         QString otpUri, qsizetype remainingLines)
    : password(std::move(password)), namedValues(std::move(namedValues)),
      remainingData(std::move(remainingData)),
      remainingDataDisplay(std::move(remainingDataDisplay)),
      otpUri(std::move(otpUri)), remainingLines(remainingLines) {}

auto FileContent::boundedForDisplay(const QString &text) -> QString {
  // Where the line after the last one shown starts, if there is one.
  qsizetype end = -1;
  for (int line = 0; line < kMaxDisplayLines; ++line) {
    end = text.indexOf(QLatin1Char('\n'), end + 1);
    if (end < 0) {
      return text;
    }
  }
  const qsizetype more =
      text.count(QLatin1Char('\n'), Qt::CaseSensitive) - (kMaxDisplayLines - 1);
  return text.left(end) + QLatin1Char('\n') + moreLinesNotShown(more);
}

auto FileContent::getRemainingLineCount() const -> qsizetype {
  return remainingLines;
}

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
