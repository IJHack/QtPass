// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Executor, the FIFO queue every gpg, git and pass child process of
 * QtPass goes through, and the WSL command parsing it shares with the
 * settings code.
 */
#ifndef SRC_EXECUTOR_H_
#define SRC_EXECUTOR_H_

#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQueue>
#include <atomic>
#include <optional>
#include <utility>

/**
 * @class Executor
 * @brief Runs non-interactive external commands in FIFO order using a single
 * QProcess.
 *
 * Queues execution requests and emits per-request completion or error signals;
 * supports optional stdin, configurable stdout/stderr capture, and per-request
 * working directory and environment.
 */

class Executor : public QObject {
  Q_OBJECT

  /**
   * @struct ExecQueueItem
   * @brief Execution queue item for non-interactive ordered execution.
   */
  struct ExecQueueItem {
    /** @brief Identifier the caller gave; echoed in finished()/error(). */
    int id;
    /** @brief Executable as configured; resolved when started. */
    QString app;
    /** @brief Arguments for the executable. */
    QStringList args;
    /** @brief Data written to the process's stdin, then closed. */
    QString input;
    /** @brief Whether stdout is captured for finished(). */
    bool readStdout;
    /**
     * @brief Whether stderr is captured; it is read regardless when the
     * process exits non-zero.
     */
    bool readStderr;
    /** @brief Working directory to start in; empty for the current one. */
    QString workingDir;
  };

  /**
   * @brief Commands waiting to run, head first. The head stays queued while
   * it runs and is dequeued in onProcessFinished().
   */
  QQueue<ExecQueueItem> m_execQueue;
  /** @brief The one process object; every queued command reuses it. */
  QProcess m_process;
  /**
   * @brief Whether the head of the queue has been started (or is being
   * dequeued, see dequeuing()) and has not finished yet.
   */
  bool running{false};
  /**
   * @brief Start the head of the queue unless something runs or the queue
   * is empty.
   *
   * Marks the executor running and emits dequeuing() before touching the
   * item, so a slot there cannot invalidate it. An item with no executable
   * is failed with a queued error() (an empty command used to wedge the
   * queue, #1682) and the next one is tried at once. Called after each
   * execute() and from onProcessFinished().
   */
  void executeNext();
  /// Post one check for drained(), coalesced: it comes after what is
  /// being emitted now, and fires only when the executor is idle and no
  /// error() of a failed start is still on its way.
  void postDrainedIfIdle();
  /// error()s of failed starts posted but not delivered yet.
  int m_pendingErrors = 0;
  /// A drained() check is posted and not run yet.
  bool m_drainCheckPosted = false;
  /**
   * @brief Start @p process running @p app, handling the "wsl " prefix once
   * for both the queued and the blocking path.
   *
   * Also applies detachFromTerminal() and the umask from setChildUmask(),
   * so every child QtPass starts gets both.
   * @param process Process object to start; configured by the caller.
   * @param app Executable as configured (see resolveExecutable() and
   *        parseWslCommand()).
   * @param args Command arguments.
   */
  static void startProcess(QProcess &process, const QString &app,
                           const QStringList &args);
  /**
   * @brief Read what a finished process produced, according to the queue
   * item's readStdout/readStderr flags; stderr is always read on a non-zero
   * exit so the caller can report it.
   * @param item The queue item the process ran for.
   * @param exitCode Its exit code.
   * @return stdout and stderr, each empty when not read.
   */
  auto collectOutput(const ExecQueueItem &item, int exitCode)
      -> std::pair<QString, QString>;
  /**
   * @brief Shared blocking run: start @p process, optionally feed @p input on
   * stdin, wait, and capture stdout/stderr. Backs the executeBlocking
   * overloads. The caller configures @p process (e.g. environment) first.
   *
   * With a null @p cancel the wait is a plain waitForFinished(-1). With a
   * non-null one the wait polls the flag; once it is set this thread
   * terminate()s the child, gives it a short grace period, kill()s it if it
   * is still running, waits one more grace period at most, and returns -1.
   * A flag that is already set skips the start altogether.
   * @param process Pre-configured QProcess to run.
   * @param app Executable path.
   * @param args Command arguments.
   * @param input Data to write to stdin (empty = none).
   * @param process_out If non-null, receives stdout.
   * @param process_err If non-null, receives stderr.
   * @param cancel Optional flag another thread may set to end the run.
   * @param timeoutMs Time for the whole run, start included; negative for
   *        none.
   * @return Process exit code, or -1 on start/crash failure, cancel or
   *         timeout.
   */
  static auto runBlocking(QProcess &process, const QString &app,
                          const QStringList &args, const QString &input,
                          QString *process_out, QString *process_err,
                          const std::atomic_bool *cancel = nullptr,
                          int timeoutMs = -1) -> int;

public:
  /**
   * @brief Construct an Executor with an optional parent QObject.
   * @param parent Parent QObject, or nullptr for no parent.
   */
  explicit Executor(QObject *parent = nullptr);

  /**
   * @brief Queue a command without stdin input.
   * @param id Caller-assigned process identifier.
   * @param app Executable path.
   * @param args Command arguments.
   * @param readStdout Whether to capture stdout.
   * @param readStderr Whether to capture stderr.
   */
  void execute(int id, const QString &app, const QStringList &args,
               bool readStdout, bool readStderr = true);

  /**
   * @brief Queue a command with an explicit working directory.
   * @param id Caller-assigned process identifier.
   * @param workDir Working directory for the process.
   * @param app Executable path.
   * @param args Command arguments.
   * @param readStdout Whether to capture stdout.
   * @param readStderr Whether to capture stderr.
   */
  void execute(int id, const QString &workDir, const QString &app,
               const QStringList &args, bool readStdout,
               bool readStderr = true);

  /**
   * @brief Queue a command with optional stdin input.
   * @param id Caller-assigned process identifier.
   * @param app Executable path.
   * @param args Command arguments.
   * @param input Data to write to stdin.
   * @param readStdout Whether to capture stdout.
   * @param readStderr Whether to capture stderr.
   */
  void execute(int id, const QString &app, const QStringList &args,
               QString input = QString(), bool readStdout = false,
               bool readStderr = true);

  /**
   * @brief Queue a command with working directory and optional stdin input.
   * @param id Caller-assigned process identifier.
   * @param workDir Working directory for the process.
   * @param app Executable path.
   * @param args Command arguments.
   * @param input Data to write to stdin.
   * @param readStdout Whether to capture stdout.
   * @param readStderr Whether to capture stderr.
   */
  void execute(int id, const QString &workDir, const QString &app,
               const QStringList &args, QString input = QString(),
               bool readStdout = false, bool readStderr = true);

  /**
   * @brief Run a command synchronously and return its exit code.
   * @param app Executable path.
   * @param args Command arguments.
   * @param input Data to write to stdin.
   * @param process_out If non-null, receives stdout output.
   * @param process_err If non-null, receives stderr output.
   * @return Process exit code.
   */
  [[nodiscard]] static auto executeBlocking(const QString &app,
                                            const QStringList &args,
                                            const QString &input = QString(),
                                            QString *process_out = nullptr,
                                            QString *process_err = nullptr)
      -> int;

  /**
   * @brief Run a command synchronously on a caller-supplied QProcess, with
   * optional cancellation.
   *
   * The caller configures @p process (environment, say) before the call;
   * it must be a fresh, not yet started object. QProcess is not thread-safe,
   * so @p process belongs to the calling thread for the whole run: no other
   * thread may call terminate(), kill() or anything else on it. To let a run
   * be cancelled, pass @p cancel: the wait then polls the flag, and once it
   * is set the calling thread itself terminate()s the child, waits a short
   * grace period, kill()s it if it is still running, waits one more grace
   * period at most, and returns -1. Setting the flag is the only thing
   * another thread does; it may do so at any time, before or during the run.
   * A run given @p timeoutMs ends the same way once that time has passed,
   * start included, so it returns within the timeout plus twice the grace
   * even when a kill()ed child cannot exit yet (uninterruptible sleep on a
   * hung mount). Not timed: QProcess::start() itself, which can block, for
   * example opening a FIFO named by a redirected channel (no caller
   * redirects one), and QProcess's destructor, which still waits for such a
   * child, for up to 30 s.
   * @param process Process object to run the command on.
   * @param app Executable path.
   * @param args Command arguments.
   * @param input Data to write to stdin.
   * @param process_out If non-null, receives stdout output.
   * @param process_err If non-null, receives stderr output.
   * @param cancel If non-null, a flag that ends the run when set.
   * @param timeoutMs Time for the whole run, start included; negative (the
   *        default) for none.
   * @return Process exit code, or -1 if it failed to start, crashed, was
   * cancelled or ran out of time.
   */
  [[nodiscard]] static auto executeBlocking(
      QProcess &process, const QString &app, const QStringList &args,
      const QString &input = QString(), QString *process_out = nullptr,
      QString *process_err = nullptr, const std::atomic_bool *cancel = nullptr,
      int timeoutMs = -1) -> int;

  /**
   * @brief Run a command synchronously capturing stdout and stderr.
   * @param app Executable path.
   * @param args Command arguments.
   * @param process_out Receives stdout output.
   * @param process_err If non-null, receives stderr output.
   * @return Process exit code.
   */
  [[nodiscard]] static auto
  executeBlocking(const QString &app, const QStringList &args,
                  QString *process_out, QString *process_err = nullptr) -> int;

  /**
   * @brief Run a command synchronously with a custom environment.
   * @param env Process environment for the child process.
   * @param app Executable path.
   * @param args Command arguments.
   * @param process_out If non-null, receives stdout output.
   * @param process_err If non-null, receives stderr output.
   * @return Process exit code.
   */
  [[nodiscard]] static auto
  executeBlocking(const QProcessEnvironment &env, const QString &app,
                  const QStringList &args, QString *process_out = nullptr,
                  QString *process_err = nullptr) -> int;

  /**
   * @brief The one rule for turning a configured executable into what runs.
   *
   * Absolute paths and "wsl ..." commands are used as given. A bare or
   * relative name is looked up next to the application first (the Windows
   * and macOS bundles ship gpg/git/pass there) and otherwise left for
   * QProcess's PATH lookup. Both the queued and the blocking path go through
   * this, so the same setting can no longer run two different binaries.
   * @param app Executable as configured.
   * @return What to hand to QProcess::start().
   */
  static auto resolveExecutable(const QString &app) -> QString;

  /**
   * @brief Make @p process start its child without a controlling terminal.
   *
   * On Unix the child starts in a new session (setsid), so it has no
   * /dev/tty. git and ssh then use an askpass helper when there is one and
   * fail at once otherwise, instead of prompting in the terminal QtPass was
   * started from while the queue waits behind them. Other Unix process
   * flags set on @p process are kept. Does nothing on Windows. Every child
   * QtPass starts goes through this; startProcess() calls it for the queued
   * and the blocking path.
   * @param process Process to configure; call it before start().
   */
  static void detachFromTerminal(QProcess &process);

  /**
   * @brief The umask every child QtPass starts gets, as pass sets its own.
   *
   * pass runs git and gpg under umask 077 (or PASSWORD_STORE_UMASK), so the
   * folders and files they create in the store, .git included, are the
   * user's alone. QtPass gives that umask to the processes it starts
   * (startProcess(), the queue and the blocking calls) and not to itself:
   * a browser or file manager it opens keeps the session's. Unix only.
   * @param mask The umask, or a negative value for none (the default).
   */
  static void setChildUmask(int mask);

  /**
   * @brief The umask setChildUmask() set.
   * @return The umask, or a negative value when none was set.
   */
  [[nodiscard]] static auto childUmask() -> int;

  /**
   * @brief A configured executable that runs a Linux program through WSL.
   *
   * The one definition of "a WSL command" in QtPass; see parseWslCommand().
   */
  struct WslCommand {
    /**
     * @brief What to start.
     *
     * On Windows a bare `wsl`/`wsl.exe` in any case becomes `wsl`, which the
     * PATH lookup resolves; elsewhere (QtPass itself running inside WSL,
     * where `wsl.exe` is reachable through interop but `wsl` need not be)
     * and for a launcher given as a path it stays as configured.
     */
    QString launcher;
    /**
     * @brief wsl.exe options that go before the program (`-d Debian`,
     * `-u me`), without any `-e`/`--exec`: argv() always adds that.
     */
    QStringList options;
    /** @brief The Linux program to run, looked up on the WSL PATH. */
    QString command;
    /**
     * @brief Arguments for @ref launcher that run @ref command with @p args.
     *
     * Always through `--exec`, so WSL starts the program itself instead of
     * handing the joined command line to the distribution's default shell.
     * Without it every argument (entry paths, .gpg-id recipients, commit
     * messages) is word-split and `$()`-expanded by that shell, and a
     * hostile password store gets code execution.
     * @param args Arguments for @ref command, passed through verbatim.
     * @return @ref options, `--exec`, @ref command, then @p args.
     */
    auto argv(const QStringList &args) const -> QStringList;
    /**
     * @brief The same launcher and options running another program.
     * @param program Linux program to run instead of @ref command.
     * @return A copy with @ref command replaced by @p program.
     */
    auto with(const QString &program) const -> WslCommand;
  };

  /**
   * @brief Parse a configured executable as a WSL command.
   *
   * Accepts `wsl` or `wsl.exe` in any case, bare or as a path, followed by
   * wsl.exe options and exactly one program: `wsl gpg`, `WSL.EXE -d Debian
   * /usr/bin/gpg`, `C:\Windows\System32\wsl.exe -u me gpg`. A `-e`/`--exec`
   * the user wrote is dropped (argv() adds it), anything after the program
   * (`wsl sh -c "gpg"`) or a `wsl` with no program is not a WSL command.
   *
   * Every place that starts, probes or translates paths for a configured
   * executable goes through this, so a form one of them accepts cannot be
   * started differently by another.
   * @param app Executable as configured.
   * @return The parsed command, or nothing when @p app is not one.
   */
  static auto parseWslCommand(const QString &app) -> std::optional<WslCommand>;

  /**
   * @brief Build the wsl.exe argument list that runs @p command directly.
   * @param command Linux command to run (looked up on the WSL PATH).
   * @param args Arguments for @p command, passed through verbatim.
   * @return Arguments for `wsl`: `--exec`, @p command, then @p args.
   * @see WslCommand::argv
   */
  static auto wslExecArgs(const QString &command, const QStringList &args)
      -> QStringList;

  /**
   * @brief Translate a native path for a WSL-routed executable.
   *
   * When @p exe is a WSL command (parseWslCommand()) the path is converted
   * with `wslpath` in the same distribution (the Linux binary cannot open
   * `C:\...`); otherwise it is only cleaned.
   * @param path Native filesystem path.
   * @param exe Executable the path is meant for.
   * @return Path suitable for @p exe.
   */
  static auto translatePathForWsl(const QString &path, const QString &exe)
      -> QString;

  /**
   * @brief Set the environment passed to all child processes.
   * @param env Process environment for child processes.
   */
  void setEnvironment(const QProcessEnvironment &env);
  /**
   * @brief Return the environment passed to child processes.
   * @return Process environment.
   */
  auto environment() const -> QProcessEnvironment;

  /**
   * @brief Cancel the next queued command without executing it.
   * @return The id of the cancelled command, or -1 if the queue was empty.
   */
  auto cancelNext() -> int;
  /**
   * @brief Whether no command is running and nothing is queued.
   * @return true when the executor has nothing in flight.
   */
  auto isIdle() const -> bool { return !running && m_execQueue.isEmpty(); }
private slots:
  /**
   * @brief Finish the running item when its process exits.
   *
   * Dequeues the head, collects its output and emits finished() for a
   * normal exit or error() with exit code -1 for a crash (Qt leaves the
   * code undefined then, and 0 or 1 would be mistaken for success or
   * "no matches"), then starts the next item and posts a drained() check.
   * @param exitCode Exit code reported by QProcess.
   * @param exitStatus Whether the process exited normally or crashed.
   */
  void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
signals:
  /**
   * @brief Emitted once the last queued item is done and nothing runs or
   * waits any more (isIdle()), after that item's finished() or error().
   */
  void drained();
  /**
   * @brief Emitted when a queued process exits normally, whatever its exit
   * code; a non-zero code is the receiver's to interpret (Pass does).
   * @param id Caller-assigned id the item was queued with.
   * @param exitCode Exit code of the process.
   * @param output stdout, when the item asked for it; empty otherwise.
   * @param errout stderr, when the item asked for it or the exit code is
   *        non-zero.
   */
  void finished(int id, int exitCode, const QString &output,
                const QString &errout);
  /**
   * @brief Emitted just before a queued process is started; the main
   * window shows the busy indicator on it.
   */
  void starting();
  /**
   * @brief Emitted, synchronously, when the queue's head leaves it: just
   * before it is started, when it cannot be (no executable, a failed
   * start), or when cancelNext() drops it. Lets a caller do per-item work
   * at the moment the item actually runs, not when it was queued.
   * @param id The item's id.
   * @param completes Whether finished() or error() follows for it: for
   *        every item but one cancelNext() dropped. Those that complete
   *        do so in the order they left the queue.
   *
   * A slot may queue more with execute(): that waits behind this item. The
   * executor counts as running while the signal is out, also for an item
   * cancelNext() drops, so cancelNext() drops nothing then (-1) and
   * nothing starts until the signal has returned.
   */
  void dequeuing(int id, bool completes);
  /**
   * @brief Emitted instead of finished() when a queued process could not be
   * started, had no executable configured, or crashed.
   * @param id Caller-assigned id the item was queued with.
   * @param exitCode Always -1; the real code is unknown in these cases.
   * @param output stdout read so far, when the item asked for it.
   * @param errout stderr, or a message saying what went wrong when the
   *        process left none.
   */
  void error(int id, int exitCode, const QString &output,
             const QString &errout);
};

#endif // SRC_EXECUTOR_H_
