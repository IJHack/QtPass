// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "rnpcrypto.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <cstring>
#include <memory>
#include <rnp/rnp.h>
#include <rnp/rnp_err.h>

namespace {

/// Message for an RNP failure, in the form "what: why".
auto rnpError(const char *what, rnp_result_t result) -> QString {
  return QStringLiteral("%1: %2").arg(
      QString::fromLatin1(what),
      QString::fromUtf8(rnp_result_to_string(result)));
}

void setError(QString *error, const QString &message) {
  if (error != nullptr) {
    *error = message;
  }
}

struct InputDeleter {
  void operator()(rnp_input_st *input) const { rnp_input_destroy(input); }
};
struct OutputDeleter {
  void operator()(rnp_output_st *output) const { rnp_output_destroy(output); }
};
struct KeyDeleter {
  void operator()(rnp_key_handle_st *key) const { rnp_key_handle_destroy(key); }
};
using Input = std::unique_ptr<rnp_input_st, InputDeleter>;
using Output = std::unique_ptr<rnp_output_st, OutputDeleter>;
using Key = std::unique_ptr<rnp_key_handle_st, KeyDeleter>;

/// An RNP input over @p data; the bytes must outlive it.
auto memoryInput(const QByteArray &data, QString *error, const char *what)
    -> Input {
  rnp_input_t input = nullptr;
  const rnp_result_t result = rnp_input_from_memory(
      &input, reinterpret_cast<const uint8_t *>(data.constData()),
      static_cast<size_t>(data.size()), false);
  if (result != RNP_SUCCESS) {
    setError(error, rnpError(what, result));
    return nullptr;
  }
  return Input(input);
}

auto memoryOutput(QString *error, const char *what) -> Output {
  rnp_output_t output = nullptr;
  const rnp_result_t result = rnp_output_to_memory(&output, 0);
  if (result != RNP_SUCCESS) {
    setError(error, rnpError(what, result));
    return nullptr;
  }
  return Output(output);
}

auto outputBytes(rnp_output_t output) -> QByteArray {
  uint8_t *buf = nullptr;
  size_t len = 0;
  if (rnp_output_memory_get_buf(output, &buf, &len, false) != RNP_SUCCESS) {
    return {};
  }
  return {reinterpret_cast<const char *>(buf), static_cast<qsizetype>(len)};
}

/**
 * The key a .gpg-id line names: a fingerprint (40 hex digits), a long key ID
 * (16), with or without 0x, or else a user ID such as an e-mail address.
 */
auto locateKey(rnp_ffi_t ffi, const QString &recipient) -> Key {
  QString id = recipient.trimmed();
  if (id.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
    id = id.mid(2);
  }
  static const QRegularExpression hex(QStringLiteral("^[0-9A-Fa-f]+$"));
  const char *type = "userid";
  if (hex.match(id).hasMatch()) {
    if (id.size() == 40) {
      type = "fingerprint";
    } else if (id.size() == 16) {
      type = "keyid";
    }
  }
  if (std::strcmp(type, "userid") != 0) {
    id = id.toUpper();
  }
  rnp_key_handle_t key = nullptr;
  if (rnp_locate_key(ffi, type, id.toUtf8().constData(), &key) != RNP_SUCCESS) {
    return nullptr;
  }
  if (key != nullptr || std::strcmp(type, "userid") != 0) {
    return Key(key);
  }
  // rnp_locate_key matches a whole user ID only; gpg -r also takes part of
  // one, and a .gpg-id often lists just the e-mail address.
  rnp_identifier_iterator_t it = nullptr;
  if (rnp_identifier_iterator_create(ffi, &it, "fingerprint") != RNP_SUCCESS) {
    return nullptr;
  }
  Key found;
  const char *fpr = nullptr;
  while (!found && rnp_identifier_iterator_next(it, &fpr) == RNP_SUCCESS &&
         fpr != nullptr) {
    rnp_key_handle_t candidate = nullptr;
    if (rnp_locate_key(ffi, "fingerprint", fpr, &candidate) != RNP_SUCCESS ||
        candidate == nullptr) {
      continue;
    }
    Key handle(candidate);
    size_t uids = 0;
    rnp_key_get_uid_count(candidate, &uids);
    for (size_t i = 0; i < uids && !found; ++i) {
      char *uid = nullptr;
      if (rnp_key_get_uid_at(candidate, i, &uid) != RNP_SUCCESS) {
        continue;
      }
      if (QString::fromUtf8(uid).contains(id, Qt::CaseInsensitive)) {
        found = std::move(handle);
      }
      rnp_buffer_destroy(uid);
    }
  }
  rnp_identifier_iterator_destroy(it);
  return found;
}

// The signature is RNP's rnp_password_cb.
auto passwordCallback(rnp_ffi_t /*ffi*/, void *appCtx, rnp_key_handle_t key,
                      const char * /*pgpContext*/,
                      char buf[], // NOLINT(modernize-avoid-c-arrays)
                      size_t bufLen) -> bool {
  const auto *provider =
      static_cast<const RnpCrypto::PassphraseProvider *>(appCtx);
  if (provider == nullptr || !*provider || bufLen == 0) {
    return false;
  }
  QString keyId;
  char *rawId = nullptr;
  if (rnp_key_get_keyid(key, &rawId) == RNP_SUCCESS) {
    keyId = QString::fromLatin1(rawId);
    rnp_buffer_destroy(rawId);
  }
  QByteArray passphrase = (*provider)(keyId).toUtf8();
  if (passphrase.isEmpty() ||
      static_cast<size_t>(passphrase.size()) >= bufLen) {
    passphrase.fill('\0');
    return false;
  }
  std::memcpy(buf, passphrase.constData(),
              static_cast<size_t>(passphrase.size()));
  buf[passphrase.size()] = '\0';
  passphrase.fill('\0');
  return true;
}

} // namespace

RnpCrypto::RnpCrypto() {
  rnp_ffi_t ffi = nullptr;
  if (rnp_ffi_create(&ffi, "GPG", "GPG") == RNP_SUCCESS) {
    m_ffi = ffi;
    rnp_ffi_set_pass_provider(m_ffi, passwordCallback, &m_passphrase);
  }
}

RnpCrypto::~RnpCrypto() {
  if (m_ffi != nullptr) {
    rnp_ffi_destroy(m_ffi);
  }
}

auto RnpCrypto::isValid() const -> bool { return m_ffi != nullptr; }

void RnpCrypto::setPassphraseProvider(PassphraseProvider provider) {
  m_passphrase = std::move(provider);
}

auto RnpCrypto::importKeys(const QByteArray &keyData, QString *error)
    -> QStringList {
  if (m_ffi == nullptr) {
    setError(error, QStringLiteral("RNP is not available"));
    return {};
  }
  Input input = memoryInput(keyData, error, "Cannot read the key data");
  if (!input) {
    return {};
  }
  char *json = nullptr;
  const rnp_result_t result = rnp_import_keys(
      m_ffi, input.get(), RNP_LOAD_SAVE_PUBLIC_KEYS | RNP_LOAD_SAVE_SECRET_KEYS,
      &json);
  if (result != RNP_SUCCESS) {
    setError(error, rnpError("Cannot import the keys", result));
    return {};
  }
  const QJsonDocument doc = QJsonDocument::fromJson(json);
  rnp_buffer_destroy(json);
  QStringList primaries;
  const QJsonArray keys = doc.object().value(QStringLiteral("keys")).toArray();
  for (const auto &entry : keys) {
    const QString fpr =
        entry.toObject().value(QStringLiteral("fingerprint")).toString();
    rnp_key_handle_t key = nullptr;
    if (rnp_locate_key(m_ffi, "fingerprint", fpr.toLatin1().constData(),
                       &key) != RNP_SUCCESS ||
        key == nullptr) {
      continue;
    }
    bool primary = false;
    rnp_key_is_primary(key, &primary);
    rnp_key_handle_destroy(key);
    if (primary && !primaries.contains(fpr)) {
      primaries << fpr;
    }
  }
  return primaries;
}

auto RnpCrypto::encrypt(const QByteArray &plaintext,
                        const QStringList &recipients, QString *error)
    -> QByteArray {
  if (m_ffi == nullptr) {
    setError(error, QStringLiteral("RNP is not available"));
    return {};
  }
  if (recipients.isEmpty()) {
    setError(error, QStringLiteral("No recipients"));
    return {};
  }
  Input input = memoryInput(plaintext, error, "Cannot read the entry");
  Output output = memoryOutput(error, "Cannot encrypt");
  if (!input || !output) {
    return {};
  }
  rnp_op_encrypt_t op = nullptr;
  rnp_result_t result =
      rnp_op_encrypt_create(&op, m_ffi, input.get(), output.get());
  if (result != RNP_SUCCESS) {
    setError(error, rnpError("Cannot encrypt", result));
    return {};
  }
  const std::unique_ptr<rnp_op_encrypt_st, rnp_result_t (*)(rnp_op_encrypt_t)>
      guard(op, rnp_op_encrypt_destroy);
  for (const QString &recipient : recipients) {
    const Key key = locateKey(m_ffi, recipient);
    if (!key) {
      setError(error, QStringLiteral("No public key for %1").arg(recipient));
      return {};
    }
    result = rnp_op_encrypt_add_recipient(op, key.get());
    if (result != RNP_SUCCESS) {
      setError(error, rnpError(QStringLiteral("Cannot encrypt to %1")
                                   .arg(recipient)
                                   .toUtf8()
                                   .constData(),
                               result));
      return {};
    }
  }
  rnp_op_encrypt_set_armor(op, false);
  result = rnp_op_encrypt_execute(op);
  if (result != RNP_SUCCESS) {
    setError(error, rnpError("Cannot encrypt", result));
    return {};
  }
  return outputBytes(output.get());
}

auto RnpCrypto::decrypt(const QByteArray &ciphertext, QString *error, bool *ok)
    -> QByteArray {
  if (ok != nullptr) {
    *ok = false;
  }
  if (m_ffi == nullptr) {
    setError(error, QStringLiteral("RNP is not available"));
    return {};
  }
  Input input = memoryInput(ciphertext, error, "Cannot read the entry");
  Output output = memoryOutput(error, "Cannot decrypt");
  if (!input || !output) {
    return {};
  }
  const rnp_result_t result = rnp_decrypt(m_ffi, input.get(), output.get());
  if (result != RNP_SUCCESS) {
    setError(error, rnpError("Cannot decrypt", result));
    return {};
  }
  if (ok != nullptr) {
    *ok = true;
  }
  return outputBytes(output.get());
}
