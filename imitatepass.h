#ifndef IMITATEPASS_H
#define IMITATEPASS_H

#include "pass.h"

class ImitatePass : public Pass {
  Q_OBJECT

  bool removeDir(const QString &dirName);

public:
  ImitatePass();
  virtual ~ImitatePass() {}
  virtual void GitInit() Q_DECL_OVERRIDE;
  virtual void GitPull() Q_DECL_OVERRIDE;
  virtual void GitPush() Q_DECL_OVERRIDE;
  virtual QProcess::ExitStatus Show(QString file, bool block = false) Q_DECL_OVERRIDE;
  virtual void Insert(QString file, QString value,
                      bool overwrite = false) Q_DECL_OVERRIDE;
  virtual void Remove(QString file, bool isDir = false) Q_DECL_OVERRIDE;
  virtual void Init(QString path, const QList<UserInfo> &list) Q_DECL_OVERRIDE;

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
