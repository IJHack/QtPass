#ifndef PASS_H
#define PASS_H

#include <QList>
#include <QProcess>
#include <QQueue>
#include <QString>
//  TODO(bezet): extract UserInfo somewhere
#include "enums.h"
#include "usersdialog.h"

/*!
    \struct execQueueItem
    \brief Execution queue items for non-interactive ordered execution.
 */
struct execQueueItem {
  /**
   * @brief app executable path.
   */
  QString app;
  /**
   * @brief args arguments for executable.
   */
  QString args;
  /**
   * @brief input stdio input.
   */
  QString input;
};

class Pass : public QObject {
  Q_OBJECT

  QQueue<execQueueItem> execQueue;
  bool wrapperRunning;
  QStringList env;

protected:
  void executeWrapper(QString, QString, QString = QString());
  QProcess process;

public:
  Pass();
  virtual ~Pass() {}
  virtual void GitInit() = 0;
  virtual void GitPull() = 0;
  virtual void GitPush() = 0;
  virtual QProcess::ExitStatus Show(QString file, bool block = false) = 0;
  virtual void Insert(QString file, QString value, bool force) = 0;
  virtual void Remove(QString file, bool isDir) = 0;
  virtual void Init(QString path, const QList<UserInfo> &users) = 0;
  virtual QString Generate(int length, const QString &charset);

  void GenerateGPGKeys(QString batch);
  QList<UserInfo> listKeys(QString keystring = "", bool secret = false);
  void waitFor(uint seconds);
  QProcess::ProcessState state();
  QByteArray readAllStandardOutput();
  QByteArray readAllStandardError();
  //  TODO(bezet): probably not needed in public interface(1 use MainWindow)
  QProcess::ExitStatus waitForProcess();
  void resetPasswordStoreDir();
  void updateEnv();
  //  TODO(bezet): those are probably temporarly here
  static QStringList getRecipientList(QString for_file);
  static QString getRecipientString(QString for_file, QString separator = " ",
                                    int *count = NULL);

private slots:
  void processFinished(int, QProcess::ExitStatus);

signals:
  void finished(int exitCode, QProcess::ExitStatus);
  void error(QProcess::ProcessError);
  void startingExecuteWrapper();
  void statusMsg(QString, int);
  void critical(QString, QString);
};

#endif // PASS_H
