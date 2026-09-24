// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "executor.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringDecoder>
#include <algorithm>
#include <utility>

#include "qtpasslogging.h"

namespace {
/// How often a cancellable blocking run re-checks its cancel flag.
constexpr int kBlockingCancelPollMs = 100;
/// Grace between terminate() and kill() once a blocking run is cancelled;
/// gpg and git exit on SIGTERM well within this.
constexpr int kBlockingKillGraceMs = 1000;
} // namespace

Executor::Executor(QObject *parent) : QObject(parent) {
  connect(&m_process, &QProcess::finished, this, &Executor::onProcessFinished);
  connect(&m_process, &QProcess::started, this, &Executor::starting);
}

void Executor::startProcess(QProcess &process, const QString &app,
                            const QStringList &args) {
  if (const auto wsl = parseWslCommand(app)) {
    process.start(wsl->launcher, wsl->argv(args));
  } else {
    process.start(resolveExecutable(app), args);
  }
}

auto Executor::resolveExecutable(const QString &app) -> QString {
  if (app.isEmpty() || QDir::isAbsolutePath(app)) {
    return app;
  }
  const QDir appDir(QCoreApplication::applicationDirPath());
  QStringList candidates{QDir::cleanPath(appDir.absoluteFilePath(app))};
#ifdef Q_OS_WIN
  candidates << QDir::cleanPath(
      appDir.absoluteFilePath(app + QStringLiteral(".exe")));
#endif
  for (const QString &candidate : std::as_const(candidates)) {
    // A stray non-executable file of that name must not mask PATH.
    const QFileInfo info(candidate);
    if (info.isFile() && info.isExecutable()) {
      return candidate;
    }
  }
  return app;
}

auto Executor::WslCommand::argv(const QStringList &args) const -> QStringList {
  return options + QStringList{QStringLiteral("--exec"), command} + args;
}

auto Executor::WslCommand::with(const QString &program) const -> WslCommand {
  return {launcher, options, program};
}

auto Executor::parseWslCommand(const QString &app)
    -> std::optional<WslCommand> {
  const QStringList parts = QProcess::splitCommand(app);
  if (parts.size() < 2) {
    return std::nullopt;
  }
  // The file name after the last separator of either kind: a Windows path
  // has to be recognised as such wherever the parser runs.
  const QString &launcher = parts.first();
  const qsizetype separator = std::max(launcher.lastIndexOf(QLatin1Char('/')),
                                       launcher.lastIndexOf(QLatin1Char('\\')));
  const QString launcherName = launcher.mid(separator + 1);
  const bool isWsl =
      launcherName.compare(QLatin1String("wsl"), Qt::CaseInsensitive) == 0 ||
      launcherName.compare(QLatin1String("wsl.exe"), Qt::CaseInsensitive) == 0;
  if (!isWsl) {
    return std::nullopt;
  }
  WslCommand wsl;
#ifdef Q_OS_WIN
  // A bare wsl or wsl.exe in any case is looked up on PATH as `wsl`; a path
  // stays as is.
  wsl.launcher = separator < 0 ? QStringLiteral("wsl") : launcher;
#else
  // QtPass running inside WSL reaches wsl.exe through interop under exactly
  // that name; nothing says `wsl` resolves too, so start what was written.
  wsl.launcher = launcher;
#endif
  // wsl.exe options that take a value; anything else starting with `-` is a
  // flag. A user-written -e/--exec is dropped, argv() always adds one.
  static const QStringList valued{
      QStringLiteral("-d"),   QStringLiteral("--distribution"),
      QStringLiteral("-u"),   QStringLiteral("--user"),
      QStringLiteral("--cd"), QStringLiteral("--shell-type")};
  qsizetype i = 1;
  for (; i < parts.size() && parts.at(i).startsWith(QLatin1Char('-')); ++i) {
    const QString &option = parts.at(i);
    if (option == QLatin1String("-e") || option == QLatin1String("--exec")) {
      continue;
    }
    if (valued.contains(option)) {
      if (i + 1 >= parts.size()) {
        return std::nullopt;
      }
      wsl.options << option << parts.at(++i);
    } else {
      wsl.options << option;
    }
  }
  // Exactly one program. More is a shell command line (`sh -c "..."`),
  // which is what --exec exists to keep out.
  if (i != parts.size() - 1) {
    return std::nullopt;
  }
  wsl.command = parts.at(i);
  return wsl;
}

auto Executor::wslExecArgs(const QString &command, const QStringList &args)
    -> QStringList {
  return WslCommand{QStringLiteral("wsl"), {}, command}.argv(args);
}

auto Executor::translatePathForWsl(const QString &path, const QString &exe)
    -> QString {
  QString normalizedPath = QDir::cleanPath(path);
  const auto wsl = parseWslCommand(exe);
  if (!wsl) {
    return normalizedPath;
  }
  QString wslPath;
  const auto wslpath = wsl->with(QStringLiteral("wslpath"));
  const int rc = executeBlocking(wslpath.launcher,
                                 wslpath.argv({normalizedPath}), &wslPath);
  const QString translated = wslPath.trimmed();
  return (rc == 0 && !translated.isEmpty()) ? translated : normalizedPath;
}

void Executor::executeNext() {
  if (running || m_execQueue.isEmpty()) {
    return;
  }
  const ExecQueueItem &i = m_execQueue.head();

  // An empty executable never emits finished(); dropping it wedged the queue
  // (#1682). Fail it like a failed-to-start process so the queue drains.
  if (i.app.isEmpty()) {
    qCDebug(lcQtPass) << "No executable set for:" << i.id;
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

  // A failed start never emits finished(), so `running` would stall the queue
  // and waiters (the keygen dialog) would hang. The emit is queued so it does
  // not re-enter KeygenDialog::done(), still on the stack; -1 goes through
  // Pass::finished's non-zero error gate.
  if (!m_process.waitForStarted(-1)) {
    qCDebug(lcQtPass) << "Process failed to start:" << i.id << " " << i.app;
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
      qCDebug(lcQtPass) << "Not all data written to process:" << i.id << " "
                        << i.app;
    }
  }
  m_process.closeWriteChannel();
}

void Executor::execute(int id, const QString &app, const QStringList &args,
                       bool readStdout, bool readStderr) {
  execute(id, QString(), app, args, QString(), readStdout, readStderr);
}

void Executor::execute(int id, const QString &workDir, const QString &app,
                       const QStringList &args, bool readStdout,
                       bool readStderr) {
  execute(id, workDir, app, args, QString(), readStdout, readStderr);
}

void Executor::execute(int id, const QString &app, const QStringList &args,
                       QString input, bool readStdout, bool readStderr) {
  execute(id, QString(), app, args, std::move(input), readStdout, readStderr);
}

void Executor::execute(int id, const QString &workDir, const QString &app,
                       const QStringList &args, QString input, bool readStdout,
                       bool readStderr) {
  // Queue even an empty executable: executeNext() reports it (#1682).
  // startProcess resolves the executable, as on the blocking path.
  m_execQueue.push_back(
      {id, app, args, std::move(input), readStdout, readStderr, workDir});
  executeNext();
}

// UTF-8 first; on a decoding error fall back to the system encoding.
static auto decodeAssumingUtf8(const QByteArray &in) -> QString {
  // Stateless: a truncated trailing byte becomes a replacement character
  // instead of being held back for a continuation that never comes.
  auto converter =
      QStringDecoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
  QString out = converter(in);
  if (!converter.hasError()) {
    return out;
  }
  auto fallback =
      QStringDecoder(QStringDecoder::System, QStringDecoder::Flag::Stateless);
  return fallback(in);
}

// Returns an error code rather than throwing, matching QtPass's error
// handling elsewhere.
namespace {

/**
 * @brief Wait for @p process to finish, stopping it when @p cancel is set.
 *
 * Polls the flag; every QProcess call, terminate()/kill() included, stays on
 * this thread, so another thread can never act on an exited process (or a
 * pid the OS has since reused). A console gpg ignores terminate() on
 * Windows (WM_CLOSE), hence the kill after a grace period.
 * @return false when it was cancelled.
 */
auto waitOrCancel(QProcess &process, const std::atomic_bool *cancel) -> bool {
  if (cancel == nullptr) {
    process.waitForFinished(-1);
    return true;
  }
  while (!process.waitForFinished(kBlockingCancelPollMs)) {
    if (process.state() == QProcess::NotRunning) {
      return true;
    }
    if (cancel->load()) {
      process.terminate();
      if (!process.waitForFinished(kBlockingKillGraceMs)) {
        process.kill();
        process.waitForFinished(-1);
      }
      return false;
    }
  }
  return true;
}

} // namespace

auto Executor::runBlocking(QProcess &process, const QString &app,
                           const QStringList &args, const QString &input,
                           QString *process_out, QString *process_err,
                           const std::atomic_bool *cancel) -> int {
  if (cancel != nullptr && cancel->load())
    return -1;
  startProcess(process, app, args);
  if (!process.waitForStarted(-1)) {
    qCDebug(lcQtPass) << "Process failed to start:" << app;
    return -1;
  }
  if (!input.isEmpty()) {
    QByteArray data = input.toUtf8();
    if (process.write(data) != data.length()) {
      qCDebug(lcQtPass) << "Not all input written:" << app;
    }
  }
  // Always close stdin so a child blocking on EOF doesn't hang.
  process.closeWriteChannel();
  if (!waitOrCancel(process, cancel)) {
    return -1;
  }
  // Read before judging the exit: a process that crashed may have said why,
  // as the queued path (onProcessFinished) keeps it too.
  if (process_out != nullptr) {
    *process_out = decodeAssumingUtf8(process.readAllStandardOutput());
  }
  if (process_err != nullptr) {
    *process_err = decodeAssumingUtf8(process.readAllStandardError());
  }
  return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
}

auto Executor::executeBlocking(const QString &app, const QStringList &args,
                               const QString &input, QString *process_out,
                               QString *process_err) -> int {
  QProcess internal;
  return runBlocking(internal, app, args, input, process_out, process_err);
}

auto Executor::executeBlocking(QProcess &process, const QString &app,
                               const QStringList &args, const QString &input,
                               QString *process_out, QString *process_err,
                               const std::atomic_bool *cancel) -> int {
  return runBlocking(process, app, args, input, process_out, process_err,
                     cancel);
}

auto Executor::executeBlocking(const QString &app, const QStringList &args,
                               QString *process_out, QString *process_err)
    -> int {
  return executeBlocking(app, args, QString(), process_out, process_err);
}

auto Executor::executeBlocking(const QProcessEnvironment &env,
                               const QString &app, const QStringList &args,
                               QString *process_out, QString *process_err)
    -> int {
  QProcess process;
  process.setProcessEnvironment(env);
  return runBlocking(process, app, args, QString(), process_out, process_err);
}

void Executor::setEnvironment(const QProcessEnvironment &env) {
  m_process.setProcessEnvironment(env);
}

auto Executor::environment() const -> QProcessEnvironment {
  return m_process.processEnvironment();
}

// Also -1 while the head is already running: that one is not cancelled.
auto Executor::cancelNext() -> int {
  if (running || m_execQueue.isEmpty()) {
    return -1;
  }
  return m_execQueue.dequeue().id;
}

void Executor::onProcessFinished(int exitCode,
                                 QProcess::ExitStatus exitStatus) {
  ExecQueueItem i = m_execQueue.dequeue();
  running = false;
  auto [output, err] = collectOutput(i, exitCode);
  if (exitStatus == QProcess::NormalExit) {
    if (exitCode != 0) {
      qCDebug(lcQtPass) << i.app << "exited with" << exitCode << err;
    }
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

auto Executor::collectOutput(const ExecQueueItem &item, int exitCode)
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
