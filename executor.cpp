#include "executor.h"

Executor::Executor(QObject *parent) : QObject(parent) {}

void Executor::executeNext() {
  if (!running) {
    if (!m_execQueue.isEmpty()) {
      const execQueueItem &i = m_execQueue.head();
      running = true;
      m_process.setWorkingDirectory(i.workingDir);
      m_process.start(i.app, i.args);
    }
  }
}

Executor::Executor(QObject *parent = 0) : running(false) {
  connect(&m_process,
          static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
              &QProcess::finished),
          this, static_cast<void (Executor::*)(int, QProcess::ExitStatus)>(
                    &Executor::finished));
}

void Executor::execute(int id, QString app, const QStringList &args,
                       bool readStdout, bool readStderr) {
  execute(id, app, args, QString(), readStdout, readStderr);
}

void Executor::execute(int id, QString app, const QStringList &args,
                       QString input = QString(), bool readStdout,
                       bool readStderr) {
  m_execQueue.push_back({id, app, args, input, readStdout, readStderr});
  executeNext();
}

void Executor::executeBlocking() {
}

void Executor::finished(int exitCode, QProcess::ExitStatus exitStatus) {
  execQueueItem i = m_execQueue.dequeue();
  running = false;
  if (exitStatus == QProcess::NormalExit) {
    QString output, err;
    if (i.readStdout)
      output = m_process.readAllStandardOutput();
    if (i.readStderr or exitCode != 0)
      err = m_process.readAllStandardError();
    emit finished(i.id, exitCode, output, err);
  }
  //	else: emit crashed with ID, which may give a chance to recover ?
  executeNext();
}
