// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "rnpcrypto.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimeZone>
#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
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

/// An RNP output into memory, failing once more than @p maxBytes are written
/// (0 for no limit).
auto memoryOutput(QString *error, const char *what, qint64 maxBytes = 0)
    -> Output {
  rnp_output_t output = nullptr;
  const rnp_result_t result =
      rnp_output_to_memory(&output, static_cast<size_t>(maxBytes));
  if (result != RNP_SUCCESS) {
    setError(error, rnpError(what, result));
    return nullptr;
  }
  return Output(output);
}

/// What was written to @p output; nothing when RNP cannot hand it over.
auto outputBytes(rnp_output_t output) -> std::optional<QByteArray> {
  uint8_t *buf = nullptr;
  size_t len = 0;
  if (rnp_output_memory_get_buf(output, &buf, &len, false) != RNP_SUCCESS) {
    return std::nullopt;
  }
  return QByteArray(reinterpret_cast<const char *>(buf),
                    static_cast<qsizetype>(len));
}

/// Whether @p key may be used: not expired, not revoked.
auto isValidKey(rnp_key_handle_t key) -> bool {
  bool valid = false;
  return rnp_key_is_valid(key, &valid) == RNP_SUCCESS && valid;
}

auto allowsEncryption(rnp_key_handle_t key) -> bool {
  bool allowed = false;
  return rnp_key_allows_usage(key, "encrypt", &allowed) == RNP_SUCCESS &&
         allowed;
}

/**
 * Whether a message can be encrypted to the primary key @p key, as gpg -r
 * judges a candidate: the key is valid and so is a key of it that may
 * encrypt (the primary itself, or a subkey). A valid primary whose
 * encryption subkeys have all expired or been revoked does not count.
 */
auto canEncryptTo(rnp_key_handle_t key) -> bool {
  if (!isValidKey(key)) {
    return false;
  }
  if (allowsEncryption(key)) {
    return true;
  }
  size_t subkeys = 0;
  rnp_key_get_subkey_count(key, &subkeys);
  for (size_t i = 0; i < subkeys; ++i) {
    rnp_key_handle_t handle = nullptr;
    if (rnp_key_get_subkey_at(key, i, &handle) != RNP_SUCCESS ||
        handle == nullptr) {
      continue;
    }
    const Key subkey(handle);
    if (isValidKey(handle) && allowsEncryption(handle)) {
      return true;
    }
  }
  return false;
}

/**
 * The key of the primary @p key that signs, as gpg --local-user picks it:
 * a valid key that may sign and whose secret part is here (the primary
 * itself, or a subkey). Null when there is none.
 */
auto signingKeyOf(rnp_key_handle_t key) -> Key {
  if (!isValidKey(key)) {
    return nullptr;
  }
  rnp_key_handle_t handle = nullptr;
  if (rnp_key_get_default_key(key, "sign", 0, &handle) != RNP_SUCCESS ||
      handle == nullptr) {
    return nullptr;
  }
  Key signer(handle);
  bool secret = false;
  if (!isValidKey(handle) ||
      rnp_key_have_secret(handle, &secret) != RNP_SUCCESS || !secret) {
    return nullptr;
  }
  return signer;
}

/// What a key is looked up for: what makes a primary key a candidate.
enum class Usage { Encrypt, Sign };

auto usableFor(rnp_key_handle_t primary, Usage usage) -> bool {
  return usage == Usage::Encrypt ? canEncryptTo(primary)
                                 : signingKeyOf(primary) != nullptr;
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
 * Fingerprints of the primary keys usable for @p usage (usableFor()) for
 * which @p match holds, given each key in the keyring (primaries and
 * subkeys) in turn.
 */
template <typename Match>
auto matchingPrimaries(rnp_ffi_t ffi, Usage usage, Match match) -> QStringList {
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
    if (usableFor(primaryHandle, usage)) {
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
auto locateKey(rnp_ffi_t ffi, const QString &recipient, QString *why,
               Usage usage = Usage::Encrypt) -> Key {
  const QString id = recipient.trimmed();
  if (id.isEmpty()) {
    setError(why, QStringLiteral("Empty recipient"));
    return nullptr;
  }
  // A key ID or fingerprint: hex digits after an optional 0x, which marks
  // one only when hex follows (gpg reads "0xAlice" as a user ID, and so
  // does this). Blanks (spaces or tabs) between groups are allowed in a
  // fingerprint only, as gpg allows them: "dead beef" is a user ID.
  static const QRegularExpression blanks(QStringLiteral("\\s+"));
  static const QRegularExpression hex(QStringLiteral("^[0-9A-Fa-f]+$"));
  const auto withoutPrefix = [](const QString &text) {
    return text.startsWith(QLatin1String("0x"), Qt::CaseInsensitive) &&
                   hex.match(text.mid(2)).hasMatch()
               ? text.mid(2)
               : text;
  };
  QString fingerprint = id;
  fingerprint.remove(blanks);
  fingerprint = withoutPrefix(fingerprint);
  const QString keyId = withoutPrefix(id);
  if (keyId.compare(QLatin1String("0x"), Qt::CaseInsensitive) == 0) {
    setError(why, QStringLiteral("Empty recipient"));
    return nullptr;
  }
  const bool isHexId = hex.match(keyId).hasMatch();
  const char *type = nullptr;
  QString lookup;
  // 40 digits for a v4 key, 64 for v5 and v6 (RFC 9580).
  if (hex.match(fingerprint).hasMatch() &&
      (fingerprint.size() == 40 || fingerprint.size() == 64)) {
    type = "fingerprint";
    lookup = fingerprint;
  } else if (isHexId && keyId.size() == 16) {
    type = "keyid";
    lookup = keyId;
  }
  if (type != nullptr) {
    rnp_key_handle_t handle = nullptr;
    rnp_locate_key(ffi, type, lookup.toUpper().toLatin1().constData(), &handle);
    if (handle == nullptr) {
      setError(why, (usage == Usage::Encrypt
                         ? QStringLiteral("No public key for %1")
                         : QStringLiteral("No secret key for %1"))
                        .arg(recipient));
      return nullptr;
    }
    Key key(handle);
    // Named exactly, so no other key is tried; say why it cannot be used
    // rather than leaving RNP's generic error for later.
    const QString primary = primaryFingerprint(handle);
    rnp_key_handle_t primaryHandle = nullptr;
    rnp_locate_key(ffi, "fingerprint", primary.toLatin1().constData(),
                   &primaryHandle);
    const Key primaryKey(primaryHandle);
    if (primaryHandle == nullptr || !usableFor(primaryHandle, usage)) {
      setError(why,
               (usage == Usage::Encrypt
                    ? QStringLiteral("The key %1 cannot be encrypted to: it "
                                     "is expired or revoked, or has no valid "
                                     "encryption subkey")
                    : QStringLiteral("The key %1 cannot sign: it is expired "
                                     "or revoked, has no valid signing key, "
                                     "or its secret key is not here"))
                   .arg(recipient));
      return nullptr;
    }
    return key;
  }

  QStringList primaries;
  if (isHexId && keyId.size() == 8) {
    // A short key ID: the last eight digits of a key's (or subkey's) ID.
    const QString shortId = keyId.toUpper();
    primaries = matchingPrimaries(ffi, usage, [&shortId](rnp_key_handle_t key) {
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
    primaries = matchingPrimaries(ffi, usage, [&id](rnp_key_handle_t key) {
      return anyUserIdMatches(key,
                              [&id](const QString &uid) { return uid == id; });
    });
    if (primaries.isEmpty()) {
      primaries = matchingPrimaries(ffi, usage, [&id](rnp_key_handle_t key) {
        return anyUserIdMatches(key, [&id](const QString &uid) {
          return uid.contains(id, Qt::CaseInsensitive);
        });
      });
    }
  }
  if (primaries.size() > 1) {
    setError(why,
             QStringLiteral("%1 matches more than one key").arg(recipient));
    return nullptr;
  }
  if (primaries.isEmpty()) {
    setError(why, (usage == Usage::Encrypt
                       ? QStringLiteral("No usable public key for %1")
                       : QStringLiteral("No usable secret key for %1"))
                      .arg(recipient));
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

void RnpCrypto::setMaxPlaintextBytes(qint64 bytes) {
  // RNP reads 0 as "no limit", and a negative value would wrap to a huge
  // size_t: either would switch the guard off without a word. RNP 0.17 and
  // 0.18 also keep the limit in 32 bits, so 4 GiB would become 0 (no limit)
  // and 4 GiB + 5 become 5: anything wider is capped to what fits.
  if (bytes > 0) {
    m_maxPlaintext =
        std::min<qint64>(bytes, std::numeric_limits<quint32>::max());
  }
}

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
  QJsonParseError parseError{};
  const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
  rnp_buffer_destroy(json);
  const QJsonValue keysValue = doc.object().value(QStringLiteral("keys"));
  if (parseError.error != QJsonParseError::NoError || !keysValue.isArray()) {
    // The keys may well be in the keyring; only which ones is unknown.
    setError(error, QStringLiteral("Cannot read RNP's import report: %1")
                        .arg(parseError.error != QJsonParseError::NoError
                                 ? parseError.errorString()
                                 : QStringLiteral("no key list")));
    return {};
  }
  QStringList primaries;
  const QJsonArray keys = keysValue.toArray();
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
  std::optional<QByteArray> cipher = outputBytes(output.get());
  if (!cipher) {
    setError(error, QStringLiteral("Cannot encrypt: no output from RNP"));
    return {};
  }
  return *cipher;
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
  Output output = memoryOutput(error, "Cannot decrypt", m_maxPlaintext);
  if (!input || !output) {
    return {};
  }
  // Through a verify operation rather than rnp_decrypt(): only that reports
  // how the message was protected, and rnp_decrypt() returns success for
  // CFB without MDC, the kind EFAIL abuses.
  rnp_op_verify_t op = nullptr;
  rnp_result_t result =
      rnp_op_verify_create(&op, m_ffi, input.get(), output.get());
  if (result != RNP_SUCCESS) {
    setError(error, rnpError("Cannot decrypt", result));
    return {};
  }
  const std::unique_ptr<rnp_op_verify_st, rnp_result_t (*)(rnp_op_verify_t)>
      guard(op, rnp_op_verify_destroy);
  // A signature by a key not in this keyring does not make an entry
  // unreadable; pass does not check signatures on entries either.
  rnp_op_verify_set_flags(op, RNP_VERIFY_IGNORE_SIGS_ON_DECRYPT);
  result = rnp_op_verify_execute(op);
  if (result != RNP_SUCCESS) {
    setError(error, rnpError("Cannot decrypt", result));
    return {};
  }
  char *mode = nullptr;
  bool protectedOk = false;
  if (rnp_op_verify_get_protection_info(op, &mode, nullptr, &protectedOk) !=
          RNP_SUCCESS ||
      !protectedOk) {
    setError(error,
             QStringLiteral("Refusing a message without integrity "
                            "protection (%1)")
                 .arg(QString::fromLatin1(mode != nullptr ? mode : "unknown")));
    rnp_buffer_destroy(mode);
    return {};
  }
  rnp_buffer_destroy(mode);
  std::optional<QByteArray> plain = outputBytes(output.get());
  if (!plain) {
    setError(error, QStringLiteral("Cannot decrypt: no output from RNP"));
    return {};
  }
  if (ok != nullptr) {
    *ok = true;
  }
  return *plain;
}

namespace {

/// @p key's own fingerprint, primary or subkey.
auto fingerprintOf(rnp_key_handle_t key) -> QString {
  char *fpr = nullptr;
  if (rnp_key_get_fprint(key, &fpr) != RNP_SUCCESS) {
    return {};
  }
  QString result = QString::fromLatin1(fpr);
  rnp_buffer_destroy(fpr);
  return result;
}

auto isRevoked(rnp_key_handle_t key) -> bool {
  bool revoked = false;
  return rnp_key_is_revoked(key, &revoked) == RNP_SUCCESS && revoked;
}

auto haveSecret(rnp_key_handle_t key) -> bool {
  bool secret = false;
  return rnp_key_have_secret(key, &secret) == RNP_SUCCESS && secret;
}

/// The primary key of @p key (itself when it is one), as a handle.
auto primaryOf(rnp_ffi_t ffi, rnp_key_handle_t key) -> Key {
  const QString primary = primaryFingerprint(key);
  rnp_key_handle_t handle = nullptr;
  if (primary.isEmpty() ||
      rnp_locate_key(ffi, "fingerprint", primary.toLatin1().constData(),
                     &handle) != RNP_SUCCESS) {
    return nullptr;
  }
  return Key(handle);
}

} // namespace

auto RnpCrypto::listKeys(bool secretOnly) const -> QList<KeyInfo> {
  QList<KeyInfo> keys;
  if (m_ffi == nullptr) {
    return keys;
  }
  rnp_identifier_iterator_t it = nullptr;
  if (rnp_identifier_iterator_create(m_ffi, &it, "fingerprint") !=
      RNP_SUCCESS) {
    return keys;
  }
  const char *fpr = nullptr;
  while (rnp_identifier_iterator_next(it, &fpr) == RNP_SUCCESS &&
         fpr != nullptr) {
    rnp_key_handle_t handle = nullptr;
    if (rnp_locate_key(m_ffi, "fingerprint", fpr, &handle) != RNP_SUCCESS ||
        handle == nullptr) {
      continue;
    }
    const Key key(handle);
    bool primary = false;
    if (rnp_key_is_primary(handle, &primary) != RNP_SUCCESS || !primary) {
      continue;
    }
    KeyInfo info;
    info.fingerprint = fingerprintOf(handle);
    char *keyId = nullptr;
    if (rnp_key_get_keyid(handle, &keyId) == RNP_SUCCESS) {
      info.keyId = QString::fromLatin1(keyId);
      rnp_buffer_destroy(keyId);
    }
    size_t uids = 0;
    rnp_key_get_uid_count(handle, &uids);
    for (size_t i = 0; i < uids; ++i) {
      char *uid = nullptr;
      if (rnp_key_get_uid_at(handle, i, &uid) == RNP_SUCCESS) {
        info.userIds << QString::fromUtf8(uid);
        rnp_buffer_destroy(uid);
      }
    }
    uint32_t created = 0;
    if (rnp_key_get_creation(handle, &created) == RNP_SUCCESS) {
      info.created = QDateTime::fromSecsSinceEpoch(created, QTimeZone::UTC);
    }
    uint32_t lifetime = 0;
    if (rnp_key_get_expiration(handle, &lifetime) == RNP_SUCCESS &&
        lifetime > 0) {
      info.expires = info.created.addSecs(lifetime);
    }
    info.revoked = isRevoked(handle);
    bool expired = false;
    info.expired =
        rnp_key_is_expired(handle, &expired) == RNP_SUCCESS && expired;
    // A secret part may be a subkey's only (gpg --export-secret-subkeys).
    info.hasSecret = haveSecret(handle);
    size_t subkeys = 0;
    rnp_key_get_subkey_count(handle, &subkeys);
    for (size_t i = 0; i < subkeys && !info.hasSecret; ++i) {
      rnp_key_handle_t sub = nullptr;
      if (rnp_key_get_subkey_at(handle, i, &sub) == RNP_SUCCESS &&
          sub != nullptr) {
        const Key subkey(sub);
        info.hasSecret = haveSecret(sub);
      }
    }
    info.canEncrypt = canEncryptTo(handle);
    info.canSign = signingKeyOf(handle) != nullptr;
    if (!secretOnly || info.hasSecret) {
      keys << info;
    }
  }
  rnp_identifier_iterator_destroy(it);
  return keys;
}

auto RnpCrypto::sign(const QByteArray &data, const QString &signer,
                     QString *error) -> QByteArray {
  if (m_ffi == nullptr) {
    setError(error, QStringLiteral("RNP is not available"));
    return {};
  }
  // As gpg --local-user: exactly the key named, never another one that
  // happens to have a secret key here.
  QString why;
  const Key named = locateKey(m_ffi, signer, &why, Usage::Sign);
  if (!named) {
    setError(error, why);
    return {};
  }
  const Key primary = primaryOf(m_ffi, named.get());
  const Key signing = primary ? signingKeyOf(primary.get()) : nullptr;
  if (!signing) {
    setError(error, QStringLiteral("The key %1 cannot sign").arg(signer));
    return {};
  }
  Input input = memoryInput(data, error, "Cannot read the data to sign");
  Output output = memoryOutput(error, "Cannot sign");
  if (!input || !output) {
    return {};
  }
  rnp_op_sign_t op = nullptr;
  rnp_result_t result =
      rnp_op_sign_detached_create(&op, m_ffi, input.get(), output.get());
  if (result != RNP_SUCCESS) {
    setError(error, rnpError("Cannot sign", result));
    return {};
  }
  const std::unique_ptr<rnp_op_sign_st, rnp_result_t (*)(rnp_op_sign_t)> guard(
      op, rnp_op_sign_destroy);
  rnp_op_sign_set_armor(op, false);
  result = rnp_op_sign_add_signature(op, signing.get(), nullptr);
  if (result == RNP_SUCCESS) {
    // Unlocks the secret key through the passphrase provider.
    result = rnp_op_sign_execute(op);
  }
  if (result != RNP_SUCCESS) {
    setError(error, rnpError("Cannot sign", result));
    return {};
  }
  std::optional<QByteArray> signature = outputBytes(output.get());
  if (!signature || signature->isEmpty()) {
    setError(error, QStringLiteral("Cannot sign: no output from RNP"));
    return {};
  }
  return *signature;
}

auto RnpCrypto::verify(const QByteArray &data, const QByteArray &signature,
                       QString *error) -> QStringList {
  if (m_ffi == nullptr) {
    setError(error, QStringLiteral("RNP is not available"));
    return {};
  }
  Input input = memoryInput(data, error, "Cannot read the signed data");
  Input sigInput = memoryInput(signature, error, "Cannot read the signature");
  if (!input || !sigInput) {
    return {};
  }
  rnp_op_verify_t op = nullptr;
  rnp_result_t result =
      rnp_op_verify_detached_create(&op, m_ffi, input.get(), sigInput.get());
  if (result != RNP_SUCCESS) {
    setError(error, rnpError("Cannot verify", result));
    return {};
  }
  const std::unique_ptr<rnp_op_verify_st, rnp_result_t (*)(rnp_op_verify_t)>
      guard(op, rnp_op_verify_destroy);
  // Fails when no signature verifies; each one's own status says why.
  result = rnp_op_verify_execute(op);
  size_t count = 0;
  rnp_op_verify_get_signature_count(op, &count);
  for (size_t i = 0; i < count; ++i) {
    rnp_op_verify_signature_t sig = nullptr;
    if (rnp_op_verify_get_signature_at(op, i, &sig) != RNP_SUCCESS ||
        rnp_op_verify_signature_get_status(sig) != RNP_SUCCESS) {
      continue;
    }
    rnp_key_handle_t handle = nullptr;
    if (rnp_op_verify_signature_get_key(sig, &handle) != RNP_SUCCESS ||
        handle == nullptr) {
      continue;
    }
    const Key key(handle);
    const Key primary = primaryOf(m_ffi, handle);
    // The first good signature decides, as the first VALIDSIG does in
    // GpgIdSigner: by a revoked key (a leaked one, say) it counts for
    // nothing. RNP 0.17 and 0.18 already give such a signature a failed
    // status; checked here too, so that holds whatever RNP decides.
    if (isRevoked(handle) || !primary || isRevoked(primary.get())) {
      setError(error, QStringLiteral("The signature is by a revoked key (%1)")
                          .arg(primaryFingerprint(handle)));
      return {};
    }
    return {fingerprintOf(handle), fingerprintOf(primary.get())};
  }
  setError(error, result != RNP_SUCCESS ? rnpError("No valid signature", result)
                                        : QStringLiteral("No valid signature"));
  return {};
}
