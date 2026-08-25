// SPDX-FileCopyrightText: 2025 KeePassXC Team <team@keepassxc.org>
// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_BASE32_H_
#define SRC_BASE32_H_

#include <QByteArray>

/**
 * @class Base32
 * @brief RFC 4648 base32 codec, used for TOTP shared secrets.
 *
 * Ported from KeePassXC (`src/core/Base32.cpp`). decode() is deliberately
 * strict: the input length must be a multiple of 8 and every character must be
 * in the base32 alphabet, so whitespace is an error rather than something to
 * skip. Secrets that come from a human (or from a QR code label) therefore go
 * through sanitizeInput() first, making the canonical entry point:
 *
 * @code
 * const QByteArray secret =
 * Base32::decode(Base32::sanitizeInput(text.toLatin1()));
 * @endcode
 *
 * Failure is reported as an empty QByteArray. A successful decode of a
 * non-empty input always yields at least one byte, so `isEmpty()` is an
 * unambiguous error test.
 */
class Base32 {
public:
  /**
   * @brief Decode strict RFC 4648 base32 data.
   * @param encodedData Padded base32 text whose length is a multiple of 8.
   * @return Decoded bytes, or an empty QByteArray when the input is empty,
   *         mis-padded, or contains a character outside "A-Za-z2-7=".
   */
  [[nodiscard]] static auto decode(const QByteArray &encodedData) -> QByteArray;

  /**
   * @brief Encode raw bytes as padded RFC 4648 base32.
   * @param data Raw bytes to encode.
   * @return Upper-case padded base32 text, or an empty QByteArray when @p data
   *         is empty.
   */
  [[nodiscard]] static auto encode(const QByteArray &data) -> QByteArray;

  /**
   * @brief Append '=' padding up to a multiple of 8 characters.
   * @param encodedData Unpadded or partially padded base32 text.
   * @return Padded text, or @p encodedData unchanged when its length remainder
   *         is impossible for base32 (1, 3 or 6).
   */
  [[nodiscard]] static auto addPadding(const QByteArray &encodedData)
      -> QByteArray;

  /**
   * @brief Strip trailing '=' padding.
   * @param encodedData Padded base32 text.
   * @return Text without padding, or @p encodedData unchanged when its length
   *         is not a multiple of 8.
   */
  [[nodiscard]] static auto removePadding(const QByteArray &encodedData)
      -> QByteArray;

  /**
   * @brief Make human-entered base32 text decodable.
   *
   * Drops every character outside "A-Za-z2-7" (so the spaces and dashes used
   * to group a printed secret are tolerated), maps the digits that are
   * ambiguous in the base32 alphabet (0 to O, 1 to L, 8 to B), then pads the
   * result with addPadding().
   * @param encodedData Raw user input.
   * @return Sanitized, padded base32 text.
   */
  [[nodiscard]] static auto sanitizeInput(const QByteArray &encodedData)
      -> QByteArray;

private:
  Base32() = default;
};

#endif // SRC_BASE32_H_
