// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_PROFILEINIT_H_
#define SRC_PROFILEINIT_H_

#include <QCoreApplication>
#include <QList>
#include <QString>

struct AppSettings;
struct UserInfo;

/**
 * @class ProfileInit
 * @brief Turn a directory into a password store without going through the
 *        Pass backend of the currently active store.
 *
 * The backend is a shared singleton bound to the active store (settings
 * snapshot, PASSWORD_STORE_DIR, one FIFO of asynchronous processes), so a
 * new profile cannot be initialised through it without racing the active
 * store (#1774). Everything here is synchronous and touches only @p dir.
 */
class ProfileInit {
  Q_DECLARE_TR_FUNCTIONS(ProfileInit)

public:
  /**
   * @brief Check if a profile path needs initialization.
   * @param path The profile path to check.
   * @return true if the path exists but has no .gpg-id file.
   */
  static auto needsInit(const QString &path) -> bool;

  /**
   * @brief Write the recipients and, if wanted, put the folder under git.
   *
   * Writes `<dir>/.gpg-id` with the enabled key ids, signs it when
   * @p s.passSigningKey is set, and when @p useGit runs `git init`,
   * `git add` and `git commit` inside @p dir with blocking processes.
   * Existing `*.gpg` files are left alone: re-encrypting them needs the
   * backend, i.e. the profile has to be the active store; @p note says so.
   * @param dir Directory to initialise; created if missing.
   * @param users Key list; only entries with enabled == true are written.
   * @param s Settings for the gpg/git executables and the signing key.
   * @param useGit Whether to create a repository and commit the file.
   * @param note Receives a translated message for the user (empty on a quiet
   *             success): the error, or the warning about existing entries.
   * @return true when the .gpg-id (and signature/commit if requested) is in
   *         place.
   */
  static auto initialise(const QString &dir, const QList<UserInfo> &users,
                         const AppSettings &s, bool useGit, QString *note)
      -> bool;

  /**
   * @brief Put an existing store under Git: `git init`, stage the `.gpg`
   * entries and the `.gpg-id` files (nothing else that may lie around in the
   * folder) and make a first commit, with blocking processes of their own in
   * @p dir.
   * @param dir The store.
   * @param s Settings for the git executable.
   * @param note Receives a translated error on failure.
   * @return true when the repository exists and the commit is made.
   */
  static auto initGit(const QString &dir, const AppSettings &s, QString *note)
      -> bool;

  /**
   * @brief Whether git has a name and e-mail to commit with, i.e.
   * `user.name` and `user.email` resolve (globally or in @p dir).
   * @param dir Directory to ask in; a repository's own config counts.
   * @param s Settings for the git executable.
   * @return true when a commit would not fail for lack of an identity.
   */
  static auto gitIdentityConfigured(const QString &dir, const AppSettings &s)
      -> bool;

private:
  ProfileInit() = default;
  /**
   * @brief Write the enabled recipients to @p gpgIdFile, whole or not at
   * all; with @p signed_ the list carries the generation and folder header a
   * signed store's lists are checked against (GpgIdGeneration).
   * @param gpgIdFile The `.gpg-id` to write.
   * @param users The recipients; the enabled ones are written.
   * @param signed_ Whether the store gets a signing key, i.e. the header.
   * @param note Receives the reason on failure.
   * @param written Receives the exact bytes written, if not null.
   * @return Whether the file was written.
   */
  static auto writeGpgId(const QString &gpgIdFile, const QList<UserInfo> &users,
                         bool signed_, QString *note,
                         QByteArray *written = nullptr) -> bool;
  /**
   * @brief Sign the list just written as @p gpgIdFile, over the bytes
   * @p contents that were written.
   * @param gpgIdFile The `.gpg-id`; `<gpgIdFile>.sig` is written.
   * @param contents The bytes written as that file.
   * @param s The settings naming gpg and the signing key.
   * @param note Receives the reason on failure.
   * @return Whether the signature is in place.
   */
  static auto signGpgId(const QString &gpgIdFile, const QByteArray &contents,
                        const AppSettings &s, QString *note) -> bool;
  static auto commitGpgId(const QString &dir, const QString &gpgIdFile,
                          const QString &sigFile, const AppSettings &s,
                          QString *note) -> bool;
};

#endif // SRC_PROFILEINIT_H_
