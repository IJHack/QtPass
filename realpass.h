#ifndef REALPASS_H
#define REALPASS_H

#include "pass.h"

class RealPass : public Pass {
private:
  void executePass(QString args, QString input = QString());

public:
  RealPass();
  virtual ~RealPass() {}
  virtual void GitInit() override;
  virtual void GitPull() override;
  virtual void GitPush() override;
  virtual QProcess::ExitStatus Show(QString file, bool block = false) override;
  virtual void Insert(QString file, QString value,
                      bool overwrite = false) override;
  virtual void Remove(QString file, bool isDir = false) override;
  virtual void Init(QString path, const QList<UserInfo> &users) override;

  // Pass interface
public:
  void Move(QDir srcDir, QDir destDir, bool force = false);
  void Move(QFile srcFile, QFile destFile, bool force = false);
  void Copy(QDir srcDir, QDir destDir, bool force = false);
  void Copy(QFile srcFile, QFile destFile, bool force = false);
};



#endif // REALPASS_H
