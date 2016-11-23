#include "realpass.h"
#include "qtpasssettings.h"

RealPass::RealPass() {}

/**
 * @brief RealPass::executePass easy wrapper for running pass
 * @param args
 */
void RealPass::executePass(QString args, QString input) {
  executeWrapper(QtPassSettings::getPassExecutable(), args, input);
}

/**
 * @brief RealPass::GitInit git init wrapper
 */
void RealPass::GitInit() { executePass("git init"); }

/**
 * @brief RealPass::GitPull git init wrapper
 */
void RealPass::GitPull() { executePass("git pull"); }

/**
 * @brief RealPass::GitPush git init wrapper
 */
void RealPass::GitPush() { executePass("git push"); }

/**
 * @brief RealPass::Show git init wrapper
 *
 * @param file      file to decrypt
 * @param block     wheater to wait for decryption process to finish
 *
 * @return  if block is set, returns exit status of internal decryption process
 *          otherwise returns QProcess::NormalExit
 */
QProcess::ExitStatus RealPass::Show(QString file, bool block) {
  executePass("show \"" + file + '"');
  if (block)
    return waitForProcess();
  return QProcess::NormalExit;
}

/**
 * @brief RealPass::Insert git init wrapper
 */
void RealPass::Insert(QString file, QString newValue, bool overwrite) {
  executePass(QString("insert ") + (overwrite ? "-f " : "") + "-m \"" + file +
                  '"',
              newValue);
}

/**
 * @brief RealPass::Remove git init wrapper
 */
void RealPass::Remove(QString file, bool isDir) {
  executePass(QString("rm ") + (isDir ? "-rf " : "-f ") + '"' + file + '"');
}

/**
 * @brief RealPass::Init initialize pass repository
 *
 * @param path  Absolute path to new password-store
 * @param users list of users with ability to decrypt new password-store
 */
void RealPass::Init(QString path, const QList<UserInfo> &users) {
  QString gpgIds = "";
  foreach (const UserInfo &user, users) {
    if (user.enabled) {
      gpgIds += user.key_id + " ";
    }
  }
  // remove the passStore directory otherwise,
  // pass would create a passStore/passStore/dir
  // but you want passStore/dir
  QString dirWithoutPassdir =
      path.remove(0, QtPassSettings::getPassStore().size());
  executePass("init --path=" + dirWithoutPassdir + " " + gpgIds);
}
