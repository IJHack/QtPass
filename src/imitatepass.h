// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_IMITATEPASS_H_
#define SRC_IMITATEPASS_H_

#include "pass.h"
#include "simpletransaction.h"

class QRegularExpression;
class QThread;

/**
 * @class ImitatePass
 * @brief Implementation that imitates 'pass' when the real tool is unavailable.
 *
 * ImitatePass provides a complete password store implementation using direct
 * GPG operations when 'pass' is not installed or not enabled. It handles:
 * - Direct GPG encryption/decryption
 * - Git operations via direct subprocess calls
 * - GPG key management and recipient lists
 * - File-based password storage (.gpg files)
 * - Re-encryption when recipients change
 *
 * This is used as a fallback when RealPass cannot be initialized.
 */
class ImitatePass : public Pass, private simpleTransaction {
  Q_OBJECT

  friend class tst_util;

protected:
  /**
   * @brief Verify .gpg-id file exists and is valid.
   * @param file Path to check.
   * @return true if valid.
   */
  auto verifyGpgIdFile(const QString &file) -> bool;
  /**
   * @brief Remove directory recursively.
   * @param dirName Directory path.
   * @return true if removed.
   */
  auto removeDir(const QString &dirName) -> bool;
  /**
   * @brief Check if signing keys are valid.
   * @param signingKeys List of key IDs.
   * @return true if all keys valid.
   */
  auto checkSigningKeys(const QStringList &signingKeys) -> bool;
  /**
   * @brief Write recipients to .gpg-id file.
   * @param gpgIdFile Path to .gpg-id file.
   * @param users List of recipients.
   */
  void writeGpgIdFile(const QString &gpgIdFile, const QList<UserInfo> &users);
  /**
   * @brief Sign .gpg-id file with signing keys.
   * @param gpgIdFile Path to .gpg-id file.
   * @param signingKeys Key IDs to sign with.
   * @return true on success, false on failure.
   */
  auto signGpgIdFile(const QString &gpgIdFile, const QStringList &signingKeys)
      -> bool;
  /**
   * @brief Add .gpg-id to git staging.
   * @param gpgIdFile .gpg-id file path.
   * @param gpgIdSigFile Signature file path.
   * @param addFile Stage .gpg-id file.
   * @param addSigFile Stage signature file.
   */
  void gitAddGpgId(const QString &gpgIdFile, const QString &gpgIdSigFile,
                   bool addFile, bool addSigFile);
  /**
   * @brief Verify .gpg-id file for a directory.
   * @param file Password file path.
   * @param gpgIdFilesVerified List of already verified .gpg-id files.
   * @param gpgId Output parameter for recipient key IDs.
   * @return true on success, false on failure.
   */
  auto verifyGpgIdForDir(const QString &file, QStringList &gpgIdFilesVerified,
                         QStringList &gpgId) -> bool;
  /**
   * @brief Create git backup commit before re-encryption.
   * @return true if backup created or not needed, false if backup failed.
   */
  auto createBackupCommit() -> bool;
  /**
   * @brief Read recipients from file.
   * @param fileName Path to file.
   * @return List of key IDs.
   */
  auto getKeysFromFile(const QString &fileName) -> QStringList;
  /**
   * @brief Re-encrypt single file with new recipients.
   * @param fileName File to re-encrypt.
   * @param recipients New recipient key IDs.
   * @return true on success, false on failure.
   */
  auto reencryptSingleFile(const QString &fileName,
                           const QStringList &recipients) -> bool;
  /**
   * @brief Resolve destination for move operation.
   * @param src Source path.
   * @param dest Destination path.
   * @param force Overwrite existing.
   * @return Resolved destination path.
   */
  auto resolveMoveDestination(const QString &src, const QString &dest,
                              bool force) -> QString;

public:
  /**
   * @brief Execute git move operation.
   * @param src Source path.
   * @param destFile Destination path.
   * @param force Overwrite existing.
   */
  void executeMoveGit(const QString &src, const QString &destFile, bool force);

  /**
   * @brief Commit changes to git.
   * @param file Changed file path.
   * @param msg Commit message.
   */
  void gitCommit(const QString &file, const QString &msg);

  /**
   * @brief Execute git command.
   * @param id Process identifier.
   * @param args Git arguments.
   * @param input Input to stdin.
   * @param readStdout Capture stdout.
   * @param readStderr Capture stderr.
   */
  void executeGit(PROCESS id, const QStringList &args,
                  QString input = QString(), bool readStdout = true,
                  bool readStderr = true);
  /**
   * @brief Execute GPG command.
   * @param id Process identifier.
   * @param args GPG arguments.
   * @param input Input to stdin.
   * @param readStdout Capture stdout.
   * @param readStderr Capture stderr.
   */
  void executeGpg(PROCESS id, const QStringList &args,
                  QString input = QString(), bool readStdout = true,
                  bool readStderr = true);

  /**
   * @class transactionHelper
   * @brief RAII helper for wrapping operations in transactions.
   */
  class transactionHelper {
    simpleTransaction *m_transaction;
    PROCESS m_result;

  public:
    /**
     * @brief Start transaction.
     * @param trans Transaction object.
     * @param result Result code on commit.
     */
    transactionHelper(simpleTransaction *trans, PROCESS result)
        : m_transaction(trans), m_result(result) {
      m_transaction->transactionStart();
    }
    /**
     * @brief End transaction on destruction.
     */
    ~transactionHelper() { m_transaction->transactionEnd(m_result); }
  };

protected:
  /**
   * @brief Handle process completion.
   */
  void finished(int id, int exitCode, const QString &out,
                const QString &err) override;

  /**
   * @brief Execute command wrapper.
   */
  void executeWrapper(PROCESS id, const QString &app, const QStringList &args,
                      QString input, bool readStdout = true,
                      bool readStderr = true) override;

public:
  /**
   * @brief Construct ImitatePass instance.
   */
  ImitatePass();
  /**
   * @brief Destructor.
   */
  ~ImitatePass() override;

  // Git operations
  /**
   * @brief Initialize Git repository.
   */
  void GitInit() override;
  /**
   * @brief Pull from remote.
   */
  void GitPull() override;
  /**
   * @brief Pull with rebase.
   */
  void GitPull_b() override;
  /**
   * @brief Push to remote.
   */
  void GitPush() override;

  // Password operations
  /**
   * @brief Show decrypted password.
   */
  void Show(QString file) override;
  /**
   * @brief Generate OTP.
   */
  void OtpGenerate(QString file) override;
  /**
   * @brief Insert new password.
   */
  void Insert(QString file, QString newValue, bool overwrite = false) override;
  /**
   * @brief Remove password.
   */
  void Remove(QString file, bool isDir = false) override;
  /**
   * @brief Initialize store.
   */
  void Init(QString path, const QList<UserInfo> &users) override;

  /**
   * @brief Re-encrypt entire directory.
   * @param dir Directory path.
   */
  void reencryptPath(const QString &dir);

signals:
  /**
   * @brief Emitted before starting re-encryption.
   */
  void startReencryptPath();
  /**
   * @brief Emitted after finishing re-encryption.
   */
  void endReencryptPath();

  // Pass interface
public:
  /**
   * @brief Move password file.
   */
  void Move(const QString src, const QString dest,
            const bool force = false) override;
  /**
   * @brief Copy password file.
   */
  void Copy(const QString src, const QString dest,
            const bool force = false) override;
  /**
   * @brief Search all password content by GPG-decrypting each .gpg file.
   *
   * Pattern is interpreted as a QRegularExpression (PCRE-like), which differs
   * from RealPass::Grep which uses system `pass grep` (POSIX BRE via grep).
   * The same pattern may produce different matches across the two backends.
   *
   * @param pattern Search pattern (QRegularExpression).
   * @param caseInsensitive true for case-insensitive search.
   */
  void Grep(QString pattern, bool caseInsensitive = false) override;

private:
  int m_grepSeq = 0;
  QList<QThread *> m_grepThreads;

  static auto grepMatchFile(const QStringList &env, const QString &gpgExe,
                            const QString &filePath,
                            const QRegularExpression &rx) -> QStringList;
  static auto grepScanStore(const QStringList &env, const QString &gpgExe,
                            const QString &storeDir,
                            const QRegularExpression &rx)
      -> QList<QPair<QString, QStringList>>;
};

#endif // SRC_IMITATEPASS_H_
