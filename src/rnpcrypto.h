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
   * @brief Creates an empty keyring.
   */
  RnpCrypto();
  /**
   * @brief Frees the keyring and everything RNP holds for it.
   */
  ~RnpCrypto();
  RnpCrypto(const RnpCrypto &) = delete;
  auto operator=(const RnpCrypto &) -> RnpCrypto & = delete;

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
   * @brief Encrypts @p plaintext to every recipient, as one message.
   * @param plaintext The entry's content.
   * @param recipients Key IDs or fingerprints, as a .gpg-id lists them.
   * @param error Set to the reason when encryption fails, naming the first
   * recipient that has no usable encryption key.
   * @return The binary OpenPGP message, empty on failure.
   */
  auto encrypt(const QByteArray &plaintext, const QStringList &recipients,
               QString *error = nullptr) -> QByteArray;

  /**
   * @brief Decrypts an OpenPGP message with any secret key in the keyring.
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
};

#endif // SRC_RNPCRYPTO_H_
