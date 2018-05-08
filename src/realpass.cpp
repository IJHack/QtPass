#include "realpass.h"
#include "qtpasssettings.h"

#include <QDir>
#include <QFileInfo>

using namespace Enums;

RealPass::RealPass() {}

/**
 * @brief RealPass::GitInit pass git init wrapper
 */
void RealPass::GitInit() { executePass(GIT_INIT, {"git", "init"}); }

/**
 * @brief RealPass::GitInit pass git pull wrapper which blocks until process
 *                          finishes
 */
void RealPass::GitPull_b() {
  exec.executeBlocking(QtPassSettings::getPassExecutable(), {"git", "pull"});
}

/**
 * @brief RealPass::GitPull pass git pull wrapper
 */
void RealPass::GitPull() { executePass(GIT_PULL, {"git", "pull"}); }

/**
 * @brief RealPass::GitPush pass git push wrapper
 */
void RealPass::GitPush() { executePass(GIT_PUSH, {"git", "push"}); }

/**
 * @brief RealPass::Show pass show
 *
 * @param file      file to decrypt
 *
 * @return  if block is set, returns exit status of internal decryption
 * process
 *          otherwise returns QProcess::NormalExit
 */
void RealPass::Show(QString file) {
  executePass(PASS_SHOW, {"show", file}, "", true);
}

/**
 * @brief RealPass::OtpGenerate pass otp
 * @param file      file containig OTP uri
 */
void RealPass::OtpGenerate(QString file) {
  executePass(PASS_OTP_SHOW, {"otp", file}, "", true);
}


/**
 * @brief RealPass::Insert pass insert
 */
void RealPass::Insert(QString file, QString newValue, bool overwrite) {
  QStringList args = {"insert", "-m"};
  if (overwrite)
    args.append("-f");
  args.append(file);
  executePass(PASS_INSERT, args, newValue);
}

/**
 * @brief RealPass::Remove pass remove wrapper
 */
void RealPass::Remove(QString file, bool isDir) {
  executePass(PASS_REMOVE, {"rm", (isDir ? "-rf" : "-f"), file});
}

/**
 * @brief RealPass::Init initialize pass repository
 *
 * @param path  Absolute path to new password-store
 * @param users list of users with ability to decrypt new password-store
 */
void RealPass::Init(QString path, const QList<UserInfo> &users) {
  // remove the passStore directory otherwise,
  // pass would create a passStore/passStore/dir
  // but you want passStore/dir
  QString dirWithoutPassdir =
      path.remove(0, QtPassSettings::getPassStore().size());
  QStringList args = {"init", "--path=" + dirWithoutPassdir};
  foreach (const UserInfo &user, users) {
    if (user.enabled)
      args.append(user.key_id);
  }
  executePass(PASS_INIT, args);
}

/**
 * @brief RealPass::Move move a file (or folder)
 * @param src source file or folder
 * @param dest destination file or folder
 * @param force overwrite
 */
void RealPass::Move(const QString src, const QString dest, const bool force) {
  QFileInfo srcFileInfo = QFileInfo(src);
  QFileInfo destFileInfo = QFileInfo(dest);

  // force mode?
  // pass uses always the force mode, when call from eg. QT. so we have to check
  // if this are to files
  // and the user didnt want to move force
  if (force == false && srcFileInfo.isFile() && destFileInfo.isFile()) {
    return;
  }

  QString passSrc = QDir(QtPassSettings::getPassStore())
                        .relativeFilePath(QDir(src).absolutePath());
  QString passDest = QDir(QtPassSettings::getPassStore())
                         .relativeFilePath(QDir(dest).absolutePath());

  // remove the .gpg because pass will not work
  if (srcFileInfo.isFile() && srcFileInfo.suffix() == "gpg") {
    passSrc.replace(QRegExp("\\.gpg$"), "");
  }
  if (destFileInfo.isFile() && destFileInfo.suffix() == "gpg") {
    passDest.replace(QRegExp("\\.gpg$"), "");
  }

  QStringList args;
  args << "mv";
  if (force) {
    args << "-f";
  }
  args << passSrc;
  args << passDest;
  executePass(PASS_MOVE, args);
}

/**
 * @brief RealPass::Copy copy a file (or folder)
 * @param src source file or folder
 * @param dest destination file or folder
 * @param force overwrite
 */
void RealPass::Copy(const QString src, const QString dest, const bool force) {
  QFileInfo srcFileInfo = QFileInfo(src);
  QFileInfo destFileInfo = QFileInfo(dest);
  // force mode?
  // pass uses always the force mode, when call from eg. QT. so we have to check
  // if this are to files
  // and the user didnt want to move force
  if (force == false && srcFileInfo.isFile() && destFileInfo.isFile()) {
    return;
  }

  QString passSrc = QDir(QtPassSettings::getPassStore())
                        .relativeFilePath(QDir(src).absolutePath());
  QString passDest = QDir(QtPassSettings::getPassStore())
                         .relativeFilePath(QDir(dest).absolutePath());

  // remove the .gpg because pass will not work
  if (srcFileInfo.isFile() && srcFileInfo.suffix() == "gpg") {
    passSrc.replace(QRegExp("\\.gpg$"), "");
  }
  if (destFileInfo.isFile() && destFileInfo.suffix() == "gpg") {
    passDest.replace(QRegExp("\\.gpg$"), "");
  }
  QStringList args;
  args << "cp";
  if (force) {
    args << "-f";
  }
  args << passSrc;
  args << passDest;
  executePass(PASS_COPY, args);
}

/**
 * @brief RealPass::executePass easy wrapper for running pass
 * @param args
 */
void RealPass::executePass(PROCESS id, const QStringList &args, QString input,
                           bool readStdout, bool readStderr) {
  executeWrapper(id, QtPassSettings::getPassExecutable(), args, input,
                 readStdout, readStderr);
}
