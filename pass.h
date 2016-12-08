#ifndef PASS_H
#define PASS_H

#include "datahelpers.h"
#include "enums.h"
#include "executor.h"
#include <QList>
#include <QProcess>
#include <QQueue>
#include <QString>
#include <QDir>
#include <QDebug>

//  TODO(bezet): extract UserInfo somewhere
#include "enums.h"
#include "usersdialog.h"

/*!
    \class Pass
    \brief Acts as an abstraction for pass or pass imitation
*/
class Pass : public QObject {
  Q_OBJECT

  bool wrapperRunning;
  QStringList env;
protected:
  Executor exec;

  enum PROCESS {
    GIT_INIT = 0,
    GIT_ADD,
    GIT_COMMIT,
    GIT_RM,
    GIT_PULL,
    GIT_PUSH,
    PASS_SHOW,
    PASS_INSERT,
    PASS_REMOVE,
    PASS_INIT,
    PASSWD_GENERATE,
    GPG_GENKEYS,
    PASS_MOVE,
    PASS_COPY,
    GIT_MOVE,
    GIT_COPY,
  };

public:
  Pass();
  void init();

  virtual ~Pass() {}
  virtual void GitInit() = 0;
  virtual void GitPull() = 0;
  virtual void GitPull_b() = 0;
  virtual void GitPush() = 0;
  virtual void Show(QString file) = 0;
  virtual int Show_b(QString file) = 0;
  virtual void Insert(QString file, QString value, bool force) = 0;
  virtual void Remove(QString file, bool isDir) = 0;
  virtual void Move(const QString srcDir, const QString dest, const bool force = false) = 0;
  virtual void Copy(const QString srcDir, const QString dest, const bool force = false) = 0;
  virtual void Init(QString path, const QList<UserInfo> &users) = 0;
  virtual QString Generate(int length, const QString &charset);

  void GenerateGPGKeys(QString batch);
  QList<UserInfo> listKeys(QString keystring = "", bool secret = false);
  void updateEnv();
  //  TODO(bezet): those are probably temporarly here
  static QStringList getRecipientList(QString for_file);
  static QString getRecipientString(QString for_file, QString separator = " ",
                                    int *count = NULL);

  void executeGit(int id, const QStringList &args, QString input = QString(),
                  bool readStdout = true, bool readStderr = true);
  void executePass(int id, const QStringList &arg, QString input = QString(),
                  bool readStdout = true,bool readStderr = true);
  void executeGpg(int id, const QStringList &args, QString input = QString(),
                  bool readStdout = true,bool readStderr = true);

private:
  void executeWrapper(int id, const QString &app, const QStringList &args,
                      bool readStdout = true, bool readStderr = true);

  void executeWrapper(int id, const QString &app, const QStringList &args,
                      QString input, bool readStdout = true,
                      bool readStderr = true);
 private slots:
  void processFinished(int, QProcess::ExitStatus);

signals:
  void finished(int, const QString &output, const QString &errout);
  void error(QProcess::ProcessError);
  void startingExecuteWrapper();
  void statusMsg(QString, int);
  void critical(QString, QString);

  void processErrorExit(int exitCode, const QString &err);
};

#endif // PASS_H
