// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
//
// gpg --with-colons output carries user ids and other fields that come from
// keys imported from anywhere (a shared store's keys, a keyserver).
#include "gpgkeystate.h"

#include <QString>

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size == 0) {
    return 0;
  }
  const QString output =
      QString::fromUtf8(reinterpret_cast<const char *>(data + 1),
                        static_cast<qsizetype>(size - 1));
  const QList<UserInfo> users = parseGpgColonOutput(output, (data[0] & 1) != 0);
  for (const UserInfo &user : users) {
    (void)user.name;
    (void)user.key_id;
  }
  return 0;
}
