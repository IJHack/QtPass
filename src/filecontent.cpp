// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Implements FileContent::parse() as a single streaming pass over the
 * entry, with the field cap, the display-line cap and the OTP candidate
 * search, plus the cached secret-field-name matcher.
 */
#include "filecontent.h"
#include "totp.h"
#include <QRegularExpression>

#include <QCoreApplication>
#include <QStringTokenizer>

#include <algorithm>
#include <limits>

#include <utility>

/**
 * @brief Shorthand for the public FileContent::isOtpUriValue.
 * @param value A line or field value.
 * @return true when it is an `otpauth://` URI.
 */
static auto isOtpUri(const QString &value) -> bool {
  return FileContent::isOtpUriValue(value);
}

/**
 * @brief Whether a body line must stay out of the parsed display because it
 * carries a TOTP shared secret (FileContent::isOtpLine()). "Display the
 * file's content as-is" bypasses this filter and shows the entry raw.
 * @param line One line of the entry body.
 * @return true when the line is kept out of getRemainingDataForDisplay().
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
  const qsizetype colon = line.indexOf(u':');
  return colon < 0 ? QString() : line.mid(colon + 1);
}

/**
 * @brief Whether a first line is written as an OTP configuration that
 * otpConfigOf() can read: a bare otpauth URI, a field whose value is one, or
 * an `OTP:`/`TOTP:` field whose value is written as a base32 seed.
 * @param line The entry's first line.
 * @return true when the line is read as the OTP configuration.
 */
static auto holdsOtpConfig(const QString &line) -> bool {
  if (isOtpUri(line)) {
    return true;
  }
  const qsizetype colon = line.indexOf(u':');
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
 * @brief The OTP configuration an OTP line holds: the line itself for a bare
 * `otpauth://` line, the value for a field.
 * @param line A line for which holdsOtpConfig() is true.
 * @return The configuration, trimmed.
 */
static auto otpConfigOf(const QString &line) -> QString {
  if (isOtpUri(line)) {
    return line.trimmed();
  }
  return line.mid(line.indexOf(u':') + 1).trimmed();
}

namespace {

/**
 * @brief The OTP configurations found of one kind: the first, and the first
 * that gives a code.
 */
struct OtpCandidate {
  QString first;  ///< The first configuration offered.
  QString usable; ///< The first one Totp::isValid() accepts.

  /**
   * @brief Note @p config, until one that gives a code was noted.
   * @param config A configuration of this kind, trimmed.
   */
  void offer(const QString &config) {
    if (!usable.isEmpty()) {
      return;
    }
    if (first.isEmpty()) {
      first = config;
    }
    if (Totp::isValid(config)) {
      usable = config;
    }
  }
};

/**
 * @brief Where an OTP configuration was found, in the order getOtpUri()
 * prefers: a named field first, then a bare `otpauth://` line, then an
 * `OTP:` line the template did not promote to a field.
 */
struct OtpCandidates {
  OtpCandidate field;          ///< OTP-named or URI-valued fields.
  OtpCandidate bareLine;       ///< Bare `otpauth://` lines (password line too).
  OtpCandidate remainingField; ///< `OTP:` lines left as text.

  /**
   * @brief The first configuration that gives a code, in the order above,
   * so an unusable one (an `otpauth-migration://` export, HOTP) does not
   * hide a later one that works. With none usable, the first found, for
   * the "Invalid OTP secret" note.
   * @return The configuration string, or empty when none was found.
   */
  auto pick() const -> QString {
    for (const OtpCandidate *kind : {&field, &bareLine, &remainingField}) {
      if (!kind->usable.isEmpty()) {
        return kind->usable;
      }
    }
    for (const OtpCandidate *kind : {&field, &bareLine, &remainingField}) {
      if (!kind->first.isEmpty()) {
        return kind->first;
      }
    }
    return {};
  }
};

/**
 * @brief Takes @p line as a field when the template lists it, "Template all
 * fields" is on and the value is not a URL, or @p alsoField asks for its
 * name; records any OTP configuration in @p otp either way.
 * @param line One line of the entry body.
 * @param templateFields Field names the template promotes.
 * @param allFields The "Template all fields" setting.
 * @param alsoField Extra predicate on the trimmed name; may be empty.
 * @param namedValues Receives the field when it is taken.
 * @param otp Is offered the OTP candidates of each kind.
 * @return true when the line became a field and is not remaining text.
 */
auto takeNamedValue(const QString &line, const QStringList &templateFields,
                    bool allFields,
                    const std::function<bool(const QString &)> &alsoField,
                    NamedValues &namedValues, OtpCandidates &otp) -> bool {
  const qsizetype colon = line.indexOf(u':');
  if (colon < 0) {
    return false;
  }
  const QString name = line.left(colon);
  const QString value = line.right(line.length() - colon - 1);
  // A value starting with // means the colon is probably from a URL.
  if (!templateFields.contains(name) &&
      !(allFields && !value.startsWith(QStringLiteral("//"))) &&
      !(alsoField && alsoField(name.trimmed()))) {
    // An `OTP:` line the template did not promote to a field.
    if (FileContent::isOtpFieldName(name)) {
      otp.remainingField.offer(value.trimmed());
    }
    return false;
  }
  namedValues.append({name.trimmed(), value.trimmed(), line});
  // Keyed on the value as well as the name: a field called anything at all
  // whose value is an otpauth URI is still a shared secret.
  if (FileContent::isOtpFieldName(name) || FileContent::isOtpUriValue(value)) {
    otp.field.offer(value.trimmed());
  }
  return true;
}

/**
 * @brief The line ending a text cut at FileContent::kMaxDisplayLines.
 * @param count How many lines were left out.
 * @return The translated "… %n more line(s) not shown" line.
 */
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
                        const QStringList &templateFields, bool allFields,
                        const std::function<bool(const QString &)> &alsoField)
    -> FileContent {
  // Line by line over the text, never a list of every line: a compressed
  // entry can expand into tens of millions of lines within the plaintext
  // limit, and a QString each (empty ones too) would take gigabytes.
  QStringTokenizer tokens = QStringView(fileContent).tokenize(u'\n');
  auto it = tokens.begin();
  QString password;
  if (it != tokens.end()) {
    password = passwordLine(*it);
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
  // which the loop below never sees; an `OTP:`/`TOTP:` field written there by
  // hand holding a usable secret is the same, and yields a code as it does
  // further down. Read only from what is written as a configuration
  // (holdsOtpConfig()); a first line with a URI elsewhere in it is kept off
  // the screen all the same (isOtpPasswordLine()).
  if (holdsOtpConfig(password)) {
    otp.bareLine.offer(otpConfigOf(password));
  }
  for (; it != tokens.end(); ++it) {
    const QString line = it->toString();
    bool secretPastTheCap = false;
    // A line with an otpauth URI in it is never a field unless the URI is
    // its value ("2fa: otpauth://..."), also not when the template lists
    // "otpauth": split at a colon, the value would lose the scheme that
    // marks it as a shared secret and be shown seed and all ("otpauth://...",
    // "AWS: backup phone: otpauth://..."). Past kMaxNamedFields, lines stay
    // text.
    if (!carriesOtpUri(line) || isOtpUri(afterFirstColon(line))) {
      if (namedValues.size() < kMaxNamedFields) {
        if (takeNamedValue(line, templateFields, allFields, alsoField,
                           namedValues, otp)) {
          continue;
        }
      } else {
        takeNamedValue(line, templateFields, allFields, alsoField, pastTheCap,
                       otp);
        // A secret past the cap stays text, but not on screen: the panel
        // would have hidden it.
        secretPastTheCap = !pastTheCap.isEmpty() && alsoField &&
                           alsoField(pastTheCap.first().name);
        pastTheCap.clear();
      }
    }
    // Joined with "\n" as they go, as split() and join() did.
    if (remainingLines > 0) {
      remainingData += QLatin1Char('\n');
    }
    remainingData += line;
    ++remainingLines;
    if (isOtpUri(line)) {
      // A bare otpauth:// line, the convention the pass-otp extension uses.
      otp.bareLine.offer(line.trimmed());
    }
    if (!isLineHidden(line) && !secretPastTheCap) {
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

auto FileContent::isSecretFieldName(const QString &name,
                                    const QStringList &secrets) -> bool {
  // One pattern for the whole list, compiled once per list: the panel asks
  // for every field of every entry shown, and the list rarely changes.
  thread_local QStringList cachedSecrets;
  thread_local QStringList cachedWords;
  thread_local QRegularExpression cachedPattern;
  thread_local bool cachedYet = false;
  constexpr auto options = QRegularExpression::CaseInsensitiveOption |
                           QRegularExpression::UseUnicodePropertiesOption;
  if (!cachedYet || secrets != cachedSecrets) {
    cachedWords.clear();
    for (const QString &secret : secrets) {
      const QString word = secret.trimmed();
      if (word.isEmpty()) {
        continue;
      }
      // A word counts when no letter touches it: digits and `_` end a word
      // as a blank does ("PIN2", "api_key"), and a plural "s" may follow
      // ("tokens", "Security answers"); "key" is still not in "keyboard".
      // A word that starts or ends with a digit is not part of a longer
      // number either ("tan1" not in "tan12").
      const QString letters = QStringLiteral("\\p{L}\\p{M}");
      const QString before =
          word.front().isDigit() ? letters + QStringLiteral("\\d") : letters;
      const QString after =
          word.back().isDigit() ? letters + QStringLiteral("\\d") : letters;
      cachedWords << QStringLiteral("(?<![%1])%2s?(?![%3])")
                         .arg(before, QRegularExpression::escape(word), after);
    }
    cachedSecrets = secrets;
    cachedYet = true;
    cachedPattern = QRegularExpression(
        QStringLiteral("(?:%1)").arg(cachedWords.join(QLatin1Char('|'))),
        options);
  }
  if (cachedWords.isEmpty()) {
    return false;
  }
  // camelCase names its words with capitals, which a case-insensitive match
  // does not see, so the name is also tried with a blank at each word start:
  // "apiKey" as "api Key", "APIKey" as "API Key". The acronym rule cuts a
  // capitals word that a lower-case letter follows ("cardPINs" as "card PI
  // Ns"), so a split at lower-to-upper starts alone is tried too ("card
  // PINs", "sim PINcode"), and the name as written ("PINcode").
  static const QRegularExpression lowerThenUpper(
      QStringLiteral("(?<=[\\p{Ll}\\d])(?=\\p{Lu})"),
      QRegularExpression::UseUnicodePropertiesOption);
  static const QRegularExpression acronymThenWord(
      QStringLiteral("(?<=\\p{Lu})(?=\\p{Lu}\\p{Ll})"),
      QRegularExpression::UseUnicodePropertiesOption);
  QString camel = name;
  camel.replace(lowerThenUpper, QStringLiteral(" "));
  QString words = camel;
  words.replace(acronymThenWord, QStringLiteral(" "));
  const auto matches = [&](const QRegularExpression &pattern) {
    return pattern.match(name).hasMatch() ||
           (camel != name && pattern.match(camel).hasMatch()) ||
           (words != camel && pattern.match(words).hasMatch());
  };
  if (cachedPattern.isValid()) {
    return matches(cachedPattern);
  }
  // Too many words for one pattern (PCRE2's size limit): word by word,
  // rather than every secret field shown in plain view.
  return std::any_of(cachedWords.cbegin(), cachedWords.cend(),
                     [&matches, options](const QString &word) {
                       return matches(QRegularExpression(word, options));
                     });
}

auto FileContent::isOtpUriValue(const QString &value) -> bool {
  return value.trimmed().startsWith(QStringLiteral("otpauth://"),
                                    Qt::CaseInsensitive);
}

auto FileContent::carriesOtpUri(const QString &text) -> bool {
  // Not only at the start: "- otpauth://", "\"otpauth://...\"", "old token
  // otpauth://", "AWS: backup phone: otpauth://" and the split "otpauth:
  // //totp/" all showed the seed, being neither a bare line nor the value
  // after the first colon. By hand, not a regular expression: parse() asks
  // this for every line of an entry, up to 64 MiB of them.
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

auto FileContent::passwordLine(QStringView text) -> QString {
  // left(-1) is the whole text when there is no line feed.
  QStringView line = text.left(text.indexOf(u'\n'));
  if (line.endsWith(u'\r')) {
    line.chop(1);
  }
  return line.toString();
}

auto FileContent::isOtpLine(const QString &line) -> bool {
  if (carriesOtpUri(line)) {
    return true;
  }
  const qsizetype colon = line.indexOf(u':');
  return colon >= 0 && isOtpFieldName(line.left(colon));
}

auto FileContent::isOtpPasswordLine(const QString &line) -> bool {
  return carriesOtpUri(line) || holdsOtpConfig(line);
}

auto FileContent::getPassword() const -> QString { return this->password; }

auto FileContent::getPasswordForDisplay() const -> QString {
  return isOtpPasswordLine(this->password) ? QString() : this->password;
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
