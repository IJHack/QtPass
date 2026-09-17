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
   * @param signingKeys Full 40-hex fingerprints from the settings, as pass's
   *        PASSWORD_STORE_SIGNING_KEY requires: gpg reports fingerprints in
   *        its KEY_CONSIDERED and VALIDSIG status lines, so a short key ID
   *        would sign (via --default-key) but never pass haveSecretKey() or
   *        verify(). The first one signs, any of them verifies.
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
   * @brief Write `<file>.sig`, a detached signature by the first key.
   *
   * Only the first key is used: repeated `--default-key` options override
   * each other and only the last would count.
   * @param gpgIdFile The `.gpg-id` to sign.
   * @param error Receives gpg's stderr when it fails.
   * @return true when gpg exited with 0. Does nothing and returns true when no
   *         key is configured.
   */
  auto sign(const QString &gpgIdFile, QString *error = nullptr) const -> bool;

  /**
   * @brief Check `<file>.sig` against @p gpgIdFile: gpg must report a
   * VALIDSIG whose key or primary-key fingerprint is one of the configured
   * keys.
   * @param gpgIdFile The `.gpg-id`.
   * @return true when the signature is good, or when no key is configured.
   */
  auto verify(const QString &gpgIdFile) const -> bool;

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
           QString *err = nullptr) const -> int;
};

#endif // SRC_GPGIDSIGNER_H_
