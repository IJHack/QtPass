// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_PASS_H_
#define SRC_PASS_H_

#include "enums.h"
#include "executor.h"
#include "userinfo.h"

#include <QProcess>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <cassert>
#include <map>

/**
 * @struct ResolvedGpgconfCommand
 * @brief Holds a resolved gpgconf executable path and its arguments.
 */
struct ResolvedGpgconfCommand {
  /**
   * @brief Full path to the gpgconf executable.
   */
  QString program;
  /**
   * @brief Arguments to pass to gpgconf (e.g. ["--kill", "gpg-agent"]).
   */
  QStringList arguments;
};

/**
 * @class Pass
 * @brief Abstract base class for password store operations.
 *
 * Pass provides an abstraction layer for password management, supporting both
 * the native 'pass' utility and gopass/imitation backends. It handles:
 * - Password file operations (show, insert, remove, move, copy)
 * - GPG key management and Git integration
 * - OTP generation
 * - Random password generation
 *
 * Subclasses must implement Git and password operations.
 */
class Pass : public QObject {
  Q_OBJECT

  bool wrapperRunning;
  QStringList env;

protected:
  /**
   * @brief Internal command executor for queuing and running subprocesses.
   */
  Executor exec;

  /**
   * @brief Alias for Enums::PROCESS used throughout this class.
   */
  using PROCESS = Enums::PROCESS;

public:
  /**
   * @brief Construct a Pass instance.
   */
  Pass();
  /**
   * @brief Initialize the Pass instance.
   */
  void init();
  /**
   * @brief Check if GPG supports Ed25519 encryption.
   * @return true if Ed25519 is supported.
   */
  static bool gpgSupportsEd25519();
  /**
   * @brief Get default key template for new GPG keys.
   * @return Default template string.
   */
  static QString getDefaultKeyTemplate();

  ~Pass() override = default;

  // Git operations
  /**
   * @brief Initialize Git repository in password store.
   */
  virtual void GitInit() = 0;
  /**
   * @brief Pull changes from remote Git repository.
   */
  virtual void GitPull() = 0;
  /**
   * @brief Pull with rebase from remote.
   */
  virtual void GitPull_b() = 0;
  /**
   * @brief Push changes to remote Git repository.
   */
  virtual void GitPush() = 0;

  // Password operations
  /**
   * @brief Show decrypted password file.
   * @param file Path to password file relative to store root.
   */
  virtual void Show(QString file) = 0;
  /**
   * @brief Generate OTP for password file.
   * @param file Path to password file.
   */
  virtual void OtpGenerate(QString file) = 0;
  /**
   * @brief Insert or update password.
   * @param file Path to password file.
   * @param value Password content to store.
   * @param force Overwrite existing file.
   */
  virtual void Insert(QString file, QString value, bool force) = 0;
  /**
   * @brief Remove password file or directory.
   * @param file Path to remove.
   * @param isDir true if removing a directory.
   */
  virtual void Remove(QString file, bool isDir) = 0;
  /**
   * @brief Move password file or directory.
   * @param srcDir Source path.
   * @param dest Destination path.
   * @param force Overwrite existing.
   */
  virtual void Move(const QString srcDir, const QString dest,
                    const bool force = false) = 0;
  /**
   * @brief Copy password file or directory.
   * @param srcDir Source path.
   * @param dest Destination path.
   * @param force Overwrite existing.
   */
  virtual void Copy(const QString srcDir, const QString dest,
                    const bool force = false) = 0;
  /**
   * @brief Initialize new password store.
   * @param path Root of password store.
   * @param users List of recipient GPG keys.
   */
  virtual void Init(QString path, const QList<UserInfo> &users) = 0;
  /**
   * @brief Search password content for a pattern.
   * @param pattern Search pattern (regular expression).
   * @param caseInsensitive true for case-insensitive search.
   */
  virtual void Grep(QString pattern, bool caseInsensitive = false) = 0;

  /**
   * @brief Generate random password.
   * @param length Password length.
   * @param charset Character set to use.
   * @return Generated password.
   */
  virtual auto generatePassword(unsigned int length, const QString &charset)
      -> QString;

  // GPG operations
  /**
   * @brief Generate GPG keys using batch script.
   * @param batch GPG batch script content.
   */
  void GenerateGPGKeys(QString batch);
  /**
   * @brief List GPG keys matching patterns.
   * @param keystrings List of search patterns.
   * @param secret Include secret keys.
   * @return List of matching keys.
   */
  auto listKeys(QStringList keystrings, bool secret = false) -> QList<UserInfo>;
  /**
   * @brief List GPG keys.
   * @param keystring Search pattern.
   * @param secret Include secret keys.
   * @return List of matching keys.
   */
  auto listKeys(const QString &keystring = "", bool secret = false)
      -> QList<UserInfo>;
  /**
   * @brief Update environment for subprocesses.
   */
  void updateEnv();

  // Static helpers
  /**
   * @brief Resolve the gpgconf command to kill agents.
   * @param gpgPath Path to gpg executable.
   * @return Resolved command with program and arguments.
   */
  static auto resolveGpgconfCommand(const QString &gpgPath)
      -> ResolvedGpgconfCommand;
  /**
   * @brief Get .gpg-id file path for a password file.
   * @param for_file Path to password file.
   * @return Path to .gpg-id file.
   */
  static auto getGpgIdPath(const QString &for_file) -> QString;
  /**
   * @brief Get list of recipients for a password file.
   * @param for_file Path to password file.
   * @return List of recipient key IDs.
   */
  static auto getRecipientList(const QString &for_file) -> QStringList;
  /**
   * @brief Get recipients as string.
   * @param for_file Path to password file.
   * @param separator Separator between recipients.
   * @param count Pointer to store recipient count.
   * @return List of recipient key IDs.
   */
  static auto getRecipientString(const QString &for_file,
                                 const QString &separator = " ",
                                 int *count = nullptr) -> QStringList;

protected:
  /**
   * @brief Execute external wrapper command.
   * @param id Process identifier.
   * @param app Executable path.
   * @param args Command arguments.
   * @param readStdout Capture stdout.
   * @param readStderr Capture stderr.
   */
  void executeWrapper(PROCESS id, const QString &app, const QStringList &args,
                      bool readStdout = true, bool readStderr = true);
  /**
   * @brief Generate random password from charset.
   * @param charset Character set.
   * @param length Password length.
   * @return Generated password.
   */
  auto generateRandomPassword(const QString &charset, unsigned int length)
      -> QString;
  /**
   * @brief Generate random number in range.
   * @param bound Upper bound (exclusive).
   * @return Random number.
   */
  auto boundedRandom(quint32 bound) -> quint32;
  /**
   * @brief Set or remove an environment variable.
   * @param key Variable name including trailing '='.
   * @param value New value; empty string removes the variable.
   */
  void setEnvVar(const QString &key, const QString &value);

  /**
   * @brief Execute wrapper with input.
   * @param id Process identifier.
   * @param app Executable path.
   * @param args Command arguments.
   * @param input Input to pass to stdin.
   * @param readStdout Capture stdout.
   * @param readStderr Capture stderr.
   */
  virtual void executeWrapper(PROCESS id, const QString &app,
                              const QStringList &args, QString input,
                              bool readStdout = true, bool readStderr = true);

protected slots:
  /**
   * @brief Handle process completion.
   * @param id Process identifier.
   * @param exitCode Process exit code.
   * @param out Standard output.
   * @param err Standard error.
   */
  virtual void finished(int id, int exitCode, const QString &out,
                        const QString &err);

private:
  void handleProcessError(PROCESS pid, int exitCode, const QString &out,
                          const QString &err);
  void handleGrepError(int exitCode, const QString &err);
  auto formatInsertError(const QString &friendly, const QString &err)
      -> QString;
  void emitProcessFinishedSignal(PROCESS pid, const QString &out,
                                 const QString &err);

signals:
  /**
   * @brief Emitted when a process error occurs.
   */
  void error(QProcess::ProcessError);
  /**
   * @brief Emitted before executing a command.
   */
  void startingExecuteWrapper();
  /**
   * @brief Emit status message.
   * @param msg Status text.
   * @param timeout Display duration in milliseconds.
   */
  void statusMsg(const QString &msg, int timeout);
  /**
   * @brief Emit critical error.
   * @param title Error dialog title.
   * @param message Error description.
   */
  void critical(const QString &title, const QString &message);

  /**
   * @brief Emitted on process error exit.
   */
  void processErrorExit(int exitCode, const QString &err);

  /**
   * @brief Emitted when any operation finishes.
   */
  void finishedAny(const QString &, const QString &);
  /**
   * @brief Emitted when Git init finishes.
   */
  void finishedGitInit(const QString &, const QString &);
  /**
   * @brief Emitted when Git pull finishes.
   */
  void finishedGitPull(const QString &, const QString &);
  /**
   * @brief Emitted when Git push finishes.
   */
  void finishedGitPush(const QString &, const QString &);
  /**
   * @brief Emitted when show finishes.
   */
  void finishedShow(const QString &);
  /**
   * @brief Emitted when OTP generation finishes.
   */
  void finishedOtpGenerate(const QString &);
  /**
   * @brief Emitted when insert finishes.
   */
  void finishedInsert(const QString &, const QString &);
  /**
   * @brief Emitted when remove finishes.
   */
  void finishedRemove(const QString &, const QString &);
  /**
   * @brief Emitted when init finishes.
   */
  void finishedInit(const QString &, const QString &);
  /**
   * @brief Emitted when move finishes.
   */
  void finishedMove(const QString &, const QString &);
  /**
   * @brief Emitted when copy finishes.
   */
  void finishedCopy(const QString &, const QString &);
  /**
   * @brief Emitted when generate finishes.
   */
  void finishedGenerate(const QString &, const QString &);
  /**
   * @brief Emitted when GPG key generation finishes.
   */
  void finishedGenerateGPGKeys(const QString &, const QString &);
  /**
   * @brief Emitted when grep finishes with matching results.
   */
  void finishedGrep(const QList<QPair<QString, QStringList>> &results);
};

/**
 * @brief Maps GPG stderr (which may include --status-fd 2 tokens) to a
 * translated user-friendly encryption error string.
 *
 * Checks machine-readable [GNUPG:] status tokens first (locale-independent),
 * then falls back to case-insensitive substring matching for GPG builds that
 * do not emit status tokens. Returns an empty string when no known pattern
 * matches.
 *
 * Declared here so it can be exercised by unit tests without linking the full
 * Pass object.
 */
QString gpgErrorMessage(const QString &err);

/**
 * @brief Parses 'pass grep' raw output into (entry, matches) pairs.
 *
 * Declared here so it can be exercised by unit tests without linking the full
 * Pass object.
 */
QList<QPair<QString, QStringList>> parseGrepOutput(const QString &rawOut);

#endif // SRC_PASS_H_
