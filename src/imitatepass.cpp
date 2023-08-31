#include "imitatepass.h"
#include "qtpasssettings.h"
#include <QDirIterator>
#include <QRegularExpression>
#include <utility>

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

using namespace Enums;

/**
 * @brief ImitatePass::ImitatePass for situaions when pass is not available
 * we imitate the behavior of pass https://www.passwordstore.org/
 */
ImitatePass::ImitatePass() = default;

static QString pgit(const QString &path) {
  if (!QtPassSettings::getGitExecutable().startsWith("wsl "))
    return path;
  QString res = "$(wslpath " + path + ")";
  return res.replace('\\', '/');
}

static QString pgpg(const QString &path) {
  if (!QtPassSettings::getGpgExecutable().startsWith("wsl "))
    return path;
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
 * @brief ImitatePass::GitPull git init wrapper
 */
void ImitatePass::GitPull() { executeGit(GIT_PULL, {"pull"}); }

/**
 * @brief ImitatePass::GitPull_b git pull wrapper
 */
void ImitatePass::GitPull_b() {
  exec.executeBlocking(QtPassSettings::getGitExecutable(), {"pull"});
}

/**
 * @brief ImitatePass::GitPush git init wrapper
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
    //  TODO(bezet): probably throw here
    emit critical(tr("Can not edit"),
                  tr("Could not read encryption key to use, .gpg-id "
                     "file missing or invalid."));
    return;
  }
  QStringList args = {"--batch", "-eq", "--output", pgpg(file)};
  for (auto &r : recipients) {
    args.append("-r");
    args.append(r);
  };
  if (overwrite)
    args.append("--yes");
  args.append("-");
  executeGpg(PASS_INSERT, args, newValue);
  if (!QtPassSettings::isUseWebDav() && QtPassSettings::isUseGit()) {
    //    TODO(bezet) why not?
    if (!overwrite)
      executeGit(GIT_ADD, {"add", pgit(file)});
    QString path = QDir(QtPassSettings::getPassStore()).relativeFilePath(file);
    path.replace(QRegularExpression("\\.gpg$"), "");
    QString msg =
        QString(overwrite ? "Edit" : "Add") + " for " + path + " using QtPass.";
    GitCommit(file, msg);
  }
}

/**
 * @brief ImitatePass::GitCommit commit a file to git with an appropriate commit
 * message
 * @param file
 * @param msg
 */
void ImitatePass::GitCommit(const QString &file, const QString &msg) {
  if (file.isEmpty())
    executeGit(GIT_COMMIT, {"commit", "-m", msg});
  else
    executeGit(GIT_COMMIT, {"commit", "-m", msg, "--", pgit(file)});
}

/**
 * @brief ImitatePass::Remove custom implementation of "pass remove"
 */
void ImitatePass::Remove(QString file, bool isDir) {
  file = QtPassSettings::getPassStore() + file;
  transactionHelper trans(this, PASS_REMOVE);
  if (!isDir)
    file += ".gpg";
  if (QtPassSettings::isUseGit()) {
    executeGit(GIT_RM, {"rm", (isDir ? "-rf" : "-f"), pgit(file)});
    //  TODO(bezet): commit message used to have pass-like file name inside(ie.
    //  getFile(file, true)
    GitCommit(file, "Remove for " + file + " using QtPass.");
  } else {
    if (isDir) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
      QDir dir(file);
      dir.removeRecursively();
#else
      removeDir(QtPassSettings::getPassStore() + file);
#endif
    } else
      QFile(file).remove();
  }
}

/**
 * @brief ImitatePass::Init initialize pass repository
 *
 * @param path      path in which new password-store will be created
 * @param users     list of users who shall be able to decrypt passwords in
 * path
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
    QString out;
    QStringList args =
        QStringList{"--status-fd=1", "--list-secret-keys"} + signingKeys;
    exec.executeBlocking(QtPassSettings::getGpgExecutable(), args, &out);
    bool found = false;
    for (auto &key : signingKeys) {
      if (out.contains("[GNUPG:] KEY_CONSIDERED " + key)) {
        found = true;
        break;
      }
    }
    if (!found) {
      emit critical(tr("No signing key!"),
                    tr("None of the secret signing keys is available.\n"
                       "You will not be able to change the user list!"));
      return;
    }
    QFileInfo checkFile(gpgIdSigFile);
    if (!checkFile.exists() || !checkFile.isFile())
      addSigFile = true;
  }

  QString gpgIdFile = path + ".gpg-id";
  QFile gpgId(gpgIdFile);
  bool addFile = false;
  transactionHelper trans(this, PASS_INIT);
  if (QtPassSettings::isAddGPGId(true)) {
    QFileInfo checkFile(gpgIdFile);
    if (!checkFile.exists() || !checkFile.isFile())
      addFile = true;
  }
  if (!gpgId.open(QIODevice::WriteOnly | QIODevice::Text)) {
    emit critical(tr("Cannot update"),
                  tr("Failed to open .gpg-id for writing."));
    return;
  }
  bool secret_selected = false;
  foreach (const UserInfo &user, users) {
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
    return;
  }

  if (!signingKeys.isEmpty()) {
    QStringList args;
    for (auto &key : signingKeys) {
      args.append(QStringList{"--default-key", key});
    }
    args.append(QStringList{"--yes", "--detach-sign", gpgIdFile});
    exec.executeBlocking(QtPassSettings::getGpgExecutable(), args);
    if (!verifyGpgIdFile(gpgIdFile)) {
      emit critical(tr("Check .gpgid file signature!"),
                    tr("Signature for %1 is invalid.").arg(gpgIdFile));
      return;
    }
  }

  if (!QtPassSettings::isUseWebDav() && QtPassSettings::isUseGit() &&
      !QtPassSettings::getGitExecutable().isEmpty()) {
    if (addFile)
      executeGit(GIT_ADD, {"add", pgit(gpgIdFile)});
    QString commitPath = gpgIdFile;
    commitPath.replace(QRegularExpression("\\.gpg$"), "");
    GitCommit(gpgIdFile, "Added " + commitPath + " using QtPass.");
    if (!signingKeys.isEmpty()) {
      if (addSigFile)
        executeGit(GIT_ADD, {"add", pgit(gpgIdSigFile)});
      commitPath = gpgIdSigFile;
      commitPath.replace(QRegularExpression("\\.gpg$"), "");
      GitCommit(gpgIdSigFile, "Added " + commitPath + " using QtPass.");
    }
  }
  reencryptPath(path);
}

/**
 * @brief ImitatePass::verifyGpgIdFile verify detached gpgid file signature.
 * @param file which gpgid file.
 * @return was verification succesful?
 */
bool ImitatePass::verifyGpgIdFile(const QString &file) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  QStringList signingKeys =
      QtPassSettings::getPassSigningKey().split(" ", Qt::SkipEmptyParts);
#else
  QStringList signingKeys =
      QtPassSettings::getPassSigningKey().split(" ", QString::SkipEmptyParts);
#endif
  if (signingKeys.isEmpty())
    return true;
  QString out;
  QStringList args =
      QStringList{"--verify", "--status-fd=1", pgpg(file) + ".sig", pgpg(file)};
  exec.executeBlocking(QtPassSettings::getGpgExecutable(), args, &out);
  QRegularExpression re(
      "^\\[GNUPG:\\] VALIDSIG ([A-F0-9]{40}) .* ([A-F0-9]{40})\\r?$",
      QRegularExpression::MultilineOption);
  QRegularExpressionMatch m = re.match(out);
  if (!m.hasMatch())
    return false;
  QStringList fingerprints = m.capturedTexts();
  fingerprints.removeFirst();
  for (auto &key : signingKeys) {
    if (fingerprints.contains(key))
      return true;
  }
  return false;
}

/**
 * @brief ImitatePass::removeDir delete folder recursive.
 * @param dirName which folder.
 * @return was removal succesful?
 */
bool ImitatePass::removeDir(const QString &dirName) {
  bool result = true;
  QDir dir(dirName);

  if (dir.exists(dirName)) {
    Q_FOREACH (QFileInfo info,
               dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System |
                                     QDir::Hidden | QDir::AllDirs | QDir::Files,
                                 QDir::DirsFirst)) {
      if (info.isDir())
        result = removeDir(info.absoluteFilePath());
      else
        result = QFile::remove(info.absoluteFilePath());

      if (!result)
        return result;
    }
    result = dir.rmdir(dirName);
  }
  return result;
}

/**
 * @brief ImitatePass::reencryptPath reencrypt all files under the chosen
 * directory
 *
 * This is stil quite experimental..
 * @param dir
 */
void ImitatePass::reencryptPath(const QString &dir) {
  emit statusMsg(tr("Re-encrypting from folder %1").arg(dir), 3000);
  emit startReencryptPath();
  if (QtPassSettings::isAutoPull()) {
    //  TODO(bezet): move statuses inside actions?
    emit statusMsg(tr("Updating password-store"), 2000);
    GitPull_b();
  }
  QDir currentDir;
  QDirIterator gpgFiles(dir, QStringList() << "*.gpg", QDir::Files,
                        QDirIterator::Subdirectories);
  QStringList gpgIdFilesVerified;
  QStringList gpgId;
  while (gpgFiles.hasNext()) {
    QString fileName = gpgFiles.next();
    if (gpgFiles.fileInfo().path() != currentDir.path()) {
      QString gpgIdPath = Pass::getGpgIdPath(fileName);
      if (!gpgIdFilesVerified.contains(gpgIdPath)) {
        if (!verifyGpgIdFile(gpgIdPath)) {
          emit critical(tr("Check .gpgid file signature!"),
                        tr("Signature for %1 is invalid.").arg(gpgIdPath));
          emit endReencryptPath();
          return;
        }
        gpgIdFilesVerified.append(gpgIdPath);
      }
      gpgId = getRecipientList(fileName);
      gpgId.sort();
    }
    //  TODO(bezet): enable --with-colons for better future-proofness?
    QStringList args = {
        "-v",          "--no-secmem-warning", "--no-permission-warning",
        "--list-only", "--keyid-format=long", pgpg(fileName)};
    QString keys, err;
    exec.executeBlocking(QtPassSettings::getGpgExecutable(), args, &keys, &err);
    QStringList actualKeys;
    keys += err;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    QStringList key =
        keys.split(QRegularExpression("[\r\n]"), Qt::SkipEmptyParts);
#else
    QStringList key =
        keys.split(QRegularExpression("[\r\n]"), QString::SkipEmptyParts);
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
    if (actualKeys != gpgId) {
      // dbg()<< actualKeys << gpgId << getRecipientList(fileName);
#ifdef QT_DEBUG
      dbg() << "reencrypt " << fileName << " for " << gpgId;
#endif
      QString local_lastDecrypt = "Could not decrypt";
      args = QStringList{
          "-d",      "--quiet",     "--yes",       "--no-encrypt-to",
          "--batch", "--use-agent", pgpg(fileName)};
      exec.executeBlocking(QtPassSettings::getGpgExecutable(), args,
                           &local_lastDecrypt);

      if (!local_lastDecrypt.isEmpty() &&
          local_lastDecrypt != "Could not decrypt") {
        if (local_lastDecrypt.right(1) != "\n")
          local_lastDecrypt += "\n";

        QStringList recipients = Pass::getRecipientList(fileName);
        if (recipients.isEmpty()) {
          emit critical(tr("Can not edit"),
                        tr("Could not read encryption key to use, .gpg-id "
                           "file missing or invalid."));
          return;
        }
        args =
            QStringList{"--yes", "--batch", "-eq", "--output", pgpg(fileName)};
        for (auto &i : recipients) {
          args.append("-r");
          args.append(i);
        }
        args.append("-");
        exec.executeBlocking(QtPassSettings::getGpgExecutable(), args,
                             local_lastDecrypt);

        if (!QtPassSettings::isUseWebDav() && QtPassSettings::isUseGit()) {
          exec.executeBlocking(QtPassSettings::getGitExecutable(),
                               {"add", pgit(fileName)});
          QString path =
              QDir(QtPassSettings::getPassStore()).relativeFilePath(fileName);
          path.replace(QRegularExpression("\\.gpg$"), "");
          exec.executeBlocking(QtPassSettings::getGitExecutable(),
                               {"commit", pgit(fileName), "-m",
                                "Edit for " + path + " using QtPass."});
        }

      } else {
#ifdef QT_DEBUG
        dbg() << "Decrypt error on re-encrypt";
#endif
      }
    }
  }
  if (QtPassSettings::isAutoPush()) {
    emit statusMsg(tr("Updating password-store"), 2000);
    //  TODO(bezet): this is non-blocking and shall be done outside
    GitPush();
  }
  emit endReencryptPath();
}

void ImitatePass::Move(const QString src, const QString dest,
                       const bool force) {
  transactionHelper trans(this, PASS_MOVE);
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
        return;
      }
    } else if (destFileInfo.isDir()) {
      destFile = QDir(dest).filePath(srcFileBaseName);
    } else {
      destFile = dest;
    }

    if (destFile.endsWith(".gpg", Qt::CaseInsensitive))
      destFile.chop(4); // make sure suffix is lowercase
    destFile.append(".gpg");
  } else if (srcFileInfo.isDir()) {
    if (destFileInfo.isDir()) {
      destFile = QDir(dest).filePath(srcFileBaseName);
    } else if (destFileInfo.isFile()) {
#ifdef QT_DEBUG
      dbg() << "Destination is a file";
#endif
      return;
    } else {
      destFile = dest;
    }
  } else {
#ifdef QT_DEBUG
    dbg() << "Source file does not exist";
#endif
    return;
  }

#ifdef QT_DEBUG
  dbg() << "Move Source: " << src;
  dbg() << "Move Destination: " << destFile;
#endif

  if (QtPassSettings::isUseGit()) {
    QStringList args;
    args << "mv";
    if (force) {
      args << "-f";
    }
    args << pgit(src);
    args << pgit(destFile);
    executeGit(GIT_MOVE, args);

    QString relSrc = QDir(QtPassSettings::getPassStore()).relativeFilePath(src);
    relSrc.replace(QRegularExpression("\\.gpg$"), "");
    QString relDest =
        QDir(QtPassSettings::getPassStore()).relativeFilePath(destFile);
    relDest.replace(QRegularExpression("\\.gpg$"), "");
    QString message = QString("Moved for %1 to %2 using QtPass.");
    message = message.arg(relSrc).arg(relDest);
    GitCommit("", message);
  } else {
    QDir qDir;
    if (force) {
      qDir.remove(destFile);
    }
    qDir.rename(src, destFile);
  }
}

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

    QString message = QString("copied from %1 to %2 using QTPass.");
    message = message.arg(src).arg(dest);
    GitCommit("", message);
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
    if (pid == INVALID)
      return;
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
