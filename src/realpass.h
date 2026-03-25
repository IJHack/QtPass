// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_REALPASS_H_
#define SRC_REALPASS_H_

#include "pass.h"

/*!
    \class RealPass
    \brief Wrapper for executing pass to handle the password-store
*/
class RealPass : public Pass {
  void executePass(PROCESS id, const QStringList &args,
                   QString input = QString(), bool readStdout = true,
                   bool readStderr = true);

public:
  RealPass();
  ~RealPass() override = default;
  void GitInit() override;
  void GitPull() override;
  void GitPull_b() override;
  void GitPush() override;
  void Show(QString file) override;
  void OtpGenerate(QString file) override;
  void Insert(QString file, QString newValue, bool overwrite = false) override;
  void Remove(QString file, bool isDir = false) override;
  void Init(QString path, const QList<UserInfo> &users) override;

  // Pass interface
public:
  void Move(const QString src, const QString dest,
            const bool force = false) override;
  void Copy(const QString src, const QString dest,
            const bool force = false) override;
};

#endif // SRC_REALPASS_H_
