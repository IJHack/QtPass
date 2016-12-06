#ifndef PASS_H
#define PASS_H

#include "datahelpers.h"
#include "enums.h"
#include "executor.h"
#include <QList>
#include <QProcess>
#include <QQueue>
#include <QString>

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
  void executeWrapper(QString, QString, QString = QString());

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
  };

public:
  Pass();
  virtual ~Pass() {}
  virtual void GitInit() = 0;
  virtual void GitPull() = 0;
  virtual void GitPull_b() = 0;
  virtual void GitPush() = 0;
  virtual void Show(QString file) = 0;
  virtual int Show_b(QString file) = 0;
  virtual void Insert(QString file, QString value, bool force) = 0;
  virtual void Remove(QString file, bool isDir) = 0;
  virtual void Init(QString path, const QList<UserInfo> &users) = 0;
  virtual QString Generate(int length, const QString &charset);

  void GenerateGPGKeys(QString batch);
  QList<UserInfo> listKeys(QString keystring = "", bool secret = false);
  void updateEnv();
  //  TODO(bezet): those are probably temporarly here
  static QStringList getRecipientList(QString for_file);
  static QString getRecipientString(QString for_file, QString separator = " ",
                                    int *count = NULL);

signals:
  void finished(int, const QString &output, const QString &errout);
  void error(QProcess::ProcessError);
  void startingExecuteWrapper();
  void statusMsg(QString, int);
  void critical(QString, QString);

  void processErrorExit(int exitCode, const QString &err);
};

#endif // PASS_H
