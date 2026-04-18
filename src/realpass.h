// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_REALPASS_H_
#define SRC_REALPASS_H_

#include "pass.h"

/**
 * @class RealPass
 * @brief Implementation of Pass that wraps the 'pass' command-line tool.
 *
 * RealPass delegates all password store operations to the external 'pass'
 * utility. It provides a Qt-native interface while handling:
 * - Git integration (init, pull, push)
 * - Password CRUD (show, insert, remove, move, copy)
 * - OTP generation
 * - Store initialization with GPG keys
 *
 * This is the primary implementation when 'pass' is available on the system.
 */
class RealPass : public Pass {
  /**
   * @brief Execute pass command with arguments.
   * @param id Process identifier.
   * @param args Command arguments.
   * @param input Input to pass to stdin.
   * @param readStdout Capture stdout.
   * @param readStderr Capture stderr.
   */
  void executePass(PROCESS id, const QStringList &args,
                   QString input = QString(), bool readStdout = true,
                   bool readStderr = true);

public:
  /**
   * @brief Construct a RealPass instance.
   */
  RealPass();
  /**
   * @brief Destructor.
   */
  ~RealPass() override = default;

  // Git operations
  /**
   * @brief Initialize Git repository in password store.
   */
  void GitInit() override;
  /**
   * @brief Pull changes from remote.
   */
  void GitPull() override;
  /**
   * @brief Pull with rebase.
   */
  void GitPull_b() override;
  /**
   * @brief Push changes to remote.
   */
  void GitPush() override;

  // Password operations
  /**
   * @brief Show decrypted password.
   * @param file Path to password file.
   */
  void Show(QString file) override;
  /**
   * @brief Generate OTP code.
   * @param file Path to password file with OTP.
   */
  void OtpGenerate(QString file) override;
  /**
   * @brief Insert new password.
   * @param file Path to password file.
   * @param newValue Password content.
   * @param overwrite true to overwrite existing.
   */
  void Insert(QString file, QString newValue, bool overwrite = false) override;
  /**
   * @brief Remove password or directory.
   * @param file Path to remove.
   * @param isDir true if removing directory.
   */
  void Remove(QString file, bool isDir = false) override;
  /**
   * @brief Initialize password store.
   * @param path Store root path.
   * @param users GPG recipients.
   */
  void Init(QString path, const QList<UserInfo> &users) override;

  // Pass interface
public:
  /**
   * @brief Move password file or directory.
   * @param src Source path.
   * @param dest Destination path.
   * @param force Overwrite existing.
   */
  void Move(const QString src, const QString dest,
            const bool force = false) override;
  /**
   * @brief Copy password file or directory.
   * @param src Source path.
   * @param dest Destination path.
   * @param force Overwrite existing.
   */
  void Copy(const QString src, const QString dest,
            const bool force = false) override;
  /**
   * @brief Search password content via 'pass grep'.
   * @param pattern Search pattern.
   * @param caseInsensitive true for case-insensitive search.
   */
  void Grep(QString pattern, bool caseInsensitive = false) override;
};

#endif // SRC_REALPASS_H_
