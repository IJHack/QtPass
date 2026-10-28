// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_REALPASS_H_
#define SRC_REALPASS_H_

#include "nativegrep.h"
#include "pass.h"

class GpgIdSigner;
#include <QQueue>
#include <QSet>

/**
 * @class RealPass
 * @brief Implementation of Pass that wraps the 'pass' command-line tool.
 *
 * RealPass delegates all password store operations to the external 'pass'
 * utility. It provides a Qt-native interface while handling:
 * - Git integration (init, pull, push)
 * - Password CRUD (show, insert, remove, move, copy)
 * - OTP generation
 * - Store initialization with GPG keys
 *
 * This is the primary implementation when 'pass' is available on the system.
 */
class RealPass : public Pass {
  /// The store's own search: `pass grep` runs `find -L`, which follows a
  /// link out of the store and decrypts what it finds there.
  NativeGrep m_grep;
  /// A linked folder whose removal commit is queued, with the store it
  /// was queued for: the profile may change before the commit ends.
  struct LinkCommit {
    QString rel;            ///< The folder, relative to the store.
    QString passExecutable; ///< The pass that runs the commit.
    /// Its environment (PASSWORD_STORE_DIR). Not "environ": a macro on
    /// Windows (stdlib.h).
    QProcessEnvironment environment;
  };
  /// Queued linked-folder removal commits (as GIT_COMMIT), oldest first;
  /// finished() judges each.
  QQueue<LinkCommit> m_linkCommits;

  /**
   * @brief Judge a queued linked-folder removal commit by what is left
   * staged in the store it ran in: `pass git` exits 0 whatever git did.
   * @param commit The folder and the store it was queued for.
   * @param out The commit's standard output.
   * @param err The commit's standard error.
   */
  void finishLinkCommit(const LinkCommit &commit, const QString &out,
                        const QString &err);

  /**
   * @brief Execute pass command with arguments.
   * @param id Process identifier.
   * @param args Command arguments.
   * @param input Input to pass to stdin.
   * @param readStdout Capture stdout.
   * @param readStderr Capture stderr.
   */
  void executePass(PROCESS id, const QStringList &args,
                   QString input = QString(), bool readStdout = true,
                   bool readStderr = true);
  /**
   * @brief Shared implementation of Move and Copy via `pass mv`/`pass cp`.
   * @param id Process id (PASS_MOVE or PASS_COPY).
   * @param subcommand The pass subcommand, "mv" or "cp".
   * @param src Source path.
   * @param dest Destination path.
   * @param force Overwrite an existing destination; without it a
   *        file-onto-file request is a no-op (pass would otherwise force).
   */
  void passMoveOrCopy(PROCESS id, const QString &subcommand, const QString &src,
                      const QString &dest, bool force);
  /**
   * @brief @p path as `pass` names it: relative to the store, without the
   * `.gpg` pass appends itself.
   * @param path The file or folder, absolute or relative to the working
   *        directory.
   * @param stripGpg Whether to drop a trailing `.gpg`.
   * @return The store-relative name.
   */
  auto passName(const QString &path, bool stripGpg) const -> QString;

  /**
   * @brief Whether the failed `pass insert` wrote its entry before its git
   * step failed. pass does not say, so: the entry's bytes differ from what
   * they were when that insert started to run (a new entry appeared, or an
   * existing one was rewritten), and pass did not say that the encryption
   * broke off (gpg can leave a partial file behind then).
   * @param err What pass reported on stderr.
   * @return true when the entry was written.
   */
  [[nodiscard]] auto insertWritten(const QString &err) const -> bool override;

  /**
   * @brief With signing on, refuse to have pass encrypt @p file when the
   * signed list it would use is not one QtPass accepts: its folder's own
   * list was passed over, the signature does not verify, or the list is
   * older than one this device accepted or signed for another folder.
   * @param file The entry (or the place a move or copy writes to).
   * @return Whether it was refused (and the user told).
   */
  auto refuseUnverifiedList(const QString &file) -> bool;

  /**
   * @brief For a folder pass is about to re-encrypt in a move or copy:
   * refuse when the signed list of one of its entries (every one pass's
   * walk takes, hidden ones included) is not one QtPass accepts where it
   * lies now (as refuseUnverifiedList()).
   * @param dir The folder moved or copied, or the existing folder written
   *        into.
   * @param onlyListsWithin Whether only lists inside @p dir count: for the
   *        folder that moves, whose entries get the destination's list in
   *        place of any list above it.
   * @return Whether it was refused (and the user told).
   */
  auto refuseUnverifiedTree(const QString &dir, bool onlyListsWithin) -> bool;

  /**
   * @brief The verifier for signed lists, gpg in the store's environment.
   * @return A GpgIdSigner with the configured signing keys.
   */
  auto listSigner() -> GpgIdSigner;

  /**
   * @brief The checks of refuseUnverifiedList() for @p file's nearest list,
   * once per list.
   * @param file The entry.
   * @param signer The verifier.
   * @param verified The lists already found good; extended.
   * @param within When set, only a list under this folder (with a trailing
   *        "/") is verified; the skipped-list check applies regardless.
   * @return Whether it was refused (and the user told).
   */
  auto refuseList(const QString &file, const GpgIdSigner &signer,
                  QSet<QString> &verified, const QString &within = QString())
      -> bool;

  /**
   * @brief The executor is about to run (or drop) its next item: for an
   * insert, take its entry from m_insertQueue and fingerprint it now, not
   * when it was queued, so an earlier queued save of the same entry counts
   * as "before". Kept in m_insertPending for that insert's own completion.
   * @param id The item's process id.
   * @param completes Whether a completion follows (not for a dropped one).
   */
  void onDequeuing(int id, bool completes);

  /**
   * @brief A SHA-256 of the regular file @p path (opened without following
   * a link or blocking on a FIFO).
   * @param path The entry's file.
   * @return The hash; empty when @p path is no readable regular file.
   */
  static auto entryFingerprint(const QString &path) -> QByteArray;

  /// An insert's entry and its fingerprint when the insert started to run
  /// (empty when the entry did not exist).
  struct InsertRecord {
    QString target;    ///< The entry's file.
    QByteArray before; ///< Its fingerprint when the insert started.
  };
  /// The entries of the queued inserts, oldest first.
  QQueue<QString> m_insertQueue;
  /// The inserts that left the queue and have yet to complete, in the
  /// order their completions come: a failed start's error is posted, and
  /// the next insert may start before it arrives.
  QQueue<InsertRecord> m_insertPending;
  /// The insert whose completion is being handled.
  InsertRecord m_insertDone;

public:
  /**
   * @brief Construct a RealPass instance.
   */
  RealPass();
  /**
   * @brief Destructor; lets the search workers wind down.
   */
  ~RealPass() override;

  // Git operations
  /**
   * @brief Initialize Git repository in password store.
   */
  void GitInit() override;
  /**
   * @brief Pull changes from remote.
   */
  void GitPull() override;
  /**
   * @brief Pull with rebase.
   */
  void GitPull_b() override;
  /**
   * @brief Push changes to remote.
   */
  void GitPush() override;

  // Password operations
  /**
   * @brief Show decrypted password.
   * @param file Path to password file.
   */
  void Show(QString file) override;
  /**
   * @brief Insert new password.
   * @param file Path to password file.
   * @param newValue Password content.
   * @param overwrite true to overwrite existing.
   */
  void Insert(QString file, QString newValue, bool overwrite) override;
  /**
   * @brief Remove password or directory.
   * @param file Path to remove.
   * @param isDir true if removing directory.
   */
  void Remove(QString file, bool isDir) override;
  /**
   * @brief Initialize password store.
   * @param path Store root path.
   * @param users GPG recipients.
   */
  void Init(QString path, const QList<UserInfo> &users) override;

protected:
  /**
   * @brief Finish a process; a linked-folder removal commit (GIT_COMMIT,
   * which RealPass uses for nothing else) is judged by finishLinkCommit().
   * @param id Process identifier.
   * @param exitCode Exit code.
   * @param out Standard output.
   * @param err Standard error.
   */
  void finished(int id, int exitCode, const QString &out,
                const QString &err) override;

  // Pass interface
public:
  /**
   * @brief Move password file or directory.
   * @param src Source path.
   * @param dest Destination path.
   * @param force Overwrite existing.
   */
  void Move(const QString src, const QString dest, const bool force) override;
  /**
   * @brief Copy password file or directory.
   * @param src Source path.
   * @param dest Destination path.
   * @param force Overwrite existing.
   */
  void Copy(const QString src, const QString dest, const bool force) override;
  /**
   * @brief Search all password content by GPG-decrypting each real `.gpg`
   * file, as ImitatePass does.
   *
   * Not `pass grep`: that enumerates with `find -L`, which follows a link
   * out of the store and decrypts what it finds there. Same pattern
   * semantics as ImitatePass::Grep (QRegularExpression).
   * @param pattern Search pattern (QRegularExpression).
   * @param caseInsensitive true for case-insensitive search.
   */
  void Grep(QString pattern, bool caseInsensitive) override;
};

#endif // SRC_REALPASS_H_
