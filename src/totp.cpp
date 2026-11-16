// SPDX-FileCopyrightText: 2025 KeePassXC Team <team@keepassxc.org>
// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file
 * @brief Implements Totp: otpauth URI parsing with its length cap and HOTP
 * and unknown-algorithm refusal, RFC 4226 dynamic truncation, Steam encoding
 * and URI round-tripping.
 *
 * @see totp.h
 */

#include "totp.h"
#include "base32.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QLatin1Char>
#include <QMessageAuthenticationCode>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QtEndian>
#include <QtGlobal>

#include <algorithm>

namespace {

/// Steam Guard's alphabet: no A, E, I, L, O, S, U, Z, 0 or 1.
constexpr char kSteamAlphabet[] = "23456789BCDFGHJKMNPQRTVWXY";
/// Number of symbols in kSteamAlphabet, excluding the terminating NUL.
constexpr quint64 kSteamAlphabetSize = sizeof(kSteamAlphabet) - 1;
/// Shortest HMAC output the dynamic truncation below can read from (SHA-1).
constexpr int kMinHmacSize = 20;

/**
 * @brief Map an `algorithm` URI value onto a hash function.
 *
 * Surrounding whitespace, case, an `HMAC` prefix and `-`/`_` separators are
 * ignored, so `SHA-256`,
 * `HmacSHA256` and `HMAC-SHA-256` all name SHA-256. Anything else is not a
 * hash function QtPass has: no result, rather than a SHA-1 code that looks
 * right and an algorithm that toUri() would then leave out.
 *
 * @param name Value of the `algorithm` parameter.
 * @return The matching algorithm, or std::nullopt for an unknown one.
 */
auto algorithmByName(const QString &name) -> std::optional<Totp::Algorithm> {
  QString compact = name.trimmed().toUpper();
  compact.remove(QLatin1Char('-'));
  compact.remove(QLatin1Char('_'));
  if (compact.startsWith(QStringLiteral("HMAC"))) {
    compact.remove(0, 4);
  }
  if (compact == QStringLiteral("SHA1")) {
    return Totp::Algorithm::Sha1;
  }
  if (compact == QStringLiteral("SHA256")) {
    return Totp::Algorithm::Sha256;
  }
  if (compact == QStringLiteral("SHA512")) {
    return Totp::Algorithm::Sha512;
  }
  return std::nullopt;
}

/**
 * @brief Canonical name for an algorithm, as written into an otpauth URI.
 * @param algorithm Hash function.
 * @return "SHA1", "SHA256" or "SHA512".
 */
auto algorithmName(Totp::Algorithm algorithm) -> QString {
  switch (algorithm) {
  case Totp::Algorithm::Sha512:
    return QStringLiteral("SHA512");
  case Totp::Algorithm::Sha256:
    return QStringLiteral("SHA256");
  case Totp::Algorithm::Sha1:
    break;
  }
  return QStringLiteral("SHA1");
}

/**
 * @brief Map an algorithm onto the QtCore hash enumerator.
 * @param algorithm Hash function.
 * @return The QCryptographicHash equivalent.
 */
auto cryptoHash(Totp::Algorithm algorithm) -> QCryptographicHash::Algorithm {
  switch (algorithm) {
  case Totp::Algorithm::Sha512:
    return QCryptographicHash::Sha512;
  case Totp::Algorithm::Sha256:
    return QCryptographicHash::Sha256;
  case Totp::Algorithm::Sha1:
    break;
  }
  return QCryptographicHash::Sha1;
}

/// Percent-encode a URI label or issuer, keeping ':' and '@' readable.
auto encodeLabel(const QString &value) -> QString {
  return QString::fromLatin1(QUrl::toPercentEncoding(value, ":@"));
}

/**
 * @brief Read a positive integer query parameter.
 * @param value Raw parameter text.
 * @param fallback Returned when @p value is absent, non-numeric or zero.
 * @return The parsed value, or @p fallback.
 */
auto parsePositiveUInt(const QString &value, uint fallback) -> uint {
  bool ok = false;
  const uint parsed = value.toUInt(&ok);
  return (ok && parsed > 0) ? parsed : fallback;
}

} // namespace

auto Totp::parse(const QString &config, Refusal *why)
    -> std::optional<Settings> {
  const auto refuse = [why](Refusal reason) -> std::optional<Settings> {
    if (why != nullptr) {
      *why = reason;
    }
    return std::nullopt;
  };
  if (why != nullptr) {
    *why = Refusal::None;
  }
  // Checked before anything else sees it (trimmed() copies it, QUrl and
  // QUrlQuery would make a crafted URI of tens of MiB tens of millions of
  // query items on the GUI thread); padding counts too.
  if (config.size() > MAX_CONFIG_LENGTH) {
    return refuse(Refusal::TooLong);
  }
  const QString trimmed = config.trimmed();
  if (trimmed.isEmpty()) {
    return refuse(Refusal::Empty);
  }

  Settings settings;
  QString rawSecret;

  const QUrl url(trimmed);
  if (url.isValid() && url.scheme().compare(QStringLiteral("otpauth"),
                                            Qt::CaseInsensitive) == 0) {
    // Counter-based HOTP cannot be generated from a clock; refuse rather than
    // return a wrong code that looks right.
    if (url.host().compare(QStringLiteral("totp"), Qt::CaseInsensitive) != 0) {
      return refuse(
          url.host().compare(QStringLiteral("hotp"), Qt::CaseInsensitive) == 0
              ? Refusal::Hotp
              : Refusal::UnreadableUri);
    }

    const QUrlQuery query(url);
    // FullyDecoded, because toUri() percent-encodes these again: the default
    // PrettyDecoded leaves escapes in place and the round trip double-encodes.
    const QList<QPair<QString, QString>> items =
        query.queryItems(QUrl::FullyDecoded);
    // Parameter names are matched without regard to case, as the list of
    // names kept below is: read exactly, `Digits=8` was neither read nor
    // kept, and toUri() wrote the defaults in its place. The first of a name
    // wins, as QUrlQuery::queryItemValue() has it.
    const auto param =
        [&items](QLatin1StringView name) -> std::optional<QString> {
      for (const QPair<QString, QString> &item : items) {
        if (item.first.compare(name, Qt::CaseInsensitive) == 0) {
          return item.second;
        }
      }
      return std::nullopt;
    };
    rawSecret = param(QLatin1StringView("secret")).value_or(QString());
    settings.issuer = param(QLatin1StringView("issuer")).value_or(QString());
    // Not part of the OTP configuration, but the user's text: kept as written,
    // from the input itself (QUrl splits at the first '#' as well, but its
    // view would re-encode '%' and '`' and decode escapes).
    if (url.hasFragment()) {
      settings.fragment = trimmed.mid(trimmed.indexOf(QLatin1Char('#')) + 1);
    }
    // The label is everything after the host, e.g. "/Example:alice".
    settings.label = url.path();
    if (settings.label.startsWith(QLatin1Char('/'))) {
      settings.label.remove(0, 1);
    }
    // toUInt() returns 0 with no error signal for anything non-numeric, and the
    // clamp below would turn that into digits=1 / step=1 — a one-character code
    // rotating every second, which normalize() would then persist. Keep the RFC
    // default unless the value really is a usable number.
    settings.digits = parsePositiveUInt(
        param(QLatin1StringView("digits")).value_or(QString()), DEFAULT_DIGITS);
    settings.step = parsePositiveUInt(
        param(QLatin1StringView("period")).value_or(QString()), DEFAULT_STEP);
    // An empty value is the default, as an absent one is. An unknown hash
    // function is refused, as HOTP is: its code cannot be made here.
    if (const std::optional<QString> algorithm =
            param(QLatin1StringView("algorithm"));
        algorithm.has_value() && !algorithm->trimmed().isEmpty()) {
      const std::optional<Algorithm> known = algorithmByName(*algorithm);
      if (!known.has_value()) {
        return refuse(Refusal::UnknownAlgorithm);
      }
      settings.algorithm = *known;
    }
    if (param(QLatin1StringView("encoder"))
            .value_or(QString())
            .compare(QStringLiteral("steam"), Qt::CaseInsensitive) == 0) {
      settings.encoder = Encoder::Steam;
    }
    // Anything we do not model is kept verbatim so toUri() can put it back;
    // dropping it silently mutated URIs carrying e.g. `image=`.
    static const QStringList known = {
        QStringLiteral("secret"),    QStringLiteral("issuer"),
        QStringLiteral("digits"),    QStringLiteral("period"),
        QStringLiteral("algorithm"), QStringLiteral("encoder")};
    for (const QPair<QString, QString> &item : items) {
      if (!known.contains(item.first, Qt::CaseInsensitive)) {
        settings.extraParams.append(item);
      }
    }
  } else if (trimmed.startsWith(QStringLiteral("otpauth:"),
                                Qt::CaseInsensitive)) {
    // Claims to be an otpauth URI but QUrl could not parse it. Falling through
    // to the bare-secret branch would hand the whole string to
    // Base32::sanitizeInput(), which strips the punctuation and leaves
    // decodable base32 — producing a confidently wrong code. Refuse instead.
    return refuse(Refusal::UnreadableUri);
  } else {
    // Not an otpauth URI: a bare base32 secret. Base32::sanitizeInput()
    // drops what is not base32, so a URI of another scheme (a Google
    // Authenticator export, otpauth-migration://...?data=..., steam://),
    // zbarimg output with its "QR-Code:" prefix, or a secret with a remark
    // in brackets would become a secret made of its leftover letters, with
    // a confident, wrong code. Only what a secret is written with passes:
    // letters, the digits sanitizeInput() reads (0, 1 and 8 as O, L and B),
    // white space, "=" padding, and dashes, dots or underscores between
    // groups (JBSW.Y3DP..., as some services show it), which it drops.
    const bool written =
        std::all_of(trimmed.cbegin(), trimmed.cend(), [](QChar c) {
          const char16_t u = c.unicode();
          return c.isSpace() || u == u'-' || u == u'=' || u == u'.' ||
                 u == u'_' || (u >= u'A' && u <= u'Z') ||
                 (u >= u'a' && u <= u'z') || (u >= u'0' && u <= u'8');
        });
    if (!written) {
      return refuse(Refusal::NotASecret);
    }
    rawSecret = trimmed;
  }

  const QByteArray sanitized = Base32::sanitizeInput(rawSecret.toLatin1());
  settings.key = Base32::decode(sanitized);
  if (settings.key.isEmpty()) {
    return refuse(Refusal::NoSecret);
  }
  settings.base32Key = QString::fromLatin1(sanitized);

  if (settings.encoder == Encoder::Steam) {
    settings.digits = STEAM_DIGITS;
  }
  // Clamp on every path: an out-of-range `digits` would otherwise overflow the
  // modulus, and a zero `step` would divide by zero.
  settings.digits = qBound(MIN_DIGITS, settings.digits, MAX_DIGITS);
  settings.step = qBound(1U, settings.step, MAX_STEP);

  return settings;
}

auto Totp::describe(Refusal why) -> QString {
  switch (why) {
  case Refusal::TooLong:
    return QCoreApplication::translate("Totp",
                                       "The OTP configuration is too long.");
  case Refusal::Hotp:
    return QCoreApplication::translate(
        "Totp", "This is a counter-based (HOTP) configuration. QtPass makes "
                "time-based (TOTP) codes only.");
  case Refusal::UnknownAlgorithm:
    return QCoreApplication::translate(
        "Totp", "The algorithm in this otpauth:// link is not supported. "
                "QtPass supports SHA1, SHA256 and SHA512.");
  case Refusal::UnreadableUri:
    return QCoreApplication::translate("Totp",
                                       "This otpauth:// link cannot be read.");
  case Refusal::NotASecret:
    return QCoreApplication::translate(
        "Totp", "This is not a TOTP secret or an otpauth://totp/ link.");
  case Refusal::NoSecret:
    return QCoreApplication::translate(
        "Totp", "The secret is missing or is not valid base32.");
  case Refusal::None:
  case Refusal::Empty:
    break;
  }
  return {};
}

auto Totp::isValid(const QString &config) -> bool {
  return parse(config).has_value();
}

auto Totp::counter(const Settings &settings, quint64 unixTime) -> quint64 {
  return unixTime / settings.step;
}

auto Totp::secondsRemaining(const Settings &settings, quint64 unixTime)
    -> uint {
  return settings.step - static_cast<uint>(unixTime % settings.step);
}

auto Totp::generate(const Settings &settings, quint64 unixTime) -> QString {
  const quint64 bigEndianCounter = qToBigEndian(counter(settings, unixTime));
  const QByteArray message(reinterpret_cast<const char *>(&bigEndianCounter),
                           sizeof(bigEndianCounter));

  const QByteArray hmac = QMessageAuthenticationCode::hash(
      message, settings.key, cryptoHash(settings.algorithm));
  if (hmac.size() < kMinHmacSize) {
    return {};
  }

  // RFC 4226 dynamic truncation. QByteArray elements are signed char, so every
  // byte is widened through quint8: without that, any byte >= 0x80
  // sign-extends and corrupts the assembled 31-bit value.
  const int offset =
      static_cast<quint8>(hmac.at(hmac.size() - 1)) & 0x0F; // NOLINT
  const quint32 binary =
      (static_cast<quint32>(static_cast<quint8>(hmac.at(offset)) & 0x7F)
       << 24) |
      (static_cast<quint32>(static_cast<quint8>(hmac.at(offset + 1))) << 16) |
      (static_cast<quint32>(static_cast<quint8>(hmac.at(offset + 2))) << 8) |
      static_cast<quint32>(static_cast<quint8>(hmac.at(offset + 3)));

  if (settings.encoder == Encoder::Steam) {
    // Steam emits the least significant symbol first.
    quint64 value = binary;
    QString code;
    code.reserve(static_cast<int>(STEAM_DIGITS));
    for (uint i = 0; i < STEAM_DIGITS; ++i) {
      code.append(QLatin1Char(kSteamAlphabet[value % kSteamAlphabetSize]));
      value /= kSteamAlphabetSize;
    }
    return code;
  }

  // Integer exponentiation into a quint64: pow() into a quint32 (as KeePassXC
  // did) overflows at ten digits.
  quint64 modulus = 1;
  for (uint i = 0; i < settings.digits; ++i) {
    modulus *= 10;
  }
  return QString::number(binary % modulus)
      .rightJustified(static_cast<int>(settings.digits), QLatin1Char('0'));
}

auto Totp::generateNow(const Settings &settings) -> QString {
  return generate(settings, currentUnixTime());
}

auto Totp::toUri(const Settings &settings) -> QString {
  const QString label =
      settings.label.isEmpty() ? QStringLiteral("QtPass") : settings.label;
  // The Key-Uri-Format spec says the secret's base32 padding is omitted, and
  // third-party importers reject or truncate `secret=...======`.
  // sanitizeInput() re-adds padding when reading, so this round-trips.
  const QString secret =
      QString::fromLatin1(Base32::removePadding(settings.base32Key.toLatin1()));
  QString uri = QStringLiteral("otpauth://totp/%1?secret=%2")
                    .arg(encodeLabel(label), secret);
  if (!settings.issuer.isEmpty()) {
    uri += QStringLiteral("&issuer=%1").arg(encodeLabel(settings.issuer));
  }
  if (settings.algorithm != Algorithm::Sha1) {
    uri +=
        QStringLiteral("&algorithm=%1").arg(algorithmName(settings.algorithm));
  }
  uri += QStringLiteral("&digits=%1&period=%2")
             .arg(settings.digits)
             .arg(settings.step);
  if (settings.encoder == Encoder::Steam) {
    uri += QStringLiteral("&encoder=steam");
  }
  // Re-emit anything the parser did not model, so canonicalising an existing
  // URI does not quietly discard parameters other authenticators honour.
  for (const QPair<QString, QString> &item : settings.extraParams) {
    uri += QStringLiteral("&%1=%2").arg(
        QString::fromLatin1(QUrl::toPercentEncoding(item.first)),
        QString::fromLatin1(QUrl::toPercentEncoding(item.second)));
  }
  if (!settings.fragment.isEmpty()) {
    uri += QLatin1Char('#') + settings.fragment;
  }
  return uri;
}

auto Totp::normalize(const QString &config, const QString &label,
                     const QString &issuer) -> QString {
  const std::optional<Settings> settings = parse(config);
  if (!settings.has_value()) {
    return {};
  }
  Settings canonical = *settings;
  if (canonical.label.isEmpty()) {
    canonical.label = label;
  }
  if (canonical.issuer.isEmpty()) {
    canonical.issuer = issuer;
  }
  // Percent-encoding and the added fields can take a value near the length
  // cap past it; a URI parse() refuses would store an OTP that makes no code.
  const QString uri = toUri(canonical);
  if (uri.size() > MAX_CONFIG_LENGTH) {
    return {};
  }
  return uri;
}

auto Totp::currentUnixTime() -> quint64 {
  return static_cast<quint64>(QDateTime::currentSecsSinceEpoch());
}
