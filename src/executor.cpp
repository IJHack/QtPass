#include "executor.h"
#include "debughelper.h"
#include <QCoreApplication>
#include <QDir>
#include <QTextCodec>

/**
 * @brief Executor::Executor executes external applications
 * @param parent
 */
Executor::Executor(QObject *parent) : QObject(parent), running(false) {
  connect(&m_process,
          static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
              &QProcess::finished),
          this, static_cast<void (Executor::*)(int, QProcess::ExitStatus)>(
                    &Executor::finished));
  connect(&m_process, &QProcess::started, this, &Executor::starting);
}

/**
 * @brief Executor::executeNext consumes executable tasks from the queue
 */
void Executor::executeNext() {
  if (!running) {
    if (!m_execQueue.isEmpty()) {
      const execQueueItem &i = m_execQueue.head();
      running = true;
      if (!i.workingDir.isEmpty())
        m_process.setWorkingDirectory(i.workingDir);
      m_process.start(i.app, i.args);
      if (!i.input.isEmpty()) {
        m_process.waitForStarted(-1);
        QByteArray data = i.input.toUtf8();
        if (m_process.write(data) != data.length())
          dbg() << "Not all data written to process:" << i.id << " " << i.app;
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
  execute(id, QString(), app, args, input, readStdout, readStderr);
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
    dbg() << "Trying to execute nothing...";
    return;
  }
  QString appPath =
      QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(app);
  m_execQueue.push_back(
      {id, appPath, args, input, readStdout, readStderr, workDir});
  executeNext();
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
 * TODO(bezet): it might make sense to throw here, a lot of possible errors
 */
int Executor::executeBlocking(QString app, const QStringList &args,
                              QString input, QString *process_out,
                              QString *process_err) {
  QProcess internal;
  internal.start(app, args);
  if (!input.isEmpty()) {
    QByteArray data = input.toUtf8();
    internal.waitForStarted(-1);
    if (internal.write(data) != data.length()) {
      dbg() << "Not all input written:" << app;
    }
    internal.closeWriteChannel();
  }
  internal.waitForFinished(-1);
  if (internal.exitStatus() == QProcess::NormalExit) {
    QTextCodec *codec = QTextCodec::codecForLocale();
    QString pout = codec->toUnicode(internal.readAllStandardOutput());
    QString perr = codec->toUnicode(internal.readAllStandardError());
    if (process_out != Q_NULLPTR)
      *process_out = pout;
    if (process_err != Q_NULLPTR)
      *process_err = perr;
    return internal.exitCode();
  } else {
    //  TODO(bezet): emit error() ?
    return -1; //    QProcess error code + qDebug error?
  }
}

/**
 * @brief Executor::executeBlocking blocking version of the executor
 * @param app
 * @param args
 * @param process_out
 * @param process_err
 * @return
 */
int Executor::executeBlocking(QString app, const QStringList &args,
                              QString *process_out, QString *process_err) {
  return executeBlocking(app, args, QString(), process_out, process_err);
}

/**
 * @brief Executor::setEnvironment set environment variables
 * for executor processes
 * @param env
 */
void Executor::setEnvironment(const QStringList &env) {
  m_process.setEnvironment(env);
}

/**
 * @brief Executor::cancelNext  cancels execution of first process in queue
 *                              if it's not already running
 *
 * @return  id of the cancelled process or -1 on error
 */
int Executor::cancelNext() {
  if (running || m_execQueue.isEmpty())
    return -1; //  TODO(bezet): definitely throw here
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
    QString output, err;
    QTextCodec *codec = QTextCodec::codecForLocale();
    if (i.readStdout)
      output = codec->toUnicode(m_process.readAllStandardOutput());
    if (i.readStderr or exitCode != 0) {
      err = codec->toUnicode(m_process.readAllStandardError());
      if (exitCode != 0)
        dbg() << exitCode << err;
    }
    emit finished(i.id, exitCode, output, err);
  }
  //	else: emit crashed with ID, which may give a chance to recover ?
  executeNext();
}
