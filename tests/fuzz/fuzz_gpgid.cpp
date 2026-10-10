// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A .gpg-id comes with the shared store. Its generation/folder header and
// its recipient lines are parsed before QtPass decides whom to encrypt to.
#include "gpgidgeneration.h"
#include "pass.h"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <cstdlib>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  const QByteArray contents(reinterpret_cast<const char *>(data),
                            static_cast<qsizetype>(size));

  (void)Pass::parseRecipients(contents, QStringLiteral("fuzz/.gpg-id"));

  QString error;
  const auto header = GpgIdGeneration::parse(contents, &error);
  if (!header || header->generation <= 0 || !header->folder) {
    return 0;
  }
  // A header QtPass accepted must survive being written out again.
  const auto again = GpgIdGeneration::parse(
      GpgIdGeneration::header(header->generation, *header->folder));
  if (!again || again->generation != header->generation ||
      again->folder != header->folder) {
    abort();
  }
  return 0;
}
