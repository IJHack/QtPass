#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <QObject>

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
     * non-0 return value of process
     */
    bool readStderr;
    QString workingDir;
  };

  QQueue<execQueueItem> m_execQueue;
  QProcess m_process;
  bool running;
  void executeNext();

public:
  explicit Executor(QObject *parent = 0);
  void execute(int id, QString app, const QStringList &args,
               bool readStdout = false, bool readStderr = false);
  void execute(int id, QString app, const QStringList &args,
               QString input = QString(), bool readStdout = false,
               bool readStderr = false);
  void executeBlocking();
private slots:
  void finished(int exitCode, QProcess::ExitStatus exitStatus);
signals:
  /**
   * @brief finished    signal that is emited when process finishes
   *
   * @param id      identifier given when starting process
   * @param exitCode    return code of the process
   * @param stdout      stdout produced by the process, if requested when
   * executing
   * @param stderr      stderr produced by the process, if requested or if
   * process failed
   */
  void finished(int id, int exitCode, const QString &stdout,
                const QString &stderr);
};

#endif // EXECUTOR_H
