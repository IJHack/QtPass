#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <QObject>
#include <QProcess>
#include <QQueue>

/*!
    \class Executor
    \brief Executes external commands for handleing password, git and other data
*/
class Executor : public QObject {
  Q_OBJECT

  /*!
      \struct execQueueItem
      \brief Execution queue items for non-interactive ordered execution.
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

public:
  explicit Executor(QObject *parent = 0);

  void execute(int id, const QString &app, const QStringList &args,
               bool readStdout, bool readStderr = true);

  void execute(int id, const QString &workDir, const QString &app,
               const QStringList &args, bool readStdout,
               bool readStderr = true);

  void execute(int id, const QString &app, const QStringList &args,
               QString input = QString(), bool readStdout = false,
               bool readStderr = true);

  void execute(int id, const QString &workDir, const QString &app,
               const QStringList &args, QString input = QString(),
               bool readStdout = false, bool readStderr = true);

  int executeBlocking(QString app, const QStringList &args,
                      QString input = QString(),
                      QString *process_out = Q_NULLPTR,
                      QString *process_err = Q_NULLPTR);

  int executeBlocking(QString app, const QStringList &args,
                      QString *process_out, QString *process_err = Q_NULLPTR);

  void setEnvironment(const QStringList &env);
private slots:
  void finished(int exitCode, QProcess::ExitStatus exitStatus);
signals:
  /**
   * @brief finished    signal that is emited when process finishes
   *
   * @param id          id of the process
   * @param exitCode    return code of the process
   * @param output      stdout produced by the process
   * @param errout      stderr produced by the process
   */
  void finished(int id, int exitCode, const QString &output,
                const QString &errout);
  /**
   * @brief starting    signal that is emited when process starts
   */
  void starting();
  /**
   * @brief error       signal that is emited when process finishes with an
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

#endif // EXECUTOR_H
