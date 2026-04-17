// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "executor.h"
#include <QCoreApplication>
#include <QDir>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QTextCodec>
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringDecoder>
#endif
#include <utility>

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

/**
 * @brief Executor::Executor executes external applications
 * @param parent
 */
Executor::Executor(QObject *parent) : QObject(parent), running(false) {
  connect(&m_process,
          static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
              &QProcess::finished),
          this,
          static_cast<void (Executor::*)(int, QProcess::ExitStatus)>(
              &Executor::finished));
  connect(&m_process, &QProcess::started, this, &Executor::starting);
}

/**
 * @brief Executor::startProcess starts the internal process, handling WSL
 * prefixes.
 * @param app Executable path (may start with "wsl ").
 * @param args Arguments to pass to the executable.
 */
void Executor::startProcess(const QString &app, const QStringList &args) {
  if (app.startsWith("wsl ")) {
    QStringList wslArgs = args;
    QString actualApp = app;
    wslArgs.prepend(actualApp.remove(0, 4));
    m_process.start("wsl", wslArgs);
  } else {
    m_process.start(app, args);
  }
}

/**
 * @brief Executor::startProcessBlocking starts a given process, handling WSL
 * prefixes.
 * @param internal QProcess reference to start.
 * @param app Executable path (may start with "wsl ").
 * @param args Arguments to pass to the executable.
 */
void Executor::startProcessBlocking(QProcess &internal, const QString &app,
                                    const QStringList &args) {
  if (app.startsWith("wsl ")) {
    QStringList wslArgs = args;
    QString actualApp = app;
    wslArgs.prepend(actualApp.remove(0, 4));
    internal.start("wsl", wslArgs);
  } else {
    internal.start(app, args);
  }
}

/**
 * @brief Executor::executeNext consumes executable tasks from the queue
 */
void Executor::executeNext() {
  if (!running) {
    if (!m_execQueue.isEmpty()) {
      const execQueueItem &i = m_execQueue.head();
      running = true;
      if (!i.workingDir.isEmpty()) {
        m_process.setWorkingDirectory(i.workingDir);
      }
      startProcess(i.app, i.args);
      if (!i.input.isEmpty()) {
        if (!m_process.waitForStarted(-1)) {
#ifdef QT_DEBUG
          dbg() << "Process failed to start:" << i.id << " " << i.app;
#endif
          m_process.closeWriteChannel();
          running = false;
          m_execQueue.dequeue();
          executeNext();
          return;
        }
        QByteArray data = i.input.toUtf8();
        if (m_process.write(data) != data.length()) {
#ifdef QT_DEBUG
          dbg() << "Not all data written to process:" << i.id << " " << i.app;
#endif
        }
      }
      m_process.closeWriteChannel();
    }
  }
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
  // Happens a lot if e.g. git binary is not set.
  // This will result in bogus "QProcess::FailedToStart" messages,
  // also hiding legitimate errors from the gpg commands.
  if (app.isEmpty()) {
#ifdef QT_DEBUG
    dbg() << "Trying to execute nothing...";
#endif
    return;
  }
  QString appPath = app;
  if (!appPath.startsWith("wsl ")) {
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
 * This should not be needed in Qt6
 *
 * @param in input data
 * @return Input bytes decoded to string
 */
static auto decodeAssumingUtf8(const QByteArray &in) -> QString {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  QTextCodec *codec = QTextCodec::codecForName("UTF-8");
  QTextCodec::ConverterState state;
  QString out = codec->toUnicode(in.constData(), in.size(), &state);
  if (state.invalidChars == 0) {
    return out;
  }
  codec = QTextCodec::codecForUtfText(in);
  return codec->toUnicode(in);
#else
  auto converter = QStringDecoder(QStringDecoder::Utf8);
  QString out = converter(in);
  if (!converter.hasError()) {
    return out;
  }
  // Fallback if UTF-8 decoding failed - try system encoding
  auto fallback = QStringDecoder(QStringDecoder::System);
  return fallback(in);
#endif
}

/**
 * @brief Executor::executeBlocking blocking version of the executor,
 * takes input and presents it as stdin
 * @param app
 * @param args
 * @param input
 * @param process_out
 * @param process_err
 * @return
 *
 * Note: Returning error code instead of throwing to maintain compatibility
 * with the existing error handling pattern used throughout QtPass.
 */
auto Executor::executeBlocking(const QString &app, const QStringList &args,
                               const QString &input, QString *process_out,
                               QString *process_err) -> int {
  QProcess internal;
  startProcessBlocking(internal, app, args);
  if (!internal.waitForStarted(-1)) {
#ifdef QT_DEBUG
    dbg() << "Process failed to start:" << app;
#endif
    return -1;
  }
  if (!input.isEmpty()) {
    QByteArray data = input.toUtf8();
    if (internal.write(data) != data.length()) {
#ifdef QT_DEBUG
      dbg() << "Not all input written:" << app;
#endif
    }
    internal.closeWriteChannel();
  }
  internal.waitForFinished(-1);
  if (internal.exitStatus() == QProcess::NormalExit) {
    QString pout = decodeAssumingUtf8(internal.readAllStandardOutput());
    QString perr = decodeAssumingUtf8(internal.readAllStandardError());
    if (process_out != nullptr) {
      *process_out = pout;
    }
    if (process_err != nullptr) {
      *process_err = perr;
    }
    return internal.exitCode();
  }
  // Process failed to start or crashed; return -1 to indicate error.
  // The calling code checks for non-zero exit codes for error handling.
  return -1;
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
auto Executor::executeBlocking(const QStringList &env, const QString &app,
                               const QStringList &args, QString *process_out,
                               QString *process_err) -> int {
  QProcess process;
  QProcessEnvironment penv;
  for (const QString &var : env) {
    int idx = var.indexOf('=');
    if (idx > 0) {
      penv.insert(var.left(idx), var.mid(idx + 1));
    }
  }
  process.setProcessEnvironment(penv);
  startProcessBlocking(process, app, args);
  if (!process.waitForStarted(-1)) {
#ifdef QT_DEBUG
    dbg() << "Process failed to start:" << app;
#endif
    return -1;
  }
  process.waitForFinished(-1);
  if (process.exitStatus() != QProcess::NormalExit) {
    return -1;
  }
  if (process_out) {
    *process_out = decodeAssumingUtf8(process.readAllStandardOutput());
  }
  if (process_err) {
    *process_err = decodeAssumingUtf8(process.readAllStandardError());
  }
  return process.exitCode();
}

/**
 * @brief Executor::setEnvironment set environment variables
 * for executor processes
 * @param env
 */
void Executor::setEnvironment(const QStringList &env) {
  m_process.setEnvironment(env);
}

auto Executor::environment() const -> QStringList {
  return m_process.environment();
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
 * @brief Executor::finished called when an executed process finishes
 * @param exitCode
 * @param exitStatus
 */
void Executor::finished(int exitCode, QProcess::ExitStatus exitStatus) {
  execQueueItem i = m_execQueue.dequeue();
  running = false;
  if (exitStatus == QProcess::NormalExit) {
    QString output;
    QString err;
    if (i.readStdout) {
      output = decodeAssumingUtf8(m_process.readAllStandardOutput());
    }
    if (i.readStderr || exitCode != 0) {
      err = decodeAssumingUtf8(m_process.readAllStandardError());
      if (exitCode != 0) {
#ifdef QT_DEBUG
        dbg() << exitCode << err;
#endif
      }
    }
    emit finished(i.id, exitCode, output, err);
  }
  //  else: emit crashed with ID, which may give a chance to recover ?
  executeNext();
}
