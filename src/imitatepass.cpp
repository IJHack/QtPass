// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "imitatepass.h"
#include "executor.h"
#include "util.h"
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QThread>
#include <QTimer>
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

ImitatePass::~ImitatePass() {
  static constexpr int kGrepThreadTimeoutMs = 5000;
  for (QThread *t : std::as_const(m_grepThreads))
    if (t && t->isRunning())
      t->requestInterruption();
  QElapsedTimer elapsed;
  elapsed.start();
  for (QThread *t : std::as_const(m_grepThreads)) {
    if (t && t->isRunning()) {
      const int remaining =
          kGrepThreadTimeoutMs - static_cast<int>(elapsed.elapsed());
      if (remaining > 0)
        t->wait(remaining);
    }
  }
  // Unlike the grep workers, the re-encryption worker calls member functions
  // and so must not outlive this object. Cancel it and join. A timeout on the
  // join would not be safe, since the worker would go on touching this
  // object's members, and none is needed: the worker polls the flag while it
  // waits on a process (see execBlocking()), ends that process itself (gpg
  // waiting on pinentry while the user quits, say) within the poll interval
  // plus the kill grace, and its remaining work is not process-bound.
  if (m_reencryptThread && m_reencryptThread->isRunning()) {
    m_reencryptCancel.store(true);
    m_reencryptThread->wait();
  }
}

/**
 * @brief Blocking process run for the re-encryption helpers.
 *
 * The helpers (verifyGpgIdFile(), getKeysFromFile(), reencryptSingleFile(),
 * createBackupCommit()) are shared between the owning thread and the
 * re-encryption worker; the ImitatePass thread affinity tells the two apart.
 * On the worker the run is handed m_reencryptCancel: nothing is started once
 * the flag is set, and while a process runs the wait polls the flag and, when
 * it gets set, terminates and if need be kills the process. All of that
 * happens on the worker thread, which owns the QProcess. The other threads
 * (cancelReencryptPath(), the destructor) only ever set the flag; they hold
 * neither the QProcess nor its pid, so they cannot act on a process that has
 * exited in the meantime, nor on a pid the OS has since reused.
 */
auto ImitatePass::execBlocking(const QString &app, const QStringList &args,
                               const QString &input, QString *process_out,
                               QString *process_err) -> int {
  if (QThread::currentThread() == thread())
    return Executor::executeBlocking(app, args, input, process_out,
                                     process_err);
  QProcess process;
  return Executor::executeBlocking(process, app, args, input, process_out,
                                   process_err, &m_reencryptCancel);
}

auto ImitatePass::execBlocking(const QString &app, const QStringList &args,
                               QString *process_out, QString *process_err)
    -> int {
  return execBlocking(app, args, QString(), process_out, process_err);
}

auto ImitatePass::translatePathForWsl(const QString &path,
                                      const QString &exe) const -> QString {
  QString normalizedPath = QDir::cleanPath(path);
  if (!exe.startsWith(QStringLiteral("wsl ")))
    return normalizedPath;
  QString wslPath;
  const int rc = Executor::executeBlocking(
      QStringLiteral("wsl"),
      Executor::wslExecArgs(QStringLiteral("wslpath"), {normalizedPath}),
      &wslPath);
  const QString translated = wslPath.trimmed();
  return (rc == 0 && !translated.isEmpty()) ? translated : normalizedPath;
}

auto ImitatePass::pgit(const QString &path) const -> QString {
  return translatePathForWsl(path, m_settings.gitExecutable);
}

auto ImitatePass::pgpg(const QString &path) const -> QString {
  return translatePathForWsl(path, m_settings.gpgExecutable);
}

/**
 * @brief ImitatePass::GitInit git init wrapper
 */
void ImitatePass::GitInit() {
  executeGit(GIT_INIT, {"init", pgit(m_settings.passStore)});
}

/**
 * @brief ImitatePass::GitPull git pull wrapper
 */
void ImitatePass::GitPull() {
  if (gitReady()) {
    executeGit(GIT_PULL, {"pull"});
  }
}

/**
 * @brief ImitatePass::GitPull_b git pull wrapper
 */
void ImitatePass::GitPull_b() {
  Executor::executeBlocking(m_settings.gitExecutable, {"pull"});
}

/**
 * @brief ImitatePass::GitPush git push wrapper
 */
void ImitatePass::GitPush() {
  if (gitReady()) {
    executeGit(GIT_PUSH, {"push"});
  }
}

/**
 * @brief ImitatePass::Show shows content of file
 */
void ImitatePass::Show(QString file) {
  file = m_settings.passStore + file + ".gpg";
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
  QString gpgIdPath = Pass::getGpgIdPath(file, m_settings.passStore);
  if (!verifyGpgIdFile(gpgIdPath)) {
    emit critical(tr("Check .gpg-id file signature!"),
                  tr("Signature for %1 is invalid.").arg(gpgIdPath));
    return;
  }
  transactionHelper trans(this, PASS_INSERT);
  QStringList recipients = Pass::getRecipientList(file, m_settings.passStore);
  if (recipients.isEmpty()) {
    // Already emit critical signal to notify user of error - no need to throw
    emit critical(tr("Can not edit"),
                  tr("Could not read encryption key to use, .gpg-id "
                     "file missing or invalid."));
    return;
  }
  // --no-encrypt-to keeps an `encrypt-to` line in the user's gpg.conf from
  // adding a recipient that is not listed in the (possibly signed) .gpg-id;
  // --compress-algo=none mirrors pass(1). Both belong on every encrypt call.
  QStringList args = {"--batch",
                      "--status-fd",
                      "2",
                      "-eq",
                      "--compress-algo=none",
                      "--no-encrypt-to",
                      "--output",
                      pgpg(file)};
  for (auto &r : recipients) {
    args.append("-r");
    args.append(r);
  }
  if (overwrite) {
    args.append("--yes");
  }
  args.append("-");
  executeGpg(PASS_INSERT, args, newValue);
  if (!m_settings.useWebDav && gitReady()) {
    // Git is used when enabled - this is the standard pass workflow
    if (!overwrite) {
      executeGit(GIT_ADD, {"add", pgit(file)});
    }
    QString path = QDir(m_settings.passStore).relativeFilePath(file);
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
  file = m_settings.passStore + file;
  transactionHelper trans(this, PASS_REMOVE);
  if (!isDir) {
    file += ".gpg";
  }
  if (gitReady()) {
    executeGit(GIT_RM, {"rm", (isDir ? "-rf" : "-f"), pgit(file)});
    // Normalize path the same way as add/edit operations
    QString path = QDir(m_settings.passStore).relativeFilePath(file);
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
  int result = Executor::executeBlocking(m_settings.gpgExecutable, args, &out);
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
  // Lock the file to owner-only access. The .gpg-id leaks which keys the
  // store is encrypted to; while the typical ~/.password-store is 0700,
  // users may relocate the store onto NFS/SMB/USB where the parent dir
  // perms are more lax. On platforms where setPermissions is a no-op
  // (Windows), this is silently best-effort.
  QFile::setPermissions(gpgIdFile, QFile::ReadOwner | QFile::WriteOwner);
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
 * @param QString &gpgIdFile - Path to the .gpg-id file to be signed.
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
  int result = Executor::executeBlocking(m_settings.gpgExecutable, args);
  if (result != 0) {
#ifdef QT_DEBUG
    dbg() << "GPG signing failed with code:" << result;
#endif
    emit critical(tr("GPG signing failed!"),
                  tr("Failed to sign %1.").arg(gpgIdFile));
    return false;
  }
  if (!verifyGpgIdFile(gpgIdFile)) {
    emit critical(tr("Check .gpg-id file signature!"),
                  tr("Signature for %1 is invalid.").arg(gpgIdFile));
    return false;
  }
  return true;
}

/**
 * @brief Adds a GPG ID file and optionally its signature file to git, then
 * creates corresponding commit(s).
 *
 * Git runs synchronously here on purpose: Init follows up with reencryptPath,
 * whose backup and re-encryption commits are blocking as well. Queuing the
 * add/commit on the asynchronous executor instead let the two race for the
 * index lock, so either the queued `git add` died on `index.lock` (and the
 * cancelled commit left the .gpg-id untracked) or the backup commit absorbed
 * the file first and `git commit -- .gpg-id` failed with nothing to commit.
 *
 * @example
 * int rc = ImitatePass::gitAddGpgId(gpgIdFile, gpgIdSigFile, true, true,
 *                                   &out, &err);
 *
 * @param const QString &gpgIdFile - Path to the GPG ID file to add and commit.
 * @param const QString &gpgIdSigFile - Path to the signature file associated
 * with the GPG ID file.
 * @param bool addFile - Whether to stage the GPG ID file before committing.
 * @param bool addSigFile - Whether to stage and commit the signature file.
 * @param QString *out - Receives the concatenated stdout of the git commands.
 * @param QString *err - Receives the concatenated stderr of the git commands.
 * @return int - Exit code of the first failing git command, 0 on success.
 */
auto ImitatePass::gitAddGpgId(const QString &gpgIdFile,
                              const QString &gpgIdSigFile, bool addFile,
                              bool addSigFile, QString *out, QString *err)
    -> int {
  const QString git = m_settings.gitExecutable;
  const QString store = pgit(m_settings.passStore);
  auto run = [&](const QStringList &args) -> int {
    QString runOut;
    QString runErr;
    const int rc = Executor::executeBlocking(
        git, QStringList{"-C", store} + args, &runOut, &runErr);
    if (out != nullptr) {
      out->append(runOut);
    }
    if (err != nullptr) {
      err->append(runErr);
    }
    return rc;
  };
  int rc = 0;
  if (addFile) {
    rc = run({"add", pgit(gpgIdFile)});
    if (rc != 0) {
      return rc;
    }
  }
  QString commitPath = gpgIdFile;
  commitPath.replace(Util::endsWithGpg(), "");
  rc = run({"commit", "-m", "Added " + commitPath + " using QtPass.", "--",
            pgit(gpgIdFile)});
  if (rc != 0 || !addSigFile) {
    return rc;
  }
  rc = run({"add", pgit(gpgIdSigFile)});
  if (rc != 0) {
    return rc;
  }
  commitPath = gpgIdSigFile;
  commitPath.replace(QRegularExpression("\\.gpg$"), "");
  return run({"commit", "-m", "Added " + commitPath + " using QtPass.", "--",
              pgit(gpgIdSigFile)});
}

/**
 * @brief Checks whether git already tracks a file in the password store.
 *
 * @param const QString &file - Absolute path of the file inside the store.
 * @return bool - true when the file is in the index, false when it is
 * untracked or the lookup failed.
 */
auto ImitatePass::gitTracks(const QString &file) -> bool {
  return Executor::executeBlocking(m_settings.gitExecutable,
                                   {"-C", pgit(m_settings.passStore),
                                    "ls-files", "--error-unmatch", "--",
                                    pgit(file)}) == 0;
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
  QStringList signingKeys =
      m_settings.passSigningKey.split(" ", Qt::SkipEmptyParts);
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

  const bool useGit = !m_settings.useWebDav && gitReady();
  QString gpgIdFile = path + ".gpg-id";
  bool addFile = false;
  if (m_settings.addGPGId && useGit) {
    // Stage the .gpg-id unless git already tracks it. Checking the working
    // tree instead is not enough: MainWindow::addFolder writes a folder's
    // .gpg-id without staging it, and since the backup commit before
    // re-encryption only picks up tracked files (#1685) nothing else ever
    // would. Without the add, `git commit -- <file>` below fails for the
    // untracked pathspec and the re-encrypted entries get pushed without
    // the recipients file they were encrypted to (#1682).
    addFile = !gitTracks(gpgIdFile);
  }
  writeGpgIdFile(gpgIdFile, users);

  if (!signingKeys.isEmpty()) {
    if (!signGpgIdFile(gpgIdFile, signingKeys)) {
      return;
    }
  }

  int gitExit = 0;
  QString gitOut;
  QString gitErr;
  if (useGit) {
    gitExit = gitAddGpgId(gpgIdFile, gpgIdSigFile, addFile, addSigFile, &gitOut,
                          &gitErr);
  }
  reencryptPath(path);
  if (useGit) {
    // Same contract the asynchronous add/commit transaction used to provide:
    // finishedInit when the .gpg-id landed in git, processErrorExit otherwise.
    Pass::finished(PASS_INIT, gitExit, gitOut, gitErr);
  }
}

/**
 * @brief ImitatePass::verifyGpgIdFile verify detached gpgid file signature.
 * @param file which gpgid file.
 * @return was verification successful?
 */
auto ImitatePass::verifyGpgIdFile(const QString &file) -> bool {
  QStringList signingKeys =
      m_settings.passSigningKey.split(" ", Qt::SkipEmptyParts);
  if (signingKeys.isEmpty()) {
    return true;
  }
  QString out;
  QStringList args =
      QStringList{"--verify", "--status-fd=1", pgpg(file) + ".sig", pgpg(file)};
  int result = execBlocking(m_settings.gpgExecutable, args, &out);
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
 * @brief ImitatePass::reencryptPath reencrypt all files under the chosen
 * directory
 *
 * This is still quite experimental..
 * @param dir
 */
auto ImitatePass::verifyGpgIdForDir(const QString &file,
                                    QStringList &gpgIdFilesVerified,
                                    QStringList &gpgId) -> bool {
  QString gpgIdPath = Pass::getGpgIdPath(file, m_settings.passStore);
  // Verify each .gpg-id signature only once, but always refresh the recipient
  // list for the current file: a cache hit means the signature was already
  // checked, not that gpgId still holds this directory's recipients — it may
  // carry a different directory's list, which would re-encrypt to wrong keys.
  if (!gpgIdFilesVerified.contains(gpgIdPath)) {
    if (!verifyGpgIdFile(gpgIdPath)) {
      // An interrupted gpg is a cancel, not a bad signature.
      if (!m_reencryptCancel.load())
        emit critical(tr("Check .gpg-id file signature!"),
                      tr("Signature for %1 is invalid.").arg(gpgIdPath));
      return false;
    }
    gpgIdFilesVerified.append(gpgIdPath);
  }
  gpgId = getRecipientList(file, m_settings.passStore);
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
  const int result = execBlocking(m_settings.gpgExecutable, args, &keys, &err);
  if (result != 0) {
    return {};
  }
  QStringList actualKeys;
  keys += err;
  QStringList key = keys.split(Util::newLinesRegex(), Qt::SkipEmptyParts);
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
  int result = execBlocking(m_settings.gpgExecutable, args, &local_lastDecrypt);

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
  // Same encrypt-only flags as Insert(): gpg.conf must not add recipients.
  args = QStringList{
      "--yes",           "--batch",  "-eq",         "--compress-algo=none",
      "--no-encrypt-to", "--output", pgpg(tempPath)};
  for (const auto &i : recipients) {
    args.append("-r");
    args.append(i);
  }
  args.append("-");
  result = execBlocking(m_settings.gpgExecutable, args, local_lastDecrypt);

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
  result = execBlocking(m_settings.gpgExecutable, args, &verifyOutput);
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

  if (!m_settings.useWebDav && gitConfigured()) {
    // -C the store so git runs there rather than in QtPass's launch directory
    // (executeBlocking sets no working directory).
    const QString store = pgit(m_settings.passStore);
    execBlocking(m_settings.gitExecutable,
                 {"-C", store, "add", pgit(fileName)});
    QString path = QDir(m_settings.passStore).relativeFilePath(fileName);
    path.replace(Util::endsWithGpg(), "");
    execBlocking(m_settings.gitExecutable,
                 {"-C", store, "commit", pgit(fileName), "-m",
                  "Re-encrypt for " + path + " using QtPass."});
  }

  return true;
}

/**
 * @brief Create git backup commit before re-encryption.
 * @return true if backup created or not needed, false if backup failed.
 */
auto ImitatePass::createBackupCommit() -> bool {
  if (!gitConfigured()) {
    return true;
  }
  emit statusMsg(tr("Creating backup commit"), 2000);
  const QString git = m_settings.gitExecutable;
  // Run git in the password store: executeBlocking does not set a working
  // directory, so without -C these commands would run in QtPass's launch
  // directory and either fail or operate on an unrelated repository.
  const QString store = pgit(m_settings.passStore);
  // Only tracked files belong in the backup. Untracked files in the store (a
  // plaintext export, an editor swap file, ...) must not be swept into a
  // commit that autoPush then sends to the shared remote, so both the status
  // check and the add are restricted to what git already knows about.
  QString statusOut;
  if (execBlocking(
          git, {"-C", store, "status", "--porcelain", "--untracked-files=no"},
          &statusOut) != 0) {
    // An interrupted git is a cancel, not a failure worth a dialog.
    if (!m_reencryptCancel.load())
      emit critical(
          tr("Backup commit failed"),
          tr("Could not inspect git status. Re-encryption was aborted."));
    return false;
  }
  if (!statusOut.trimmed().isEmpty()) {
    if (execBlocking(git, {"-C", store, "add", "-u"}) != 0 ||
        execBlocking(git, {"-C", store, "commit", "-m",
                           "Backup before re-encryption"}) != 0) {
      if (!m_reencryptCancel.load())
        emit critical(tr("Backup commit failed"),
                      tr("Re-encryption was aborted because a git backup "
                         "could not be created."));
      return false;
    }
  }
  return true;
}

/**
 * @brief Outcome of one reencryptPath() run.
 */
struct ImitatePass::ReencryptResult {
  int total = 0;          ///< `.gpg` files found under the directory.
  int checked = 0;        ///< Files whose recipients were inspected.
  int reencrypted = 0;    ///< Files rewritten for the current recipients.
  QStringList failed;     ///< Files that could not be re-encrypted.
  bool cancelled = false; ///< Stopped early by cancelReencryptPath(); the
                          ///< interrupted file, if any, is not in `failed`.
  bool aborted = false;   ///< Stopped early on an error already reported.
};

namespace {
/// Poll interval while waiting for queued git commands to drain.
constexpr int kReencryptRetryMs = 100;
/// Cap on the file names listed in the aggregated failure dialog.
constexpr int kReencryptMaxListedFailures = 15;
} // namespace

/**
 * @brief Re-encrypts all `.gpg` files under the given directory using the
 *        verified GPG key configuration for each folder.
 *
 * Emits startReencryptPath() and hands the actual work to a worker thread
 * (see reencryptFiles()), so the GUI stays responsive and the run can be
 * cancelled. The worker optionally pulls first, creates a backup commit,
 * verifies `.gpg-id` files per directory and re-encrypts files whose current
 * recipients do not match the expected keys, reporting progress through
 * reencryptProgress(). Per-file failures are aggregated into a single
 * critical() by finishReencrypt(), which also pushes when configured and
 * emits endReencryptPath().
 *
 * @param dir - Root directory to scan recursively for `.gpg` files.
 * @return void
 */
void ImitatePass::reencryptPath(const QString &dir) {
  if (m_reencryptActive) {
    emit statusMsg(tr("A re-encryption is already running"), 3000);
    return;
  }
  m_reencryptActive = true;
  m_reencryptCancel.store(false);
  emit statusMsg(tr("Re-encrypting from folder %1").arg(dir), 3000);
  emit startReencryptPath();
  startReencryptWorker(dir);
}

/**
 * @brief Stop a running re-encryption promptly.
 *
 * Only sets the cancel flag. The worker starts no further process once it is
 * set, and the process it is blocked on is ended by the worker itself: the
 * cancellable Executor::executeBlocking() polls the flag and, on seeing it,
 * terminate()s the child and kill()s it if it is still running after the
 * grace period (terminate() is only a request: SIGTERM, or WM_CLOSE on
 * Windows, which a console gpg ignores). Nothing here touches the worker's
 * QProcess or its pid. A new run clears the flag.
 */
void ImitatePass::cancelReencryptPath() {
  if (!m_reencryptActive)
    return;
  m_reencryptCancel.store(true);
}

/**
 * @brief Start the re-encryption worker once the Executor queue is idle.
 *
 * Callers such as Init(), Move() and Copy() queue git commands on `exec`
 * right before calling reencryptPath(). Those run asynchronously on this
 * thread, so the worker's blocking git calls would otherwise compete with
 * them for the repository's index lock. Poll until the queue has drained.
 */
void ImitatePass::startReencryptWorker(const QString &dir) {
  if (m_reencryptCancel.load()) {
    ReencryptResult result;
    result.cancelled = true;
    finishReencrypt(result);
    return;
  }
  if (!exec.isIdle()) {
    QTimer::singleShot(kReencryptRetryMs, this,
                       [this, dir]() { startReencryptWorker(dir); });
    return;
  }

  // The worker calls member functions, so `this` must outlive it: the
  // destructor cancels and joins m_reencryptThread. `self` only guards the
  // queued completion, which may run after the thread object is gone.
  QPointer<ImitatePass> self(this);
  QThread *thread = QThread::create([this, self, dir]() {
    ReencryptResult result = reencryptFiles(dir);
    QMetaObject::invokeMethod(
        self,
        [self, result = std::move(result)]() {
          if (self)
            self->finishReencrypt(result);
        },
        Qt::QueuedConnection);
  });
  m_reencryptThread = thread;
  connect(thread, &QThread::finished, this, [this, thread]() {
    if (m_reencryptThread == thread)
      m_reencryptThread = nullptr;
  });
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);
  thread->start();
}

/**
 * @brief Worker-thread body of reencryptPath().
 *
 * Only blocking helpers (execBlocking() directly and through
 * createBackupCommit(), verifyGpgIdForDir(), getKeysFromFile() and
 * reencryptSingleFile()) run here; anything that touches `exec` or the
 * transaction state stays on the owning thread. Signals emitted from here
 * (statusMsg, critical, reencryptProgress) are delivered queued to their
 * GUI-thread receivers. The cancel flag is checked between files, and a
 * cancel also ends the process in progress from this thread (see
 * execBlocking()): a helper that fails while the flag is set was
 * interrupted, so its file is neither counted as checked nor reported as
 * failed.
 */
auto ImitatePass::reencryptFiles(const QString &dir) -> ReencryptResult {
  ReencryptResult result;
  if (m_settings.autoPull && gitConfigured()) {
    emit statusMsg(tr("Updating password-store"), 2000);
    execBlocking(m_settings.gitExecutable, {"pull"});
  }

  // Create backup before re-encryption - abort if it fails
  if (!createBackupCommit()) {
    if (m_reencryptCancel.load())
      result.cancelled = true;
    else
      result.aborted = true;
    return result;
  }

  QStringList files;
  QDirIterator gpgFiles(dir, QStringList() << "*.gpg", QDir::Files,
                        QDirIterator::Subdirectories);
  while (gpgFiles.hasNext()) {
    files << gpgFiles.next();
  }
  result.total = files.size();
  emit reencryptProgress(0, result.total);

  QString currentDir;
  QStringList gpgIdFilesVerified;
  QStringList gpgId;
  for (const QString &fileName : std::as_const(files)) {
    if (m_reencryptCancel.load()) {
      result.cancelled = true;
      break;
    }
    const QString fileDir = QFileInfo(fileName).path();
    if (fileDir != currentDir) {
      if (!verifyGpgIdForDir(fileName, gpgIdFilesVerified, gpgId)) {
        if (m_reencryptCancel.load())
          result.cancelled = true;
        else
          result.aborted = true;
        return result;
      }
      if (gpgId.isEmpty() && !gpgIdFilesVerified.isEmpty()) {
        emit critical(tr("GPG ID verification failed"),
                      tr("Could not verify .gpg-id for directory."));
        result.aborted = true;
        return result;
      }
      currentDir = fileDir;
    }
    QStringList actualKeys = getKeysFromFile(fileName);
    if (actualKeys != gpgId) {
      if (reencryptSingleFile(fileName, gpgId)) {
        result.reencrypted++;
      } else if (m_reencryptCancel.load()) {
        // Interrupted by the cancel: the file is untouched, not failed.
        result.cancelled = true;
        break;
      } else {
        result.failed << fileName;
      }
    }
    result.checked++;
    emit reencryptProgress(result.checked, result.total);
  }
  return result;
}

/**
 * @brief Owning-thread epilogue of reencryptPath().
 *
 * Reports the aggregated failures in one dialog, summarises the run in the
 * status bar, pushes when configured (not after a cancel, an abort or a
 * per-file failure: a partially re-encrypted store must not reach the remote,
 * and the user should inspect the result first) and releases the UI.
 */
void ImitatePass::finishReencrypt(const ReencryptResult &result) {
  if (!result.failed.isEmpty()) {
    QStringList listed = result.failed.mid(0, kReencryptMaxListedFailures);
    const int more = result.failed.size() - listed.size();
    if (more > 0) {
      listed << tr("... and %n more", nullptr, more);
    }
    emit critical(tr("Re-encryption failed"),
                  tr("%n file(s) could not be re-encrypted:", nullptr,
                     result.failed.size()) +
                      "\n\n" + listed.join('\n'));
  }

  if (result.cancelled) {
    emit statusMsg(tr("Re-encryption cancelled: %1 of %2 files checked, "
                      "%3 re-encrypted, %4 failed")
                       .arg(result.checked)
                       .arg(result.total)
                       .arg(result.reencrypted)
                       .arg(result.failed.size()),
                   5000);
  } else if (!result.aborted) {
    if (!result.failed.isEmpty()) {
      emit statusMsg(tr("Re-encryption completed: %1 succeeded, %2 failed")
                         .arg(result.reencrypted)
                         .arg(result.failed.size()),
                     5000);
    } else {
      emit statusMsg(tr("Re-encryption completed: %1 files re-encrypted")
                         .arg(result.reencrypted),
                     3000);
    }
    if (m_settings.autoPush && gitConfigured()) {
      if (result.failed.isEmpty()) {
        emit statusMsg(tr("Updating password-store"), 2000);
        GitPush();
      } else {
        emit statusMsg(tr("Not pushing: %n file(s) failed to re-encrypt",
                          nullptr, result.failed.size()),
                       5000);
      }
    }
  }
  m_reencryptActive = false;
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
        return {};
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
      return {};
    } else {
      destFile = dest;
    }
  } else {
#ifdef QT_DEBUG
    dbg() << "Source file does not exist";
#endif
    return {};
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

  QString relSrc = QDir(m_settings.passStore).relativeFilePath(src);
  relSrc.replace(Util::endsWithGpg(), "");
  QString relDest = QDir(m_settings.passStore).relativeFilePath(destFile);
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

  if (gitReady()) {
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
 * @brief Copies a regular file onto dst, replacing dst atomically.
 *
 * The bytes are written to a temporary sibling that QSaveFile renames over
 * dst only once all of them are in, so a failure part-way (disk full,
 * permissions, a vanished source) leaves an existing dst untouched instead of
 * removed first and never rewritten. The source's permissions are carried
 * over, as QFile::copy would.
 * @return true on success; on failure nothing at dst has changed.
 */
static auto copyFileReplacing(const QString &src, const QString &dst) -> bool {
  QFile in(src);
  if (!QFileInfo(in).isFile() || !in.open(QIODevice::ReadOnly))
    return false;
  QSaveFile out(dst);
  if (!out.open(QIODevice::WriteOnly))
    return false;
  out.setPermissions(in.permissions());
  char buf[64 * 1024];
  for (;;) {
    const qint64 n = in.read(buf, sizeof buf);
    if (n < 0) {
      out.cancelWriting();
      return false;
    }
    if (n == 0)
      break;
    if (out.write(buf, n) != n) {
      out.cancelWriting();
      return false;
    }
  }
  return out.commit();
}

/**
 * @brief Copies a file or directory from source to destination, optionally
 * forcing overwrite.
 * @example
 * void result = ImitatePass::Copy(src, dest, force);
 *
 * @param QString src - Source path to copy from.
 * @param QString dest - Destination path to copy to: a new file name, or an
 * existing folder to copy into (like `pass cp`).
 * @param bool force - If true, overwrites the destination when it already
 * exists.
 * @return void - This function does not return a value.
 */
void ImitatePass::Copy(const QString src, const QString dest,
                       const bool force) {
  transactionHelper trans(this, PASS_COPY);
  // Like `pass cp`, dest may be an existing folder (a drag-and-drop copy hands
  // over the folder, not the new file name). Resolve the real target the same
  // way Move does: into the folder, .gpg appended, no clobbering without force.
  QString destFile = resolveMoveDestination(src, dest, force);
  if (destFile.isEmpty()) {
    emit critical(tr("Copy failed"),
                  tr("Could not copy %1 to %2.").arg(src, dest));
    return;
  }
  QFileInfo destFileInfo(destFile);
  // A folder destination that is the source's own folder resolves to the
  // source itself; with force that would replace the only copy with itself.
  if (QFileInfo(src) == destFileInfo) {
    emit critical(tr("Copy failed"),
                  tr("Could not copy %1 to %2.").arg(src, destFile));
    return;
  }
  // resolveMoveDestination only sees a clash when dest names the file; for a
  // folder destination the resolved <folder>/<entry>.gpg may exist as well.
  if (!force && destFileInfo.exists()) {
    emit critical(tr("Copy failed"),
                  tr("Could not copy %1 to %2.").arg(src, destFile));
    return;
  }
  // git has no "cp" subcommand, so copy on the filesystem in both modes and,
  // when using git, stage the new path afterwards. The copy is synchronous and
  // replaces the destination atomically (see copyFileReplacing), so it exists
  // before the re-encryption below runs and an entry being overwritten with
  // force survives a copy that fails half-way.
  if (!copyFileReplacing(src, destFile)) {
    emit critical(tr("Copy failed"),
                  tr("Could not copy %1 to %2.").arg(src, destFile));
    return;
  }
  // QFileInfo caches; the comparison above may have looked at a path that did
  // not exist yet, so re-read it before deciding what to re-encrypt.
  destFileInfo.refresh();
  if (gitReady()) {
    executeGit(GIT_COPY, {"add", pgit(destFile)});
    QString message = QString("Copied from %1 to %2 using QtPass.");
    message = message.arg(src, destFile);
    gitCommit("", message);
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
  executeWrapper(id, m_settings.gpgExecutable, args, std::move(input),
                 readStdout, readStderr);
}

/**
 * @brief ImitatePass::gitConfigured git is enabled and an executable is set.
 *
 * useGit can be on while gitExecutable is empty (fresh setup, git removed
 * later). Handing that empty executable to the Executor used to wedge the
 * command queue (#1682); the git-only paths must not be taken in that case.
 * @return true when git commands can actually run.
 */
auto ImitatePass::gitConfigured() const -> bool {
  return m_settings.useGit && !m_settings.gitExecutable.isEmpty();
}

/**
 * @brief ImitatePass::gitReady gitConfigured() plus a status message.
 *
 * For the user-facing operations: tells the user once per operation why git
 * was skipped, so the store silently drifting from git does not go unnoticed.
 * @return true when git commands can actually run.
 */
auto ImitatePass::gitReady() -> bool {
  if (m_settings.useGit && m_settings.gitExecutable.isEmpty()) {
    emit statusMsg(tr("Git executable not configured, skipping git"), 3000);
    return false;
  }
  return m_settings.useGit;
}

/**
 * @brief ImitatePass::executeGit easy wrapper for running git commands
 * @param args
 */
void ImitatePass::executeGit(PROCESS id, const QStringList &args, QString input,
                             bool readStdout, bool readStderr) {
  // Callers check gitReady() first and fall back to plain filesystem
  // operations when no git executable is configured. Should an empty
  // executable still get here, the Executor now reports it as an error
  // instead of wedging the queue (#1682).
  executeWrapper(id, m_settings.gitExecutable, args, std::move(input),
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
  PROCESS pid = transactionIsOver(static_cast<PROCESS>(id));
  m_transactionOutput.append(out);

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
  Pass::finished(pid, exitCode, m_transactionOutput, err);
  m_transactionOutput.clear();
}

/**
 * @brief Register a transaction before each wrapped execution.
 *
 * Native mode treats every git/gpg invocation as a transaction; the base
 * Pass::executeWrapper calls this hook just before dispatching.
 * @param id Process identifier of the command about to run.
 */
void ImitatePass::beforeExecute(PROCESS id) { transactionAdd(id); }

/**
 * @brief Decrypt one .gpg file and return lines matching rx.
 */
auto ImitatePass::grepMatchFile(const QProcessEnvironment &env,
                                const QString &gpgExe, const QString &filePath,
                                const QRegularExpression &rx) -> QStringList {
  QString translatedPath = filePath;
  if (gpgExe.startsWith(QStringLiteral("wsl "))) {
    QString wslPath;
    const int wrc = Executor::executeBlocking(
        QStringLiteral("wsl"),
        Executor::wslExecArgs(QStringLiteral("wslpath"), {filePath}), &wslPath);
    const QString translated = wslPath.trimmed();
    if (wrc == 0 && !translated.isEmpty())
      translatedPath = translated;
  }
  QString plaintext;
  const int rc =
      Executor::executeBlocking(env, gpgExe,
                                {"-d", "--quiet", "--yes", "--no-encrypt-to",
                                 "--batch", "--use-agent", translatedPath},
                                &plaintext);
  if (rc != 0 || plaintext.isEmpty())
    return {};
  QStringList matches;
  for (const QString &line : plaintext.split('\n')) {
    QString candidate = line;
    if (candidate.endsWith('\r'))
      candidate.chop(1);
    const QString t = candidate.trimmed();
    if (!t.isEmpty() && candidate.contains(rx))
      matches << t;
  }
  return matches;
}

/**
 * @brief Walk the store, decrypt every .gpg file, collect matches.
 */
auto ImitatePass::grepScanStore(const QProcessEnvironment &env,
                                const QString &gpgExe, const QString &storeDir,
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
 * The pattern is evaluated with `QRegularExpression` (**PCRE**), which differs
 * from the POSIX BRE dialect of the `pass` backend — see Pass::Grep for the
 * cross-backend caveat.
 *
 * Runs a background thread to avoid blocking the UI. Results are emitted on
 * the main thread via QMetaObject::invokeMethod. A sequence counter discards
 * results from superseded searches.
 */
void ImitatePass::Grep(QString pattern, bool caseInsensitive) {
  for (QThread *t : std::as_const(m_grepThreads))
    if (t && t->isRunning())
      t->requestInterruption();
  // No wait() — blocking the UI thread while GPG decrypts would freeze the
  // interface. Stale results are discarded via the sequence counter.

  // Advance the sequence before any early return so in-flight workers from the
  // previous query fail the seq check and cannot publish stale results.
  const int seq = ++m_grepSeq;

  // Use trimmed() rather than isEmpty(): a whitespace-only string is a valid
  // regex that matches every non-empty line, which is almost never intentional
  // and would decrypt the entire store.
  //
  // Both early returns post finishedGrep via Qt::QueuedConnection so that the
  // signal is always delivered asynchronously after Grep() returns, matching
  // the contract of the threaded path.
  if (pattern.trimmed().isEmpty()) {
    QMetaObject::invokeMethod(
        this,
        [this, seq]() {
          if (m_grepSeq == seq)
            emit finishedGrep({});
        },
        Qt::QueuedConnection);
    return;
  }

  const QRegularExpression rx(
      pattern, caseInsensitive ? QRegularExpression::CaseInsensitiveOption
                               : QRegularExpression::PatternOptions{});
  if (!rx.isValid()) {
    QMetaObject::invokeMethod(
        this,
        [this, seq]() {
          if (m_grepSeq == seq)
            emit finishedGrep({});
        },
        Qt::QueuedConnection);
    return;
  }
  const QString gpgExe = m_settings.gpgExecutable;
  const QString storeDir = m_settings.passStore;
  const QProcessEnvironment env = exec.environment();
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

  QThread *thread = QThread::create(
      [gpgExe, storeDir, env, rx, emitResults = std::move(emitResults)]() {
        std::move(emitResults)(grepScanStore(env, gpgExe, storeDir, rx));
      });

  m_grepThreads.append(thread);
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);
  connect(thread, &QThread::finished, this,
          [this, thread]() { m_grepThreads.removeOne(thread); });
  thread->start();
}
