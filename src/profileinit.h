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

private:
  ProfileInit() = default;
  static auto writeGpgId(const QString &gpgIdFile, const QList<UserInfo> &users,
                         QString *note) -> bool;
  static auto signGpgId(const QString &gpgIdFile, const AppSettings &s,
                        QString *note) -> bool;
  static auto commitGpgId(const QString &dir, const QString &gpgIdFile,
                          const QString &sigFile, const AppSettings &s,
                          QString *note) -> bool;
};

#endif // SRC_PROFILEINIT_H_
