#ifndef PASS_H
#define PASS_H

#include "datahelpers.h"
#include "enums.h"
#include "executor.h"
#include <QDebug>
#include <QDir>
#include <QList>
#include <QProcess>
#include <QQueue>
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
#include <QRandomGenerator>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
#include <QString>
#include <QTextCodec>
#include <map>

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

  typedef Enums::PROCESS PROCESS;

public:
  Pass();
  void init();

  virtual ~Pass() {}
  virtual void GitInit() = 0;
  virtual void GitPull() = 0;
  virtual void GitPull_b() = 0;
  virtual void GitPush() = 0;
  virtual void Show(QString file) = 0;
  virtual void Insert(QString file, QString value, bool force) = 0;
  virtual void Remove(QString file, bool isDir) = 0;
  virtual void Move(const QString srcDir, const QString dest,
                    const bool force = false) = 0;
  virtual void Copy(const QString srcDir, const QString dest,
                    const bool force = false) = 0;
  virtual void Init(QString path, const QList<UserInfo> &users) = 0;
  virtual QString Generate_b(unsigned int length, const QString &charset);

  void GenerateGPGKeys(QString batch);
  QList<UserInfo> listKeys(QString keystring = "", bool secret = false);
  void updateEnv();
  static QStringList getRecipientList(QString for_file);
  //  TODO(bezet): getRecipientString is useless, refactor
  static QString getRecipientString(QString for_file, QString separator = " ",
                                    int *count = NULL);

protected:
  void executeWrapper(PROCESS id, const QString &app, const QStringList &args,
                      bool readStdout = true, bool readStderr = true);
  QString generateRandomPassword(const QString &charset, unsigned int length);
  quint32 boundedRandom(quint32 bound);

  virtual void executeWrapper(PROCESS id, const QString &app,
                              const QStringList &args, QString input,
                              bool readStdout = true, bool readStderr = true);

protected slots:
  virtual void finished(int id, int exitCode, const QString &out,
                        const QString &err);

signals:
  void error(QProcess::ProcessError);
  void startingExecuteWrapper();
  void statusMsg(QString, int);
  void critical(QString, QString);

  void processErrorExit(int exitCode, const QString &err);

  void finishedAny(const QString &, const QString &);
  void finishedGitInit(const QString &, const QString &);
  void finishedGitPull(const QString &, const QString &);
  void finishedGitPush(const QString &, const QString &);
  void finishedShow(const QString &);
  void finishedInsert(const QString &, const QString &);
  void finishedRemove(const QString &, const QString &);
  void finishedInit(const QString &, const QString &);
  void finishedMove(const QString &, const QString &);
  void finishedCopy(const QString &, const QString &);
  void finishedGenerate(const QString &, const QString &);
  void finishedGenerateGPGKeys(const QString &, const QString &);
};

#endif // PASS_H
