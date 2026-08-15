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

} // namespace

/**
 * @brief Decode strict RFC 4648 base32 data.
 * @param encodedData Padded base32 text whose length is a multiple of 8.
 * @return Decoded bytes, or an empty QByteArray on any error.
 */
auto Base32::decode(const QByteArray &encodedData) -> QByteArray {
  if (encodedData.isEmpty()) {
    return {};
  }

  // Strict: a well-formed base32 string is always padded to a multiple of 8.
  if (encodedData.size() % 8 != 0) {
    return {};
  }

  const int nPads = countPadding(encodedData);

  int specialOffset = 0;
  int nSpecialBytes = 0;

  switch (nPads) { // in {0, 1, 3, 4, 6}
  case 1:
    nSpecialBytes = 4;
    specialOffset = 3;
    break;
  case 3:
    nSpecialBytes = 3;
    specialOffset = 1;
    break;
  case 4:
    nSpecialBytes = 2;
    specialOffset = 4;
    break;
  case 6:
    nSpecialBytes = 1;
    specialOffset = 2;
    break;
  case 0:
    break;
  default:
    // 2, 5 and 6+ trailing '=' cannot be produced by a base32 encoder.
    // KeePassXC fell through to "no special bytes" here and returned
    // wrong-length data; reject instead.
    return {};
  }

  const qsizetype nQuanta = encodedData.size() / 8;
  const qsizetype nBytes =
      nSpecialBytes > 0 ? (nQuanta - 1) * 5 + nSpecialBytes : nQuanta * 5;

  QByteArray data(nBytes, Qt::Uninitialized);
  // Written through a raw pointer: Qt 5.15 declares only operator[](int) and
  // operator[](uint), so subscripting with a qsizetype is an ambiguous overload
  // there. Qt 6 added a qsizetype overload, which is why this only breaks the
  // Qt 5.15 build.
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
      auto ch = static_cast<quint8>(encodedData.at(i++));
      if ((ASCII_A <= ch && ch <= ASCII_Z) ||
          (ASCII_a <= ch && ch <= ASCII_z)) {
        ch -= ASCII_A;
        if (ch >= ALPH_POS_2) {
          // Fold lower case onto the same alphabet positions as upper case.
          ch -= ASCII_a - ASCII_A;
        }
      } else if (ASCII_2 <= ch && ch <= ASCII_7) {
        ch -= ASCII_2;
        ch += ALPH_POS_2;
      } else if (ASCII_EQ == ch) {
        if (i - 1 < firstPad) {
          // '=' before the trailing run is not padding, it is malformed input.
          return {};
        }
        if (i == encodedData.size()) {
          // Finished with the short final quantum.
          quantum >>= specialOffset;
          nQuantumBytes = nSpecialBytes;
        }
        continue;
      } else {
        // Illegal character.
        return {};
      }

      quantum <<= 5;
      quantum |= ch;
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
  // Raw pointer: see the note in decode() about Qt 5.15's operator[] overloads.
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
  // Raw pointer: see the note in decode() about Qt 5.15's operator[] overloads.
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
