// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_IMITATEPASS_H_
#define SRC_IMITATEPASS_H_

#include "pass.h"
#include "simpletransaction.h"

/*!
    \class ImitatePass
    \brief Imitates pass features when pass is not enabled or available
*/
class ImitatePass : public Pass, private simpleTransaction {
  Q_OBJECT

  friend class tst_util;

protected:
  auto verifyGpgIdFile(const QString &file) -> bool;
  auto removeDir(const QString &dirName) -> bool;
  auto checkSigningKeys(const QStringList &signingKeys) -> bool;
  void writeGpgIdFile(const QString &gpgIdFile, const QList<UserInfo> &users);
  void signGpgIdFile(const QString &gpgIdFile, const QStringList &signingKeys);
  void gitAddGpgId(const QString &gpgIdFile, const QString &gpgIdSigFile,
                   bool addFile, bool addSigFile);
  void verifyGpgIdForDir(const QString &file, QStringList &gpgIdFilesVerified,
                         QStringList &gpgId);
  auto getKeysFromFile(const QString &fileName) -> QStringList;
  void reencryptSingleFile(const QString &fileName,
                           const QStringList &recipients);
  auto resolveMoveDestination(const QString &src, const QString &dest,
                              bool force) -> QString;

public:
  void executeMoveGit(const QString &src, const QString &destFile, bool force);

  void GitCommit(const QString &file, const QString &msg);

  void executeGit(PROCESS id, const QStringList &args,
                  QString input = QString(), bool readStdout = true,
                  bool readStderr = true);
  void executeGpg(PROCESS id, const QStringList &args,
                  QString input = QString(), bool readStdout = true,
                  bool readStderr = true);

  class transactionHelper {
    simpleTransaction *m_transaction;
    PROCESS m_result;

  public:
    transactionHelper(simpleTransaction *trans, PROCESS result)
        : m_transaction(trans), m_result(result) {
      m_transaction->transactionStart();
    }
    ~transactionHelper() { m_transaction->transactionEnd(m_result); }
  };

protected:
  void finished(int id, int exitCode, const QString &out,
                const QString &err) override;

  void executeWrapper(PROCESS id, const QString &app, const QStringList &args,
                      QString input, bool readStdout = true,
                      bool readStderr = true) override;

public:
  ImitatePass();
  ~ImitatePass() override = default;
  void GitInit() override;
  void GitPull() override;
  void GitPull_b() override;
  void GitPush() override;
  void Show(QString file) override;
  void OtpGenerate(QString file) override;
  void Insert(QString file, QString newValue, bool overwrite = false) override;
  void Remove(QString file, bool isDir = false) override;
  void Init(QString path, const QList<UserInfo> &users) override;

  void reencryptPath(const QString &dir);
Q_SIGNALS:
  void startReencryptPath();
  void endReencryptPath();

  // Pass interface
public:
  void Move(const QString src, const QString dest,
            const bool force = false) override;
  void Copy(const QString src, const QString dest,
            const bool force = false) override;
};

#endif // SRC_IMITATEPASS_H_
