// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
//
// An otpauth:// URI or bare secret comes from entry text (untrusted in a
// shared store) or from a scanned QR code. Totp::parse() promises that what
// it returns is valid, so generate() and a toUri() round trip must work.
#include "base32.h"
#include "totp.h"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <cstdlib>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  const QByteArray bytes(reinterpret_cast<const char *>(data),
                         static_cast<qsizetype>(size));

  // Base32 on its own: decoding what encode() produced gives the input back.
  if (Base32::decode(Base32::encode(bytes)) != bytes) {
    abort();
  }
  (void)Base32::decode(bytes);

  const QString config = QString::fromUtf8(bytes);
  const auto settings = Totp::parse(config);
  if (settings.has_value() != Totp::isValid(config)) {
    abort();
  }
  if (!settings) {
    return 0;
  }
  if (settings->key.isEmpty()) {
    abort();
  }
  const QString code = Totp::generate(*settings, 1700000000ULL);
  if (code.size() != static_cast<qsizetype>(settings->digits)) {
    abort();
  }
  const auto again = Totp::parse(Totp::toUri(*settings));
  if (!again || again->key != settings->key ||
      again->digits != settings->digits || again->step != settings->step ||
      again->algorithm != settings->algorithm ||
      again->encoder != settings->encoder) {
    abort();
  }
  return 0;
}
