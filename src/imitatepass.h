// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_IMITATEPASS_H_
#define SRC_IMITATEPASS_H_

#include "gpgidsigner.h"
#include "nativegrep.h"
#include "pass.h"
#include "simpletransaction.h"
#include <QQueue>
#include <QTemporaryDir>

#include <atomic>
#include <memory>

class QRegularExpression;
class QThread;

/**
 * @class ImitatePass
 * @brief Implementation that imitates 'pass' when the real tool is unavailable.
 *
 * ImitatePass provides a complete password store implementation using direct
 * GPG operations when 'pass' is not installed or not enabled. It handles:
 * - Direct GPG encryption/decryption
 * - Git operations via direct subprocess calls
 * - GPG key management and recipient lists
 * - File-based password storage (.gpg files)
 * - Re-encryption when recipients change
 *
 * This is used as a fallback when RealPass cannot be initialized.
 */
class ImitatePass : public Pass {
  Q_OBJECT

  friend class tst_util;
  friend class tst_imitatepass;

protected:
  /**
   * @brief The signer for this store's `.gpg-id`, bound to the configured gpg
   * and signing keys and to execBlocking(), so a verification on the
   * re-encryption worker can be interrupted like every other gpg run there.
   * @return A signer; cheap to build, valid while m_settings is.
   */
  auto gpgIdSigner() -> GpgIdSigner;
  /**
   * @brief Read a `.gpg-id`, verify its detached signature and parse the
   * recipients from the bytes that were verified.
   *
   * One read serves both the check and the parse, so a file swapped in
   * between cannot smuggle in a recipient the signature never covered.
   * @param gpgIdFile The `.gpg-id`.
   * @param recipients Receives the recipients; empty on failure.
   * @param why Receives, if not null, what to tell the user on failure: a
   *        bad signature, or a signed list older than one accepted before
   *        (GpgIdGeneration).
   * @return false when the file cannot be read, the signature is bad or the
   *         list is a rollback (with a signing key configured). true
   *         otherwise, also for an empty list; the caller decides what an
   *         empty list means.
   */
  auto loadVerifiedRecipients(const QString &gpgIdFile, QStringList *recipients,
                              QString *why = nullptr) -> bool;
  /**
   * @brief Write the enabled recipients to a .gpg-id, atomically: a write
   * that fails halfway (full disk, dead network share) leaves the previous
   * file in place instead of a truncated list that would then be signed
   * and encrypted to.
   * @param gpgIdFile Path to .gpg-id file.
   * @param users List of recipients.
   * @param written Receives the exact bytes written, if not null: what the
   *        signature is then made over.
   * @return true when the file was written; false after reporting through
   *         critical().
   */
  auto writeGpgIdFile(const QString &gpgIdFile, const QList<UserInfo> &users,
                      QByteArray *written = nullptr) -> bool;
  /**
   * @brief Sign a `.gpg-id` with the configured key and verify the result;
   * failures are reported through critical().
   * @param gpgIdFile Path to .gpg-id file.
   * @param contents The bytes just written as that file: the signature is
   *        made over these, not over whatever is under the name by then.
   * @return true on success, false on failure.
   */
  auto signGpgIdFile(const QString &gpgIdFile, const QByteArray &contents)
      -> bool;
  /**
   * @brief Stage a `.gpg-id` and, when given, its `.sig`, and commit both in
   * one commit, so no commit in the history has a recipient list without
   * the signature that covers it. Nothing is committed when neither file
   * changed.
   * @param gpgIdFile Absolute path of the `.gpg-id`.
   * @param gpgIdSigFile Absolute path of the `.gpg-id.sig`, or empty when no
   *        signing key is configured.
   * @param out Receives the concatenated stdout of the git commands.
   * @param err Receives the concatenated stderr of the git commands.
   * @return Exit code of the first failing git command, 0 on success.
   */
  auto gitAddGpgId(const QString &gpgIdFile, const QString &gpgIdSigFile,
                   QString *out, QString *err) -> int;
  /**
   * @brief Check whether git already tracks a file in the store.
   * @param file Absolute path inside the password store.
   * @return true when the file is in the git index.
   */
  auto gitTracks(const QString &file) -> bool;
  /**
   * @brief Recipients for the directory of @p file, from its verified
   * `.gpg-id`.
   * @param file Password file path.
   * @param verified Cache of `.gpg-id` path to the recipients its verified
   *        contents held; a hit skips gpg and reuses exactly that list.
   * @param gpgId Output parameter for recipient key IDs, sorted.
   * @return true on success, false when the file is unreadable or its
   *         signature is bad (reported through critical()).
   */
  auto verifyGpgIdForDir(const QString &file,
                         QHash<QString, QStringList> &verified,
                         QStringList &gpgId) -> bool;
  /**
   * @brief Create git backup commit before re-encryption.
   * @return true if backup created or not needed, false if backup failed.
   */
  auto createBackupCommit() -> bool;
  /**
   * @brief Read recipients from file.
   * @param fileName Path to file.
   * @return List of key IDs.
   */
  auto getKeysFromFile(const QString &fileName) -> QStringList;
  /**
   * @brief Re-encrypt single file with new recipients.
   * @param fileName File to re-encrypt.
   * @param recipients New recipient key IDs.
   * @return true on success, false on failure.
   */
  auto reencryptSingleFile(const QString &fileName,
                           const QStringList &recipients) -> bool;
  /**
   * @brief Resolve destination for move operation.
   * @param src Source path.
   * @param dest Destination path.
   * @param force Overwrite existing.
   * @return Resolved destination path.
   */
  auto resolveMoveDestination(const QString &src, const QString &dest,
                              bool force) -> QString;

public:
  /**
   * @brief Execute git move operation.
   * @param src Source path.
   * @param destFile Destination path.
   * @param force Overwrite existing.
   */
  void executeMoveGit(const QString &src, const QString &destFile, bool force);

  /**
   * @brief Commit changes to git.
   * @param file Changed file path.
   * @param msg Commit message.
   */
  void gitCommit(const QString &file, const QString &msg);

  /**
   * @brief Execute git command.
   * @param id Process identifier.
   * @param args Git arguments.
   * @param input Input to stdin.
   * @param readStdout Capture stdout.
   * @param readStderr Capture stderr.
   */
  void executeGit(PROCESS id, const QStringList &args,
                  QString input = QString(), bool readStdout = true,
                  bool readStderr = true);
  /**
   * @brief Git is enabled and an executable is configured.
   * @return true when git commands can actually run.
   */
  auto gitConfigured() const -> bool;
  /**
   * @brief gitConfigured() plus a status message when git is enabled but
   * no executable is configured.
   * @return true when git commands can actually run.
   */
  auto gitReady() -> bool;
  /**
   * @brief Execute GPG command.
   * @param id Process identifier.
   * @param args GPG arguments.
   * @param input Input to stdin.
   * @param readStdout Capture stdout.
   * @param readStderr Capture stderr.
   */
  void executeGpg(PROCESS id, const QStringList &args,
                  QString input = QString(), bool readStdout = true,
                  bool readStderr = true);

  /**
   * @class TransactionHelper
   * @brief RAII helper for wrapping operations in transactions.
   */
  class TransactionHelper {
    SimpleTransaction *m_transaction;
    PROCESS m_result;

  public:
    /**
     * @brief Start transaction.
     * @param trans Transaction object.
     * @param result Result code on commit.
     */
    TransactionHelper(SimpleTransaction *trans, PROCESS result)
        : m_transaction(trans), m_result(result) {
      m_transaction->transactionStart();
    }
    /**
     * @brief End transaction on destruction.
     */
    ~TransactionHelper() { m_transaction->transactionEnd(m_result); }
  };

protected:
  /**
   * @brief Handle process completion.
   * @param id Process identifier.
   * @param exitCode Process exit code.
   * @param out Standard output from the process.
   * @param err Standard error from the process.
   */
  void finished(int id, int exitCode, const QString &out,
                const QString &err) override;

  /**
   * @brief Open a transaction for every execution.
   *
   * In native (imitate) mode each wrapped command is a git/gpg transaction;
   * register it before the subprocess runs.
   * @param id Process identifier.
   */
  void beforeExecute(PROCESS id) override;

public:
  /**
   * @brief Construct ImitatePass instance.
   */
  ImitatePass();
  /**
   * @brief Destructor.
   */
  ~ImitatePass() override;

  // Git operations
  /**
   * @brief Initialize Git repository.
   */
  void GitInit() override;
  /**
   * @brief Pull from remote.
   */
  void GitPull() override;
  /**
   * @brief Pull with rebase.
   */
  void GitPull_b() override;
  /**
   * @brief Push to remote.
   */
  void GitPush() override;

  // Password operations
  /**
   * @brief Show decrypted password.
   * @param file Path to the password file relative to store root.
   */
  void Show(QString file) override;
  /**
   * @brief Insert new password.
   * @param file Path to the password file.
   * @param newValue Password content to store.
   * @param overwrite true to overwrite an existing file.
   */
  void Insert(QString file, QString newValue, bool overwrite) override;
  /**
   * @brief Remove password.
   * @param file Path to the file or directory to remove.
   * @param isDir true if removing a directory.
   */
  void Remove(QString file, bool isDir) override;
  /**
   * @brief Initialize store.
   * @param path Root path of the password store.
   * @param users List of recipient GPG keys.
   */
  void Init(QString path, const QList<UserInfo> &users) override;

  /**
   * @brief Re-encrypt entire directory.
   *
   * Emits startReencryptPath() synchronously, then runs the scan and the
   * per-file gpg calls on a worker thread. Progress is reported through
   * reencryptProgress(); per-file failures are collected and reported in a
   * single critical() once the run is over, followed by endReencryptPath().
   * A second call while a run is active is ignored.
   * @param dir Directory path.
   */
  void reencryptPath(const QString &dir);
  /**
   * @brief Ask a running reencryptPath() to stop.
   *
   * Only sets a flag. The worker starts no further gpg or git process, and
   * the one it is blocked on (gpg waiting on pinentry, say) is terminated by
   * the worker itself, and killed if it is still running after a short grace
   * period, so the cancel takes effect promptly rather than after the current
   * file. The store stays
   * consistent: reencryptSingleFile() only replaces a file once the new
   * ciphertext is verified, so an interrupted file is left untouched, or
   * already re-encrypted but not yet committed when git was the process that
   * was interrupted. The remaining files are left untouched and the
   * interrupted file is not reported as a failure. endReencryptPath() is
   * still emitted when the worker has stopped.
   */
  void cancelReencryptPath();

signals:
  /**
   * @brief Emitted before starting re-encryption.
   */
  void startReencryptPath();
  /**
   * @brief Emitted after finishing re-encryption.
   */
  void endReencryptPath();
  /**
   * @brief Re-encryption progress: @p current of @p total files checked.
   *
   * Emitted from the worker thread; connect with a queued (or automatic)
   * connection.
   * @param current Number of files checked so far, including this one.
   * @param total Total number of files found by the scan.
   */
  void reencryptProgress(int current, int total);

  // Pass interface
public:
  /**
   * @brief Move password file.
   * @param src Source path.
   * @param dest Destination path.
   * @param force true to overwrite existing destination.
   */
  void Move(const QString src, const QString dest, const bool force) override;
  /**
   * @brief Copy password file.
   * @param src Source path.
   * @param dest Destination path.
   * @param force true to overwrite existing destination.
   */
  void Copy(const QString src, const QString dest, const bool force) override;
  /**
   * @brief Search all password content by GPG-decrypting each .gpg file.
   *
   * Pattern is interpreted as a QRegularExpression (PCRE-like); RealPass::Grep
   * uses the same search, so both backends match alike.
   *
   * @param pattern Search pattern (QRegularExpression).
   * @param caseInsensitive true for case-insensitive search.
   */
  void Grep(QString pattern, bool caseInsensitive) override;

private:
  /// Groups the git/gpg processes of one operation (Insert, Remove, Move,
  /// Copy) into a single completion; see finished() and TransactionHelper.
  SimpleTransaction m_transaction;
  /// Background search over the decrypted store; relays to finishedGrep.
  NativeGrep m_grep;
  QString m_transactionOutput;

  /// An Insert() whose gpg is running or queued: the private directory gpg
  /// writes into (removed with it), the entry it is for, and whether an
  /// existing entry may be replaced. Consumed by finished() when the gpg
  /// step ends; one per Insert(), in order.
  struct PendingInsert {
    std::shared_ptr<QTemporaryDir> scratch;
    QString output;
    QString file;
    bool overwrite = false;
  };
  QQueue<PendingInsert> m_pendingInserts;

  /**
   * @brief Bring the ciphertext gpg wrote to @p output into the store as
   * @p file (Util::copyFileReplacing): through a temporary created next to
   * the entry and written by its open handle, then the operating system's
   * rename, which replaces the entry under that name (or, without
   * @p overwrite, fails when one has appeared) and follows nothing. Reports
   * nothing itself: Insert()'s finished() routes the reason through the
   * failed-operation path once the queued git steps are cancelled, and
   * reencryptSingleFile() shows it.
   * @param output The file gpg wrote, in a directory of QtPass's own.
   * @param file The entry's path.
   * @param overwrite Whether an entry already there may be replaced.
   * @param error Receives why @p file could not be written, if not.
   * @return Whether @p file now holds the new ciphertext.
   */
  auto placeEncryptedFile(const QString &output, const QString &file,
                          bool overwrite, QString *error) -> bool;

  /**
   * @brief Outcome of one reencryptPath() run; filled on the worker thread and
   * consumed by finishReencrypt() on the owning thread.
   */
  struct ReencryptResult;
  /// Set while a re-encryption run is pending or on the worker thread.
  bool m_reencryptActive = false;
  /// Set by cancelReencryptPath() and the destructor; read by the worker
  /// between files and polled by Executor::executeBlocking() while the worker
  /// waits on a process, which the worker then ends itself. The flag is the
  /// only thing shared with the worker: its QProcess is not thread-safe and
  /// is never touched from another thread.
  std::atomic<bool> m_reencryptCancel{false};
  QThread *m_reencryptThread = nullptr;
  /**
   * @brief Executor::executeBlocking() for the re-encryption helpers.
   *
   * On the owning thread this is a plain blocking run. On the worker thread
   * it hands m_reencryptCancel to the cancellable executeBlocking() overload:
   * nothing is started once the flag is set, and a process that is running
   * when it gets set is terminated (then killed) by the worker thread, which
   * is what lets cancelReencryptPath() and the destructor interrupt an
   * active gpg or git without touching its QProcess or pid.
   * @return Exit code, or -1 when refused, failed to start or interrupted.
   */
  [[nodiscard]] auto execBlocking(const QString &app, const QStringList &args,
                                  const QString &input = QString(),
                                  QString *process_out = nullptr,
                                  QString *process_err = nullptr) -> int;
  /// Overload capturing stdout (and stderr) without stdin input.
  [[nodiscard]] auto execBlocking(const QString &app, const QStringList &args,
                                  QString *process_out,
                                  QString *process_err = nullptr) -> int;
  /**
   * @brief Start the worker thread once the Executor queue is idle.
   * @param dir Directory passed to reencryptPath().
   */
  void startReencryptWorker(const QString &dir);
  /**
   * @brief Worker-thread body: scan @p dir and re-encrypt stale files.
   * @param dir Directory passed to reencryptPath().
   * @return Counts and the list of files that could not be re-encrypted.
   */
  auto reencryptFiles(const QString &dir) -> ReencryptResult;
  /**
   * @brief Deal with what a crashed or failed earlier run left under @p dir
   * before touching anything: stale temporaries (today's staged
   * `.qtpass-XXXXXX.tmp`, 1.8.x's `<entry>.reencrypt.tmp`, the
   * `<entry>.XXXXXX.tmp` of builds in between) are removed, a 1.8.x
   * `<entry>.reencrypt.bak` whose original is missing is put back, one next
   * to a present original is reported and left alone (both are valid
   * ciphertexts, the choice is the user's).
   * @param dir Directory to scan, recursively.
   * @return false when something was reported that needs a human first.
   */
  auto recoverReencryptLeftovers(const QString &dir) -> bool;
  /**
   * @brief Report the outcome of a run and emit endReencryptPath().
   * @param result Outcome produced by reencryptFiles().
   */
  void finishReencrypt(const ReencryptResult &result);

  /**
   * @brief Translate @p path for the given @p exe when WSL-routed: wraps in
   * wslpath substitution, otherwise returns @p path unchanged.
   * @param path Native filesystem path.
   * @param exe Executable path (checked for "wsl " prefix).
   * @return Path suitable for the given executable.
   */
  auto translatePathForWsl(const QString &path, const QString &exe) const
      -> QString;
  /**
   * @brief Translate @p path for git when WSL-routed: wraps in wslpath
   * substitution, otherwise returns @p path unchanged.
   * @param path Native filesystem path.
   * @return Path suitable for the configured git executable.
   */
  auto pgit(const QString &path) const -> QString;
  /**
   * @brief Translate @p path for gpg when WSL-routed: wraps in wslpath
   * substitution, otherwise returns @p path unchanged.
   * @param path Native filesystem path.
   * @return Path suitable for the configured gpg executable.
   */
  auto pgpg(const QString &path) const -> QString;
};

#endif // SRC_IMITATEPASS_H_
