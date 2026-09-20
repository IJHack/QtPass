// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "realpass.h"
#include "executor.h"
#include "pathvalidator.h"
#include "qtpasslogging.h"
#include "util.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <utility>

using Enums::GIT_INIT;
using Enums::GIT_PULL;
using Enums::GIT_PUSH;
using Enums::PASS_COPY;
using Enums::PASS_INIT;
using Enums::PASS_INSERT;
using Enums::PASS_MOVE;
using Enums::PASS_REMOVE;
using Enums::PASS_SHOW;

RealPass::RealPass() : m_grep(this) {
  connect(&m_grep, &NativeGrep::finished, this, &RealPass::finishedGrep);
}

RealPass::~RealPass() { m_grep.cancel(); }

/**
 * @brief RealPass::GitInit pass git init wrapper
 */
void RealPass::GitInit() { executePass(GIT_INIT, {"git", "init"}); }

/**
 * @brief RealPass::GitInit pass git pull wrapper which blocks until process
 *                          finishes
 */
void RealPass::GitPull_b() {
  int result =
      Executor::executeBlocking(m_settings.passExecutable, {"git", "pull"});
  if (result != 0) {
    qCDebug(lcQtPass) << "Git pull failed with code:" << result;
  }
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
  // pass follows links as readily as gpg does.
  if (refuseLinkedPath(file + ".gpg")) {
    return;
  }
  queueShow(file);
  executePass(PASS_SHOW, {"show", file}, "", true);
}

/**
 * @brief RealPass::Insert pass insert
 */
void RealPass::Insert(QString file, QString newValue, bool overwrite) {
  if (refuseLinkedPath(file + ".gpg")) {
    return;
  }
  QStringList args = {"insert", "-m"};
  if (overwrite) {
    args.append("-f");
  }
  args.append(file);
  executePass(PASS_INSERT, args, newValue);
}

/**
 * @brief RealPass::Remove pass remove wrapper
 */
void RealPass::Remove(QString file, bool isDir) {
  // Nothing behind a link is the store's to delete.
  if (refuseLinkedPath(isDir ? file : file + ".gpg", false)) {
    return;
  }
  // pass rm turns a folder into "<folder>/" and rm -rf then follows a link
  // and empties its target, so a linked folder is unlinked here and pass's
  // git is only asked to forget it. pass rm -f on a linked .gpg unlinks.
  const QString full = QDir::cleanPath(m_settings.passStore + file);
  if (isDir && Util::isLinkedFolder(full)) {
    if (!Util::removeTree(full)) {
      emit critical(tr("Delete failed"),
                    tr("Could not remove the link %1.").arg(full));
      return;
    }
    if (m_settings.useGit) {
      // `pass git` exits 0 whatever git did, so ask by output whether git
      // knew the link, and run the rm blocking so the one commit is the one
      // PASS_REMOVE that finishes.
      const QString rel = QDir(m_settings.passStore).relativeFilePath(full);
      // With the store's environment (PASSWORD_STORE_DIR), as executePass.
      QString known;
      QString err;
      Executor::executeBlocking(exec.environment(), m_settings.passExecutable,
                                {"git", "ls-files", "--", rel}, &known, &err);
      if (!known.trimmed().isEmpty()) {
        Executor::executeBlocking(exec.environment(), m_settings.passExecutable,
                                  {"git", "rm", "-q", "--cached", "--", rel},
                                  &known, &err);
        executePass(PASS_REMOVE,
                    {"git", "commit", "-q", "-m",
                     "Remove for " + rel + " using QtPass.", "--", rel});
      }
    }
    return;
  }
  executePass(PASS_REMOVE, {"rm", (isDir ? "-rf" : "-f"), file});
}

/**
 * @brief RealPass::Init initialize pass repository
 *
 * @param path  Absolute path to new password-store
 * @param users list of users with ability to decrypt new password-store
 */
void RealPass::Init(QString path, const QList<UserInfo> &users) {
  // pass init writes .gpg-id with a shell redirection and gpg writes the
  // signature with --output; both follow a link planted under those names.
  const QString folder = QDir::cleanPath(path);
  if (refuseLinkedPath(path) ||
      refuseLinkedPath(folder + QStringLiteral("/.gpg-id")) ||
      refuseLinkedPath(folder + QStringLiteral("/.gpg-id.sig"))) {
    return;
  }
  // remove the passStore directory otherwise,
  // pass would create a passStore/passStore/dir
  // but you want passStore/dir
  // A plain prefix test would take /home/me/store-other for a folder of
  // /home/me/store; ask the same question every other store-boundary check
  // asks, then let QDir compute the relative part.
  const QString normalizedPath = QDir::cleanPath(path);
  const QString normalizedStore = QDir::cleanPath(m_settings.passStore);
  QString dirWithoutPassdir = normalizedPath;
  if (PathValidator::isPathInStore(normalizedStore, normalizedPath)) {
    dirWithoutPassdir = QDir(normalizedStore).relativeFilePath(normalizedPath);
    if (dirWithoutPassdir == QStringLiteral(".")) {
      dirWithoutPassdir.clear();
    }
  }
  QStringList args = {"init", "--path=" + dirWithoutPassdir};
  for (const UserInfo &user : users) {
    if (user.enabled) {
      args.append(user.key_id);
    }
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
  passMoveOrCopy(PASS_MOVE, QStringLiteral("mv"), src, dest, force);
}

/**
 * @brief RealPass::Copy copy a file (or folder)
 * @param src source file or folder
 * @param dest destination file or folder
 * @param force overwrite
 */
void RealPass::Copy(const QString src, const QString dest, const bool force) {
  passMoveOrCopy(PASS_COPY, QStringLiteral("cp"), src, dest, force);
}

/**
 * @brief RealPass::passMoveOrCopy shared `pass mv` / `pass cp` implementation.
 * @param id PASS_MOVE or PASS_COPY.
 * @param subcommand "mv" or "cp".
 * @param src source file or folder.
 * @param dest destination file or folder.
 * @param force overwrite.
 */
void RealPass::passMoveOrCopy(PROCESS id, const QString &subcommand,
                              const QString &src, const QString &dest,
                              const bool force) {
  if (refuseLinkedPath(src) || refuseLinkedPath(dest)) {
    return;
  }
  QFileInfo srcFileInfo = QFileInfo(src);
  QFileInfo destFileInfo = QFileInfo(dest);
  // A drop hands over the folder; pass then writes <folder>/<name>, and cp
  // writes through a link planted under that name.
  if (destFileInfo.isDir() &&
      refuseLinkedPath(QDir(dest).filePath(srcFileInfo.fileName()))) {
    return;
  }

  // force mode?
  // pass uses always the force mode, when call from eg. QT. so we have to
  // check if this are to files and the user didn't want to move force
  if (!force && srcFileInfo.isFile() && destFileInfo.isFile()) {
    return;
  }

  QString normalizedStore = QDir::cleanPath(m_settings.passStore);
  QString normalizedSrc = QDir::cleanPath(QDir(src).absolutePath());
  QString normalizedDest = QDir::cleanPath(QDir(dest).absolutePath());

  QString passSrc = QDir(normalizedStore).relativeFilePath(normalizedSrc);
  QString passDest = QDir(normalizedStore).relativeFilePath(normalizedDest);

  // remove the .gpg because pass will not work
  if (srcFileInfo.isFile() && srcFileInfo.suffix() == "gpg") {
    passSrc.replace(Util::endsWithGpg(), "");
  }
  // The destination usually does not exist yet; pass appends .gpg itself,
  // so a name that already ends in .gpg would come out as "name.gpg.gpg".
  if (!destFileInfo.isDir() && destFileInfo.suffix() == "gpg") {
    passDest.replace(Util::endsWithGpg(), "");
  }

  QStringList args;
  args << subcommand;
  if (force) {
    args << "-f";
  }
  args << passSrc;
  args << passDest;
  executePass(id, args);
}

/**
 * @brief Search all password content via 'pass grep'.
 *
 * The pattern is interpreted by `pass grep` (GNU grep, **POSIX BRE**), which
 * differs from the PCRE dialect used by the native backend — see
 * Pass::Grep for the cross-backend caveat.
 * @param pattern Search pattern (POSIX BRE).
 * @param caseInsensitive true for case-insensitive search.
 */
/**
 * @brief RealPass::Grep searches the store the way ImitatePass does.
 *
 * `pass grep` enumerates with `find -L`, which follows a link out of the
 * store and hands what it finds there to gpg; the native search walks real
 * files only (Util::regularFilesUnder) and needs the GPG executable, which
 * the pass backend has configured for everything else gpg does.
 */
void RealPass::Grep(QString pattern, bool caseInsensitive) {
  if (m_settings.gpgExecutable.isEmpty()) {
    emit statusMsg(tr("Search needs the GPG executable to be configured."),
                   5000);
    emit finishedGrep({});
    return;
  }
  m_grep.search(pattern, caseInsensitive, m_settings.gpgExecutable,
                m_settings.passStore, exec.environment());
}

/**
 * @brief Wrapper for executing pass commands.
 * @param id Process identifier for this operation
 * @param args Command-line arguments for pass
 * @param input Input to pass to the process (typically empty)
 * @param readStdout Whether to capture standard output
 * @param readStderr Whether to capture standard error output
 */
void RealPass::executePass(PROCESS id, const QStringList &args, QString input,
                           bool readStdout, bool readStderr) {
  executeWrapper(id, m_settings.passExecutable, args, std::move(input),
                 readStdout, readStderr);
}
