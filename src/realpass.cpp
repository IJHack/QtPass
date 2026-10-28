// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "realpass.h"
#include "executor.h"
#include "pathvalidator.h"
#include "qtpasslogging.h"
#include "util.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <utility>

using Enums::GIT_COMMIT;
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

void RealPass::GitInit() { executePass(GIT_INIT, {"git", "init"}); }

void RealPass::GitPull_b() {
  int result =
      Executor::executeBlocking(m_settings.passExecutable, {"git", "pull"});
  if (result != 0) {
    qCDebug(lcQtPass) << "Git pull failed with code:" << result;
  }
}

void RealPass::GitPull() { executePass(GIT_PULL, {"git", "pull"}); }

void RealPass::GitPush() { executePass(GIT_PUSH, {"git", "push"}); }

void RealPass::Show(QString file) {
  // pass follows links as readily as gpg does.
  if (refuseLinkedPath(file + ".gpg")) {
    return;
  }
  queueShow(file);
  executePass(PASS_SHOW, {"show", file}, "", true);
}

void RealPass::Insert(QString file, QString newValue, bool overwrite) {
  if (refuseLinkedPath(file + ".gpg")) {
    return;
  }
  m_insertTarget =
      QDir::cleanPath(QDir::isAbsolutePath(file)
                          ? file + ".gpg"
                          : QDir(m_settings.passStore).filePath(file) + ".gpg");
  m_insertTargetBefore = entryFingerprint(m_insertTarget);
  QStringList args = {"insert", "-m"};
  if (overwrite) {
    args.append("-f");
  }
  args.append(file);
  executePass(PASS_INSERT, args, newValue);
}

auto RealPass::insertWritten() const -> bool {
  if (m_insertTarget.isEmpty()) {
    return false;
  }
  // pass writes the new ciphertext to the entry's name before it commits,
  // and an encryption never gives the same bytes twice (a fresh session
  // key), so an edited entry has changed as surely as a new one appeared.
  const QByteArray after = entryFingerprint(m_insertTarget);
  return !after.isEmpty() && after != m_insertTargetBefore;
}

auto RealPass::entryFingerprint(const QString &path) -> QByteArray {
  QFile file;
  if (!Util::openRegularFile(path, file)) {
    return {};
  }
  QCryptographicHash hash(QCryptographicHash::Sha256);
  if (!hash.addData(&file)) {
    return {};
  }
  return hash.result();
}

/**
 * @brief Remove an entry with pass, unlinking linked folders locally without
 * deleting their targets. Commit a linked folder's removal only when Git is
 * enabled and the tracking lookup and index removal commands report success.
 * @param file Store-relative entry path, without .gpg for a password file.
 * @param isDir true to remove a directory; false to remove a password file.
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
      const QString why = tr("Could not remove the link %1.").arg(full);
      emit critical(tr("Delete failed"), why);
      emit processErrorExit(1, why);
      return;
    }
    if (m_settings.useGit) {
      // `pass git` exits 0 whatever git did, so ask by output whether git
      // knew the link (these quick lookups block; the commit is queued).
      const QString rel = QDir(m_settings.passStore).relativeFilePath(full);
      // With the store's environment (PASSWORD_STORE_DIR), as executePass.
      QString known;
      QString err;
      // The link is gone from disk either way. A failed lookup or rm leaves
      // git tracking it: say so rather than report a clean removal.
      bool recorded = Executor::executeBlocking(
                          exec.environment(), m_settings.passExecutable,
                          {"git", "ls-files", "--", rel}, &known, &err) == 0;
      if (recorded && !known.trimmed().isEmpty()) {
        recorded =
            Executor::executeBlocking(
                exec.environment(), m_settings.passExecutable,
                {"git", "rm", "-q", "--cached", "--", rel}, &known, &err) == 0;
        if (recorded) {
          // `pass git commit` is git's commit as it is: pass adds -S only
          // to the commits it makes itself, so ask as its git_commit() does.
          QString signs;
          const bool sign =
              Executor::executeBlocking(
                  exec.environment(), m_settings.passExecutable,
                  {"git", "config", "--bool", "--get", "pass.signcommits"},
                  &signs, nullptr) == 0 &&
              signs.trimmed() == QLatin1String("true");
          QStringList commit{"git", "commit", "-q"};
          if (sign) {
            commit << "-S";
          }
          // Queued, as every other commit, so a signer or hook that waits
          // (a pinentry, an agent) does not freeze the window; finished()
          // judges it by what is left staged, since `pass git` exits 0
          // whatever git did.
          m_linkCommits.enqueue(
              {rel, m_settings.passExecutable, exec.environment()});
          executePass(
              GIT_COMMIT,
              commit + QStringList{"-m", "Remove for " + rel + " using QtPass.",
                                   "--", rel});
          return;
        }
      }
      if (!recorded) {
        const QString why =
            tr("The link %1 was removed, but git could not record it: %2")
                .arg(rel, err.trimmed());
        emit critical(tr("Delete incomplete"), why);
        emit processErrorExit(1, why);
      }
    }
    // Nothing ran for pass to finish: the removal is done here, and the
    // store changed.
    emit finishedRemove(QString(), QString());
    return;
  }
  executePass(PASS_REMOVE, {"rm", (isDir ? "-rf" : "-f"), file});
}

void RealPass::finished(int id, int exitCode, const QString &out,
                        const QString &err) {
  if (static_cast<PROCESS>(id) == GIT_COMMIT && !m_linkCommits.isEmpty()) {
    // Whatever the exit code (pass itself failing, or 0 whatever git
    // did), what is left staged decides.
    finishLinkCommit(m_linkCommits.dequeue(), out, err);
    return;
  }
  Pass::finished(id, exitCode, out, err);
}

void RealPass::finishLinkCommit(const LinkCommit &commit, const QString &out,
                                const QString &err) {
  // In the store the commit ran in, which a profile switch meanwhile does
  // not change.
  const QString &rel = commit.rel;
  QString staged;
  const bool committed =
      Executor::executeBlocking(
          commit.environment, commit.passExecutable,
          {"git", "diff", "--cached", "--name-only", "--", rel}, &staged,
          nullptr) == 0 &&
      staged.trimmed().isEmpty();
  if (committed) {
    // As Pass::finished() would: git's output (a hook's warning, say) for
    // the output panel, then the removal.
    emit finishedAnyWithPid(out, err, GIT_COMMIT);
    emit finishedRemove(out, err);
    return;
  }
  const QString why = tr("The link %1 was removed and git no longer tracks "
                         "it, but the commit failed: %2")
                          .arg(rel, err.trimmed());
  emit critical(tr("Delete incomplete"), why);
  emit processErrorExit(1, why);
  emit finishedRemove(QString(), QString());
}

void RealPass::Init(QString path, const QList<UserInfo> &users) {
  // pass init writes .gpg-id with a shell redirection and gpg writes the
  // signature with --output; both follow a link planted under those names.
  const QString folder = QDir::cleanPath(path);
  if (refuseLinkedPath(path) ||
      refuseLinkedPath(folder + QStringLiteral("/.gpg-id")) ||
      refuseLinkedPath(folder + QStringLiteral("/.gpg-id.sig"))) {
    return;
  }
  // --path is store-relative, or pass creates passStore/passStore/dir. A
  // plain prefix test would take /home/me/store-other for /home/me/store.
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

void RealPass::Move(const QString src, const QString dest, const bool force) {
  passMoveOrCopy(PASS_MOVE, QStringLiteral("mv"), src, dest, force);
}

void RealPass::Copy(const QString src, const QString dest, const bool force) {
  passMoveOrCopy(PASS_COPY, QStringLiteral("cp"), src, dest, force);
}

auto RealPass::passName(const QString &path, bool stripGpg) const -> QString {
  QString name =
      QDir(QDir::cleanPath(m_settings.passStore))
          .relativeFilePath(QDir::cleanPath(QDir(path).absolutePath()));
  if (stripGpg) {
    name.replace(Util::endsWithGpg(), "");
  }
  return name;
}

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

  // pass always forces when called from QtPass, so refuse file-onto-file.
  if (!force && srcFileInfo.isFile() && destFileInfo.isFile()) {
    return;
  }

  // pass appends .gpg itself, so an entry named with it would come out as
  // "name.gpg.gpg"; the destination usually does not exist yet.
  const QString passSrc =
      passName(src, srcFileInfo.isFile() &&
                        srcFileInfo.suffix() == QLatin1String("gpg"));
  const QString passDest =
      passName(dest, !destFileInfo.isDir() &&
                         destFileInfo.suffix() == QLatin1String("gpg"));

  QStringList args;
  args << subcommand;
  if (force) {
    args << "-f";
  }
  args << passSrc;
  args << passDest;
  executePass(id, args);
}

// Not `pass grep` (find -L follows links out of the store): the native
// search walks real files only (Util::regularFilesUnder), so it needs gpg.
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

void RealPass::executePass(PROCESS id, const QStringList &args, QString input,
                           bool readStdout, bool readStderr) {
  executeWrapper(id, m_settings.passExecutable, args, std::move(input),
                 readStdout, readStderr);
}
