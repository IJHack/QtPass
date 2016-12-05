#ifndef REALPASS_H
#define REALPASS_H

#include "pass.h"

class RealPass : public Pass {
private:
  void executePass(PROCESS id, const QStringList &args, QString input,
                   bool readStdout = true, bool readStderr = false);
  void executePass(PROCESS id, const QStringList &args, bool readStdout = true,
                   bool readStderr = false);

public:
  RealPass();
  virtual ~RealPass() {}
  virtual void GitInit() override;
  virtual void GitPull() override;
  virtual void GitPull_b() override;
  virtual void GitPush() override;
  virtual void Show(QString file) override;
  virtual int Show_b(QString file) override;
  virtual void Insert(QString file, QString value,
                      bool overwrite = false) override;
  virtual void Remove(QString file, bool isDir = false) override;
  virtual void Init(QString path, const QList<UserInfo> &users) override;
};

#endif // REALPASS_H
