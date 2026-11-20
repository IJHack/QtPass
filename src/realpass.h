// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief RealPass, the backend that hands every store operation to the `pass`
 * command-line tool.
 */
#ifndef SRC_REALPASS_H_
#define SRC_REALPASS_H_

#include "nativegrep.h"
#include "pass.h"

class GpgIdSigner;
#include <QQueue>
#include <QSet>

/**
 * @class RealPass
 * @ingroup backend
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
   * @brief Report a `pass init`, `mv` or `cp` that exited 0 having left
   * entries encrypted to the previous recipients: a critical message and
   * processErrorExit() instead of the completion signal, so the store is
   * not pushed as it now is (a later push still sends what pass committed).
   * @param pid PASS_INIT, PASS_MOVE or PASS_COPY.
   * @param out pass's standard output.
   * @param err pass's and gpg's standard error, status lines included.
   */
  void reportReencryptFailure(Enums::PROCESS pid, const QString &out,
                              const QString &err);
  /**
   * @brief Once the last queued init, move or copy has completed, make the
   * push asked for meanwhile (GitPush()). An automatic one is dropped while
   * a re-encryption in this store is held, and the status line says why; one
   * asked for by hand goes.
   */
  void sendDeferredPush();
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
   * @brief Refuse to have pass encrypt @p file when the list it would use
   * is a link (refuseLinkedList()), or, with signing on, not one QtPass
   * accepts: its folder's own list was passed over, the signature does not
   * verify, or the list is older than one this device accepted or signed
   * for another folder.
   * @param file The entry (or the place a move or copy writes to).
   * @return Whether it was refused (and the user told).
   */
  auto refuseUnverifiedList(const QString &file) -> bool;

  /**
   * @brief Refuse to have pass encrypt @p file when the walk to its list
   * meets a `.gpg-id` that is a link or junction (Pass::linkedGpgId()),
   * with or without a signing key: pass would follow it, while QtPass shows
   * the list of a folder above it.
   * @param file The entry or folder.
   * @param except A list about to be written (`pass init`'s own), not
   *        refused.
   * @return Whether it was refused (and the user told).
   */
  auto refuseLinkedList(const QString &file, const QString &except = QString())
      -> bool;

  /**
   * @brief Run a git command in the store through `pass git`, blocking.
   * @param args The git arguments (without `git`).
   * @param out Receives stdout; may be null.
   * @return true when `pass git` ran and exited 0 (git on).
   */
  auto storeGit(const QStringList &args, QString *out) -> bool override;

  /**
   * @brief The Git repository pass commits @p path in: the store's own, or
   * one a folder in it holds (pass looks for the repository from the
   * path's folder, as git does).
   * @param path An absolute path in the store; it need not exist yet.
   * @return The repository's folder relative to the store, empty for the
   *         store's own (or when git cannot say).
   */
  auto repositoryOf(const QString &path) -> QString;

  /**
   * @brief `git status --porcelain -z` through `pass git`, read: one
   * "XY name" per entry, a rename's old name dropped. Empty without Git, or
   * when git cannot say.
   * @param args Further status arguments (options, then a pathspec relative
   *        to @p repository).
   * @param repository The repository to ask, as repositoryOf() gives it;
   *        its entries are named from the store.
   * @return The entries.
   */
  auto statusEntries(const QStringList &args,
                     const QString &repository = QString()) -> QStringList;

  /**
   * @brief What git has not recorded at @p paths: `git status --porcelain`
   * lines, through `pass git`. Empty without Git, or when git cannot say.
   * @param paths Absolute paths in the store.
   * @param ignored Whether ignored files are listed too (as `!!`).
   * @return The status entries, "XY name" each.
   */
  auto unrecorded(const QStringList &paths, bool ignored) -> QStringList;

  /**
   * @brief Refuse a `pass init`, `mv`, `cp` or `insert` when @p paths hold
   * changes git has not recorded, or anything at all is staged in the
   * repository it commits in. pass stages the folder it wrote with
   * `git add` and then commits the whole index, so a plaintext export lying
   * in the folder or staged anywhere, or files a failed pull wrote, would be
   * committed and pushed along with the change. Ignored files are left
   * alone, as `git add` leaves them. When the changes include untracked
   * files a file manager writes by itself (`.DS_Store`, `Thumbs.db`), the
   * message says which `.gitignore` lines keep them out for good, for the
   * `.gitignore` of each repository that reports them (unless a deeper
   * rule there un-ignores them again).
   * @param paths Absolute paths in the store that pass will stage; empty
   *        for none (an insert stages its own entry alone).
   * @param committing Absolute paths pass commits: the index of each one's
   *        repository (see repositoryOf()) is checked.
   * @return Whether it was refused (and the user told).
   */
  auto refuseUnrecordedChanges(const QStringList &paths,
                               const QStringList &committing) -> bool;

  /**
   * @brief After a `pass insert`, `init`, `mv` or `cp` that exited 0, say
   * so when git did not record the entries it wrote: pass exits 0 whatever
   * its `git add` and commit did (an entry a .gitignore matches, say), and
   * an unrecorded entry is not pushed and a pull may overwrite it.
   * @param paths Absolute paths the change wrote.
   */
  void reportUnrecorded(const QStringList &paths);

  /**
   * @brief Refuse a `pass mv` or `cp` of the folder @p src when a file in
   * it that Git ignores would not be ignored where it lands: a rule naming
   * the old place (`/team/export.csv`) does not follow the file, and pass
   * stages the new path whole. Entries and recipient lists are not counted:
   * committing those is what pass is for.
   * @param src The folder moved or copied (anything else passes).
   * @param dest Where to, as handed to pass: an existing folder takes it in.
   * @return Whether it was refused (and the user told).
   */
  auto refuseIgnoredThatMoves(const QString &src, const QString &dest) -> bool;

  /**
   * @brief `git check-ignore --stdin -z` on @p paths through `pass git`.
   * @param repository Where to ask, as repositoryOf() gives it.
   * @param paths Paths relative to that repository.
   * @param verbose Whether to ask which rule matched (`-v`): four fields
   *        per ignored path (rule's file, line, pattern, path).
   * @param fields Receives the output's fields: the ignored paths, or the
   *        four fields each with @p verbose.
   * @return false when git could not say.
   */
  auto checkIgnore(const QString &repository, const QStringList &paths,
                   bool verbose, QStringList *fields) -> bool;

  /**
   * @brief For a folder pass is about to re-encrypt in a move or copy:
   * refuse when the signed list of one of its entries (every one pass's
   * walk takes, hidden ones included) is not one QtPass accepts where it
   * lies now (as refuseUnverifiedList()).
   * @param dir The folder moved or copied, or the existing folder written
   *        into. Each of its folders' lists counts, used by an entry yet or
   *        not: a copy merges into a folder of the same name, so lists on
   *        either side meet the other side's entries.
   * @param onlyListsWithin Whether only lists inside @p dir count: for the
   *        folder that moves, whose entries get the destination's list in
   *        place of any list above it.
   * @param except A list about to be written (`pass init`'s own), neither
   *        verified nor refused for being missing.
   * @return Whether it was refused (and the user told).
   */
  auto refuseUnverifiedTree(const QString &dir, bool onlyListsWithin,
                            const QString &except = QString()) -> bool;

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
   * @param within When set, only a list under this folder (absolute,
   *        cleaned, with a trailing "/") is verified; the skipped-list check
   *        applies regardless.
   * @param except A list about to be written, neither verified nor refused
   *        for being missing.
   * @return Whether it was refused (and the user told).
   */
  auto refuseList(const QString &file, const GpgIdSigner &signer,
                  QSet<QString> &verified, const QString &within = QString(),
                  const QString &except = QString()) -> bool;

  /**
   * @brief The executor is about to run (or drop) its next item: for an
   * insert, take its entry from m_insertQueue and fingerprint it now, not
   * when it was queued, so an earlier queued save of the same entry counts
   * as "before". Kept in m_insertPending for that insert's own completion.
   * For `pass init`, `mv` and `cp`, add `--status-fd 2` to that one
   * process's PASSWORD_STORE_GPG_OPTS, so finished() can tell whether every
   * entry was re-encrypted (reencryptFailed()), and move its scope from
   * m_reencryptScopes to m_reencryptPending (dropped when it does not
   * complete).
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
  /// What each queued init, move or copy re-encrypts (its folder, or the
  /// destination), oldest first, for the push hold.
  QQueue<QString> m_reencryptScopes;
  /// The scopes of the inits, moves and copies that left the queue and have
  /// yet to complete, in completion order; one the executor drops never
  /// gets here, so it cannot pair with a later command's completion.
  QQueue<QString> m_reencryptPending;
  /// A push was asked for while an init, move or copy was queued or
  /// running: sendDeferredPush() makes it once they have completed.
  bool m_pushAfterReencrypt = false;
  /// One of those pushes was asked for by hand: it goes despite a hold.
  bool m_deferredPushByHand = false;

public:
  /**
   * @brief Construct a RealPass instance.
   */
  RealPass();
  /**
   * @brief Whether a `pass init`, `mv` or `cp` that exited 0 left entries
   * as they were: pass carries on past an entry gpg could not re-encrypt.
   *
   * Judged by gpg's status lines (onDequeuing() adds `--status-fd 2`),
   * which no locale changes: an INV_RECP, FAILURE or DECRYPTION_FAILED
   * line, or fewer END_ENCRYPTION lines than the entries pass said it was
   * re-encrypting ("name: reencrypting to ..." on stdout). KEYEXPIRED and
   * NO_SECKEY alone are not failures.
   * @param out pass's standard output.
   * @param err pass's and gpg's standard error.
   * @return true on a failure; false without any status line, since then
   *         the option did not reach gpg and there is nothing to judge by.
   */
  static auto reencryptFailed(const QString &out, const QString &err) -> bool;
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
   * @brief Remove an entry or folder with `pass rm`.
   *
   * A folder that is a link is unlinked here instead, without deleting its
   * target; that removal is committed only when Git is enabled and the
   * tracking lookup and index removal both report success.
   * @param file Store-relative entry path, without `.gpg` for a password
   *        file.
   * @param isDir true to remove a directory; false to remove a password
   *        file.
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
  /**
   * @brief Stop the running search (NativeGrep::cancel()).
   */
  void CancelGrep() override { m_grep.cancel(); }
};

#endif // SRC_REALPASS_H_
