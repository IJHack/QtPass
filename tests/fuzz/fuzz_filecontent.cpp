// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Entry text is untrusted: in a shared store any co-writer can put anything
// in a .gpg file, and FileContent parses it as soon as the entry is shown.
#include "filecontent.h"

#include <QString>
#include <QStringList>

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size == 0) {
    return 0;
  }
  // First byte: the two parse modes and whether a template is configured.
  const uint8_t mode = data[0];
  const QString text =
      QString::fromUtf8(reinterpret_cast<const char *>(data + 1),
                        static_cast<qsizetype>(size - 1));
  const QStringList templateFields =
      (mode & 2) != 0 ? QStringList{"login", "url", "email", "otpauth"}
                      : QStringList{};

  FileContent content =
      FileContent::parse(text, templateFields, (mode & 1) != 0);
  (void)content.getPassword();
  (void)content.getPasswordForDisplay();
  (void)content.getRemainingData();
  (void)content.getRemainingDataForDisplay();
  (void)content.getOtpUri();
  NamedValues values = content.getNamedValues();
  for (const NamedValue &value : values) {
    (void)FileContent::isOtpFieldName(value.name);
    (void)FileContent::isOtpUriValue(value.value);
  }
  if (!values.isEmpty()) {
    (void)values.takeValue(values.first().name);
  }
  return 0;
}
