#ifndef IMITATEPASS_H
#define IMITATEPASS_H

#include "pass.h"

class ImitatePass : public Pass {
  Q_OBJECT

  bool removeDir(const QString &dirName);

  void executeWrapper(int id, const QString &app, const QStringList &args,
                      bool readStdout = true, bool readStderr = true);

  void executeWrapper(int id, const QString &app, const QStringList &args,
                      QString input, bool readStdout = true,
                      bool readStderr = true);

  void GitCommit(const QString &file, const QString &msg);

public:
  ImitatePass();
  virtual ~ImitatePass() {}
  virtual void GitInit() override;
  virtual void GitPull() override;
  virtual void GitPull_b() override;
  virtual void GitPush() override;
  virtual void Show(QString file) override;
  virtual int Show_b(QString file) override;
  virtual void Insert(QString file, QString value,
                      bool overwrite = false) override;
  virtual void Remove(QString file, bool isDir = false) override;
  virtual void Init(QString path, const QList<UserInfo> &list) override;

  void reencryptPath(QString dir);
signals:
  void startReencryptPath();
  void endReencryptPath();
  void lastDecrypt(QString);
};

#endif // IMITATEPASS_H
