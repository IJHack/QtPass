// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "executor.h"
#include <QCoreApplication>
#include <QDir>
#include <QStringDecoder>
#include <utility>

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

namespace {
/// How often a cancellable blocking run re-checks its cancel flag.
constexpr int kBlockingCancelPollMs = 100;
/// Grace between terminate() and kill() once a blocking run is cancelled;
/// gpg and git exit on SIGTERM well within this.
constexpr int kBlockingKillGraceMs = 1000;
} // namespace

/**
 * @brief Executor::Executor executes external applications
 * @param parent
 */
Executor::Executor(QObject *parent) : QObject(parent) {
  connect(&m_process, &QProcess::finished, this, &Executor::onProcessFinished);
  connect(&m_process, &QProcess::started, this, &Executor::starting);
}

/**
 * @brief Executor::startProcess starts @p process, handling the "wsl "
 * prefix. One implementation for the queued (m_process) and the blocking
 * (caller-owned QProcess) path, so the WSL handling cannot drift between
 * them.
 * @param process QProcess to start.
 * @param app Executable path (may start with "wsl ").
 * @param args Arguments to pass to the executable.
 */
void Executor::startProcess(QProcess &process, const QString &app,
                            const QStringList &args) {
  if (app.startsWith(QLatin1String("wsl "))) {
    process.start(QStringLiteral("wsl"), wslExecArgs(app.mid(4), args));
  } else {
    process.start(app, args);
  }
}

/**
 * @brief Executor::wslExecArgs builds the wsl.exe argv for @p command.
 *
 * `wsl <command> <args>` hands the joined command line to the default Linux
 * shell, which word-splits and expands `$()` in every argument. `--exec`
 * makes WSL launch the binary directly so arguments arrive verbatim.
 * @param command Linux command to run.
 * @param args Arguments for @p command.
 * @return `--exec`, @p command, then @p args.
 */
auto Executor::wslExecArgs(const QString &command, const QStringList &args)
    -> QStringList {
  QStringList wslArgs = args;
  wslArgs.prepend(command);
  wslArgs.prepend(QStringLiteral("--exec"));
  return wslArgs;
}

/**
 * @brief Executor::executeNext consumes executable tasks from the queue
 */
void Executor::executeNext() {
  if (running || m_execQueue.isEmpty()) {
    return;
  }
  const execQueueItem &i = m_execQueue.head();

  // An empty executable can never produce a finished() signal. Silently
  // dropping it used to wedge the queue: the command stayed at the head until
  // the next completion, whose signal only caused the dequeue; a second
  // completion then stalled everything (#1682). Fail it through the same
  // deferred path as a failed-to-start process so the queue keeps draining.
  if (i.app.isEmpty()) {
#ifdef QT_DEBUG
    dbg() << "No executable set for:" << i.id;
#endif
    // Capture before dequeue() invalidates the head reference.
    const int failedId = i.id;
    m_execQueue.dequeue();
    QMetaObject::invokeMethod(
        this,
        [this, failedId]() {
          emit error(failedId, -1, QString(),
                     tr("No executable configured for this command"));
        },
        Qt::QueuedConnection);
    executeNext();
    return;
  }

  running = true;
  // Always set it: an item without a directory runs in the application's
  // cwd, not wherever the previous item happened to run.
  m_process.setWorkingDirectory(i.workingDir);
  startProcess(m_process, i.app, i.args);

  // Confirm the process actually started, regardless of whether it takes stdin.
  // A process that fails to start emits errorOccurred(FailedToStart) but never
  // finished(), so without this check `running` would stay true forever and
  // stall the whole queue (and, for stdin commands, the input would be dropped
  // silently). Surface the failure so callers waiting on a finished/error
  // signal (e.g. the GPG keygen dialog) do not hang. Defer the emit via a
  // queued call so we do not re-enter a caller still on the stack —
  // KeygenDialog::done() drives key generation synchronously — which mirrors
  // the normal asynchronous QProcess::finished path. A -1 exit code routes
  // through Pass::finished's non-zero error gate.
  if (!m_process.waitForStarted(-1)) {
#ifdef QT_DEBUG
    dbg() << "Process failed to start:" << i.id << " " << i.app;
#endif
    // Capture before dequeue() invalidates the head reference.
    const int failedId = i.id;
    const QString failedApp = i.app;
    m_process.closeWriteChannel();
    running = false;
    m_execQueue.dequeue();
    QMetaObject::invokeMethod(
        this,
        [this, failedId, failedApp]() {
          emit error(failedId, -1, QString(),
                     tr("Failed to start %1").arg(failedApp));
        },
        Qt::QueuedConnection);
    executeNext();
    return;
  }

  if (!i.input.isEmpty()) {
    QByteArray data = i.input.toUtf8();
    if (m_process.write(data) != data.length()) {
#ifdef QT_DEBUG
      dbg() << "Not all data written to process:" << i.id << " " << i.app;
#endif
    }
  }
  m_process.closeWriteChannel();
}

/**
 * @brief Executor::execute execute an app
 * @param id
 * @param app
 * @param args
 * @param readStdout
 * @param readStderr
 */
void Executor::execute(int id, const QString &app, const QStringList &args,
                       bool readStdout, bool readStderr) {
  execute(id, QString(), app, args, QString(), readStdout, readStderr);
}

/**
 * @brief Executor::execute executes an app from a workDir
 * @param id
 * @param workDir
 * @param app
 * @param args
 * @param readStdout
 * @param readStderr
 */
void Executor::execute(int id, const QString &workDir, const QString &app,
                       const QStringList &args, bool readStdout,
                       bool readStderr) {
  execute(id, workDir, app, args, QString(), readStdout, readStderr);
}

/**
 * @brief Executor::execute an app, takes input and presents it as stdin
 * @param id
 * @param app
 * @param args
 * @param input
 * @param readStdout
 * @param readStderr
 */
void Executor::execute(int id, const QString &app, const QStringList &args,
                       QString input, bool readStdout, bool readStderr) {
  execute(id, QString(), app, args, std::move(input), readStdout, readStderr);
}

/**
 * @brief Executor::execute  executes an app from a workDir, takes input and
 * presents it as stdin
 * @param id
 * @param workDir
 * @param app
 * @param args
 * @param input
 * @param readStdout
 * @param readStderr
 */
void Executor::execute(int id, const QString &workDir, const QString &app,
                       const QStringList &args, QString input, bool readStdout,
                       bool readStderr) {
  // An empty executable (e.g. git not configured yet) used to be dropped
  // here. That left its command permanently at the head of the queue: no
  // process ever runs, no finished()/error() is emitted, and every later
  // completion signal is swallowed for the rest of the session (#1682).
  // ExecuteNext() now surfaces it as an error instead, so keep queueing it.
  QString appPath = app;
  if (!appPath.isEmpty() && !appPath.startsWith("wsl ")) {
    appPath =
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(app);
  }
  m_execQueue.push_back(
      {id, appPath, args, std::move(input), readStdout, readStderr, workDir});
  executeNext();
}

/**
 * @brief decodes the input into a string assuming UTF-8 encoding.
 * If this fails (which is likely if it is not actually UTF-8)
 * it will then fall back to Qt's decoding function, which
 * will try based on BOM and if that fails fall back to local encoding.
 *
 * @param in input data
 * @return Input bytes decoded to string
 */
static auto decodeAssumingUtf8(const QByteArray &in) -> QString {
  // Stateless: the whole output is decoded in one go, so a truncated or
  // stray byte at the end becomes a replacement character instead of being
  // held back for a continuation that never comes (and thereby dropped).
  auto converter =
      QStringDecoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
  QString out = converter(in);
  if (!converter.hasError()) {
    return out;
  }
  // Fallback if UTF-8 decoding failed - try system encoding
  auto fallback =
      QStringDecoder(QStringDecoder::System, QStringDecoder::Flag::Stateless);
  return fallback(in);
}

/**
 * @brief Executor::executeBlocking blocking version of the executor,
 * takes input and presents it as stdin
 * @param app
 * @param args
 * @param input
 * @param process_out
 * @param process_err
 * @param cancel
 * @return
 *
 * Note: Returning error code instead of throwing to maintain compatibility
 * with the existing error handling pattern used throughout QtPass.
 */
auto Executor::runBlocking(QProcess &process, const QString &app,
                           const QStringList &args, const QString &input,
                           QString *process_out, QString *process_err,
                           const std::atomic_bool *cancel) -> int {
  if (cancel != nullptr && cancel->load())
    return -1;
  startProcess(process, app, args);
  if (!process.waitForStarted(-1)) {
#ifdef QT_DEBUG
    dbg() << "Process failed to start:" << app;
#endif
    return -1;
  }
  if (!input.isEmpty()) {
    QByteArray data = input.toUtf8();
    if (process.write(data) != data.length()) {
#ifdef QT_DEBUG
      dbg() << "Not all input written:" << app;
#endif
    }
  }
  // Always close stdin so a child blocking on EOF doesn't hang when no
  // input is written (these are one-shot blocking runs that never stream).
  process.closeWriteChannel();
  if (cancel == nullptr) {
    process.waitForFinished(-1);
  } else {
    // Poll so a flag set by another thread is noticed within one interval.
    // Every QProcess call, including the terminate()/kill() that end the
    // child, stays on this thread: the other thread only sets the flag, so
    // it can never act on a process that has already exited (or on a pid
    // the OS has since handed to something else).
    while (!process.waitForFinished(kBlockingCancelPollMs)) {
      if (process.state() == QProcess::NotRunning)
        break;
      if (cancel->load()) {
        process.terminate();
        if (!process.waitForFinished(kBlockingKillGraceMs)) {
          process.kill();
          process.waitForFinished(-1);
        }
        return -1;
      }
    }
  }
  if (process.exitStatus() != QProcess::NormalExit) {
    // Process failed to start or crashed; return -1 to indicate error.
    // The calling code checks for non-zero exit codes for error handling.
    return -1;
  }
  if (process_out != nullptr) {
    *process_out = decodeAssumingUtf8(process.readAllStandardOutput());
  }
  if (process_err != nullptr) {
    *process_err = decodeAssumingUtf8(process.readAllStandardError());
  }
  return process.exitCode();
}

auto Executor::executeBlocking(const QString &app, const QStringList &args,
                               const QString &input, QString *process_out,
                               QString *process_err) -> int {
  QProcess internal;
  return runBlocking(internal, app, args, input, process_out, process_err);
}

/**
 * @brief Executor::executeBlocking blocking run on a caller-supplied QProcess
 * @param process Process object to run the command on.
 * @param app
 * @param args
 * @param input
 * @param process_out
 * @param process_err
 * @param cancel Optional flag that ends the run when set (see the header).
 * @return
 */
auto Executor::executeBlocking(QProcess &process, const QString &app,
                               const QStringList &args, const QString &input,
                               QString *process_out, QString *process_err,
                               const std::atomic_bool *cancel) -> int {
  return runBlocking(process, app, args, input, process_out, process_err,
                     cancel);
}

/**
 * @brief Executor::executeBlocking blocking version of the executor
 * @param app
 * @param args
 * @param process_out
 * @param process_err
 * @return
 */
auto Executor::executeBlocking(const QString &app, const QStringList &args,
                               QString *process_out, QString *process_err)
    -> int {
  return executeBlocking(app, args, QString(), process_out, process_err);
}

/**
 * @brief Executor::executeBlocking blocking version with custom environment
 * @param env Environment variables to set
 * @param app Executable path
 * @param args Arguments
 * @param process_out Standard output
 * @param process_err Standard error
 * @return Exit code
 */
auto Executor::executeBlocking(const QProcessEnvironment &env,
                               const QString &app, const QStringList &args,
                               QString *process_out, QString *process_err)
    -> int {
  QProcess process;
  process.setProcessEnvironment(env);
  return runBlocking(process, app, args, QString(), process_out, process_err);
}

/**
 * @brief Executor::setEnvironment set environment variables
 * for executor processes
 * @param env
 */
void Executor::setEnvironment(const QProcessEnvironment &env) {
  m_process.setProcessEnvironment(env);
}

auto Executor::environment() const -> QProcessEnvironment {
  return m_process.processEnvironment();
}

/**
 * @brief Executor::cancelNext  cancels execution of first process in queue
 *                              if it's not already running
 *
 * @return  id of the cancelled process or -1 on error
 */
auto Executor::cancelNext() -> int {
  if (running || m_execQueue.isEmpty()) {
    return -1; // Return -1 to indicate no process was cancelled
               // (queue empty or currently executing).
  }
  return m_execQueue.dequeue().id;
}

/**
 * @brief Executor::onProcessFinished called when an executed process finishes
 * @param exitCode
 * @param exitStatus
 */
void Executor::onProcessFinished(int exitCode,
                                 QProcess::ExitStatus exitStatus) {
  execQueueItem i = m_execQueue.dequeue();
  running = false;
  auto [output, err] = collectOutput(i, exitCode);
  if (exitStatus == QProcess::NormalExit) {
#ifdef QT_DEBUG
    if (exitCode != 0) {
      dbg() << exitCode << err;
    }
#endif
    emit finished(i.id, exitCode, output, err);
  } else {
    // A signal-killed process usually leaves stderr empty; without this the
    // password pane would show nothing at all for the failure.
    if (err.trimmed().isEmpty()) {
      err = tr("%1 crashed or was killed").arg(i.app);
    }
    // Qt leaves exitCode undefined after a CrashExit (the signal number on
    // Unix), and Pass::finished gates on it: 0 would pass as success and 1
    // as grep's "no matches". Report -1, like the failed-to-start path.
    emit error(i.id, -1, output, err);
  }
  executeNext();
}

auto Executor::collectOutput(const execQueueItem &item, int exitCode)
    -> std::pair<QString, QString> {
  QString output;
  QString err;
  if (item.readStdout) {
    output = decodeAssumingUtf8(m_process.readAllStandardOutput());
  }
  if (item.readStderr || exitCode != 0) {
    err = decodeAssumingUtf8(m_process.readAllStandardError());
  }
  return {output, err};
}
