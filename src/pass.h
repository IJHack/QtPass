// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Pass, the abstract password-store backend that RealPass and
 * ImitatePass implement, and gpgErrorMessage(), which turns gpg's stderr into
 * a message for the user.
 */
#ifndef SRC_PASS_H_
#define SRC_PASS_H_

#include "appsettings.h"
#include "enums.h"
#include "executor.h"
#include "gpgidgeneration.h"
#include "userinfo.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QQueue>
#include <QString>
#include <QStringList>

class GpgIdSigner;
#include <cassert>
#include <map>
#include <optional>

/**
 * @struct ResolvedGpgconfCommand
 * @ingroup backend
 * @brief Represents a command invocation for gpgconf operations.
 *
 * On Unix this is typically just the gpgconf binary; on Windows/WSL it may be
 * a wrapper such as wsl.exe with gpgconf as a prefixed argument.
 */
struct ResolvedGpgconfCommand {
  /**
   * @brief Executable to run — may be a bare name resolved via PATH or a
   * wrapper (e.g. wsl.exe) rather than a direct gpgconf path.
   */
  QString program;
  /**
   * @brief Full argv list, including any wrapper/prefix args followed by the
   * actual gpgconf operation flags (e.g. ["--kill", "gpg-agent"]).
   */
  QStringList arguments;
};

/**
 * @class Pass
 * @ingroup backend
 * @brief Abstract base class for password store operations.
 *
 * Pass provides an abstraction layer for password management, supporting both
 * the native 'pass' utility and gopass/imitation backends. It handles:
 * - Password file operations (show, insert, remove, move, copy)
 * - GPG key management and Git integration
 * - OTP generation
 * - Random password generation
 *
 * Subclasses must implement Git and password operations.
 *
 * Paths: the operations take a path relative to the store, or an absolute
 * path spelled from the store as configured (`passStore`), as the store
 * tree and the dialogs build them. The link checks, `pass` names and Git
 * paths are worked out against that spelling. A path spelled through the
 * target of a store root that is itself a link is not under the store as
 * named. Init() places such a folder from its canonical path; the other
 * operations would check only the entry itself for a link and give `pass`
 * and Git `../` names, so callers must not hand one over. Resolving every
 * link instead is no way out: a removal would then reach a link's target,
 * not the link.
 */
class Pass : public QObject {
  Q_OBJECT

  /** @brief Unused; kept from the time Pass tracked the running wrapper. */
  bool wrapperRunning{false};
  /**
   * @brief The environment the store's processes run in: the system
   * environment plus what updateEnv() sets (PASSWORD_STORE_ variables,
   * GNUPGHOME, PATH additions, WSLENV). Read through storeEnvironment().
   */
  QProcessEnvironment env;

protected:
  /**
   * @brief The environment QtPass runs the store's processes in.
   *
   * What updateEnv() made: the git ceiling and the PASSWORD_STORE_ variables
   * QtPass's own environment does not carry. For blocking calls that start
   * their own process.
   * @return A copy of the environment.
   */
  [[nodiscard]] auto storeEnvironment() const -> QProcessEnvironment {
    return withCurrentGitPin(env);
  }

  /**
   * @brief @p base with GIT_DIR and GIT_WORK_TREE as the store is now
   * (Util::gitStoreVariables()), not as it was at updateEnv(): a `.git` that
   * appeared or changed hands since then is judged when Git runs.
   * @param base The environment to update.
   * @return @p base, the two variables recomputed (left as they are when
   *         the store keeps to a parent repository or there is none).
   */
  [[nodiscard]] auto withCurrentGitPin(QProcessEnvironment base) const
      -> QProcessEnvironment;

  /**
   * @brief Application settings snapshot injected at init() time.
   *
   * Populated by init(AppSettings) and valid for the lifetime of this backend
   * instance. Subclasses read it in place of calling QtPassSettings directly.
   */
  AppSettings m_settings;
  /**
   * @brief Internal command executor for queuing and running subprocesses.
   */
  Executor exec;

  /**
   * @brief Alias for Enums::PROCESS used throughout this class.
   */
  using PROCESS = Enums::PROCESS;

public:
  /**
   * @brief Construct a Pass instance.
   */
  Pass();
  /**
   * @brief Initialize the Pass instance with a settings snapshot.
   * @param settings Application settings to use for this backend lifetime.
   */
  void init(const AppSettings &settings);
  /**
   * @brief Check if GPG supports Ed25519 encryption.
   * @param gpgExecutable Path to the gpg binary (defaults to "gpg").
   * @return true if Ed25519 is supported.
   */
  static bool gpgSupportsEd25519(const QString &gpgExecutable = {});
  /**
   * @brief Get default key template for new GPG keys.
   * @param gpgExecutable Path to the gpg binary (defaults to "gpg").
   * @return Default template string.
   */
  static QString getDefaultKeyTemplate(const QString &gpgExecutable = {});

  ~Pass() override = default;

  // Git operations
  /**
   * @brief Initialize Git repository in password store.
   */
  virtual void GitInit() = 0;
  /**
   * @brief Pull changes from remote Git repository.
   */
  virtual void GitPull() = 0;
  /**
   * @brief Pull with rebase from remote.
   */
  virtual void GitPull_b() = 0;
  /**
   * @brief Push changes to remote Git repository.
   */
  virtual void GitPush() = 0;

  // Password operations
  /**
   * @brief Show decrypted password file.
   * @param file Path to password file relative to store root.
   */
  virtual void Show(QString file) = 0;
  /**
   * @brief Insert or update password.
   * @param file Path to password file.
   * @param value Password content to store.
   * @param force Overwrite existing file.
   */
  virtual void Insert(QString file, QString value, bool force) = 0;
  /**
   * @brief How many inserts have started queueing but not yet reported.
   *
   * One Insert() still in the command queue, or running, counts here until its
   * PASS_INSERT result goes out. A PasswordDialog reads this just before its
   * own Insert() so it can tell an earlier insert's report (which it is also
   * connected to, once a hanging queue let it open while its save was still
   * pending) from its own.
   * @return The number of inserts awaiting their report.
   */
  [[nodiscard]] auto insertsPending() const -> int { return m_insertsPending; }
  /**
   * @brief Remove password file or directory.
   * @param file Path to remove.
   * @param isDir true if removing a directory.
   */
  virtual void Remove(QString file, bool isDir) = 0;
  /**
   * @brief Move password file or directory.
   * @param srcDir Source path.
   * @param dest Destination path.
   * @param force Overwrite existing.
   */
  virtual void Move(const QString srcDir, const QString dest,
                    const bool force) = 0;
  /**
   * @brief Copy password file or directory.
   * @param srcDir Source path.
   * @param dest Destination path.
   * @param force Overwrite existing.
   */
  virtual void Copy(const QString srcDir, const QString dest,
                    const bool force) = 0;
  /**
   * @brief Initialize new password store.
   * @param path Root of password store.
   * @param users List of recipient GPG keys.
   */
  virtual void Init(QString path, const QList<UserInfo> &users) = 0;
  /**
   * @brief Search password content for a pattern.
   *
   * Both backends decrypt each entry with gpg and match it in-process
   * (NativeGrep), never through `pass grep`, so the pattern is one dialect
   * for both: QRegularExpression, Perl-compatible (PCRE).
   *
   * @param pattern Search pattern, a Perl-compatible regular expression.
   * @param caseInsensitive true for case-insensitive search.
   */
  virtual void Grep(QString pattern, bool caseInsensitive) = 0;

  /**
   * @brief Generate random password.
   * @param length Password length.
   * @param charset Character set to use.
   * @return Generated password.
   */
  virtual auto generatePassword(unsigned int length, const QString &charset)
      -> QString;

  // GPG operations
  /**
   * @brief Generate GPG keys using batch script.
   * @param batch GPG batch script content.
   */
  void GenerateGPGKeys(QString batch);
  /**
   * @brief List GPG keys matching patterns.
   * @param keystrings List of search patterns.
   * @param secret Include secret keys.
   * @return List of matching keys.
   */
  auto listKeys(QStringList keystrings, bool secret = false) -> QList<UserInfo>;
  /**
   * @brief List GPG keys.
   * @param keystring Search pattern.
   * @param secret Include secret keys.
   * @return List of matching keys.
   */
  auto listKeys(const QString &keystring = "", bool secret = false)
      -> QList<UserInfo>;
  /**
   * @brief Update environment for subprocesses.
   */
  void updateEnv();

  /**
   * @brief Most bytes of plaintext a gpg decryption may produce.
   *
   * An OpenPGP message is compressed, so a small entry in a shared store can
   * decompress to gigabytes; gpg stops at this many (--max-output) instead of
   * filling QtPass's memory. Far above any real entry.
   */
  static constexpr qint64 kMaxPlaintextBytes = 64LL * 1024 * 1024;

  /**
   * @brief The gpg options that enforce kMaxPlaintextBytes.
   * @return "--max-output" and the limit, for a gpg decryption's arguments.
   */
  static auto maxOutputArgs() -> QStringList;

  // Static helpers
  /**
   * @brief Resolve the gpgconf command to kill agents.
   * @param gpgPath Path to gpg executable.
   * @return Resolved command with program and arguments.
   */
  static auto resolveGpgconfCommand(const QString &gpgPath)
      -> ResolvedGpgconfCommand;
  /**
   * @brief Get .gpg-id file path for a password file.
   * @param for_file Path to password file.
   * @param passStore Root directory of the password store.
   * @return Path to .gpg-id file.
   */
  static auto getGpgIdPath(const QString &for_file, const QString &passStore)
      -> QString;
  /**
   * @brief The folder list getGpgIdPath() passed over on its way up to the
   * one it returns, if it passed one over that a signed store must not.
   *
   * getGpgIdPath() skips a `.gpg-id` that is missing, a link or a junction
   * and falls back to a parent folder's list, as `pass` does. In a signed
   * store that lets anyone who can write to it, without the signing key,
   * move a folder's entries to the parent's (also validly signed)
   * recipients by removing or linking the folder's own list. This names the
   * first such list between the start and the folder of the list found
   * (that folder and the store root excluded): one that is under the name
   * but not a regular file, or one that is gone although this device has
   * accepted a list at that path before (GpgIdGeneration::remembered()).
   * When the generation record cannot be read, whether a list was accepted
   * at a path cannot be told: the first folder without its own list is
   * named then too (fail closed), and @p recordError says why.
   * @param forFile Entry or folder, as for getGpgIdPath().
   * @param passStore Root directory of the password store.
   * @param recordError Receives the record's error when it could not be
   *        read, if not null; left untouched otherwise.
   * @return The skipped `.gpg-id` path, or nothing when the list found is
   *         the one that applies.
   */
  static auto skippedGpgId(const QString &forFile, const QString &passStore,
                           QString *recordError = nullptr)
      -> std::optional<QString>;
  /**
   * @brief Get list of recipients for a password file.
   * @param for_file Path to password file.
   * @param passStore Root directory of the password store.
   * @return List of recipient key IDs.
   */
  static auto getRecipientList(const QString &for_file,
                               const QString &passStore) -> QStringList;

  /**
   * @brief What a dialog may do with the list of a folder
   * (recipientsForEditing()).
   */
  struct RecipientsForEditing {
    /** @brief How the list on disk stood up. */
    enum class State {
      /** @brief No signing key: the list as it is. */
      Unsigned,
      /** @brief Signature and generation and folder all accepted. */
      Verified,
      /**
       * @brief Signed, authentic and bound to this folder, but older than a
       * generation this device has accepted: preselected for the recovery,
       * flagged for review.
       */
      VerifiedRollback,
      /**
       * @brief Not to be used: signature invalid, written for another folder
       * or for no folder in particular while a generation was accepted here,
       * malformed header, or no freshness record to judge by.
       */
      Rejected,
    };
    /** @brief How the list on disk stood up. */
    State state = State::Unsigned;
    /** @brief The recipients to preselect; empty when Rejected. */
    QStringList recipients;
    /** @brief What the dialog should show, if anything. */
    QString warning;
  };

  /**
   * @brief The recipients a dialog may preselect for editing the list of
   * @p dir.
   *
   * With a signing key configured, only a list whose signature, generation
   * and folder binding are all accepted: the Users dialog preselects what it
   * loads and OK writes and signs the selection, so a list read
   * unauthenticated would turn an attacker's unsigned edit into a genuinely
   * signed one with one click. The one exception is an authentic list the
   * generation record calls older (the recovery from a rollback goes through
   * this dialog): preselected, with the reason shown for review. Anything
   * else that fails preselects nothing and says why. Without a signing key,
   * the list as it is.
   * @param dir Folder whose list is edited; empty for the store root.
   * @param passStore The store the folder belongs to: the dialog's, which
   *        for a profile being set up is not this backend's.
   * @return The state, the recipients to preselect and the warning to show.
   */
  auto recipientsForEditing(const QString &dir, const QString &passStore)
      -> RecipientsForEditing;
  /**
   * @brief Parse the contents of a .gpg-id: one recipient per line, `#`
   * starts a comment, unusable lines are logged and skipped.
   * @param contents The file's bytes.
   * @param sourceName Where they came from, for the log.
   * @return The recipient key IDs in file order.
   */
  static auto parseRecipients(const QByteArray &contents,
                              const QString &sourceName) -> QStringList;
  /**
   * @brief The arguments as they may appear in the debug log: the value of
   * `--passphrase` and friends, and otpauth URIs, are replaced.
   * @param args The argv about to be run.
   * @return The same list with secret-shaped values redacted.
   */
  static auto loggableArgs(const QStringList &args) -> QStringList;
  /**
   * @brief Seed a new folder's .gpg-id from the recipients it inherits.
   *
   * Writes the recipient list that is in effect for @p newDir (found by
   * walking up to the nearest parent .gpg-id, see getGpgIdPath) into
   * @p newDir/.gpg-id atomically with owner-only permissions. Refuses to
   * overwrite an existing .gpg-id and writes nothing when no recipient is
   * inherited, so a zero-byte file never shadows the parent's list.
   * @param newDir Absolute path of the freshly created folder.
   * @param passStore Root directory of the password store.
   * @return true when the file was written, false otherwise.
   */
  static auto seedGpgIdFile(const QString &newDir, const QString &passStore)
      -> bool;

  /**
   * @brief Whether no gpg, git or pass of this backend runs or waits.
   * @return true when its executor is idle.
   */
  [[nodiscard]] auto isIdle() const -> bool { return exec.isIdle(); }

protected:
  /**
   * @brief With signing on, refuse to encrypt @p file when the lookup passed
   * over its folder's own list (skippedGpgId()): the parent's list is not
   * what was signed for it.
   * @param file The entry or folder about to be encrypted to.
   * @param signing Whether a signing key is set (signed lists in use).
   * @param except A list about to be written (by `pass init`), which may
   *        be missing now without that being refused.
   * @return Whether it was refused (and the user told).
   */
  auto refuseSkippedGpgId(const QString &file, bool signing,
                          const QString &except = QString()) -> bool;

  /**
   * @brief Whether @p gpgIdFile is a signed list QtPass accepts: its
   * signature verifies (GpgIdSigner::verifyFile()), and it is current and
   * bound to the folder it lies in (GpgIdGeneration::accept()), so an older
   * authentic list put back, or one signed for another folder, is refused.
   * @param signer The verifier, with the configured signing keys.
   * @param gpgIdFile The `.gpg-id`.
   * @param contents Receives the verified bytes.
   * @param why Receives why a current-and-bound check refused the list;
   *        left as it is when the signature itself does not verify.
   * @return Whether the list may be encrypted to.
   */
  auto verifySignedList(const GpgIdSigner &signer, const QString &gpgIdFile,
                        QByteArray *contents, QString *why) const -> bool;

  /**
   * @brief Whether the insert that just failed had put its entry in the
   * store before a later step failed. The backend that writes the entry
   * knows; this default says no.
   * @param err What the failed insert reported on stderr.
   * @return true when the failed insert's entry was written.
   */
  [[nodiscard]] virtual auto insertWritten(const QString &err) const -> bool {
    Q_UNUSED(err);
    return false;
  }

  /**
   * @brief Remember what a Show() asked for, so its completion can say so.
   *
   * Backends call this first thing in Show(); Pass::finished() pops the
   * entry again on success or failure. The Executor runs one queue in
   * order, so the two stay aligned.
   * @param file The Show() argument.
   */
  void queueShow(const QString &file) { m_pendingShows.enqueue(file); }
  /**
   * @brief Count one insert that has started queueing but not yet reported.
   *
   * Backends call this where Insert() queues its work, past every refusal;
   * Pass::finished() counts it off again when the insert reports. Each queued
   * insert reports once, as a PASS_INSERT, so the count tracks the inserts in
   * flight. A dialog reads insertsPending() before its own Insert() to learn
   * how many earlier reports are not its own.
   */
  void queueInsert() { ++m_insertsPending; }
  /** @brief What refuseLinkedPath() judges. */
  enum class LinkScope {
    /** @brief The entry itself and every folder above it in the store. */
    EntryAndParents,
    /**
     * @brief Only the folders above it: a removal unlinks a link and
     * touches nothing behind it.
     */
    ParentsOnly,
  };
  /** @brief How a path handed to refuseLinkedPath() is spelled. */
  enum class PathSpelling {
    /**
     * @brief As the store is configured or relative to it, the class's path
     * rule. A path spelled through the target of a linked store root is
     * then logged as a warning, so tests catch a caller that hands one over.
     */
    Configured,
    /**
     * @brief Through the target of a linked store root, by a caller that
     * places such a folder itself (Init()): not logged.
     */
    PlacedByCaller,
  };
  /**
   * @brief Refuse an operation on something that is, or lies behind, a
   * symbolic link or NTFS junction inside the store.
   *
   * The store tree shows a linked entry or folder like any other, and gpg,
   * pass and QFile all follow links, so without this a planted link would
   * have an entry shown, edited, copied or written outside the store, with
   * the store's recipients. The configured store root itself may be a link
   * (a synced folder); that is not judged.
   * @param path Store-relative (as Show/Insert/Remove get it, `.gpg`
   *        included where it applies) or absolute (as Move/Copy get it).
   * @param scope Whether the entry itself counts, or only the folders it
   *        lies in (a removal).
   * @param spelling Whether @p path follows the class's path rule, or the
   *        caller places a target-spelled path itself (Init()).
   * @return true when critical() and processErrorExit() were emitted and
   *         the caller must stop.
   */
  auto refuseLinkedPath(const QString &path,
                        LinkScope scope = LinkScope::EntryAndParents,
                        PathSpelling spelling = PathSpelling::Configured)
      -> bool;
  /**
   * @brief Refuse recipients for a folder outside the store, as pass
   * refuses a path with `..` in it.
   *
   * ImitatePass would write a `.gpg-id` that the store never reads, and
   * `pass init --path` would take the folder's absolute path as a folder
   * under the store. Links on the way are resolved, as for the store root.
   * @param folder The folder, absolute.
   * @param relative Where the folder lies in the store, as
   *        PathValidator::storeRelativePath() gives it (empty for the root),
   *        when it is not refused; may be null.
   * @return true when critical() and processErrorExit() were emitted and
   *         the caller must stop.
   */
  auto refuseFolderOutsideStore(const QString &folder,
                                QString *relative = nullptr) -> bool;

  /**
   * @brief Run a git command in the store's repository, blocking.
   *
   * The two backends reach git differently: ImitatePass runs the configured
   * git with `-C <store>`, RealPass runs `pass git`. The default (no git)
   * answers false. Used by refuseUnfinishedGitOperation() to ask the
   * repository its state before an operation changes the store.
   * @param args The git arguments (without the executable or `-C`).
   * @param out Receives the command's standard output; may be null.
   * @return true when git ran and exited 0, false otherwise (git off, the
   *         store is not a repository, or git failed).
   */
  virtual auto storeGit(const QStringList &args, QString *out) -> bool {
    Q_UNUSED(args);
    Q_UNUSED(out);
    return false;
  }

  /**
   * @brief Refuse to change the store while git is mid-rebase, -merge,
   * -cherry-pick or -revert.
   *
   * A pull that stopped in a conflict leaves the repository part-way through
   * an operation. Committing a save or a re-encryption into it lands the
   * change on a detached HEAD (a later `git rebase --abort` drops it) or
   * concludes the merge in favour of the local copy (pushing over a
   * colleague's change). Every store-changing operation calls this at its
   * entry, next to refuseLinkedPath(), so the change is refused before gpg or
   * pass runs and the dialog keeps what was typed. Nothing is refused when git
   * is off, the store is not a repository, or git gives no answer. A HEAD
   * detached by hand (no operation in progress) is deliberately accepted.
   * @return true when critical() and processErrorExit() were emitted and the
   *         caller must stop.
   */
  auto refuseUnfinishedGitOperation() -> bool;

  /**
   * @brief The unfinished git operation in progress in the store, if any.
   *
   * The detection behind refuseUnfinishedGitOperation(), without emitting a
   * refusal: a backend re-checks it when a save's gpg step has finished (a
   * pull queued before the save, from the asynchronous Update, may have
   * finished and left a conflict meanwhile) so the entry is not placed or
   * committed into the operation.
   * @return A ready-to-show reason naming the operation (rebase, merge,
   *         cherry-pick or revert), or an empty string when none is in
   *         progress, git is off, or the store is not a repository.
   */
  auto unfinishedGitOperationReason() -> QString;

  /**
   * @brief Execute external wrapper command.
   * @param id Process identifier.
   * @param app Executable path.
   * @param args Command arguments.
   * @param readStdout Capture stdout.
   * @param readStderr Capture stderr.
   */
  void executeWrapper(PROCESS id, const QString &app, const QStringList &args,
                      bool readStdout = true, bool readStderr = true);
  /**
   * @brief Generate random password from charset.
   * @param charset Character set.
   * @param length Password length.
   * @return Generated password.
   */
  auto generateRandomPassword(const QString &charset, unsigned int length)
      -> QString;
  /**
   * @brief Generate random number in range.
   * @param bound Upper bound (exclusive).
   * @return Random number.
   */
  auto boundedRandom(quint32 bound) -> quint32;
  /**
   * @brief Set or remove an environment variable.
   *
   * @param name Variable name (e.g. "PASSWORD_STORE_DIR"), no trailing '='.
   * @param value New value; an empty string removes the variable entirely.
   */
  void setEnvVar(const QString &name, const QString &value);

  /**
   * @brief Execute wrapper with input.
   * @param id Process identifier.
   * @param app Executable path.
   * @param args Command arguments.
   * @param input Input to pass to stdin.
   * @param readStdout Capture stdout.
   * @param readStderr Capture stderr.
   */
  void executeWrapper(PROCESS id, const QString &app, const QStringList &args,
                      QString input, bool readStdout, bool readStderr);
  /**
   * @brief Hook invoked just before a wrapped command is dispatched.
   *
   * Default is a no-op; backends override it to wrap execution (e.g.
   * ImitatePass registers a transaction). Keeps the dispatch surface a
   * single non-virtual executeWrapper instead of an overridden one.
   * @param id Process identifier of the command about to run.
   */
  virtual void beforeExecute(PROCESS id);

protected slots:
  /**
   * @brief Handle process completion.
   * @param id Process identifier.
   * @param exitCode Process exit code.
   * @param out Standard output.
   * @param err Standard error.
   */
  virtual void finished(int id, int exitCode, const QString &out,
                        const QString &err);

private:
  /// Files of the Show() calls still in flight, oldest first.
  QQueue<QString> m_pendingShows;
  /// Inserts that have started queueing but not yet reported; see queueInsert.
  int m_insertsPending = 0;
  /**
   * @brief Report a process that exited non-zero; called by finished().
   *
   * Always ends in processErrorExit(). A failed insert first emits
   * insertFailed(), and critical() too when the entry was written and only
   * a git step after it failed; failed key generation first emits
   * generateGPGKeysFailed().
   * @param pid Which kind of process failed.
   * @param exitCode Its exit code.
   * @param out Its stdout; not used.
   * @param err Its stderr, or the Executor's message for a failed start.
   */
  void handleProcessError(PROCESS pid, int exitCode, const QString &out,
                          const QString &err);
  /**
   * @brief Put the translated reason for a failed insert above what gpg
   * itself printed, leaving out the machine-readable `[GNUPG:]` lines.
   * @param friendly Message from gpgErrorMessage().
   * @param err gpg's stderr.
   * @return @p friendly, followed by the human-readable rest of @p err when
   *         there is any.
   */
  auto formatInsertError(const QString &friendly, const QString &err)
      -> QString;
  /**
   * @brief Emit the signals for a process that exited with 0; called by
   * finished() for everything but PASS_SHOW.
   *
   * finishedAnyWithPid() goes out only for a process whose output cannot
   * hold a secret (Enums::processInfo()), then the signal of that kind of
   * process (finishedGitPull() and so on), if it has one.
   * @param pid Which kind of process finished.
   * @param out Its stdout.
   * @param err Its stderr.
   */
  void emitProcessFinishedSignal(PROCESS pid, const QString &out,
                                 const QString &err);
  /**
   * @brief Refuse an operation on something that is not part of the store.
   *
   * Emits startingExecuteWrapper(), critical() and processErrorExit(), so
   * an interface that arms itself when an operation starts is released
   * again although nothing was started. refuseLinkedPath() and
   * refuseFolderOutsideStore() say why.
   * @param why Translated reason, shown to the user.
   */
  void refuseAsNotPartOfStore(const QString &why);

signals:
  /**
   * @brief Nothing of the store's runs or waits any more (isIdle()).
   *
   * Emitted when the command queue drains. The main window re-enables the
   * profile selector on it: switching stores is held back while commands
   * for the current one are still queued.
   */
  void idle();
  /**
   * @brief Emitted each time a queued process is about to start, and once
   * for an operation that is refused before anything starts.
   *
   * The main window shows the busy state on it. An operation made of
   * several processes emits it once per process; the operation then ends
   * with a `finished...` signal or with processErrorExit().
   */
  void startingExecuteWrapper();
  /**
   * @brief Progress or a non-fatal problem worth a line in the status bar
   * ("Creating backup commit", "Git pull failed: ...").
   *
   * Nothing depends on it being shown; an error the user must see goes out
   * as critical() instead.
   * @param msg Translated status text.
   * @param timeout How long to show it, in milliseconds.
   */
  void statusMsg(const QString &msg, int timeout);
  /**
   * @brief Something went wrong that the user has to be told about in a
   * dialog, such as a refused path or a failed backup commit.
   *
   * Says only what happened. When it ends an operation, processErrorExit()
   * follows separately.
   * @param title Translated dialog title.
   * @param message Translated description.
   */
  void critical(const QString &title, const QString &message);

  /**
   * @brief An operation ended in failure: its process exited non-zero,
   * could not be started or crashed, or the operation was refused.
   *
   * The counterpart of the `finished...` signals: every operation ends in
   * one or the other. The main window releases the busy state and shows
   * @p err in the password pane. One case emits both: when RealPass has
   * removed a linked folder but could not commit that, this signal is
   * followed by finishedRemove(), because the store did change.
   *
   * insertFailed() and generateGPGKeysFailed() are emitted before this
   * signal whenever their operation fails through a queued command, be it
   * a non-zero exit, a crash or a process that could not be started, so
   * their dialogs need not guess whose failure it is.
   * generateGPGKeysFailed() also comes first when no gpg executable is
   * configured and nothing is queued at all. A refusal made before
   * anything is queued, such as a path behind a link or an unfinished Git
   * operation, emits this signal without either of them.
   * @param exitCode Exit code of the process; -1 when it did not exit
   *        normally, 1 for a refusal.
   * @param err What to show the user: stderr, or a translated message.
   */
  void processErrorExit(int exitCode, const QString &err);

  /**
   * @brief A process of any kind finished successfully and its output
   * cannot contain a secret.
   *
   * Feeds the process output panel. Processes whose output may hold a
   * decrypted entry (Enums::processInfo()) never emit it, so a receiver
   * may log or display @p out and @p err as they are.
   * @param out Standard output from the process.
   * @param err Standard error from the process.
   * @param pid Which kind of process it was.
   */
  void finishedAnyWithPid(const QString &out, const QString &err,
                          Enums::PROCESS pid);
  /**
   * @brief `git init` in the store succeeded; the store is a repository
   * now and the tree is re-read.
   * @param out Standard output.
   * @param err Standard error.
   */
  void finishedGitInit(const QString &out, const QString &err);
  /**
   * @brief `git pull` succeeded; entries may have changed on disk.
   * @param out Standard output.
   * @param err Standard error.
   */
  void finishedGitPull(const QString &out, const QString &err);
  /**
   * @brief `git push` succeeded.
   * @param out Standard output.
   * @param err Standard error.
   */
  void finishedGitPush(const QString &out, const QString &err);
  /**
   * @brief Emitted when a Show() succeeds.
   *
   * Requests complete in the order they were queued, so @p file is the
   * argument of the Show() this result belongs to. Every receiver checks it
   * against what it asked for; a failed decrypt emits processErrorExit()
   * instead and never finishedShow().
   * @param out Decrypted password file content.
   * @param file The Show() argument this content is for.
   */
  void finishedShow(const QString &out, const QString &file);
  /**
   * @brief An Insert() succeeded: the entry is in the store and, with Git
   * in use, committed.
   *
   * In ImitatePass this ends the whole transaction (gpg, then git add and
   * commit), not just the gpg step. The password dialog closes on it; a
   * failure goes out as insertFailed() and processErrorExit() instead.
   * @param out Standard output of the operation's processes.
   * @param err Standard error.
   */
  void finishedInsert(const QString &out, const QString &err);
  /**
   * @brief A Remove() succeeded; the entry or folder is gone from the store.
   * @param out Standard output.
   * @param err Standard error.
   */
  void finishedRemove(const QString &out, const QString &err);
  /**
   * @brief An Init() succeeded: the folder's `.gpg-id` holds the new
   * recipient list.
   * @param out Standard output.
   * @param err Standard error.
   */
  void finishedInit(const QString &out, const QString &err);
  /**
   * @brief A Move() succeeded; the entry or folder is under its new name.
   * @param out Standard output.
   * @param err Standard error.
   */
  void finishedMove(const QString &out, const QString &err);
  /**
   * @brief A Copy() succeeded; the copy exists beside the original.
   * @param out Standard output.
   * @param err Standard error.
   */
  void finishedCopy(const QString &out, const QString &err);
  /**
   * @brief GenerateGPGKeys() succeeded; the new key pair is in the keyring.
   *
   * The key generation dialog closes on it. A failure emits
   * generateGPGKeysFailed() instead.
   * @param out Standard output.
   * @param err Standard error.
   */
  void finishedGenerateGPGKeys(const QString &out, const QString &err);
  /**
   * @brief Emitted when GPG key generation fails, alongside the generic
   *        processErrorExit(); lets the keygen dialog react to its own
   *        failure only, not to any other command failing meanwhile.
   * @param err What gpg (or the setup) reported.
   */
  void generateGPGKeysFailed(const QString &err);
  /**
   * @brief Emitted when an Insert() fails after it started a process,
   *        alongside the generic processErrorExit(); lets the password
   *        dialog keep what was typed and say why, without reacting to any
   *        other command failing meanwhile.
   * @param err What gpg, pass or the placement of the entry reported.
   * @param written true when this insert did put the entry in the store and
   *        only a later step (git add or commit) failed.
   */
  void insertFailed(const QString &err, bool written);
  /**
   * @brief The newest Grep() is over; an empty list means no entry matched.
   *
   * Comes from NativeGrep on both backends and bypasses finished(), since
   * the matching lines are decrypted content. A search superseded by a
   * newer one never reports.
   * @param results List of (entry path, matching lines) pairs.
   */
  void finishedGrep(const QList<QPair<QString, QStringList>> &results);
};

/**
 * @brief Maps GPG stderr (which may include --status-fd 2 tokens) to a
 * translated user-friendly encryption error string.
 *
 * Checks machine-readable [GNUPG:] status tokens first (locale-independent),
 * then falls back to case-insensitive substring matching for GPG builds that
 * do not emit status tokens. Returns an empty string when no known pattern
 * matches.
 *
 * Declared here so it can be exercised by unit tests without linking the full
 * Pass object.
 * @param err gpg's stderr, with or without `[GNUPG:]` status lines.
 * @return A translated message, or an empty string for an error it does
 *         not recognise.
 */
QString gpgErrorMessage(const QString &err);

#endif // SRC_PASS_H_
