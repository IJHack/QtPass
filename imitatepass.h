#ifndef IMITATEPASS_H
#define IMITATEPASS_H

#include "pass.h"

class ImitatePass : public Pass {
  Q_OBJECT

  bool removeDir(const QString &dirName);

public:
  ImitatePass();
  virtual ~ImitatePass() {}
  virtual void GitInit() override;
  virtual void GitPull() override;
  virtual void GitPush() override;
  virtual QProcess::ExitStatus Show(QString file, bool block = false) override;
  virtual void Insert(QString file, QString value,
                      bool overwrite = false) override;
  virtual void Remove(QString file, bool isDir = false) override;
  virtual void Init(QString path, const QList<UserInfo> &list) override;

  void reencryptPath(QString dir);
signals:
  void startReencryptPath();
  void endReencryptPath();
  void lastDecrypt(QString);

  // Pass interface
public:
  void Move(const QString src, const QString dest, const bool force = false);
  void Copy(const QString src, const QString dest, const bool force = false);
};

#endif // IMITATEPASS_H
