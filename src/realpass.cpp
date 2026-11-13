// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "realpass.h"
#include "debughelper.h"
#include "pathvalidator.h"
#include "util.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <utility>

#ifndef Q_OS_WIN
#include <sys/stat.h>
#endif

using Enums::GIT_INIT;
using Enums::GIT_PULL;
using Enums::GIT_PUSH;
using Enums::PASS_COPY;
using Enums::PASS_GREP;
using Enums::PASS_INIT;
using Enums::PASS_INSERT;
using Enums::PASS_MOVE;
using Enums::PASS_OTP_GENERATE;
using Enums::PASS_REMOVE;
using Enums::PASS_SHOW;

RealPass::RealPass() = default;

/**
 * @brief RealPass::GitInit pass git init wrapper
 */
void RealPass::GitInit() { executePass(GIT_INIT, {"git", "init"}); }

/**
 * @brief RealPass::GitInit pass git pull wrapper which blocks until process
 *                          finishes
 */
void RealPass::GitPull_b() {
  // In the store's environment, as every queued pass call: pass picks its
  // store from PASSWORD_STORE_DIR alone, so without it the pull before an
  // edit went to ~/.password-store instead of the configured store.
  QString err;
  int result =
      Executor::executeBlocking(exec.environment(), m_settings.passExecutable,
                                {"git", "pull"}, nullptr, &err);
  if (result != 0) {
#ifdef QT_DEBUG
    dbg() << "Git pull failed with code:" << result;
#endif
    // The entry about to be edited is not up to date with the remote.
    emit statusMsg(tr("Git pull failed: %1").arg(err.trimmed()), 5000);
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
  // "--" ends the options so an entry whose name begins with "-" (for example
  // "--" or "-c", which a co-writer can push) is read as a path, not a `pass
  // show` option that could decrypt a file outside the store or run a clip.
  executePass(PASS_SHOW, {"show", "--", file}, "", true);
}

/**
 * @brief RealPass::OtpGenerate pass otp
 * @param file      file containing OTP uri
 */
void RealPass::OtpGenerate(QString file) {
  executePass(PASS_OTP_GENERATE, {"otp", "--", file}, "", true);
}

/**
 * @brief RealPass::Insert pass insert
 */
void RealPass::Insert(QString file, QString newValue, bool overwrite) {
  m_insertTarget =
      QDir::cleanPath(QDir::isAbsolutePath(file)
                          ? file + ".gpg"
                          : QDir(m_settings.passStore).filePath(file) + ".gpg");
  m_insertTargetExisted = QFileInfo::exists(m_insertTarget);
  if (refuseMisreadList(m_insertTarget, QString())) {
    emit insertFailed(tr("The recipient list for this entry reads differently "
                         "in pass. Nothing was saved."),
                      false);
    return;
  }
  QStringList args = {"insert", "-m"};
  if (overwrite) {
    args.append("-f");
  }
  args.append("--");
  args.append(file);
  executePass(PASS_INSERT, args, newValue);
}

auto RealPass::insertWritten() const -> bool {
  return !m_insertTarget.isEmpty() && !m_insertTargetExisted &&
         QFileInfo::exists(m_insertTarget);
}

auto RealPass::refuseMisreadList(const QString &entry, const QString &tree,
                                 const QString &except) -> bool {
  QStringList lists;
  if (!entry.isEmpty()) {
    lists << Pass::getGpgIdPath(entry, m_settings.passStore);
  }
  if (!tree.isEmpty() && QFileInfo(tree).isDir()) {
    // pass re-encrypts every entry below the folder to its own nearest list,
    // so each list in the tree counts. Links are not followed: pass's find
    // does not follow them either.
    QDirIterator it(tree, {QStringLiteral(".gpg-id")},
                    QDir::Files | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
      lists << it.next();
    }
  }
  const QString skip =
      except.isEmpty() ? QString()
                       : QDir::cleanPath(QFileInfo(except).absoluteFilePath());
  constexpr qint64 kMaxGpgIdBytes = 64 * 1024;
  for (const QString &list : std::as_const(lists)) {
    const QString absolute =
        QDir::cleanPath(QFileInfo(list).absoluteFilePath());
    if (absolute == skip) {
      continue;
    }
    // A link is read through, as pass reads it.
    const QFileInfo info(list);
    if (!info.exists()) {
      continue; // a missing list is pass's to report
    }
    QString why;
    QFile file(list);
    if (!info.isFile() || info.size() > kMaxGpgIdBytes ||
        !file.open(QIODevice::ReadOnly)) {
      why = tr("it is not a regular file of at most 64 KiB");
    } else {
      why = Pass::passReadsDifferently(file.read(kMaxGpgIdBytes + 1));
    }
    if (why.isEmpty()) {
      continue;
    }
    //: %1 is the list's path, %2 says what is wrong with it.
    emit critical(tr("Recipient list reads differently"),
                  tr("pass reads %1 differently from QtPass (%2), so it could "
                     "encrypt to recipients the Users dialog does not show, "
                     "or run gpg with an option from the list. Nothing was "
                     "encrypted. Check the recipients in Users for that "
                     "folder and press OK to write the list anew.")
                      .arg(QDir::toNativeSeparators(list), why));
    return true;
  }
  return false;
}

/**
 * @brief RealPass::Remove pass remove wrapper
 */
void RealPass::Remove(QString file, bool isDir) {
  // A symbolic link found inside the store is not a folder of the store. For a
  // folder, `pass rm -rf` is handed the name with a trailing slash and follows
  // the link, emptying whatever it points to outside the store. Such a link can
  // arrive with a `git pull` from a shared store, so refuse the delete rather
  // than recurse through it; the link itself can be removed outside QtPass.
  // A folder comes with a trailing separator, through which lstat() follows
  // the link: look at the name without it.
  if (isDir &&
      QFileInfo(QDir::cleanPath(QDir(m_settings.passStore).filePath(file)))
          .isSymLink()) {
    emit critical(
        tr("Refusing to delete a link"),
        tr("\"%1\" is a symbolic link, not a folder of the store, so "
           "it was not deleted. Remove the link outside QtPass if you "
           "meant to.")
            .arg(file));
    emit finishedRemove(QString(), QString());
    return;
  }
  // A folder or entry under a linked folder is outside the store too: pass
  // would delete in the folder the link points to.
  if (Util::isBehindLink(QDir(m_settings.passStore).filePath(file),
                         m_settings.passStore)) {
    emit critical(tr("Refusing to delete through a link"),
                  tr("\"%1\" lies under a symbolic link, so it is not in the "
                     "store and was not deleted.")
                      .arg(file));
    emit finishedRemove(QString(), QString());
    return;
  }
  executePass(PASS_REMOVE, {"rm", (isDir ? "-rf" : "-f"), "--", file});
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
  QString normalizedPath = QDir::cleanPath(path);
  QString normalizedStore = QDir::cleanPath(m_settings.passStore);
  QString dirWithoutPassdir;
  if (normalizedPath.startsWith(normalizedStore)) {
    dirWithoutPassdir = normalizedPath.mid(normalizedStore.size());
    if (dirWithoutPassdir.startsWith('/')) {
      dirWithoutPassdir.remove(0, 1);
    }
  } else {
    dirWithoutPassdir = normalizedPath;
  }
  // pass init writes this folder's own list, then re-encrypts every entry
  // below it, each to its nearest list: the lists further down count.
  const QString folder =
      QDir::cleanPath(QDir(normalizedStore).filePath(dirWithoutPassdir));
  if (refuseMisreadList(QString(), folder,
                        QDir(folder).filePath(QStringLiteral(".gpg-id")))) {
    return;
  }
  QStringList args = {"init", "--path=" + dirWithoutPassdir};
  foreach (const UserInfo &user, users) {
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

namespace {
/**
 * @brief Whether @p a and @p b name the same file or folder, as a respelling
 * in case only does on a case-insensitive file system.
 * @param a One path.
 * @param b The other.
 * @return true when both exist and are one object.
 */
auto sameObject(const QString &a, const QString &b) -> bool {
#ifdef Q_OS_WIN
  const QString canonical = QFileInfo(a).canonicalFilePath();
  return !canonical.isEmpty() &&
         canonical.compare(QFileInfo(b).canonicalFilePath(),
                           Qt::CaseInsensitive) == 0;
#else
  struct stat first{};
  struct stat second{};
  return ::stat(QFile::encodeName(a).constData(), &first) == 0 &&
         ::stat(QFile::encodeName(b).constData(), &second) == 0 &&
         first.st_dev == second.st_dev && first.st_ino == second.st_ino;
#endif
}
} // namespace

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
  QFileInfo srcFileInfo = QFileInfo(src);
  QFileInfo destFileInfo = QFileInfo(dest);

  // force mode?
  // pass uses always the force mode, when call from eg. QT. so we have to
  // check if this are to files and the user didn't want to move force
  if (!force && srcFileInfo.isFile() && destFileInfo.isFile()) {
    return;
  }

  QString normalizedStore = QDir::cleanPath(m_settings.passStore);
  QString normalizedSrc = QDir::cleanPath(QDir(src).absolutePath());
  QString normalizedDest = QDir::cleanPath(QDir(dest).absolutePath());

  // pass resolves the names by its own rules, so judge what it does with
  // them, not dest as spelled here. It takes the entry <name>.gpg over a
  // folder of the same name unless the name ends in a slash; it appends
  // .gpg to the destination unless a folder is moved or the destination is
  // a folder (a link to one too: -d follows it); and mv and cp put the
  // source inside such a folder.
  const bool srcIsDir = QFileInfo(normalizedSrc).isDir();
  QString passSrc = QDir(normalizedStore).relativeFilePath(normalizedSrc);
  QString passDest = QDir(normalizedStore).relativeFilePath(normalizedDest);
  // remove the .gpg because pass will not work
  if (srcFileInfo.isFile() && srcFileInfo.suffix() == "gpg") {
    passSrc.replace(Util::endsWithGpg(), "");
  }
  if (srcIsDir) {
    passSrc += QLatin1Char('/');
  }
  if (destFileInfo.isFile() && destFileInfo.suffix() == "gpg") {
    passDest.replace(Util::endsWithGpg(), "");
  }
  QString newPath = QDir::cleanPath(QDir(normalizedStore).filePath(passDest));
  if (!srcIsDir && !QFileInfo(newPath).isDir()) {
    newPath += QLatin1String(".gpg");
  }
  const QString written =
      QFileInfo(newPath).isDir()
          ? QDir(newPath).filePath(QFileInfo(normalizedSrc).fileName())
          : newPath;

  // Refuse a move or copy whose source or destination resolves, after any
  // symbolic links are followed, to a path outside the store: a co-writer
  // can push a link that pass would follow, moving ciphertext out of the
  // store or over a file outside it. cp follows a link at the name it
  // writes, so that name must not be one either.
  if (!PathValidator::isPathInStore(normalizedStore, normalizedSrc) ||
      !PathValidator::isPathInStore(normalizedStore, normalizedDest) ||
      !PathValidator::isPathInStore(normalizedStore, newPath) ||
      QFileInfo(written).isSymLink() ||
      !PathValidator::isPathInStore(normalizedStore, written)) {
    emit critical(tr("Refusing to move or copy through a link"),
                  tr("The source or destination lies outside the store, or "
                     "behind a symbolic link that points outside it, so the "
                     "operation was not carried out."));
    if (subcommand == QStringLiteral("mv"))
      emit finishedMove(QString(), QString());
    else
      emit finishedCopy(QString(), QString());
    return;
  }
  // Nor anything already where pass writes: the entry of that name in a
  // folder an entry is dropped on or renamed into, or a folder of the moved
  // folder's name, which cp -r merges into, writing through a link inside
  // it. Force is consent for the entry named as dest only.
  // What is there may be the source itself. Renamed to its own name or
  // dropped on its own folder: nothing to do. Respelled in case only on a
  // case-insensitive file system: pass renames it.
  const bool isSource = sameObject(written, normalizedSrc);
  if (isSource && written == normalizedSrc) {
    return;
  }
  if (QFileInfo::exists(written) && !isSource &&
      !(force && srcFileInfo.isFile() && written == normalizedDest)) {
    emit critical(subcommand == QStringLiteral("mv") ? tr("Not moved")
                                                     : tr("Not copied"),
                  tr("%1 already exists, and pass would replace it. Nothing "
                     "was changed; rename one of them first.")
                      .arg(QDir::toNativeSeparators(written)));
    if (subcommand == QStringLiteral("mv"))
      emit finishedMove(QString(), QString());
    else
      emit finishedCopy(QString(), QString());
    return;
  }

  // pass re-encrypts what lands at the written path to its nearest list, and
  // a folder's own lists travel with it. Into an existing folder it
  // re-encrypts that whole folder (reencrypt_path "$new_path"), so every
  // list below it counts too.
  if (refuseMisreadList(written, srcIsDir ? normalizedSrc : QString()) ||
      (QFileInfo(newPath).isDir() && refuseMisreadList(QString(), newPath))) {
    if (subcommand == QStringLiteral("mv"))
      emit finishedMove(QString(), QString());
    else
      emit finishedCopy(QString(), QString());
    return;
  }

  QStringList args;
  args << subcommand;
  if (force) {
    args << "-f";
  }
  args << "--";
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
void RealPass::Grep(QString pattern, bool caseInsensitive) {
  QStringList args = {"grep"};
  if (caseInsensitive)
    args << "-i";
  args << "--" << pattern;
  executePass(PASS_GREP, args, QString(), true);
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
