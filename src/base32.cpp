// SPDX-FileCopyrightText: 2025 KeePassXC Team <team@keepassxc.org>
// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @class Base32
 * @brief RFC 4648 base32 codec implementation.
 *
 * Conforms to RFC 4648, see https://tools.ietf.org/html/rfc4648. Ported from
 * KeePassXC `src/core/Base32.cpp`; the differences are documented at each
 * site.
 *
 * @see base32.h
 */

#include "base32.h"

#include <optional>

namespace {

constexpr quint64 MASK_40BIT = quint64(0xF8) << 32;
constexpr quint64 MASK_35BIT = quint64(0x7C0000000);
constexpr quint64 MASK_25BIT = quint64(0x1F00000);
constexpr quint64 MASK_20BIT = quint64(0xF8000);
constexpr quint64 MASK_10BIT = quint64(0x3E0);

constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
constexpr quint8 ALPH_POS_2 = 26;

constexpr quint8 ASCII_2 = static_cast<quint8>('2');
constexpr quint8 ASCII_7 = static_cast<quint8>('7');
constexpr quint8 ASCII_A = static_cast<quint8>('A');
constexpr quint8 ASCII_Z = static_cast<quint8>('Z');
constexpr quint8 ASCII_a = static_cast<quint8>('a');
constexpr quint8 ASCII_z = static_cast<quint8>('z');
constexpr quint8 ASCII_EQ = static_cast<quint8>('=');

/**
 * @brief Length of the trailing run of '=' characters.
 *
 * Stops at the first non-'=': counting every '=' in the last few positions
 * would treat an interior one as padding, and removePadding() would then
 * truncate real data.
 * @param encodedData Base32 text.
 * @return Number of trailing '=', at most 6.
 */
auto countPadding(const QByteArray &encodedData) -> int {
  int nPads = 0;
  for (qsizetype i = encodedData.size() - 1;
       i >= 0 && i > encodedData.size() - 7; --i) {
    if (encodedData.at(i) != '=') {
      break;
    }
    ++nPads;
  }
  return nPads;
}

/**
 * @brief What the padding says about the last quantum.
 *
 * A base32 encoder pads to a multiple of 8 with 0, 1, 3, 4 or 6 '='; any
 * other count cannot come from one. KeePassXC fell through to "no special
 * bytes" for 2, 5 and 7 and returned wrong-length data.
 */
struct Tail {
  int bytes = 0;  ///< Bytes the final quantum carries; 0 for a full one.
  int offset = 0; ///< Bits of the final quantum that are padding.
};

/// The tail shape for @p nPads trailing '=', or nothing for a count no
/// encoder produces.
auto tailShape(int nPads) -> std::optional<Tail> {
  switch (nPads) {
  case 0:
    return Tail{};
  case 1:
    return Tail{4, 3};
  case 3:
    return Tail{3, 1};
  case 4:
    return Tail{2, 4};
  case 6:
    return Tail{1, 2};
  default:
    return std::nullopt;
  }
}

/// The alphabet position of @p ch (upper or lower case, digits 2-7), or
/// nothing when it is not a base32 symbol.
auto symbolValue(quint8 ch) -> std::optional<quint8> {
  if ((ASCII_A <= ch && ch <= ASCII_Z) || (ASCII_a <= ch && ch <= ASCII_z)) {
    ch -= ASCII_A;
    if (ch >= ALPH_POS_2) {
      // Fold lower case onto the same alphabet positions as upper case.
      ch -= ASCII_a - ASCII_A;
    }
    return ch;
  }
  if (ASCII_2 <= ch && ch <= ASCII_7) {
    return static_cast<quint8>(ch - ASCII_2 + ALPH_POS_2);
  }
  return std::nullopt;
}

} // namespace

/**
 * @brief Decode strict RFC 4648 base32 data.
 * @param encodedData Padded base32 text whose length is a multiple of 8.
 * @return Decoded bytes, or an empty QByteArray on any error.
 */
auto Base32::decode(const QByteArray &encodedData) -> QByteArray {
  // Strict: a well-formed base32 string is always padded to a multiple of 8.
  if (encodedData.isEmpty() || encodedData.size() % 8 != 0) {
    return {};
  }
  const int nPads = countPadding(encodedData);
  const std::optional<Tail> tail = tailShape(nPads);
  if (!tail) {
    return {};
  }

  const qsizetype nQuanta = encodedData.size() / 8;
  const qsizetype nBytes =
      tail->bytes > 0 ? (nQuanta - 1) * 5 + tail->bytes : nQuanta * 5;

  QByteArray data(nBytes, Qt::Uninitialized);
  // Written through a raw pointer to avoid QByteArray's detach check on every
  // subscript in the hot loop.
  char *out = data.data();

  qsizetype i = 0;
  qsizetype o = 0;
  // Everything from the first '=' onward must be padding. Reuse the pad count
  // computed above rather than scanning the tail a second time.
  const qsizetype firstPad = encodedData.size() - nPads;

  while (i < encodedData.size()) {
    quint64 quantum = 0;
    int nQuantumBytes = 5;

    for (int n = 0; n < 8; ++n) {
      const auto ch = static_cast<quint8>(encodedData.at(i++));
      if (ASCII_EQ == ch) {
        if (i - 1 < firstPad) {
          // '=' before the trailing run is not padding, it is malformed input.
          return {};
        }
        if (i == encodedData.size()) {
          // Finished with the short final quantum.
          quantum >>= tail->offset;
          nQuantumBytes = tail->bytes;
        }
        continue;
      }
      const std::optional<quint8> value = symbolValue(ch);
      if (!value) {
        return {};
      }
      quantum <<= 5;
      quantum |= *value;
    }

    const int offset = (nQuantumBytes - 1) * 8;
    quint64 mask = quint64(0xFF) << offset;
    for (int n = offset; n >= 0 && o < nBytes; n -= 8) {
      out[o++] = static_cast<char>((quantum & mask) >> n);
      mask >>= 8;
    }
  }

  return data;
}

/**
 * @brief Encode raw bytes as padded RFC 4648 base32.
 * @param data Raw bytes to encode.
 * @return Upper-case padded base32 text, empty when @p data is empty.
 */
auto Base32::encode(const QByteArray &data) -> QByteArray {
  if (data.isEmpty()) {
    return {};
  }

  const qsizetype nBits = data.size() * 8;
  const int rBits = static_cast<int>(nBits % 40); // in {0, 8, 16, 24, 32}
  const qsizetype nQuanta = nBits / 40 + (rBits > 0 ? 1 : 0);
  const qsizetype nBytes = nQuanta * 8;
  QByteArray encodedData(nBytes, Qt::Uninitialized);
  // Raw pointer: see the note in decode().
  char *out = encodedData.data();

  qsizetype i = 0;
  qsizetype o = 0;
  int n = 0;
  quint64 mask = 0;
  quint64 quantum = 0;

  // QByteArray elements are signed char: widening them directly (as KeePassXC
  // does) sign-extends any byte >= 0x80 and the resulting high bits corrupt
  // the neighbouring bytes of the quantum. Cast through quint8.
  const auto byteAt = [&data](qsizetype index) -> quint64 {
    return static_cast<quint64>(static_cast<quint8>(data.at(index)));
  };

  // 40 bits of input per input group.
  while (i + 5 <= data.size()) {
    quantum = 0;
    for (n = 32; n >= 0; n -= 8) {
      quantum |= byteAt(i++) << n;
    }

    mask = MASK_40BIT;
    for (n = 35; n >= 0; n -= 5) {
      out[o++] = kAlphabet[(quantum & mask) >> n];
      mask >>= 5;
    }
  }

  // Fewer than 40 bits of input in the final input group.
  if (i < data.size()) {
    quantum = 0;
    for (n = rBits - 8; n >= 0; n -= 8) {
      quantum |= byteAt(i++) << n;
    }

    switch (rBits) {
    case 8: // expand to 10 bits
      quantum <<= 2;
      mask = MASK_10BIT;
      n = 5;
      break;
    case 16: // expand to 20 bits
      quantum <<= 4;
      mask = MASK_20BIT;
      n = 15;
      break;
    case 24: // expand to 25 bits
      quantum <<= 1;
      mask = MASK_25BIT;
      n = 20;
      break;
    default: // expand to 35 bits
      quantum <<= 3;
      mask = MASK_35BIT;
      n = 30;
    }

    while (n >= 0) {
      out[o++] = kAlphabet[(quantum & mask) >> n];
      mask >>= 5;
      n -= 5;
    }

    // Add the pad characters.
    while (o < encodedData.size()) {
      out[o++] = '=';
    }
  }

  return encodedData;
}

/**
 * @brief Append '=' padding up to a multiple of 8 characters.
 * @param encodedData Unpadded or partially padded base32 text.
 * @return Padded text, unchanged for impossible length remainders.
 */
auto Base32::addPadding(const QByteArray &encodedData) -> QByteArray {
  if (encodedData.isEmpty() || encodedData.size() % 8 == 0) {
    return encodedData;
  }

  const int rBytes = static_cast<int>(encodedData.size() % 8);
  // rBytes must be a member of {2, 4, 5, 7}.
  if (1 == rBytes || 3 == rBytes || 6 == rBytes) {
    return encodedData;
  }

  QByteArray newEncodedData(encodedData);
  for (int nPads = 8 - rBytes; nPads > 0; --nPads) {
    newEncodedData.append('=');
  }

  return newEncodedData;
}

/**
 * @brief Strip trailing '=' padding.
 * @param encodedData Padded base32 text.
 * @return Text without padding, unchanged when not a multiple of 8.
 */
auto Base32::removePadding(const QByteArray &encodedData) -> QByteArray {
  if (encodedData.isEmpty() || encodedData.size() % 8 != 0) {
    return encodedData; // return the same bad input
  }

  QByteArray newEncodedData(encodedData);
  newEncodedData.resize(encodedData.size() - countPadding(encodedData));

  return newEncodedData;
}

/**
 * @brief Make human-entered base32 text decodable.
 * @param encodedData Raw user input.
 * @return Sanitized, padded base32 text.
 */
auto Base32::sanitizeInput(const QByteArray &encodedData) -> QByteArray {
  if (encodedData.isEmpty()) {
    return encodedData;
  }

  QByteArray newEncodedData(encodedData.size(), Qt::Uninitialized);
  // Raw pointer: see the note in decode().
  char *out = newEncodedData.data();
  qsizetype i = 0;
  for (auto ch : encodedData) {
    switch (ch) {
    case '0':
      out[i++] = 'O';
      break;
    case '1':
      out[i++] = 'L';
      break;
    case '8':
      out[i++] = 'B';
      break;
    default:
      if (('A' <= ch && ch <= 'Z') || ('a' <= ch && ch <= 'z') ||
          ('2' <= ch && ch <= '7')) {
        out[i++] = ch;
      }
    }
  }
  newEncodedData.resize(i);

  return addPadding(newEncodedData);
}
