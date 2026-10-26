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
auto rnpError(const QString &what, rnp_result_t result) -> QString {
  return QStringLiteral("%1: %2").arg(
      what, QString::fromUtf8(rnp_result_to_string(result)));
}

auto rnpError(const char *what, rnp_result_t result) -> QString {
  return rnpError(QString::fromUtf8(what), result);
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

/// Whether @p key may be used: not expired, not revoked.
auto isValidKey(rnp_key_handle_t key) -> bool {
  bool valid = false;
  return rnp_key_is_valid(key, &valid) == RNP_SUCCESS && valid;
}

/// The primary key's fingerprint for @p key, itself if it is primary.
auto primaryFingerprint(rnp_key_handle_t key) -> QString {
  bool primary = false;
  rnp_key_is_primary(key, &primary);
  char *fpr = nullptr;
  if ((primary ? rnp_key_get_fprint(key, &fpr)
               : rnp_key_get_primary_fprint(key, &fpr)) != RNP_SUCCESS) {
    return {};
  }
  QString result = QString::fromLatin1(fpr);
  rnp_buffer_destroy(fpr);
  return result;
}

/// Whether any user ID of @p key satisfies @p hit.
template <typename Hit>
auto anyUserIdMatches(rnp_key_handle_t key, Hit hit) -> bool {
  size_t uids = 0;
  rnp_key_get_uid_count(key, &uids);
  for (size_t i = 0; i < uids; ++i) {
    char *uid = nullptr;
    if (rnp_key_get_uid_at(key, i, &uid) != RNP_SUCCESS) {
      continue;
    }
    const bool matched = hit(QString::fromUtf8(uid));
    rnp_buffer_destroy(uid);
    if (matched) {
      return true;
    }
  }
  return false;
}

/**
 * Fingerprints of the valid primary keys for which @p match holds, given
 * each key in the keyring (primaries and subkeys) in turn.
 */
template <typename Match>
auto matchingPrimaries(rnp_ffi_t ffi, Match match) -> QStringList {
  QStringList primaries;
  rnp_identifier_iterator_t it = nullptr;
  if (rnp_identifier_iterator_create(ffi, &it, "fingerprint") != RNP_SUCCESS) {
    return primaries;
  }
  const char *fpr = nullptr;
  while (rnp_identifier_iterator_next(it, &fpr) == RNP_SUCCESS &&
         fpr != nullptr) {
    rnp_key_handle_t handle = nullptr;
    if (rnp_locate_key(ffi, "fingerprint", fpr, &handle) != RNP_SUCCESS ||
        handle == nullptr) {
      continue;
    }
    const Key key(handle);
    if (!match(handle)) {
      continue;
    }
    const QString primary = primaryFingerprint(handle);
    rnp_key_handle_t primaryHandle = nullptr;
    if (primary.isEmpty() || primaries.contains(primary) ||
        rnp_locate_key(ffi, "fingerprint", primary.toLatin1().constData(),
                       &primaryHandle) != RNP_SUCCESS ||
        primaryHandle == nullptr) {
      continue;
    }
    const Key primaryKey(primaryHandle);
    if (isValidKey(primaryHandle)) {
      primaries << primary;
    }
  }
  rnp_identifier_iterator_destroy(it);
  return primaries;
}

/**
 * The key a .gpg-id line names, as gpg -r reads it: a fingerprint (40 hex
 * digits, spaces allowed), a long (16) or short (8) key ID, with or without
 * 0x, or else a user ID or part of one, such as an e-mail address. A short
 * key ID or a partial user ID must name exactly one valid key. Sets @p why
 * when there is no such key.
 */
auto locateKey(rnp_ffi_t ffi, const QString &recipient, QString *why) -> Key {
  QString id = recipient.trimmed();
  if (id.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
    id = id.mid(2).trimmed();
  }
  if (id.isEmpty()) {
    *why = QStringLiteral("Empty recipient");
    return nullptr;
  }
  static const QRegularExpression hex(QStringLiteral("^[0-9A-Fa-f]+$"));
  QString compact = id;
  compact.remove(QLatin1Char(' '));
  const bool isHex = hex.match(compact).hasMatch();
  const char *type = nullptr;
  if (isHex && compact.size() == 40) {
    type = "fingerprint";
  } else if (isHex && compact.size() == 16) {
    type = "keyid";
  }
  if (type != nullptr) {
    rnp_key_handle_t key = nullptr;
    rnp_locate_key(ffi, type, compact.toUpper().toLatin1().constData(), &key);
    if (key == nullptr) {
      *why = QStringLiteral("No public key for %1").arg(recipient);
    }
    return Key(key);
  }

  QStringList primaries;
  if (isHex && compact.size() == 8) {
    // A short key ID: the last eight digits of a key's (or subkey's) ID.
    const QString shortId = compact.toUpper();
    primaries = matchingPrimaries(ffi, [&shortId](rnp_key_handle_t key) {
      char *keyId = nullptr;
      if (rnp_key_get_keyid(key, &keyId) != RNP_SUCCESS) {
        return false;
      }
      const bool hit = QString::fromLatin1(keyId).endsWith(shortId);
      rnp_buffer_destroy(keyId);
      return hit;
    });
  } else {
    // The whole user ID first, then part of one (gpg -r takes both; a
    // .gpg-id often lists just the e-mail address). Both through
    // matchingPrimaries, so an expired key is skipped and two keys with the
    // same user ID (after a key rotation, say) are an error, not a pick.
    primaries = matchingPrimaries(ffi, [&id](rnp_key_handle_t key) {
      return anyUserIdMatches(key,
                              [&id](const QString &uid) { return uid == id; });
    });
    if (primaries.isEmpty()) {
      primaries = matchingPrimaries(ffi, [&id](rnp_key_handle_t key) {
        return anyUserIdMatches(key, [&id](const QString &uid) {
          return uid.contains(id, Qt::CaseInsensitive);
        });
      });
    }
  }
  if (primaries.size() > 1) {
    *why = QStringLiteral("%1 matches more than one key").arg(recipient);
    return nullptr;
  }
  if (primaries.isEmpty()) {
    *why = QStringLiteral("No public key for %1").arg(recipient);
    return nullptr;
  }
  rnp_key_handle_t key = nullptr;
  rnp_locate_key(ffi, "fingerprint", primaries.first().toLatin1().constData(),
                 &key);
  return Key(key);
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
    QString why;
    const Key key = locateKey(m_ffi, recipient, &why);
    if (!key) {
      setError(error, why);
      return {};
    }
    result = rnp_op_encrypt_add_recipient(op, key.get());
    if (result != RNP_SUCCESS) {
      setError(error,
               rnpError(QStringLiteral("Cannot encrypt to %1").arg(recipient),
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
