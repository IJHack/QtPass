#include "executor.h"
#include <QCoreApplication>
#include <QDir>
#include <QTextCodec>

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
 * @brief Executor::executeNext consumes executable tasks from the queue
 */
void Executor::executeNext() {
  if (!running) {
    if (!m_execQueue.isEmpty()) {
      const execQueueItem &i = m_execQueue.head();
      running = true;
      if (!i.workingDir.isEmpty())
        m_process.setWorkingDirectory(i.workingDir);
      if (i.app.startsWith("wsl ")) {
        QStringList tmp = i.args;
        QString app = i.app;
        tmp.prepend(app.remove(0, 4));
        m_process.start("wsl", tmp);
      } else
        m_process.start(i.app, i.args);
      if (!i.input.isEmpty()) {
        m_process.waitForStarted(-1);
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
#ifdef QT_DEBUG
    dbg() << "Trying to execute nothing...";
#endif
    return;
  }
  QString appPath = app;
  if (!appPath.startsWith("wsl "))
    appPath =
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(app);
  m_execQueue.push_back(
      {id, appPath, args, input, readStdout, readStderr, workDir});
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
static QString decodeAssumingUtf8(QByteArray in) {
  QTextCodec *codec = QTextCodec::codecForName("UTF-8");
  QTextCodec::ConverterState state;
  QString out = codec->toUnicode(in.constData(), in.size(), &state);
  if (!state.invalidChars)
    return out;
  codec = QTextCodec::codecForUtfText(in);
  return codec->toUnicode(in);
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
  if (app.startsWith("wsl ")) {
    QStringList tmp = args;
    tmp.prepend(app.remove(0, 4));
    internal.start("wsl", tmp);
  } else
    internal.start(app, args);
  if (!input.isEmpty()) {
    QByteArray data = input.toUtf8();
    internal.waitForStarted(-1);
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
    if (process_out != Q_NULLPTR)
      *process_out = pout;
    if (process_err != Q_NULLPTR)
      *process_err = perr;
    return internal.exitCode();
  }
  //  TODO(bezet): emit error() ?
  return -1; //    QProcess error code + qDebug error?
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
    if (i.readStdout)
      output = decodeAssumingUtf8(m_process.readAllStandardOutput());
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
  //	else: emit crashed with ID, which may give a chance to recover ?
  executeNext();
}
