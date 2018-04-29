#ifndef REALPASS_H
#define REALPASS_H

#include "pass.h"

/*!
    \class RealPass
    \brief Wrapper for executing pass to handle the password-store
*/
class RealPass : public Pass {
  void executePass(PROCESS id, const QStringList &arg,
                   QString input = QString(), bool readStdout = true,
                   bool readStderr = true);

public:
  RealPass();
  virtual ~RealPass() {}
  virtual void GitInit() Q_DECL_OVERRIDE;
  virtual void GitPull() Q_DECL_OVERRIDE;
  virtual void GitPull_b() Q_DECL_OVERRIDE;
  virtual void GitPush() Q_DECL_OVERRIDE;
  virtual void Show(QString file) Q_DECL_OVERRIDE;
  virtual void Insert(QString file, QString value,
                      bool overwrite = false) Q_DECL_OVERRIDE;
  virtual void Remove(QString file, bool isDir = false) Q_DECL_OVERRIDE;
  virtual void Init(QString path, const QList<UserInfo> &users) Q_DECL_OVERRIDE;

  // Pass interface
public:
  void Move(const QString src, const QString dest,
            const bool force = false) Q_DECL_OVERRIDE;
  void Copy(const QString src, const QString dest,
            const bool force = false) Q_DECL_OVERRIDE;
};

#endif // REALPASS_H
