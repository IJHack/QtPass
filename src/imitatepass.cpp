// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "imitatepass.h"
#include "executor.h"
#include "qtpasssettings.h"
#include "util.h"
#include <QDirIterator>
#include <QPointer>
#include <QRegularExpression>
#include <QThread>
#include <utility>

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

using Enums::CLIPBOARD_ALWAYS;
using Enums::CLIPBOARD_NEVER;
using Enums::CLIPBOARD_ON_DEMAND;
using Enums::GIT_ADD;
using Enums::GIT_COMMIT;
using Enums::GIT_COPY;
using Enums::GIT_INIT;
using Enums::GIT_MOVE;
using Enums::GIT_PULL;
using Enums::GIT_PUSH;
using Enums::GIT_RM;
using Enums::GPG_GENKEYS;
using Enums::INVALID;
using Enums::PASS_COPY;
using Enums::PASS_GREP;
using Enums::PASS_INIT;
using Enums::PASS_INSERT;
using Enums::PASS_MOVE;
using Enums::PASS_OTP_GENERATE;
using Enums::PASS_REMOVE;
using Enums::PASS_SHOW;
using Enums::PROCESS_COUNT;

/**
 * @brief ImitatePass::ImitatePass for situations when pass is not available
 * we imitate the behavior of pass https://www.passwordstore.org/
 */
ImitatePass::ImitatePass() = default;

static auto pgit(const QString &path) -> QString {
  if (!QtPassSettings::getGitExecutable().startsWith("wsl ")) {
    return path;
  }
  QString res = "$(wslpath " + path + ")";
  return res.replace('\\', '/');
}

static auto pgpg(const QString &path) -> QString {
  if (!QtPassSettings::getGpgExecutable().startsWith("wsl ")) {
    return path;
  }
  QString res = "$(wslpath " + path + ")";
  return res.replace('\\', '/');
}

/**
 * @brief ImitatePass::GitInit git init wrapper
 */
void ImitatePass::GitInit() {
  executeGit(GIT_INIT, {"init", pgit(QtPassSettings::getPassStore())});
}

/**
 * @brief ImitatePass::GitPull git pull wrapper
 */
void ImitatePass::GitPull() { executeGit(GIT_PULL, {"pull"}); }

/**
 * @brief ImitatePass::GitPull_b git pull wrapper
 */
void ImitatePass::GitPull_b() {
  Executor::executeBlocking(QtPassSettings::getGitExecutable(), {"pull"});
}

/**
 * @brief ImitatePass::GitPush git push wrapper
 */
void ImitatePass::GitPush() {
  if (QtPassSettings::isUseGit()) {
    executeGit(GIT_PUSH, {"push"});
  }
}

/**
 * @brief ImitatePass::Show shows content of file
 */
void ImitatePass::Show(QString file) {
  file = QtPassSettings::getPassStore() + file + ".gpg";
  QStringList args = {"-d",      "--quiet",     "--yes",   "--no-encrypt-to",
                      "--batch", "--use-agent", pgpg(file)};
  executeGpg(PASS_SHOW, args);
}

/**
 * @brief ImitatePass::OtpGenerate generates an otp code
 */
void ImitatePass::OtpGenerate(QString file) {
#ifdef QT_DEBUG
  dbg() << "No OTP generation code for fake pass yet, attempting for file: " +
               file;
#else
  Q_UNUSED(file)
#endif
}

/**
 * @brief ImitatePass::Insert create new file with encrypted content
 *
 * @param file      file to be created
 * @param newValue  value to be stored in file
 * @param overwrite whether to overwrite existing file
 */
void ImitatePass::Insert(QString file, QString newValue, bool overwrite) {
  file = file + ".gpg";
  QString gpgIdPath = Pass::getGpgIdPath(file);
  if (!verifyGpgIdFile(gpgIdPath)) {
    emit critical(tr("Check .gpgid file signature!"),
                  tr("Signature for %1 is invalid.").arg(gpgIdPath));
    return;
  }
  transactionHelper trans(this, PASS_INSERT);
  QStringList recipients = Pass::getRecipientList(file);
  if (recipients.isEmpty()) {
    // Already emit critical signal to notify user of error - no need to throw
    emit critical(tr("Can not edit"),
                  tr("Could not read encryption key to use, .gpg-id "
                     "file missing or invalid."));
    return;
  }
  QStringList args = {"--batch", "--status-fd", "2",
                      "-eq",     "--output",    pgpg(file)};
  for (auto &r : recipients) {
    args.append("-r");
    args.append(r);
  }
  if (overwrite) {
    args.append("--yes");
  }
  args.append("-");
  executeGpg(PASS_INSERT, args, newValue);
  if (!QtPassSettings::isUseWebDav() && QtPassSettings::isUseGit()) {
    // Git is used when enabled - this is the standard pass workflow
    if (!overwrite) {
      executeGit(GIT_ADD, {"add", pgit(file)});
    }
    QString path = QDir(QtPassSettings::getPassStore()).relativeFilePath(file);
    path.replace(Util::endsWithGpg(), "");
    QString msg =
        QString(overwrite ? "Edit" : "Add") + " for " + path + " using QtPass.";
    gitCommit(file, msg);
  }
}

/**
 * @brief ImitatePass::gitCommit commit a file to git with an appropriate commit
 * message
 * @param file
 * @param msg
 */
void ImitatePass::gitCommit(const QString &file, const QString &msg) {
  if (file.isEmpty()) {
    executeGit(GIT_COMMIT, {"commit", "-m", msg});
  } else {
    executeGit(GIT_COMMIT, {"commit", "-m", msg, "--", pgit(file)});
  }
}

/**
 * @brief ImitatePass::Remove custom implementation of "pass remove"
 */
void ImitatePass::Remove(QString file, bool isDir) {
  file = QtPassSettings::getPassStore() + file;
  transactionHelper trans(this, PASS_REMOVE);
  if (!isDir) {
    file += ".gpg";
  }
  if (QtPassSettings::isUseGit()) {
    executeGit(GIT_RM, {"rm", (isDir ? "-rf" : "-f"), pgit(file)});
    // Normalize path the same way as add/edit operations
    QString path = QDir(QtPassSettings::getPassStore()).relativeFilePath(file);
    path.replace(Util::endsWithGpg(), "");
    gitCommit(file, "Remove for " + path + " using QtPass.");
  } else {
    if (isDir) {
      QDir dir(file);
      dir.removeRecursively();
    } else {
      QFile(file).remove();
    }
  }
}

/**
 * @brief ImitatePass::Init initialize pass repository
 *
 * @param path      path in which new password-store will be created
 * @param users     list of users who shall be able to decrypt passwords in
 * path
 */
auto ImitatePass::checkSigningKeys(const QStringList &signingKeys) -> bool {
  QString out;
  QStringList args =
      QStringList{"--status-fd=1", "--list-secret-keys"} + signingKeys;
  int result =
      Executor::executeBlocking(QtPassSettings::getGpgExecutable(), args, &out);
  if (result != 0) {
#ifdef QT_DEBUG
    dbg() << "GPG list-secret-keys failed with code:" << result;
#endif
    return false;
  }
  for (auto &key : signingKeys) {
    if (out.contains("[GNUPG:] KEY_CONSIDERED " + key)) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Writes the selected users' GPG key IDs to a .gpg-id file.
 * @details Opens the specified file for writing, stores the key ID of each
 * enabled user on a separate line, and warns if none of the selected users has
 * a secret key available.
 *
 * @param QString &gpgIdFile - Path to the .gpg-id file to be written.
 * @param QList<UserInfo> &users - List of users to evaluate and write to the
 * file.
 * @return void - This function does not return a value.
 *
 */
void ImitatePass::writeGpgIdFile(const QString &gpgIdFile,
                                 const QList<UserInfo> &users) {
  QFile gpgId(gpgIdFile);
  if (!gpgId.open(QIODevice::WriteOnly | QIODevice::Text)) {
    emit critical(tr("Cannot update"),
                  tr("Failed to open .gpg-id for writing."));
    return;
  }
  bool secret_selected = false;
  for (const UserInfo &user : users) {
    if (user.enabled) {
      gpgId.write((user.key_id + "\n").toUtf8());
      secret_selected |= user.have_secret;
    }
  }
  gpgId.close();
  if (!secret_selected) {
    emit critical(
        tr("Check selected users!"),
        tr("None of the selected keys have a secret key available.\n"
           "You will not be able to decrypt any newly added passwords!"));
  }
}

/**
 * @brief Signs a GPG ID file and verifies its signature.
 * @example
 * bool result = ImitatePass::signGpgIdFile(gpgIdFile, signingKeys);
 * std::cout << result << std::endl; // Expected output: true if signing and
 * verification succeed
 *
 * @param QString &gpgIdFile - Path to the .gpgid file to be signed.
 * @param QStringList &signingKeys - List of signing keys; only the first key is
 * used.
 * @return bool - True if the file was signed and its signature verified
 * successfully; otherwise false.
 */
auto ImitatePass::signGpgIdFile(const QString &gpgIdFile,
                                const QStringList &signingKeys) -> bool {
  QStringList args;
  // Use only the first signing key; multiple --default-key options would
  // override each other and only the last one would take effect.
  if (!signingKeys.isEmpty()) {
#ifdef QT_DEBUG
    if (signingKeys.size() > 1) {
      dbg() << "Multiple signing keys configured; using only the first key:"
            << signingKeys.first();
    }
#endif
    args.append(QStringList{"--default-key", signingKeys.first()});
  }
  args.append(QStringList{"--yes", "--detach-sign", gpgIdFile});
  int result =
      Executor::executeBlocking(QtPassSettings::getGpgExecutable(), args);
  if (result != 0) {
#ifdef QT_DEBUG
    dbg() << "GPG signing failed with code:" << result;
#endif
    emit critical(tr("GPG signing failed!"),
                  tr("Failed to sign %1.").arg(gpgIdFile));
    return false;
  }
  if (!verifyGpgIdFile(gpgIdFile)) {
    emit critical(tr("Check .gpgid file signature!"),
                  tr("Signature for %1 is invalid.").arg(gpgIdFile));
    return false;
  }
  return true;
}

/**
 * @brief Adds a GPG ID file and optionally its signature file to git, then
 * creates corresponding commit(s).
 * @example
 * void result = ImitatePass::gitAddGpgId(gpgIdFile, gpgIdSigFile, true, true);
 *
 * @param const QString &gpgIdFile - Path to the GPG ID file to add and commit.
 * @param const QString &gpgIdSigFile - Path to the signature file associated
 * with the GPG ID file.
 * @param bool addFile - Whether to stage and commit the GPG ID file.
 * @param bool addSigFile - Whether to stage and commit the signature file.
 * @return void - This function does not return a value.
 */
void ImitatePass::gitAddGpgId(const QString &gpgIdFile,
                              const QString &gpgIdSigFile, bool addFile,
                              bool addSigFile) {
  if (addFile) {
    executeGit(GIT_ADD, {"add", pgit(gpgIdFile)});
  }
  QString commitPath = gpgIdFile;
  commitPath.replace(Util::endsWithGpg(), "");
  gitCommit(gpgIdFile, "Added " + commitPath + " using QtPass.");
  if (!addSigFile) {
    return;
  }
  executeGit(GIT_ADD, {"add", pgit(gpgIdSigFile)});
  commitPath = gpgIdSigFile;
  commitPath.replace(QRegularExpression("\\.gpg$"), "");
  gitCommit(gpgIdSigFile, "Added " + commitPath + " using QtPass.");
}

/**
 * @brief Initializes the pass entry by writing and optionally signing the GPG
 * ID files.
 *
 * @example
 * void result = ImitatePass::Init(path, users);
 *
 * @param QString path - Base path for the pass entry where ".gpg-id" and
 * optional signature files are created.
 * @param const QList<UserInfo> &users - List of users whose keys are written
 * into the GPG ID file.
 * @return void - No return value.
 */
void ImitatePass::Init(QString path, const QList<UserInfo> &users) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  QStringList signingKeys =
      QtPassSettings::getPassSigningKey().split(" ", Qt::SkipEmptyParts);
#else
  QStringList signingKeys =
      QtPassSettings::getPassSigningKey().split(" ", QString::SkipEmptyParts);
#endif
  QString gpgIdSigFile = path + ".gpg-id.sig";
  bool addSigFile = false;
  if (!signingKeys.isEmpty()) {
    if (!checkSigningKeys(signingKeys)) {
      emit critical(tr("No signing key!"),
                    tr("None of the secret signing keys is available.\n"
                       "You will not be able to change the user list!"));
      return;
    }
    QFileInfo checkFile(gpgIdSigFile);
    if (!checkFile.exists() || !checkFile.isFile()) {
      addSigFile = true;
    }
  }

  QString gpgIdFile = path + ".gpg-id";
  bool addFile = false;
  transactionHelper trans(this, PASS_INIT);
  if (QtPassSettings::isAddGPGId(true)) {
    QFileInfo checkFile(gpgIdFile);
    if (!checkFile.exists() || !checkFile.isFile()) {
      addFile = true;
    }
  }
  writeGpgIdFile(gpgIdFile, users);

  if (!signingKeys.isEmpty()) {
    if (!signGpgIdFile(gpgIdFile, signingKeys)) {
      return;
    }
  }

  if (!QtPassSettings::isUseWebDav() && QtPassSettings::isUseGit() &&
      !QtPassSettings::getGitExecutable().isEmpty()) {
    gitAddGpgId(gpgIdFile, gpgIdSigFile, addFile, addSigFile);
  }
  reencryptPath(path);
}

/**
 * @brief ImitatePass::verifyGpgIdFile verify detached gpgid file signature.
 * @param file which gpgid file.
 * @return was verification successful?
 */
auto ImitatePass::verifyGpgIdFile(const QString &file) -> bool {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  QStringList signingKeys =
      QtPassSettings::getPassSigningKey().split(" ", Qt::SkipEmptyParts);
#else
  QStringList signingKeys =
      QtPassSettings::getPassSigningKey().split(" ", QString::SkipEmptyParts);
#endif
  if (signingKeys.isEmpty()) {
    return true;
  }
  QString out;
  QStringList args =
      QStringList{"--verify", "--status-fd=1", pgpg(file) + ".sig", pgpg(file)};
  int result =
      Executor::executeBlocking(QtPassSettings::getGpgExecutable(), args, &out);
  if (result != 0) {
#ifdef QT_DEBUG
    dbg() << "GPG verify failed with code:" << result;
#endif
    return false;
  }
  QRegularExpression re(
      R"(^\[GNUPG:\] VALIDSIG ([A-F0-9]{40}) .* ([A-F0-9]{40})\r?$)",
      QRegularExpression::MultilineOption);
  QRegularExpressionMatch m = re.match(out);
  if (!m.hasMatch()) {
    return false;
  }
  QStringList fingerprints = m.capturedTexts();
  fingerprints.removeFirst();
  for (auto &key : signingKeys) {
    if (fingerprints.contains(key)) {
      return true;
    }
  }
  return false;
}

/**
 * @brief ImitatePass::removeDir delete folder recursive.
 * @param dirName which folder.
 * @return was removal successful?
 */
auto ImitatePass::removeDir(const QString &dirName) -> bool {
  bool result = true;
  QDir dir(dirName);

  if (dir.exists(dirName)) {
    for (const QFileInfo &info :
         dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden |
                               QDir::AllDirs | QDir::Files,
                           QDir::DirsFirst)) {
      if (info.isDir()) {
        result = removeDir(info.absoluteFilePath());
      } else {
        result = QFile::remove(info.absoluteFilePath());
      }

      if (!result) {
        return result;
      }
    }
    result = dir.rmdir(dirName);
  }
  return result;
}

/**
 * @brief ImitatePass::reencryptPath reencrypt all files under the chosen
 * directory
 *
 * This is still quite experimental..
 * @param dir
 */
auto ImitatePass::verifyGpgIdForDir(const QString &file,
                                    QStringList &gpgIdFilesVerified,
                                    QStringList &gpgId) -> bool {
  QString gpgIdPath = Pass::getGpgIdPath(file);
  if (gpgIdFilesVerified.contains(gpgIdPath)) {
    return true;
  }
  if (!verifyGpgIdFile(gpgIdPath)) {
    emit critical(tr("Check .gpgid file signature!"),
                  tr("Signature for %1 is invalid.").arg(gpgIdPath));
    return false;
  }
  gpgIdFilesVerified.append(gpgIdPath);
  gpgId = getRecipientList(file);
  gpgId.sort();
  return true;
}

/**
 * @brief Extracts and returns a sorted list of valid key IDs from a GPG key
 * listing file.
 * @example
 * QStringList result = ImitatePass::getKeysFromFile(fileName);
 * std::cout << result.join(", ").toStdString() << std::endl;
 *
 * @param fileName - Path to the file used to query and parse GPG key
 * information.
 * @return QStringList - A sorted list of 16-character key IDs found in the
 * file.
 */
auto ImitatePass::getKeysFromFile(const QString &fileName) -> QStringList {
  QStringList args = {
      "-v",          "--no-secmem-warning", "--no-permission-warning",
      "--list-only", "--keyid-format=long", pgpg(fileName)};
  QString keys;
  QString err;
  const int result = Executor::executeBlocking(
      QtPassSettings::getGpgExecutable(), args, &keys, &err);
  if (result != 0 && keys.isEmpty() && err.isEmpty()) {
    return QStringList();
  }
  QStringList actualKeys;
  keys += err;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  QStringList key = keys.split(Util::newLinesRegex(), Qt::SkipEmptyParts);
#else
  QStringList key = keys.split(Util::newLinesRegex(), QString::SkipEmptyParts);
#endif
  QListIterator<QString> itr(key);
  while (itr.hasNext()) {
    QString current = itr.next();
    QStringList cur = current.split(" ");
    if (cur.length() > 4) {
      QString actualKey = cur.takeAt(4);
      if (actualKey.length() == 16) {
        actualKeys << actualKey;
      }
    }
  }
  actualKeys.sort();
  return actualKeys;
}

/**
 * @brief Re-encrypts a single encrypted file for a new set of recipients.
 * @example
 * bool result = ImitatePass::reencryptSingleFile(fileName, recipients);
 * std::cout << result << std::endl; // Expected output: true on success, false
 * on failure
 *
 * @param const QString &fileName - Path to the encrypted file to re-encrypt.
 * @param const QStringList &recipients - List of recipient keys to encrypt the
 * file to.
 * @return bool - True if the file was successfully decrypted, re-encrypted,
 * verified, and replaced; otherwise false.
 */
auto ImitatePass::reencryptSingleFile(const QString &fileName,
                                      const QStringList &recipients) -> bool {
#ifdef QT_DEBUG
  dbg() << "reencrypt " << fileName << " for " << recipients;
#endif
  QString local_lastDecrypt;
  QStringList args = {
      "-d",      "--quiet",     "--yes",       "--no-encrypt-to",
      "--batch", "--use-agent", pgpg(fileName)};
  int result = Executor::executeBlocking(QtPassSettings::getGpgExecutable(),
                                         args, &local_lastDecrypt);

  if (result != 0 || local_lastDecrypt.isEmpty()) {
#ifdef QT_DEBUG
    dbg() << "Decrypt error on re-encrypt for:" << fileName;
#endif
    return false;
  }

  if (local_lastDecrypt.right(1) != "\n") {
    local_lastDecrypt += "\n";
  }

  // Use passed recipients instead of re-reading from file
  if (recipients.isEmpty()) {
    emit critical(tr("Can not edit"),
                  tr("Could not read encryption key to use, .gpg-id "
                     "file missing or invalid."));
    return false;
  }

  // Encrypt to temporary file for atomic replacement
  QString tempPath = fileName + ".reencrypt.tmp";
  args = QStringList{"--yes", "--batch", "-eq", "--output", pgpg(tempPath)};
  for (const auto &i : recipients) {
    args.append("-r");
    args.append(i);
  }
  args.append("-");
  result = Executor::executeBlocking(QtPassSettings::getGpgExecutable(), args,
                                     local_lastDecrypt);

  if (result != 0) {
#ifdef QT_DEBUG
    dbg() << "Encrypt error on re-encrypt for:" << fileName;
#endif
    QFile::remove(tempPath);
    return false;
  }

  // Verify encryption worked by attempting to decrypt the temp file
  QString verifyOutput;
  args = QStringList{"-d", "--quiet", "--batch", "--use-agent", pgpg(tempPath)};
  result = Executor::executeBlocking(QtPassSettings::getGpgExecutable(), args,
                                     &verifyOutput);
  if (result != 0 || verifyOutput.isEmpty()) {
#ifdef QT_DEBUG
    dbg() << "Verification failed for:" << tempPath;
#endif
    QFile::remove(tempPath);
    return false;
  }
  // Verify content matches original decrypted content (defense in depth)
  if (verifyOutput.trimmed() != local_lastDecrypt.trimmed()) {
#ifdef QT_DEBUG
    dbg() << "Verification content mismatch for:" << tempPath;
#endif
    QFile::remove(tempPath);
    return false;
  }

  // Atomic replace with backup: rename original to .bak, rename temp to
  // original, then remove backup
  QString backupPath = fileName + ".reencrypt.bak";
  if (!QFile::rename(fileName, backupPath)) {
#ifdef QT_DEBUG
    dbg() << "Failed to backup original file:" << fileName;
#endif
    QFile::remove(tempPath);
    return false;
  }
  if (!QFile::rename(tempPath, fileName)) {
#ifdef QT_DEBUG
    dbg() << "Failed to rename temp file to:" << fileName;
#endif
    // Restore backup and clean up temp file
    QFile::rename(backupPath, fileName);
    QFile::remove(tempPath);
    emit critical(
        tr("Re-encryption failed"),
        tr("Failed to replace %1. Original has been restored.").arg(fileName));
    return false;
  }
  // Success - remove backup
  QFile::remove(backupPath);

  if (!QtPassSettings::isUseWebDav() && QtPassSettings::isUseGit()) {
    Executor::executeBlocking(QtPassSettings::getGitExecutable(),
                              {"add", pgit(fileName)});
    QString path =
        QDir(QtPassSettings::getPassStore()).relativeFilePath(fileName);
    path.replace(Util::endsWithGpg(), "");
    Executor::executeBlocking(QtPassSettings::getGitExecutable(),
                              {"commit", pgit(fileName), "-m",
                               "Re-encrypt for " + path + " using QtPass."});
  }

  return true;
}

/**
 * @brief Create git backup commit before re-encryption.
 * @return true if backup created or not needed, false if backup failed.
 */
auto ImitatePass::createBackupCommit() -> bool {
  if (!QtPassSettings::isUseGit() ||
      QtPassSettings::getGitExecutable().isEmpty()) {
    return true;
  }
  emit statusMsg(tr("Creating backup commit"), 2000);
  const QString git = QtPassSettings::getGitExecutable();
  QString statusOut;
  if (Executor::executeBlocking(git, {"status", "--porcelain"}, &statusOut) !=
      0) {
    emit critical(
        tr("Backup commit failed"),
        tr("Could not inspect git status. Re-encryption was aborted."));
    return false;
  }
  if (!statusOut.trimmed().isEmpty()) {
    if (Executor::executeBlocking(git, {"add", "-A"}) != 0 ||
        Executor::executeBlocking(
            git, {"commit", "-m", "Backup before re-encryption"}) != 0) {
      emit critical(tr("Backup commit failed"),
                    tr("Re-encryption was aborted because a git backup could "
                       "not be created."));
      return false;
    }
  }
  return true;
}

/**
 * @brief Re-encrypts all `.gpg` files under the given directory using the
 *        verified GPG key configuration for each folder.
 *
 * This method optionally pulls the latest changes before starting, creates a
 * backup commit, verifies `.gpg-id` files per directory, and re-encrypts files
 * whose current recipients do not match the expected keys. It emits progress,
 * status, and error signals throughout the process, and optionally pushes the
 * updated password-store when finished.
 *
 * @param dir - Root directory to scan recursively for `.gpg` files.
 * @return void
 */
void ImitatePass::reencryptPath(const QString &dir) {
  emit statusMsg(tr("Re-encrypting from folder %1").arg(dir), 3000);
  emit startReencryptPath();
  if (QtPassSettings::isAutoPull()) {
    emit statusMsg(tr("Updating password-store"), 2000);
    GitPull_b();
  }

  // Create backup before re-encryption - abort if it fails
  if (!createBackupCommit()) {
    emit endReencryptPath();
    return;
  }

  QDir currentDir;
  QDirIterator gpgFiles(dir, QStringList() << "*.gpg", QDir::Files,
                        QDirIterator::Subdirectories);
  QStringList gpgIdFilesVerified;
  QStringList gpgId;
  int successCount = 0;
  int failCount = 0;
  while (gpgFiles.hasNext()) {
    QString fileName = gpgFiles.next();
    if (gpgFiles.fileInfo().path() != currentDir.path()) {
      if (!verifyGpgIdForDir(fileName, gpgIdFilesVerified, gpgId)) {
        emit endReencryptPath();
        return;
      }
      if (gpgId.isEmpty() && !gpgIdFilesVerified.isEmpty()) {
        emit critical(tr("GPG ID verification failed"),
                      tr("Could not verify .gpg-id for directory."));
        emit endReencryptPath();
        return;
      }
    }
    QStringList actualKeys = getKeysFromFile(fileName);
    if (actualKeys != gpgId) {
      if (reencryptSingleFile(fileName, gpgId)) {
        successCount++;
      } else {
        failCount++;
        emit critical(tr("Re-encryption failed"),
                      tr("Failed to re-encrypt %1").arg(fileName));
      }
    }
  }

  if (failCount > 0) {
    emit statusMsg(tr("Re-encryption completed: %1 succeeded, %2 failed")
                       .arg(successCount)
                       .arg(failCount),
                   5000);
  } else {
    emit statusMsg(
        tr("Re-encryption completed: %1 files re-encrypted").arg(successCount),
        3000);
  }

  if (QtPassSettings::isAutoPush()) {
    emit statusMsg(tr("Updating password-store"), 2000);
    GitPush();
  }
  emit endReencryptPath();
}

/**
 * @brief Resolves the final destination path for moving a file or directory,
 * applying .gpg handling for files.
 * @example
 * QString result = ImitatePass::resolveMoveDestination("/tmp/source.txt",
 * "/backup", false); std::cout << result.toStdString() << std::endl; //
 * Expected output sample: "/backup/source.txt.gpg"
 *
 * @param src - Source path to the file or directory.
 * @param dest - Requested destination path, which may be a file or directory.
 * @param force - When true, allows overwriting an existing destination file.
 * @return QString - Resolved destination path, or an empty QString if the
 * source/destination is invalid or conflicts occur.
 */
auto ImitatePass::resolveMoveDestination(const QString &src,
                                         const QString &dest, bool force)
    -> QString {
  QFileInfo srcFileInfo(src);
  QFileInfo destFileInfo(dest);
  QString destFile;
  QString srcFileBaseName = srcFileInfo.fileName();

  if (srcFileInfo.isFile()) {
    if (destFileInfo.isFile()) {
      if (!force) {
#ifdef QT_DEBUG
        dbg() << "Destination file already exists";
#endif
        return QString();
      }
      destFile = dest;
    } else if (destFileInfo.isDir()) {
      destFile = QDir(dest).filePath(srcFileBaseName);
    } else {
      destFile = dest;
    }

    if (destFile.endsWith(".gpg", Qt::CaseInsensitive)) {
      destFile.chop(4);
    }
    destFile.append(".gpg");
  } else if (srcFileInfo.isDir()) {
    if (destFileInfo.isDir()) {
      destFile = QDir(dest).filePath(srcFileBaseName);
    } else if (destFileInfo.isFile()) {
#ifdef QT_DEBUG
      dbg() << "Destination is a file";
#endif
      return QString();
    } else {
      destFile = dest;
    }
  } else {
#ifdef QT_DEBUG
    dbg() << "Source file does not exist";
#endif
    return QString();
  }
  return destFile;
}

/**
 * @brief Moves a password store item in the Git repository and commits the
 * change.
 * @example
 * void result = className.executeMoveGit(src, destFile, force);
 *
 * @param const QString &src - Source path of the item to move.
 * @param const QString &destFile - Destination path of the item after the move.
 * @param bool force - Whether to force the move using Git's -f option.
 * @return void - This method does not return a value.
 */
void ImitatePass::executeMoveGit(const QString &src, const QString &destFile,
                                 bool force) {
  QStringList args;
  args << "mv";
  if (force) {
    args << "-f";
  }
  args << pgit(src);
  args << pgit(destFile);
  executeGit(GIT_MOVE, args);

  QString relSrc = QDir(QtPassSettings::getPassStore()).relativeFilePath(src);
  relSrc.replace(Util::endsWithGpg(), "");
  QString relDest =
      QDir(QtPassSettings::getPassStore()).relativeFilePath(destFile);
  relDest.replace(Util::endsWithGpg(), "");
  QString message = QString("Moved for %1 to %2 using QtPass.");
  message = message.arg(relSrc, relDest);
  gitCommit("", message);
}

/**
 * @brief Moves a password entry from the source path to the destination path.
 * @example
 * ImitatePass::Move(src, dest, true);
 *
 * @param const QString src - The source path or entry name to move.
 * @param const QString dest - The destination path or entry name.
 * @param const bool force - If true, overwrites an existing destination entry
 * when necessary.
 * @return void - This function does not return a value.
 */
void ImitatePass::Move(const QString src, const QString dest,
                       const bool force) {
  transactionHelper trans(this, PASS_MOVE);
  QString destFile = resolveMoveDestination(src, dest, force);
  if (destFile.isEmpty()) {
    return;
  }

#ifdef QT_DEBUG
  dbg() << "Move Source: " << src;
  dbg() << "Move Destination: " << destFile;
#endif

  if (QtPassSettings::isUseGit()) {
    executeMoveGit(src, destFile, force);
  } else {
    QDir qDir;
    if (force) {
      qDir.remove(destFile);
    }
    qDir.rename(src, destFile);
  }
}

/**
 * @brief Copies a file or directory from source to destination, optionally
 * forcing overwrite.
 * @example
 * void result = ImitatePass::Copy(src, dest, force);
 *
 * @param QString src - Source path to copy from.
 * @param QString dest - Destination path to copy to.
 * @param bool force - If true, overwrites the destination when it already
 * exists.
 * @return void - This function does not return a value.
 */
void ImitatePass::Copy(const QString src, const QString dest,
                       const bool force) {
  QFileInfo destFileInfo(dest);
  transactionHelper trans(this, PASS_COPY);
  if (QtPassSettings::isUseGit()) {
    QStringList args;
    args << "cp";
    if (force) {
      args << "-f";
    }
    args << pgit(src);
    args << pgit(dest);
    executeGit(GIT_COPY, args);

    QString message = QString("Copied from %1 to %2 using QtPass.");
    message = message.arg(src, dest);
    gitCommit("", message);
  } else {
    QDir qDir;
    if (force) {
      qDir.remove(dest);
    }
    QFile::copy(src, dest);
  }
  // reecrypt all files under the new folder
  if (destFileInfo.isDir()) {
    reencryptPath(destFileInfo.absoluteFilePath());
  } else if (destFileInfo.isFile()) {
    reencryptPath(destFileInfo.dir().path());
  }
}

/**
 * @brief ImitatePass::executeGpg easy wrapper for running gpg commands
 * @param args
 */
void ImitatePass::executeGpg(PROCESS id, const QStringList &args, QString input,
                             bool readStdout, bool readStderr) {
  executeWrapper(id, QtPassSettings::getGpgExecutable(), args, std::move(input),
                 readStdout, readStderr);
}

/**
 * @brief ImitatePass::executeGit easy wrapper for running git commands
 * @param args
 */
void ImitatePass::executeGit(PROCESS id, const QStringList &args, QString input,
                             bool readStdout, bool readStderr) {
  executeWrapper(id, QtPassSettings::getGitExecutable(), args, std::move(input),
                 readStdout, readStderr);
}

/**
 * @brief ImitatePass::finished this function is overloaded to ensure
 *                              identical behaviour to RealPass ie. only PASS_*
 *                              processes are visible inside Pass::finish, so
 *                              that interface-wise it all looks the same
 * @param id
 * @param exitCode
 * @param out
 * @param err
 */
void ImitatePass::finished(int id, int exitCode, const QString &out,
                           const QString &err) {
#ifdef QT_DEBUG
  dbg() << "Imitate Pass";
#endif
  static QString transactionOutput;
  PROCESS pid = transactionIsOver(static_cast<PROCESS>(id));
  transactionOutput.append(out);

  if (exitCode == 0) {
    if (pid == INVALID) {
      return;
    }
  } else {
    while (pid == INVALID) {
      id = exec.cancelNext();
      if (id == -1) {
        //  this is probably irrecoverable and shall not happen
#ifdef QT_DEBUG
        dbg() << "No such transaction!";
#endif
        return;
      }
      pid = transactionIsOver(static_cast<PROCESS>(id));
    }
  }
  Pass::finished(pid, exitCode, transactionOutput, err);
  transactionOutput.clear();
}

/**
 * @brief executeWrapper    overrided so that every execution is a transaction
 * @param id
 * @param app
 * @param args
 * @param input
 * @param readStdout
 * @param readStderr
 */
void ImitatePass::executeWrapper(PROCESS id, const QString &app,
                                 const QStringList &args, QString input,
                                 bool readStdout, bool readStderr) {
  transactionAdd(id);
  Pass::executeWrapper(id, app, args, input, readStdout, readStderr);
}

/**
 * @brief Decrypt one .gpg file and return lines matching rx.
 */
auto ImitatePass::grepMatchFile(const QStringList &env, const QString &gpgExe,
                                const QString &filePath,
                                const QRegularExpression &rx) -> QStringList {
  QString plaintext;
  const int rc =
      Executor::executeBlocking(env, gpgExe,
                                {"-d", "--quiet", "--yes", "--no-encrypt-to",
                                 "--batch", "--use-agent", pgpg(filePath)},
                                &plaintext);
  if (rc != 0 || plaintext.isEmpty())
    return {};
  QStringList matches;
  for (const QString &line : plaintext.split('\n')) {
    const QString t = line.trimmed();
    if (!t.isEmpty() && t.contains(rx))
      matches << t;
  }
  return matches;
}

/**
 * @brief Walk the store, decrypt every .gpg file, collect matches.
 */
auto ImitatePass::grepScanStore(const QStringList &env, const QString &gpgExe,
                                const QString &storeDir,
                                const QRegularExpression &rx)
    -> QList<QPair<QString, QStringList>> {
  QList<QPair<QString, QStringList>> results;
  QDirIterator it(storeDir, QStringList() << "*.gpg", QDir::Files,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    if (QThread::currentThread()->isInterruptionRequested())
      return {};
    const QString filePath = it.next();
    const QStringList matches = grepMatchFile(env, gpgExe, filePath, rx);
    if (!matches.isEmpty()) {
      QString entry = QDir(storeDir).relativeFilePath(filePath);
      if (entry.endsWith(QLatin1String(".gpg")))
        entry.chop(4);
      results.append({entry, matches});
    }
  }
  return results;
}

/**
 * @brief Search all password content by GPG-decrypting each .gpg file.
 *
 * Runs a background thread to avoid blocking the UI. Results are emitted on
 * the main thread via QMetaObject::invokeMethod. A sequence counter discards
 * results from superseded searches.
 */
void ImitatePass::Grep(QString pattern, bool caseInsensitive) {
  if (m_grepThread && m_grepThread->isRunning())
    m_grepThread->requestInterruption();

  const int seq = ++m_grepSeq;
  const QString gpgExe = QtPassSettings::getGpgExecutable();
  const QString storeDir = QtPassSettings::getPassStore();
  const QStringList env = exec.environment();
  QPointer<ImitatePass> self(this);

  auto emitResults = [self, seq](QList<QPair<QString, QStringList>> results) {
    if (!self)
      return;
    QMetaObject::invokeMethod(
        self,
        [self, seq, results = std::move(results)]() {
          if (self && self->m_grepSeq == seq)
            emit self->finishedGrep(results);
        },
        Qt::QueuedConnection);
  };

  QThread *thread = QThread::create([self, seq, pattern, caseInsensitive,
                                     gpgExe, storeDir, env, emitResults]() {
    const QRegularExpression rx(
        pattern, caseInsensitive ? QRegularExpression::CaseInsensitiveOption
                                 : QRegularExpression::PatternOptions{});
    if (!rx.isValid()) {
      if (self)
        emitResults({});
      return;
    }
    if (self)
      emitResults(grepScanStore(env, gpgExe, storeDir, rx));
  });

  m_grepThread = thread;
  connect(thread, &QThread::finished, this, [this, thread]() {
    if (m_grepThread == thread)
      m_grepThread = nullptr;
    thread->deleteLater();
  });
  thread->start();
}
