// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_RNPCRYPTO_H_
#define SRC_RNPCRYPTO_H_

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <functional>

struct rnp_ffi_st;

/**
 * @class RnpCrypto
 * @brief OpenPGP encryption and decryption in-process, through RNP.
 *
 * A spike towards a backend that needs no gpg, git or pass binary (#2005):
 * the same work ImitatePass hands to gpg, done by the RNP library (the
 * OpenPGP code Thunderbird uses). Keys live in this object's own keyring,
 * filled from armored exports (gpg --export / --export-secret-keys); it never
 * reads or writes ~/.gnupg. Built only with CONFIG+=rnp.
 *
 * Output is binary OpenPGP, as pass and QtPass write .gpg files. Every call
 * reports failure through its return value and a human-readable error.
 *
 * Security trade-off (SECURITY.md, "In-process OpenPGP"): with gpg, QtPass
 * never holds a secret key; gpg-agent does, or a smartcard. Here the secret
 * key is imported into this process and, while a message is decrypted, the
 * unlocked key and the passphrase are in its memory. Imported keys keep their
 * own passphrase protection; no smartcard support. A mode built on this must
 * be chosen deliberately, never used as a silent fallback for a missing gpg.
 */
class RnpCrypto {
public:
  /**
   * @brief Asks for the passphrase of a secret key.
   *
   * Called with the key's 16-hex-digit key ID; return an empty string to
   * give up.
   */
  using PassphraseProvider = std::function<QString(const QString &keyId)>;

  /**
   * @brief Most bytes of plaintext decrypt() produces by default.
   *
   * An OpenPGP message is compressed, so a small entry in a shared store can
   * decompress to gigabytes; decrypt() fails at this many instead of filling
   * memory. The same limit QtPass gives gpg (--max-output).
   */
  static constexpr qint64 kMaxPlaintextBytes = 64LL * 1024 * 1024;

  /**
   * @brief Creates an empty keyring.
   */
  RnpCrypto();
  /**
   * @brief Frees the keyring and everything RNP holds for it.
   */
  ~RnpCrypto();
  // RNP holds a pointer to m_passphrase (the passphrase callback's context),
  // so the object must stay where it is: neither copied nor moved.
  RnpCrypto(const RnpCrypto &) = delete;
  auto operator=(const RnpCrypto &) -> RnpCrypto & = delete;
  RnpCrypto(RnpCrypto &&) = delete;
  auto operator=(RnpCrypto &&) -> RnpCrypto & = delete;

  /**
   * @brief Whether the RNP context could be created at all.
   * @return False only when RNP itself failed to initialise.
   */
  [[nodiscard]] auto isValid() const -> bool;

  /**
   * @brief Imports public and secret keys from armored or binary key data.
   * @param keyData Output of gpg --export or --export-secret-keys.
   * @param error Set to the reason when the import fails.
   * @return The primary key fingerprints imported, empty on failure.
   */
  auto importKeys(const QByteArray &keyData, QString *error = nullptr)
      -> QStringList;

  /**
   * @brief Sets how secret-key passphrases are obtained.
   * @param provider Called when decryption needs a locked key.
   */
  void setPassphraseProvider(PassphraseProvider provider);

  /**
   * @brief Sets the most bytes of plaintext decrypt() may produce.
   * @param bytes The limit; kMaxPlaintextBytes until set. A value that is
   * not positive is ignored and the limit stays as it was: there is no
   * "unlimited". One above 4 GiB - 1 is capped to that, as RNP keeps the
   * limit in 32 bits.
   */
  void setMaxPlaintextBytes(qint64 bytes);

  /**
   * @brief Encrypts @p plaintext to every recipient, as one message.
   * @param plaintext The entry's content.
   * @param recipients As a .gpg-id lists them, in the forms gpg -r takes: a
   * fingerprint (40 or 64 hex digits, blanks allowed), a long or short (8
   * hex digits) key ID with or without 0x, a whole user ID, or part of one
   * such as the e-mail address. Each must name exactly one key that can
   * encrypt (not expired or revoked, with a usable encryption key); an
   * empty recipient is an error.
   * @param error Set to the reason when encryption fails, naming the first
   * recipient that has no usable key or names more than one.
   * @return The binary OpenPGP message, empty on failure.
   */
  auto encrypt(const QByteArray &plaintext, const QStringList &recipients,
               QString *error = nullptr) -> QByteArray;

  /**
   * @brief Decrypts an OpenPGP message with any secret key in the keyring.
   *
   * Only an integrity-protected message is accepted (SEIPD with MDC, or
   * AEAD such as the OCB packets GnuPG 2.4 writes): one without (CFB with
   * no MDC, the EFAIL kind) is refused, as GnuPG has since 2.2.8. A
   * plaintext over the limit (setMaxPlaintextBytes()) fails too.
   * @param ciphertext Binary or armored OpenPGP message.
   * @param error Set to the reason when decryption fails.
   * @param ok Set to whether decryption succeeded, since an entry can be
   * empty.
   * @return The plaintext, empty on failure.
   */
  auto decrypt(const QByteArray &ciphertext, QString *error = nullptr,
               bool *ok = nullptr) -> QByteArray;

private:
  rnp_ffi_st *m_ffi = nullptr;
  PassphraseProvider m_passphrase;
  qint64 m_maxPlaintext = kMaxPlaintextBytes;
};

#endif // SRC_RNPCRYPTO_H_
