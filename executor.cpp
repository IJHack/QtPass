#include "executor.h"
#include "debughelper.h"
#include <QCoreApplication>
#include <QDir>
#include <QTextCodec>

Executor::Executor(QObject *parent) : QObject(parent), running(false) {
  connect(&m_process,
          static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
              &QProcess::finished),
          this, static_cast<void (Executor::*)(int, QProcess::ExitStatus)>(
                    &Executor::finished));
  connect(&m_process, &QProcess::started, this, &Executor::starting);
}

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
  execute(id, QString(), app, args, input, readStdout, readStderr);
}

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

//  TODO(bezet): it might make sense to throw here, a lot of possible errors
int Executor::executeBlocking(QString app, const QStringList &args,
                              QString input, QString *process_out,
                              QString *process_err) {
  QProcess internal;
  emit starting();
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
    if (process_out != nullptr)
      *process_out = pout;
    if (process_err != nullptr)
      *process_err = perr;
    emit finished(internal.exitCode(), pout, perr);
    return internal.exitCode();
  } else {
    //  TODO(bezet): emit error() ?
    return -1; //    QProcess error code + qDebug error?
  }
}

int Executor::executeBlocking(QString app, const QStringList &args,
                              QString *process_out, QString *process_err) {
  return executeBlocking(app, args, QString(), process_out, process_err);
}

void Executor::setEnvironment(const QStringList &env) {
  m_process.setEnvironment(env);
}

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
    emit finished(exitCode, output, err);
  }
  //	else: emit crashed with ID, which may give a chance to recover ?
  executeNext();
}
