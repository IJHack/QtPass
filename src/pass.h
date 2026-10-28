// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
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
 */
class Pass : public QObject {
  Q_OBJECT

  bool wrapperRunning{false};
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
    return env;
  }

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
   * @param includeSelf Whether the entry itself counts; false for a removal,
   *        which unlinks a link and touches nothing behind it.
   * @return true when critical() and processErrorExit() were emitted and
   *         the caller must stop.
   */
  auto refuseLinkedPath(const QString &path, bool includeSelf = true) -> bool;

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
  void handleProcessError(PROCESS pid, int exitCode, const QString &out,
                          const QString &err);
  auto formatInsertError(const QString &friendly, const QString &err)
      -> QString;
  void emitProcessFinishedSignal(PROCESS pid, const QString &out,
                                 const QString &err);

signals:
  /**
   * @brief Nothing of the store's runs or waits any more (isIdle()).
   */
  void idle();
  /**
   * @brief Emitted before executing a command.
   */
  void startingExecuteWrapper();
  /**
   * @brief Emit status message.
   * @param msg Status text.
   * @param timeout Display duration in milliseconds.
   */
  void statusMsg(const QString &msg, int timeout);
  /**
   * @brief Emit critical error.
   * @param title Error dialog title.
   * @param message Error description.
   */
  void critical(const QString &title, const QString &message);

  /**
   * @brief Emitted on process error exit.
   * @param exitCode Process exit code.
   * @param err Error message or stderr output.
   */
  void processErrorExit(int exitCode, const QString &err);

  /**
   * @brief Emitted when any operation finishes with process ID for filtering.
   * @param out Standard output from the process.
   * @param err Standard error from the process.
   * @param pid Process identifier for filtering/display.
   */
  void finishedAnyWithPid(const QString &out, const QString &err,
                          Enums::PROCESS pid);
  /**
   * @brief Emitted when Git init finishes.
   * @param out Standard output.
   * @param err Standard error.
   */
  void finishedGitInit(const QString &out, const QString &err);
  /**
   * @brief Emitted when Git pull finishes.
   * @param out Standard output.
   * @param err Standard error.
   */
  void finishedGitPull(const QString &out, const QString &err);
  /**
   * @brief Emitted when Git push finishes.
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
   * @brief Emitted when insert finishes.
   * @param out Standard output.
   * @param err Standard error.
   */
  void finishedInsert(const QString &out, const QString &err);
  /**
   * @brief Emitted when remove finishes.
   * @param out Standard output.
   * @param err Standard error.
   */
  void finishedRemove(const QString &out, const QString &err);
  /**
   * @brief Emitted when init finishes.
   * @param out Standard output.
   * @param err Standard error.
   */
  void finishedInit(const QString &out, const QString &err);
  /**
   * @brief Emitted when move finishes.
   * @param out Standard output.
   * @param err Standard error.
   */
  void finishedMove(const QString &out, const QString &err);
  /**
   * @brief Emitted when copy finishes.
   * @param out Standard output.
   * @param err Standard error.
   */
  void finishedCopy(const QString &out, const QString &err);
  /**
   * @brief Emitted when GPG key generation finishes.
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
   * @brief Emitted when grep finishes with matching results.
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
 */
QString gpgErrorMessage(const QString &err);

#endif // SRC_PASS_H_
