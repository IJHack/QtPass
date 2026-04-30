// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_EXECUTOR_H_
#define SRC_EXECUTOR_H_

#include <QObject>
#include <QProcess>
#include <QQueue>

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
   * @struct execQueueItem
   * @brief Execution queue item for non-interactive ordered execution.
   */
  struct execQueueItem {
    /**
     * @brief id    identifier of process given by the caller
     */
    int id;
    /**
     * @brief app   executable path.
     */
    QString app;
    /**
     * @brief args  arguments for executable.
     */
    QStringList args;
    /**
     * @brief input     data to write to stdin of process
     */
    QString input;
    /**
     * @brief readStdout    whether to read stdout
     */
    bool readStdout;
    /**
     * @brief readStderr    whether to read stderr
     *                      it's read regardless of this setting in case of
     *                      non-0 return value of process
     */
    bool readStderr;
    /**
     * @brief workingDir    working directory in which the process will be
     *                      started
     */
    QString workingDir;
  };

  QQueue<execQueueItem> m_execQueue;
  QProcess m_process;
  bool running;
  void executeNext();
  void startProcess(const QString &app, const QStringList &args);
  static void startProcessBlocking(QProcess &internal, const QString &app,
                                   const QStringList &args);

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
  static auto executeBlocking(const QString &app, const QStringList &args,
                              const QString &input = QString(),
                              QString *process_out = nullptr,
                              QString *process_err = nullptr) -> int;

  /**
   * @brief Run a command synchronously capturing stdout and stderr.
   * @param app Executable path.
   * @param args Command arguments.
   * @param process_out Receives stdout output.
   * @param process_err If non-null, receives stderr output.
   * @return Process exit code.
   */
  static auto executeBlocking(const QString &app, const QStringList &args,
                              QString *process_out,
                              QString *process_err = nullptr) -> int;

  /**
   * @brief Run a command synchronously with a custom environment.
   * @param env Environment variable list in "KEY=value" format.
   * @param app Executable path.
   * @param args Command arguments.
   * @param process_out If non-null, receives stdout output.
   * @param process_err If non-null, receives stderr output.
   * @return Process exit code.
   */
  static auto executeBlocking(const QStringList &env, const QString &app,
                              const QStringList &args,
                              QString *process_out = nullptr,
                              QString *process_err = nullptr) -> int;

  /**
   * @brief Set the environment passed to all child processes.
   * @param env Environment variable list in "KEY=value" format.
   */
  void setEnvironment(const QStringList &env);
  /**
   * @brief Return the environment passed to child processes.
   * @return Environment as a list of "KEY=value" strings.
   */
  auto environment() const -> QStringList;

  /**
   * @brief Cancel the next queued command without executing it.
   * @return The id of the cancelled command, or -1 if the queue was empty.
   */
  auto cancelNext() -> int;
private slots:
  void finished(int exitCode, QProcess::ExitStatus exitStatus);
signals:
  /**
   * @brief finished    signal that is emitted when process finishes
   *
   * @param id          id of the process
   * @param exitCode    return code of the process
   * @param output      stdout produced by the process
   * @param errout      stderr produced by the process
   */
  void finished(int id, int exitCode, const QString &output,
                const QString &errout);
  /**
   * @brief starting    signal that is emitted when process starts
   */
  void starting();
  /**
   * @brief error       signal that is emitted when process finishes with an
   * error
   *
   * @param id          id of the process
   * @param exitCode    return code of the process
   * @param output      stdout produced by the process
   * @param errout      stderr produced by the process
   */
  void error(int id, int exitCode, const QString &output,
             const QString &errout);
};

#endif // SRC_EXECUTOR_H_
