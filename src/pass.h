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
#include <QSet>
#include <QString>
#include <QStringList>

class GpgIdSigner;
#include <QHash>
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
   * @brief gpg's stderr without its machine-readable `[GNUPG:]` lines (and
   * without carriage returns), as a person reads it.
   * @param err gpg's (or pass's) standard error.
   * @return The remaining lines, joined with "\n".
   */
  static auto withoutStatusLines(const QString &err) -> QString;
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
  /// What skippedGpgId() found on its way up from one folder.
  struct SkippedWalk {
    /// The skipped `.gpg-id`, if any.
    std::optional<QString> skipped;
    /// The generation record's error, when that is why it was named.
    QString recordError;
  };
  /// SkippedWalk per absolute folder, for one check of many folders.
  using SkippedWalks = QHash<QString, SkippedWalk>;
  /// What linkedGpgId() found on its way up, per absolute folder, for one
  /// check of many folders.
  using LinkedWalks = QHash<QString, std::optional<QString>>;

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
   * @brief Pull from the remote and wait for it, at most @p timeoutMs: the
   * pull before an edit, on the GUI thread.
   *
   * A remote that never answers froze the window, autoclear timers
   * included, until git gave up, which it may never do. Past the deadline
   * git is stopped and the status bar says the pull did not finish.
   * @param timeoutMs Most milliseconds to wait (kBlockingPullMs from the
   *        window).
   */
  virtual void GitPull_b(int timeoutMs) = 0;

  /** @brief How long the pull before an edit may block the window. */
  static constexpr int kBlockingPullMs = 30000;
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
   * @brief The entry whose insert is reporting now: set while
   * finishedInsert() or insertFailed() goes out, empty otherwise.
   *
   * Inserts report in the order they were queued, so each report is matched
   * to the entry its Insert() named. A window refreshing after a save shows
   * only that entry again, not whatever row is selected by then.
   * @return Absolute path of the entry file (with `.gpg`), as the backend
   *         wrote it; empty outside a report, or for a report no queued
   *         insert accounts for.
   */
  [[nodiscard]] auto reportedInsert() const -> QString {
    return m_reportedInsert;
  }
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
   * @brief Stop a running Grep(): the walk ends at the next entry and the gpg
   * decrypting one is terminated. Its (partial) result still arrives through
   * finishedGrep(). Does nothing when no search runs.
   */
  virtual void CancelGrep() {}

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
   * @brief The keys a gpg encryption to @p recipients reaches: a throwaway
   *        encryption outside the store, read back for whom it went to.
   *
   * What `-r` makes of each line, where listKeys() searches: a key that
   * only resembles a recipient (a user ID that contains its address, a
   * subkey bound to its key) is not reached. No key lookups over the
   * network (`--no-auto-key-locate`), so opening a dialog fetches nothing.
   * @param recipients The lines of a recipient list.
   * @return The long key IDs (upper case) of the keys and subkeys encrypted
   *         to, possibly empty when gpg did not say; nothing when gpg could
   *         not encrypt to the list.
   */
  virtual auto encryptionReaches(const QStringList &recipients)
      -> std::optional<QStringList>;
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
   * @brief The status-bar text for a blocking pull stopped at its deadline.
   * @param timeoutMs The deadline that passed.
   * @return The message.
   */
  [[nodiscard]] static auto pullTimedOut(int timeoutMs) -> QString;

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
   * @param walks What earlier calls found per folder, for one check of many
   *        folders (RealPass::refuseUnverifiedTree()); read and extended, so
   *        each folder is looked up once. Null for none.
   * @return The skipped `.gpg-id` path, or nothing when the list found is
   *         the one that applies.
   */
  static auto skippedGpgId(const QString &forFile, const QString &passStore,
                           QString *recordError = nullptr,
                           SkippedWalks *walks = nullptr)
      -> std::optional<QString>;
  /**
   * @brief The link or junction under the name `.gpg-id` that getGpgIdPath()
   * skips on its way up from @p forFile, if it skips one before the list it
   * returns.
   *
   * `pass` follows such a link and encrypts to the recipients of the file it
   * points to, while QtPass shows and checks the list of a folder above it.
   * Unlike skippedGpgId(), this asks no generation record and includes the
   * store root, so it applies with or without a signing key.
   * @param forFile Entry or folder, as for getGpgIdPath().
   * @param passStore Root directory of the password store.
   * @param walks What earlier calls found per folder, for one check of many
   *        folders (RealPass::refuseUnverifiedTree()); read and extended, so
   *        each folder is looked at once. Null for none.
   * @return The linked `.gpg-id` path, or nothing when the walk meets none.
   */
  static auto linkedGpgId(const QString &forFile, const QString &passStore,
                          LinkedWalks *walks = nullptr)
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
   * @brief Init(), but only while the list of @p dir still reads as the
   *        caller read it with recipientsForEditing().
   *
   * The users dialog ticks its selection from the list it read, and writes
   * the whole selection: a newer list that arrived meanwhile (a pull in a
   * terminal, a sync) would be replaced by the old one plus the change. The
   * backend reads the list again at the last point before it writes (or
   * starts `pass init`) and refuses with refuseChangedList() when it differs.
   * @param path Folder to initialize, as for Init().
   * @param users Recipients, as for Init().
   * @param dir The folder the list was read for, as given to
   *        recipientsForEditing().
   * @param passStore The store root given to recipientsForEditing().
   * @param read What recipientsForEditing() returned then; its warning is
   *        not compared.
   * @return false when refuseChangedList() or refuseBusyForExpectedList()
   *         refused, so the caller can keep its selection; true otherwise,
   *         also when Init() refused for another reason it reported itself.
   */
  auto initIfUnchanged(const QString &path, const QList<UserInfo> &users,
                       const QString &dir, const QString &passStore,
                       const RecipientsForEditing &read) -> bool;
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
   * @brief Why pass would read a `.gpg-id` differently from
   * parseRecipients(), if it would.
   *
   * pass reads the list with `while read -r`, which keeps NUL-free bytes as
   * they are, strips only blanks and tabs, and drops a last line without a
   * line feed; parseRecipients() decodes UTF-8 (losing a byte-order mark and
   * invalid bytes), trims every kind of whitespace and skips a line that
   * starts with a dash. Where the two differ, the Users dialog shows other
   * recipients than pass encrypts to.
   * @param contents The file's bytes.
   * @return The first difference, as a phrase for a message; empty when
   *         both read the same recipients.
   */
  static auto passReadsDifferently(const QByteArray &contents) -> QString;
  /**
   * @brief The arguments as they may appear in the debug log: the value of
   * `--passphrase` and friends, and otpauth URIs, are replaced.
   * @param args The argv about to be run.
   * @return The same list with secret-shaped values redacted.
   */
  static auto loggableArgs(const QStringList &args) -> QStringList;

  /**
   * @brief Whether no gpg, git or pass of this backend runs or waits.
   * @return true when its executor is idle.
   */
  [[nodiscard]] auto isIdle() const -> bool { return exec.isIdle(); }

  /**
   * @brief Whether a Git push of this backend is queued or running.
   * @return true while one waits or runs.
   */
  [[nodiscard]] auto isPushing() const -> bool {
    return exec.hasQueued(Enums::GIT_PUSH);
  }

  /**
   * @brief Whether automatic pushes of the current store are held because
   *        its last re-encryption did not finish cleanly.
   *
   * The recipients change is committed locally first, so after a failed or
   * cancelled re-encryption the next automatic push (after any later edit)
   * would publish a .gpg-id whose entries are partly still encrypted to the
   * previous recipients. Held while any folder of the store has such a run
   * outstanding, until a clean re-encryption covers it; a push the user asks
   * for is not held. Shared by both backends, so switching between them
   * keeps it; kept for this session only.
   * @return true when an automatic push of the current store should not run.
   */
  [[nodiscard]] auto autoPushHeld() const -> bool;

  /**
   * @brief GitPush() for an automatic push, the one after a change. A
   *        backend that has to make the push later drops it then if the
   *        store is held by that time (autoPushHeld()); a push by hand
   *        (GitPush()) still goes.
   */
  void GitPushAutomatic();

protected:
  /**
   * @brief Whether the GitPush() running now is an automatic one.
   * @return true inside GitPushAutomatic().
   */
  [[nodiscard]] auto pushIsAutomatic() const -> bool;

  /**
   * @brief Hold automatic pushes of the store that holds @p scope
   *        (autoPushHeld()): its re-encryption did not finish cleanly.
   * @param scope The folder (or entry) the run re-encrypted, absolute or
   *        relative to the current store.
   */
  void holdAutoPush(const QString &scope);

  /**
   * @brief Release the holds a clean re-encryption of @p folder covers:
   *        those for @p folder itself and everything under it. Holds for
   *        other folders stay.
   * @param folder The folder the clean run re-encrypted, absolute or
   *        relative to the current store.
   */
  void releaseAutoPush(const QString &folder);

  /**
   * @brief With signing on, refuse to encrypt @p file when the lookup passed
   * over its folder's own list (skippedGpgId()): the parent's list is not
   * what was signed for it.
   * @param file The entry or folder about to be encrypted to.
   * @param signing Whether a signing key is set (signed lists in use).
   * @param except A list about to be written (by `pass init`), which may
   *        be missing now without that being refused.
   * @param walks Passed on to skippedGpgId(); null for none.
   * @return Whether it was refused (and the user told).
   */
  auto refuseSkippedGpgId(const QString &file, bool signing,
                          const QString &except = QString(),
                          SkippedWalks *walks = nullptr) -> bool;

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
   * @param entry Absolute path of the entry file the insert writes, handed
   *        out by reportedInsert() while its report goes out.
   */
  void queueInsert(const QString &entry = QString()) {
    ++m_insertsPending;
    m_insertedEntries.enqueue(entry);
  }
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
   * @brief Refuse a store change while a Git pull is still queued or running.
   *
   * An operation checks the repository when it is requested, but a pull queued
   * before it (the asynchronous Update) has not run yet, so a conflict it will
   * cause is not visible. For operations that commit without a later point to
   * re-check (every delete, move and copy, and the `pass` backend, whose
   * single `pass insert` commits by itself), refuse while the pull is in
   * flight so the change cannot land in the conflict it leaves. Nothing is
   * refused when Git is off or no pull is pending.
   * @return true when critical() and processErrorExit() were emitted and the
   *         caller must stop.
   */
  auto refusePendingPull() -> bool;

  /**
   * @brief Refuse an Init() under initIfUnchanged() when the folder's list no
   *        longer reads as the caller read it.
   *
   * Called by each backend at the last point before the list is written or
   * `pass init` is started. Outside initIfUnchanged() nothing is compared.
   * @return true when critical() and processErrorExit() were emitted and the
   *         caller must stop.
   */
  auto refuseChangedList() -> bool;

  /**
   * @brief Refuse an Init() under initIfUnchanged() while the executor has
   *        work queued or running, for a backend whose external tool writes
   *        the list only when its process runs.
   *
   * refuseChangedList() can only compare when the request is made. With
   * nothing ahead of it, `pass init` starts right away and the gap stays
   * that of a process start.
   * @return true when critical() and processErrorExit() were emitted and the
   *         caller must stop.
   */
  auto refuseBusyForExpectedList() -> bool;

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
   * @brief Pin byte-for-byte Git attributes for the store's encrypted files.
   *
   * Writes override lines for `*.gpg`, `.gpg-id` and `.gpg-id.sig` into the
   * store repository's private `info/attributes`, which outranks any in-tree
   * `.gitattributes` a co-writer may push (`*.gpg text`, a
   * working-tree-encoding or a filter) that would otherwise make Git rewrite
   * the ciphertext QtPass commits so it no longer decrypts. Written only when
   * the store is its own Git repository (a `.git` directory); a worktree `.git`
   * file or a store nested in a larger repository is left untouched.
   * Idempotent: marked and rewritten only when absent or changed.
   */
  void pinStoreGitAttributes();

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
  /**
   * @brief The folders (cleaned absolute paths) whose re-encryption did not
   *        finish, for every store and both backends.
   * @return The shared set, for this session.
   */
  static auto heldScopes() -> QSet<QString> &;
  /**
   * @brief @p path as an absolute, cleaned path, relative ones taken from
   *        the current store.
   * @param path A folder or entry.
   * @return The cleaned absolute path.
   */
  [[nodiscard]] auto scopePath(const QString &path) const -> QString;
  /// Set while GitPushAutomatic() runs GitPush().
  bool m_pushIsAutomatic = false;
  /**
   * @brief What initIfUnchanged() expects the list to read as, for
   *        refuseChangedList(); empty outside it.
   */
  struct ExpectedList {
    /** @brief The folder given to recipientsForEditing(). */
    QString dir;
    /** @brief The store root given to recipientsForEditing(). */
    QString passStore;
    /** @brief The state read then. */
    RecipientsForEditing::State state = RecipientsForEditing::State::Unsigned;
    /** @brief The recipients read then. */
    QStringList recipients;
  };
  /** @brief Set during initIfUnchanged(). */
  std::optional<ExpectedList> m_expectedList;
  /** @brief Set when refuseChangedList() or refuseBusyForExpectedList()
   *  refused during initIfUnchanged(). */
  bool m_expectedListRefused = false;
  /// Files of the Show() calls still in flight, oldest first.
  QQueue<QString> m_pendingShows;
  /// Inserts that have started queueing but not yet reported; see queueInsert.
  int m_insertsPending = 0;
  /// The entries of the inserts counted in m_insertsPending, in queue order.
  QQueue<QString> m_insertedEntries;
  /// The entry of the insert whose report is going out; see reportedInsert().
  QString m_reportedInsert;
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
   * @brief Emitted each time a queued process is about to start, and by
   * the refusals that go on to emit processErrorExit().
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
   * Says only what happened; it does not by itself mean an operation is
   * over. Every failure reported through the command queue goes on to
   * processErrorExit(), and so do some refusals. Other checks made before
   * anything is queued, such as an entry that already exists or a
   * recipient list that cannot be used, emit this signal and nothing
   * else.
   * @param title Translated dialog title.
   * @param message Translated description.
   */
  void critical(const QString &title, const QString &message);

  /**
   * @brief An operation ended in failure: its process exited non-zero,
   * could not be started or crashed, or the operation was refused.
   *
   * The counterpart of the `finished...` signals for an operation that
   * queued a command: it ends in one or the other. The main window
   * releases the busy state and shows @p err in the password pane. One
   * case emits both: when RealPass has removed a linked folder but could
   * not commit that, this signal is followed by finishedRemove(), because
   * the store did change.
   *
   * Before anything is queued the paths differ, so a caller must not wait
   * for this signal after every call. A path behind a link and an
   * unfinished Git operation are refused with startingExecuteWrapper(),
   * critical() and this signal. Other early checks, such as an entry that
   * already exists, emit critical() alone and return.
   *
   * insertFailed() and generateGPGKeysFailed() are emitted before this
   * signal whenever their operation fails through a queued command, be it
   * a non-zero exit, a crash or a process that could not be started, so
   * their dialogs need not guess whose failure it is.
   * generateGPGKeysFailed() also comes first when no gpg executable is
   * configured and nothing is queued at all. A refusal that reaches this
   * signal without queuing anything emits neither of them.
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
   * @param out Decrypted password file content, as text: fine to show or
   *        copy, but only to write back when @p lossless.
   * @param file The Show() argument this content is for.
   * @param lossless false when @p out does not encode back to the entry's
   *        bytes (they are not UTF-8, or begin with a byte order mark), so
   *        saving it would change the entry (Executor::lastOutputLossless()).
   */
  void finishedShow(const QString &out, const QString &file,
                    bool lossless = true);
  /**
   * @brief An Insert() succeeded: the entry is in the store and, with Git
   * in use, committed.
   *
   * In ImitatePass this ends the whole transaction (gpg, then git add and
   * commit), not just the gpg step. The password dialog closes on it. A
   * failure reported through the command queue goes out as insertFailed()
   * and processErrorExit() instead; an insert refused before anything is
   * queued emits neither this signal nor insertFailed().
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
   * @brief Emitted when an Insert() fails through a queued command, before
   *        the generic processErrorExit(): the process exited non-zero,
   *        crashed or could not be started, or the entry could not be put in
   *        place afterwards. Lets the password dialog keep what was typed
   *        and say why, without reacting to any other command failing
   *        meanwhile. An insert refused before anything is queued does not
   *        emit it.
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
