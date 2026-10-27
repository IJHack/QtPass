// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_REALPASS_H_
#define SRC_REALPASS_H_

#include "nativegrep.h"
#include "pass.h"
#include <QQueue>

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
  /// The store's own search: `pass grep` runs `find -L`, which follows a
  /// link out of the store and decrypts what it finds there.
  NativeGrep m_grep;
  /// Store-relative paths of linked folders whose removal commit is queued
  /// (as GIT_COMMIT), oldest first; finished() judges each.
  QQueue<QString> m_linkCommits;

  /**
   * @brief Judge a queued linked-folder removal commit by what is left
   * staged: `pass git` exits 0 whatever git did.
   * @param rel The folder, relative to the store.
   * @param out The commit's standard output.
   * @param err The commit's standard error.
   */
  void finishLinkCommit(const QString &rel, const QString &out,
                        const QString &err);

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
  /**
   * @brief Shared implementation of Move and Copy via `pass mv`/`pass cp`.
   * @param id Process id (PASS_MOVE or PASS_COPY).
   * @param subcommand The pass subcommand, "mv" or "cp".
   * @param src Source path.
   * @param dest Destination path.
   * @param force Overwrite an existing destination; without it a
   *        file-onto-file request is a no-op (pass would otherwise force).
   */
  void passMoveOrCopy(PROCESS id, const QString &subcommand, const QString &src,
                      const QString &dest, bool force);
  /**
   * @brief @p path as `pass` names it: relative to the store, without the
   * `.gpg` pass appends itself.
   * @param path The file or folder, absolute or relative to the working
   *        directory.
   * @param stripGpg Whether to drop a trailing `.gpg`.
   * @return The store-relative name.
   */
  auto passName(const QString &path, bool stripGpg) const -> QString;

  /**
   * @brief Whether the failed `pass insert` wrote its entry before its git
   * step failed. pass does not say, so: the entry did not exist when the
   * insert started and does now.
   * @return true when a new entry appeared.
   */
  [[nodiscard]] auto insertWritten() const -> bool override;

  /// The entry the last Insert() writes, and whether it existed beforehand.
  QString m_insertTarget;
  bool m_insertTargetExisted = false;

public:
  /**
   * @brief Construct a RealPass instance.
   */
  RealPass();
  /**
   * @brief Destructor; lets the search workers wind down.
   */
  ~RealPass() override;

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
   * @brief Insert new password.
   * @param file Path to password file.
   * @param newValue Password content.
   * @param overwrite true to overwrite existing.
   */
  void Insert(QString file, QString newValue, bool overwrite) override;
  /**
   * @brief Remove password or directory.
   * @param file Path to remove.
   * @param isDir true if removing directory.
   */
  void Remove(QString file, bool isDir) override;
  /**
   * @brief Initialize password store.
   * @param path Store root path.
   * @param users GPG recipients.
   */
  void Init(QString path, const QList<UserInfo> &users) override;

protected:
  /**
   * @brief Finish a process; a linked-folder removal commit (GIT_COMMIT,
   * which RealPass uses for nothing else) is judged by finishLinkCommit().
   * @param id Process identifier.
   * @param exitCode Exit code.
   * @param out Standard output.
   * @param err Standard error.
   */
  void finished(int id, int exitCode, const QString &out,
                const QString &err) override;

  // Pass interface
public:
  /**
   * @brief Move password file or directory.
   * @param src Source path.
   * @param dest Destination path.
   * @param force Overwrite existing.
   */
  void Move(const QString src, const QString dest, const bool force) override;
  /**
   * @brief Copy password file or directory.
   * @param src Source path.
   * @param dest Destination path.
   * @param force Overwrite existing.
   */
  void Copy(const QString src, const QString dest, const bool force) override;
  /**
   * @brief Search all password content by GPG-decrypting each real `.gpg`
   * file, as ImitatePass does.
   *
   * Not `pass grep`: that enumerates with `find -L`, which follows a link
   * out of the store and decrypts what it finds there. Same pattern
   * semantics as ImitatePass::Grep (QRegularExpression).
   * @param pattern Search pattern (QRegularExpression).
   * @param caseInsensitive true for case-insensitive search.
   */
  void Grep(QString pattern, bool caseInsensitive) override;
};

#endif // SRC_REALPASS_H_
