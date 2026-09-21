// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_GPGIDSIGNER_H_
#define SRC_GPGIDSIGNER_H_

#include <QString>
#include <QStringList>
#include <functional>

/**
 * @class GpgIdSigner
 * @brief The detached signature on a store's `.gpg-id`: whether the
 * configured signing key is usable, signing the file, and checking that an
 * existing `.gpg-id.sig` was made by one of the configured keys.
 *
 * Every gpg run goes through the @c Exec given at construction, so the
 * re-encryption worker can hand in its cancellable runner and tests a fake.
 * With no signing keys configured signing is off and verification passes:
 * `pass` itself only checks signatures when `PASSWORD_STORE_SIGNING_KEY` is
 * set.
 */
class GpgIdSigner {
public:
  /**
   * @brief Blocking process runner: Executor::executeBlocking()'s shape.
   * @return The exit code, or -1 when the process could not run.
   */
  using Exec =
      std::function<int(const QString &app, const QStringList &args,
                        const QString &input, QString *out, QString *err)>;

  /**
   * @brief Create a signer.
   * @param gpgExecutable The gpg binary; a `wsl ` prefix routes file paths
   *        through wslpath.
   * @param signingKeys Full fingerprints from the settings (40 hex characters
   *        for v4 keys, 64 for v5/v6), as pass's PASSWORD_STORE_SIGNING_KEY
   *        requires: gpg reports fingerprints in its KEY_CONSIDERED and
   *        VALIDSIG status lines, so a short key ID would sign (via
   *        --default-key) but never pass haveSecretKey() or verify(). The
   *        first one signs, any of them verifies.
   * @param exec Process runner; Executor::executeBlocking() when empty.
   */
  GpgIdSigner(QString gpgExecutable, QStringList signingKeys,
              Exec exec = Exec());

  /**
   * @brief Split the settings value into keys, upper-cased the way gpg
   * prints fingerprints.
   * @param passSigningKey The space-separated setting.
   * @return The keys, without empties.
   */
  static auto keysFromSetting(const QString &passSigningKey) -> QStringList;

  /**
   * @brief Whether any signing key is configured at all.
   * @return true when sign() will do something.
   */
  auto enabled() const -> bool { return !m_keys.isEmpty(); }

  /**
   * @brief The keys this signer was given.
   * @return The list from the constructor.
   */
  auto keys() const -> const QStringList & { return m_keys; }

  /**
   * @brief Whether gpg has the secret key that sign() will use, i.e. the
   * first configured key.
   * @return true when signing can work; false when no key is configured.
   */
  auto haveSecretKey() const -> bool;

  /**
   * @brief Write `<file>.sig`, a detached signature by the first key over
   * @p contents, the bytes the caller just wrote as @p gpgIdFile.
   *
   * gpg signs what it is fed on stdin, not what is under the file's name by
   * the time it reads it (a co-writer of the store could have swapped the
   * list between the write and the signing, and the user's key would have
   * vouched for theirs), and writes the signature into a directory of
   * QtPass's own; the signature then goes next to the list through
   * Util::writeFileReplacing(), never by opening the `.sig` name.
   *
   * Only the first key is used: repeated `--default-key` options override
   * each other and only the last would count.
   * @param gpgIdFile The `.gpg-id` the signature is for; `<gpgIdFile>.sig`
   *        is written.
   * @param contents The exact bytes of that file, as written. UTF-8 text,
   *        as a recipient list is; anything else is refused, as verify()
   *        refuses it.
   * @param error Receives gpg's stderr, or the reason, when it fails.
   * @return true when the signature is in place. Does nothing and returns
   *         true when no key is configured.
   */
  auto sign(const QString &gpgIdFile, const QByteArray &contents,
            QString *error = nullptr) const -> bool;

  /**
   * @brief Check a detached signature against the given bytes: gpg must
   * report a VALIDSIG whose key or primary-key fingerprint is one of the
   * configured keys.
   *
   * The data goes to gpg on stdin, so the caller verifies exactly the bytes
   * it is about to use as the recipient list; verifying a path and reading
   * it again afterwards would let anyone who can write to the store swap
   * the file in between (#1842).
   * @param contents The `.gpg-id` bytes.
   * @param signatureFile The `.gpg-id.sig` next to it.
   * @return true when the signature is good, or when no key is configured.
   */
  auto verify(const QByteArray &contents, const QString &signatureFile) const
      -> bool;

  /**
   * @brief Read @p gpgIdFile and verify it against `<file>.sig`.
   * @param gpgIdFile The `.gpg-id`.
   * @param contents Receives the bytes that were verified, so the caller can
   *        parse those and nothing else.
   * @return true when the signature is good, or when no key is configured.
   *         false when the file cannot be read.
   */
  auto verifyFile(const QString &gpgIdFile, QByteArray *contents) const -> bool;

  /**
   * @brief Pick the fingerprints out of gpg's `--status-fd` VALIDSIG line.
   * @param statusOutput gpg's status output.
   * @return The signing key's fingerprint and the primary key's fingerprint,
   *         or an empty list when there is no VALIDSIG.
   */
  static auto validSigFingerprints(const QString &statusOutput) -> QStringList;

private:
  QString m_gpg;
  QStringList m_keys;
  Exec m_exec;

  auto run(const QStringList &args, QString *out = nullptr,
           QString *err = nullptr, const QString &input = QString()) const
      -> int;
};

#endif // SRC_GPGIDSIGNER_H_
