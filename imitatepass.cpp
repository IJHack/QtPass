#include "imitatepass.h"
#include "mainwindow.h"
#include "qtpasssettings.h"

ImitatePass::ImitatePass() {}

/**
 * @brief ImitatePass::GitInit git init wrapper
 */
void ImitatePass::GitInit() {
  executeWrapper(QtPassSettings::getGitExecutable(),
                 "init \"" + QtPassSettings::getPassStore() + '"');
}

/**
 * @brief ImitatePass::GitPull git init wrapper
 */
void ImitatePass::GitPull() {
  executeWrapper(QtPassSettings::getGitExecutable(), "pull");
}

/**
 * @brief ImitatePass::GitPush git init wrapper
 */
void ImitatePass::GitPush() {
  executeWrapper(QtPassSettings::getGitExecutable(), "push");
}

/**
 * @brief ImitatePass::Show git init wrapper
 */
QProcess::ExitStatus ImitatePass::Show(QString file, bool block) {
  //  TODO(bezet): apparently not yet needed
  //  file += ".gpg";
  executeWrapper(QtPassSettings::getGpgExecutable(),
                 "-d --quiet --yes --no-encrypt-to --batch --use-agent \"" +
                     file + '"');
  if (block)
    return waitForProcess();
  return QProcess::NormalExit;
}

/**
 * @brief ImitatePass::Insert git init wrapper
 *
 * @param file      file to be created
 * @param value     value to be stored in file
 * @param overwrite whether to overwrite existing file
 */
void ImitatePass::Insert(QString file, QString newValue, bool overwrite) {
  file += ".gpg";
  //  TODO(bezet): getRecipientString is in MainWindow for now - fix this ;)
  QString recipients = Pass::getRecipientString(file, " -r ");
  if (recipients.isEmpty()) {
    //  TODO(bezet): probably throw here
    emit critical(tr("Can not edit"),
                  tr("Could not read encryption key to use, .gpg-id "
                     "file missing or invalid."));
    return;
  }
  QString force(overwrite ? " --yes " : " ");
  executeWrapper(QtPassSettings::getGpgExecutable(),
                 force + "--batch -eq --output \"" + file + "\" " + recipients +
                     " -",
                 newValue);
  if (!QtPassSettings::isUseWebDav() && QtPassSettings::isUseGit()) {
    if (!overwrite)
      executeWrapper(QtPassSettings::getGitExecutable(), "add \"" + file + '"');
    QString path = QDir(QtPassSettings::getPassStore()).relativeFilePath(file);
    path.replace(QRegExp("\\.gpg$"), "");
    executeWrapper(QtPassSettings::getGitExecutable(),
                   "commit \"" + file + "\" -m \"" +
                       (overwrite ? "Edit" : "Add") + " for " + path +
                       " using QtPass.\"");
  }
}

/**
 * @brief ImitatePass::Remove git init wrapper
 */
void ImitatePass::Remove(QString file, bool isDir) {
  if (QtPassSettings::isUseGit()) {
    executeWrapper(QtPassSettings::getGitExecutable(),
                   QString("rm ") + (isDir ? "-rf " : "-f ") + '"' + file +
                       '"');
    //  TODO(bezet): commit message used to have pass-like file name inside(ie.
    //  getFile(file, true)
    executeWrapper(QtPassSettings::getGitExecutable(),
                   "commit \"" + file + "\" -m \"Remove for " + file +
                       " using QtPass.\"");
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
 * @param users     list of users who shall be able to decrypt passwords in path
 */
void ImitatePass::Init(QString path, const QList<UserInfo> &users) {
  QString gpgIdFile = path + ".gpg-id";
  QFile gpgId(gpgIdFile);
  bool addFile = false;
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

  if (!QtPassSettings::isUseWebDav() && QtPassSettings::isUseGit() &&
      !QtPassSettings::getGitExecutable().isEmpty()) {
    if (addFile)
      executeWrapper(QtPassSettings::getGitExecutable(),
                     "add \"" + gpgIdFile + '"');
    QString path = gpgIdFile;
    path.replace(QRegExp("\\.gpg$"), "");
    executeWrapper(QtPassSettings::getGitExecutable(),
                   "commit \"" + gpgIdFile + "\" -m \"Added " + path +
                       " using QtPass.\"");
  }
  reencryptPath(path);
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
 * @brief MainWindow::reencryptPath reencrypt all files under the chosen
 * directory
 *
 * This is stil quite experimental..
 * @param dir
 */
void ImitatePass::reencryptPath(QString dir) {
  emit statusMsg(tr("Re-encrypting from folder %1").arg(dir), 3000);
  emit startReencryptPath();
  if (QtPassSettings::isAutoPull()) {
    //  TODO(bezet): move statuses inside actions?
    emit statusMsg(tr("Updating password-store"), 2000);
    GitPull();
  }
  waitFor(50);
  process.waitForFinished();
  QDir currentDir;
  QDirIterator gpgFiles(dir, QStringList() << "*.gpg", QDir::Files,
                        QDirIterator::Subdirectories);
  QStringList gpgId;
  while (gpgFiles.hasNext()) {
    QString fileName = gpgFiles.next();
    if (gpgFiles.fileInfo().path() != currentDir.path()) {
      gpgId = getRecipientList(fileName);
      gpgId.sort();
    }
    process.waitForFinished();
    executeWrapper(QtPassSettings::getGpgExecutable(),
                   "-v --no-secmem-warning "
                   "--no-permission-warning --list-only "
                   "--keyid-format long " +
                       fileName);
    process.waitForFinished(3000);
    QStringList actualKeys;
    QString keys =
        process.readAllStandardOutput() + process.readAllStandardError();
    QStringList key = keys.split("\n");
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
      // qDebug() << actualKeys << gpgId << getRecipientList(fileName);
      qDebug() << "reencrypt " << fileName << " for " << gpgId;
      QString local_lastDecrypt = "Could not decrypt";
      emit lastDecrypt(local_lastDecrypt);
      executeWrapper(QtPassSettings::getGpgExecutable(),
                     "-d --quiet --yes --no-encrypt-to --batch --use-agent \"" +
                         fileName + '"');
      process.waitForFinished(30000); // long wait (passphrase stuff)
      local_lastDecrypt = process.readAllStandardOutput();
      emit lastDecrypt(local_lastDecrypt);

      if (!local_lastDecrypt.isEmpty() &&
          local_lastDecrypt != "Could not decrypt") {
        if (local_lastDecrypt.right(1) != "\n")
          local_lastDecrypt += "\n";

        emit lastDecrypt(local_lastDecrypt);
        QString recipients = getRecipientString(fileName, " -r ");
        if (recipients.isEmpty()) {
          emit critical(tr("Can not edit"),
                        tr("Could not read encryption key to use, .gpg-id "
                           "file missing or invalid."));
          return;
        }
        executeWrapper(QtPassSettings::getGpgExecutable(),
                       "--yes --batch -eq --output \"" + fileName + "\" " +
                           recipients + " -",
                       local_lastDecrypt);
        process.waitForFinished(3000);

        if (!QtPassSettings::isUseWebDav() && QtPassSettings::isUseGit()) {
          executeWrapper(QtPassSettings::getGitExecutable(),
                         "add \"" + fileName + '"');
          QString path =
              QDir(QtPassSettings::getPassStore()).relativeFilePath(fileName);
          path.replace(QRegExp("\\.gpg$"), "");
          executeWrapper(QtPassSettings::getGitExecutable(),
                         "commit \"" + fileName + "\" -m \"" + "Edit for " +
                             path + " using QtPass.\"");
          process.waitForFinished(3000);
        }

      } else {
        qDebug() << "Decrypt error on re-encrypt";
      }
    }
  }
  if (QtPassSettings::isAutoPush()) {
    emit statusMsg(tr("Updating password-store"), 2000);
    GitPush();
  }
  emit endReencryptPath();
}
