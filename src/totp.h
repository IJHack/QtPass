// SPDX-FileCopyrightText: 2025 KeePassXC Team <team@keepassxc.org>
// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_TOTP_H_
#define SRC_TOTP_H_

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>

#include <optional>

/**
 * @class Totp
 * @brief RFC 6238 time-based one-time password generation.
 *
 * Ported and trimmed from KeePassXC (`src/core/Totp.cpp`). QtPass stores TOTP
 * configuration as an `otpauth://totp/...` URI — canonically in the `OTP`
 * template field, and also accepted as a bare line in the entry body, which is
 * the convention the `pass-otp` extension uses. Only that dialect plus a bare
 * base32 secret is parsed; KeePassXC's KeeOtp and legacy `[step];[digits]`
 * formats exist purely to read `.kdbx` entry attributes and have no meaning in
 * a password store.
 *
 * Totp::parse() validates everything it returns, so Settings is valid by
 * construction and Totp::generate() cannot fail. HMAC comes from QtCore
 * (QMessageAuthenticationCode), so no crypto dependency is added.
 */
class Totp {
public:
  /**
   * @brief HMAC hash function, selected by the `algorithm` URI parameter.
   */
  enum class Algorithm {
    Sha1,   ///< HMAC-SHA-1, the RFC 6238 default.
    Sha256, ///< HMAC-SHA-256.
    Sha512, ///< HMAC-SHA-512.
  };

  /**
   * @brief Digit encoding, selected by the `encoder` URI parameter.
   */
  enum class Encoder {
    Base10, ///< RFC 6238 decimal digits, zero padded.
    Steam,  ///< Steam Guard's five-character alphabet.
  };

  /**
   * @struct Settings
   * @brief Validated TOTP parameters. Only Totp::parse() produces one.
   */
  struct Settings {
    QByteArray key;    ///< Decoded shared secret; never empty.
    QString base32Key; ///< Sanitized base32 secret, for re-serialisation.
    Algorithm algorithm{Algorithm::Sha1}; ///< HMAC hash function.
    Encoder encoder{Encoder::Base10};     ///< Digit encoding.
    uint digits{DEFAULT_DIGITS}; ///< Code length, MIN_DIGITS..MAX_DIGITS.
    uint step{DEFAULT_STEP};     ///< Period in seconds, 1..MAX_STEP.
    QString label;               ///< otpauth label; may be empty.
    QString issuer;              ///< otpauth issuer; may be empty.
    /// Query parameters the parser does not model, kept verbatim so toUri()
    /// can put them back instead of silently dropping them.
    QList<QPair<QString, QString>> extraParams;
  };

  static constexpr uint DEFAULT_DIGITS = 6U; ///< RFC 6238 default code length.
  static constexpr uint DEFAULT_STEP = 30U;  ///< RFC 6238 default period.
  static constexpr uint STEAM_DIGITS = 5U; ///< Steam codes are always 5 chars.
  static constexpr uint MIN_DIGITS = 1U;   ///< Lower clamp for `digits`.
  static constexpr uint MAX_DIGITS = 10U;  ///< Upper clamp for `digits`.
  static constexpr uint MAX_STEP = 86400U; ///< Upper clamp for `period`.

  /**
   * @brief Parse an OTP configuration string.
   *
   * Accepts an `otpauth://totp/<label>?secret=...` URI, reading the `secret`,
   * `digits`, `period`, `algorithm`, `encoder` and `issuer` parameters, or a
   * bare base32 secret. `otpauth://hotp/...` is rejected: a counter-based HOTP
   * value cannot be derived from a clock, and returning a plausible-looking
   * TOTP code for it would be silently wrong.
   *
   * `digits` is clamped to MIN_DIGITS..MAX_DIGITS and `period` to 1..MAX_STEP.
   * @param config URI or bare base32 secret; surrounding whitespace is ignored.
   * @return Validated settings, or std::nullopt when the secret is missing or
   *         is not decodable base32.
   */
  [[nodiscard]] static auto parse(const QString &config)
      -> std::optional<Settings>;

  /**
   * @brief Convenience validity test, for UI feedback.
   * @param config URI or bare base32 secret.
   * @return true when parse() would succeed.
   */
  [[nodiscard]] static auto isValid(const QString &config) -> bool;

  /**
   * @brief Generate the code for a given point in time.
   * @param settings Validated settings.
   * @param unixTime Seconds since the Unix epoch (UTC).
   * @return The code, exactly Settings::digits characters long.
   */
  [[nodiscard]] static auto generate(const Settings &settings, quint64 unixTime)
      -> QString;

  /**
   * @brief Generate the code for the current wall-clock time.
   * @param settings Validated settings.
   * @return The current code.
   */
  [[nodiscard]] static auto generateNow(const Settings &settings) -> QString;

  /**
   * @brief Time-step counter for a point in time.
   * @param settings Validated settings.
   * @param unixTime Seconds since the Unix epoch (UTC).
   * @return `unixTime / settings.step`.
   */
  [[nodiscard]] static auto counter(const Settings &settings, quint64 unixTime)
      -> quint64;

  /**
   * @brief Seconds until the code for @p unixTime expires.
   * @param settings Validated settings.
   * @param unixTime Seconds since the Unix epoch (UTC).
   * @return A value in 1..settings.step, so a period boundary reads as a full
   *         step rather than zero.
   */
  [[nodiscard]] static auto secondsRemaining(const Settings &settings,
                                             quint64 unixTime) -> uint;

  /**
   * @brief Serialise settings as a canonical otpauth URI.
   *
   * Always emits `secret`, `digits` and `period`; emits `issuer` when it is
   * set, `algorithm` only when it differs from the RFC 6238 default, and
   * `encoder=steam` for Encoder::Steam.
   * @param settings Validated settings.
   * @return An `otpauth://totp/...` URI.
   */
  [[nodiscard]] static auto toUri(const Settings &settings) -> QString;

  /**
   * @brief Normalise a user-entered value to a canonical otpauth URI.
   * @param config URI or bare base32 secret.
   * @param label Fallback label, used when @p config carries none. The entry
   *        name is a good choice.
   * @param issuer Fallback issuer, used when @p config carries none.
   * @return A canonical URI, or an empty string when @p config is not valid.
   */
  [[nodiscard]] static auto normalize(const QString &config,
                                      const QString &label = {},
                                      const QString &issuer = {}) -> QString;

  /**
   * @brief Current wall-clock time.
   * @return Seconds since the Unix epoch (UTC).
   */
  [[nodiscard]] static auto currentUnixTime() -> quint64;

private:
  Totp() = default;
};

#endif // SRC_TOTP_H_
