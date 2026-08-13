// SPDX-FileCopyrightText: 2025 KeePassXC Team <team@keepassxc.org>
// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @class Totp
 * @brief RFC 6238 TOTP implementation.
 *
 * @see totp.h
 */

#include "totp.h"
#include "base32.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QLatin1Char>
#include <QMessageAuthenticationCode>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QtEndian>
#include <QtGlobal>

namespace {

/// Steam Guard's alphabet: no A, E, I, L, O, S, U, Z, 0 or 1.
constexpr char kSteamAlphabet[] = "23456789BCDFGHJKMNPQRTVWXY";
/// Number of symbols in kSteamAlphabet, excluding the terminating NUL.
constexpr quint64 kSteamAlphabetSize = sizeof(kSteamAlphabet) - 1;
/// Shortest HMAC output the dynamic truncation below can read from (SHA-1).
constexpr int kMinHmacSize = 20;

/**
 * @brief Map an `algorithm` URI value onto a hash function.
 * @param name Value of the `algorithm` parameter.
 * @return The matching algorithm, defaulting to SHA-1.
 */
auto algorithmByName(const QString &name) -> Totp::Algorithm {
  const QString upper = name.toUpper();
  if (upper == QStringLiteral("SHA512") ||
      upper == QStringLiteral("HMAC-SHA-512")) {
    return Totp::Algorithm::Sha512;
  }
  if (upper == QStringLiteral("SHA256") ||
      upper == QStringLiteral("HMAC-SHA-256")) {
    return Totp::Algorithm::Sha256;
  }
  return Totp::Algorithm::Sha1;
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

/**
 * @brief Parse an OTP configuration string.
 * @param config URI or bare base32 secret.
 * @return Validated settings, or std::nullopt when the secret is unusable.
 */
auto Totp::parse(const QString &config) -> std::optional<Settings> {
  const QString trimmed = config.trimmed();
  if (trimmed.isEmpty()) {
    return std::nullopt;
  }

  Settings settings;
  QString rawSecret;

  const QUrl url(trimmed);
  if (url.isValid() && url.scheme().compare(QStringLiteral("otpauth"),
                                            Qt::CaseInsensitive) == 0) {
    // Counter-based HOTP cannot be generated from a clock; refuse rather than
    // return a wrong code that looks right.
    if (url.host().compare(QStringLiteral("totp"), Qt::CaseInsensitive) != 0) {
      return std::nullopt;
    }

    const QUrlQuery query(url);
    rawSecret =
        query.queryItemValue(QStringLiteral("secret"), QUrl::FullyDecoded);
    settings.issuer =
        query.queryItemValue(QStringLiteral("issuer"), QUrl::FullyDecoded);
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
        query.queryItemValue(QStringLiteral("digits")), DEFAULT_DIGITS);
    settings.step = parsePositiveUInt(
        query.queryItemValue(QStringLiteral("period")), DEFAULT_STEP);
    if (query.hasQueryItem(QStringLiteral("algorithm"))) {
      settings.algorithm =
          algorithmByName(query.queryItemValue(QStringLiteral("algorithm")));
    }
    if (query.queryItemValue(QStringLiteral("encoder"))
            .compare(QStringLiteral("steam"), Qt::CaseInsensitive) == 0) {
      settings.encoder = Encoder::Steam;
    }
    // Anything we do not model is kept verbatim so toUri() can put it back;
    // dropping it silently mutated URIs carrying e.g. `image=`.
    static const QStringList known = {
        QStringLiteral("secret"),    QStringLiteral("issuer"),
        QStringLiteral("digits"),    QStringLiteral("period"),
        QStringLiteral("algorithm"), QStringLiteral("encoder")};
    const QList<QPair<QString, QString>> items = query.queryItems();
    for (const QPair<QString, QString> &item : items) {
      if (!known.contains(item.first, Qt::CaseInsensitive)) {
        settings.extraParams.append(item);
      }
    }
  } else {
    // Not a URI: treat the whole string as a bare base32 secret.
    rawSecret = trimmed;
  }

  const QByteArray sanitized = Base32::sanitizeInput(rawSecret.toLatin1());
  settings.key = Base32::decode(sanitized);
  if (settings.key.isEmpty()) {
    return std::nullopt;
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

/**
 * @brief Convenience validity test, for UI feedback.
 * @param config URI or bare base32 secret.
 * @return true when parse() would succeed.
 */
auto Totp::isValid(const QString &config) -> bool {
  return parse(config).has_value();
}

/**
 * @brief Time-step counter for a point in time.
 * @param settings Validated settings.
 * @param unixTime Seconds since the Unix epoch.
 * @return `unixTime / settings.step`.
 */
auto Totp::counter(const Settings &settings, quint64 unixTime) -> quint64 {
  return unixTime / settings.step;
}

/**
 * @brief Seconds until the code for @p unixTime expires.
 * @param settings Validated settings.
 * @param unixTime Seconds since the Unix epoch.
 * @return A value in 1..settings.step.
 */
auto Totp::secondsRemaining(const Settings &settings, quint64 unixTime)
    -> uint {
  return settings.step - static_cast<uint>(unixTime % settings.step);
}

/**
 * @brief Generate the code for a given point in time.
 * @param settings Validated settings.
 * @param unixTime Seconds since the Unix epoch.
 * @return The code, exactly Settings::digits characters long.
 */
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

/**
 * @brief Generate the code for the current wall-clock time.
 * @param settings Validated settings.
 * @return The current code.
 */
auto Totp::generateNow(const Settings &settings) -> QString {
  return generate(settings, currentUnixTime());
}

/**
 * @brief Serialise settings as a canonical otpauth URI.
 * @param settings Validated settings.
 * @return An `otpauth://totp/...` URI.
 */
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
  return uri;
}

/**
 * @brief Normalise a user-entered value to a canonical otpauth URI.
 * @param config URI or bare base32 secret.
 * @param label Fallback label, used when @p config carries none.
 * @param issuer Fallback issuer, used when @p config carries none.
 * @return A canonical URI, or an empty string when @p config is not valid.
 */
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
  return toUri(canonical);
}

/**
 * @brief Current wall-clock time.
 * @return Seconds since the Unix epoch (UTC).
 */
auto Totp::currentUnixTime() -> quint64 {
  return static_cast<quint64>(QDateTime::currentSecsSinceEpoch());
}
