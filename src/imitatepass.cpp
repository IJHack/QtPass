// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "imitatepass.h"
#include "executor.h"
#include "gpgidgeneration.h"
#include "util.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QThread>
#include <QTimer>
#include <utility>

#include "qtpasslogging.h"

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
using Enums::PASS_REMOVE;
using Enums::PASS_SHOW;
using Enums::PROCESS_COUNT;

/**
 * @brief ImitatePass::ImitatePass for situations when pass is not available
 * we imitate the behavior of pass https://www.passwordstore.org/
 */
ImitatePass::ImitatePass() : m_grep(this) {
  connect(&m_grep, &NativeGrep::finished, this, &ImitatePass::finishedGrep);
}

ImitatePass::~ImitatePass() {
  // Let the search workers wind down while the re-encryption worker is
  // joined; m_grep's own destructor waits for them afterwards.
  m_grep.cancel();
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
 * The helpers (loadVerifiedRecipients(), getKeysFromFile(),
 * reencryptSingleFile(), createBackupCommit()) are shared between the owning
 * thread and the re-encryption worker; the ImitatePass thread affinity tells
 * the two apart. On the worker the run is handed m_reencryptCancel: nothing is
 * started once the flag is set, and while a process runs the wait polls the
 * flag and, when it gets set, terminates and if need be kills the process. All
 * of that happens on the worker thread, which owns the QProcess. The other
 * threads (cancelReencryptPath(), the destructor) only ever set the flag; they
 * hold neither the QProcess nor its pid, so they cannot act on a process that
 * has exited in the meantime, nor on a pid the OS has since reused.
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
  return Executor::translatePathForWsl(path, exe);
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
 * @brief ImitatePass::GitPull_b git pull wrapper which blocks until the
 *        process finishes
 */
void ImitatePass::GitPull_b() {
  if (!gitReady())
    return;
  // -C the store: executeBlocking sets no working directory, so without it
  // git would run in QtPass's launch directory and pull an unrelated
  // repository, or fail with "not a git repository".
  QString err;
  const int rc = Executor::executeBlocking(
      m_settings.gitExecutable, {"-C", pgit(m_settings.passStore), "pull"},
      QString(), nullptr, &err);
  if (rc != 0) {
    emit statusMsg(tr("Git pull failed: %1").arg(err.trimmed()), 5000);
  }
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
  if (refuseLinkedPath(file + ".gpg")) {
    return;
  }
  queueShow(file);
  file = m_settings.passStore + file + ".gpg";
  QStringList args = {"-d",      "--quiet",     "--yes", "--no-encrypt-to",
                      "--batch", "--use-agent", "--",    pgpg(file)};
  executeGpg(PASS_SHOW, args);
}

/**
 * @brief ImitatePass::Insert create new file with encrypted content
 *
 * @param file      file to be created
 * @param newValue  value to be stored in file
 * @param overwrite whether to overwrite existing file
 */
void ImitatePass::Insert(QString file, QString newValue, bool overwrite) {
  // The dialog names a new entry relative to the store; gpg used to resolve
  // that in its working directory. Everything below works on the one path.
  file =
      QDir::cleanPath(QDir::isAbsolutePath(file)
                          ? file + ".gpg"
                          : QDir(m_settings.passStore).filePath(file) + ".gpg");
  if (refuseLinkedPath(file)) {
    return;
  }
  // gpg used to refuse an existing output without --yes; it writes
  // elsewhere now, so the check is ours here, and again at the rename.
  const QFileInfo target(file);
  if (!overwrite && (target.exists() || target.isSymLink())) {
    emit critical(tr("Cannot add"), tr("%1 already exists.").arg(file));
    return;
  }
  QString gpgIdPath = Pass::getGpgIdPath(file, m_settings.passStore);
  QStringList recipients;
  QString why;
  if (!loadVerifiedRecipients(gpgIdPath, &recipients, &why)) {
    emit critical(tr("Check .gpg-id file signature!"), why);
    return;
  }
  if (recipients.isEmpty()) {
    // Already emit critical signal to notify user of error - no need to throw
    emit critical(tr("Can not edit"),
                  tr("Could not read encryption key to use, .gpg-id "
                     "file missing or invalid."));
    return;
  }
  // gpg never opens a path in the store for output: refuseLinkedPath()
  // judged the name a moment ago, and whoever can write to the store could
  // make it, or any name in the store, a link to somewhere else before gpg
  // opens it. gpg writes into a directory of QtPass's own (0700, in the
  // temporary location), which no co-writer of the store can reach;
  // placeEncryptedFile() then brings the bytes into the store through an
  // open handle and the operating system's rename.
  auto scratch = std::make_shared<QTemporaryDir>();
  if (!scratch->isValid()) {
    emit critical(tr("Cannot write"),
                  tr("Cannot create a temporary directory: %1")
                      .arg(scratch->errorString()));
    return;
  }
  const QString output = scratch->filePath(QStringLiteral("entry.gpg"));
  TransactionHelper trans(&m_transaction, PASS_INSERT);
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
                      pgpg(output)};
  for (auto &r : recipients) {
    args.append("-r");
    args.append(r);
  }
  args.append("-");
  m_pendingInserts.enqueue({std::move(scratch), output, file, overwrite});
  executeGpg(PASS_INSERT, args, newValue);
  if (gitReady()) {
    // Git is used when enabled - this is the standard pass workflow
    if (!overwrite) {
      executeGit(GIT_ADD, {"add", "--", pgit(file)});
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
  // No trailing separator: "link/" makes git look for what is behind the
  // link ("pathspec did not match") and rm follow it.
  file = QDir::cleanPath(m_settings.passStore + file);
  if (!isDir) {
    file += ".gpg";
  }
  // A link itself may go; anything behind one is not the store's to delete.
  if (refuseLinkedPath(file, false)) {
    return;
  }
  TransactionHelper trans(&m_transaction, PASS_REMOVE);
  QString path = QDir(m_settings.passStore).relativeFilePath(file);
  path.replace(Util::endsWithGpg(), "");
  if (Util::isLinkedFolder(file)) {
    // Unlink it here, never through a recursive rm: the link goes, what it
    // points to stays. Git then only has to forget it, if it knew it.
    if (!Util::removeTree(file)) {
      emit critical(tr("Delete failed"),
                    tr("Could not remove the link %1.").arg(file));
      return;
    }
    // Only when git knew it: a commit whose pathspec matches nothing (a link
    // synced or planted, never committed) exits 1, and that would be
    // reported as the removal failing. ls-files reads the index, so it still
    // answers now that the link is gone.
    if (gitReady() && gitTracks(file)) {
      executeGit(GIT_RM, {"rm", "-q", "--cached", "--", pgit(file)});
      gitCommit(file, "Remove for " + path + " using QtPass.");
    }
    return;
  }
  if (gitReady()) {
    executeGit(GIT_RM, {"rm", (isDir ? "-rf" : "-f"), "--", pgit(file)});
    gitCommit(file, "Remove for " + path + " using QtPass.");
  } else {
    if (isDir) {
      // Never QDir::removeRecursively(): it follows a junction inside the
      // folder and empties whatever that points to.
      Util::removeTree(file);
    } else {
      QFile(file).remove();
    }
  }
}

/**
 * @brief The `.gpg-id` signer for the configured gpg and signing keys.
 */
auto ImitatePass::gpgIdSigner() -> GpgIdSigner {
  return {m_settings.gpgExecutable,
          GpgIdSigner::keysFromSetting(m_settings.passSigningKey),
          [this](const QString &app, const QStringList &args,
                 const QString &input, QString *out, QString *err) {
            return execBlocking(app, args, input, out, err);
          }};
}

/**
 * @brief Writes the selected users' GPG key IDs to a .gpg-id file.
 * @details Composes one key ID per enabled user (with the generation and
 * folder header for a signed store), writes the list through a staged
 * sibling and a rename (Util::writeFileReplacing), hands the bytes back for
 * the signature, and warns if none of the selected users has a secret key
 * available.
 * @param gpgIdFile Path to the .gpg-id file to be written.
 * @param users List of users to evaluate and write to the file.
 * @param written Receives the exact bytes written, if not null.
 * @return true when the file was written; false after reporting through
 * critical().
 */
auto ImitatePass::writeGpgIdFile(const QString &gpgIdFile,
                                 const QList<UserInfo> &users,
                                 QByteArray *written) -> bool {
  QByteArray contents;
  bool secret_selected = false;
  for (const UserInfo &user : users) {
    if (user.enabled) {
      contents += (user.key_id + "\n").toUtf8();
      secret_selected |= user.have_secret;
    }
  }
  // With a signing key: reserve one generation above whatever this device
  // has accepted and whatever the list on disk says, if that list verifies
  // (an unverified number is not taken), recorded before the file is
  // written, and bind the list to its folder. The signature will cover both
  // lines; a later, older or relocated signed list cannot come back, and a
  // list that could not be recorded is not written at all (it would let the
  // previous one back in). Without a signing key nothing checks freshness,
  // and the plain list stays readable for every client (GpgIdGeneration).
  const GpgIdSigner signer = gpgIdSigner();
  if (signer.enabled()) {
    const std::optional<QString> folder =
        GpgIdGeneration::folderOf(gpgIdFile, m_settings.passStore);
    if (!folder) {
      emit critical(tr("Cannot update"),
                    tr("%1 is not inside the password store.").arg(gpgIdFile));
      return false;
    }
    // The list on disk counts only if it is this folder's own, verified
    // list: a signature vouches for the bytes, the folder line for the place.
    std::optional<qint64> verifiedOnDisk;
    QByteArray current;
    if (signer.verifyFile(gpgIdFile, &current)) {
      const auto header = GpgIdGeneration::parse(current);
      if (header && (!header->folder || *header->folder == *folder)) {
        verifiedOnDisk = header->generation;
      }
    }
    QString why;
    const std::optional<qint64> generation =
        GpgIdGeneration::reserveNext(gpgIdFile, verifiedOnDisk, &why);
    if (!generation) {
      emit critical(tr("Cannot update"), why);
      return false;
    }
    contents = GpgIdGeneration::withHeader(*generation, *folder, contents);
  }
  // Whole or not at all, owner-only (the list names the keys the store is
  // encrypted to), and never by opening the name: a link planted under it
  // since Init's check is replaced as an entry, not written through
  // (Util::writeFileReplacing).
  QString writeError;
  if (!Util::writeFileReplacing(gpgIdFile, contents, true, &writeError)) {
    emit critical(tr("Cannot update"), writeError);
    return false;
  }
  if (written != nullptr) {
    *written = contents;
  }
  if (signer.enabled()) {
    // The bytes on disk are the ones this device recognises from now on;
    // another list of the same generation is a conflict. The list is
    // written and goes on to be signed either way (an unsigned list would
    // be refused everywhere); when the record could not take the bytes,
    // because another writer reserved the next number in between or the
    // record was busy, the user hears so: the list on disk will read as
    // older than the record, and saving once more is the way through.
    QString why;
    if (!GpgIdGeneration::recordWritten(gpgIdFile, contents, &why)) {
      qCWarning(lcQtPass) << "Could not record the written .gpg-id:" << why;
      emit critical(
          tr("Recipient list written, but not recorded"),
          tr("%1 Save the recipients once more to get through.").arg(why));
    }
  }
  if (!secret_selected) {
    emit critical(
        tr("Check selected users!"),
        tr("None of the selected keys have a secret key available.\n"
           "You will not be able to decrypt any newly added passwords!"));
  }
  return true;
}

/**
 * @brief Signs a `.gpg-id` with the configured key and verifies the result.
 * @param gpgIdFile Path to the .gpg-id file to be signed.
 * @param contents The bytes just written as that file; the signature is made
 * over these.
 * @return true if the file was signed and its signature verified; otherwise
 * false, after reporting the failure through critical().
 */
auto ImitatePass::signGpgIdFile(const QString &gpgIdFile,
                                const QByteArray &contents) -> bool {
  const GpgIdSigner signer = gpgIdSigner();
  QString why;
  if (!signer.sign(gpgIdFile, contents, &why)) {
    emit critical(
        tr("GPG signing failed!"),
        why.trimmed().isEmpty()
            ? tr("Failed to sign %1.").arg(gpgIdFile)
            : tr("Failed to sign %1: %2").arg(gpgIdFile, why.trimmed()));
    return false;
  }
  QByteArray signedBytes;
  if (!signer.verifyFile(gpgIdFile, &signedBytes)) {
    emit critical(tr("Check .gpg-id file signature!"),
                  tr("Signature for %1 is invalid.").arg(gpgIdFile));
    return false;
  }
  return true;
}

/**
 * @brief Commit a `.gpg-id` together with its signature.
 *
 * Git runs synchronously here on purpose: Init follows up with reencryptPath,
 * whose backup and re-encryption commits are blocking as well. Queuing the
 * add/commit on the asynchronous executor instead let the two race for the
 * index lock, so either the queued `git add` died on `index.lock` (and the
 * cancelled commit left the .gpg-id untracked) or the backup commit absorbed
 * the file first and `git commit -- .gpg-id` failed with nothing to commit.
 *
 * Both files go into one commit: two commits left a moment (and, when the
 * second failed, a history) in which the repository held a new recipient
 * list with the old signature, which every clone then refused.
 */
auto ImitatePass::gitAddGpgId(const QString &gpgIdFile,
                              const QString &gpgIdSigFile, QString *out,
                              QString *err) -> int {
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
  QStringList paths{pgit(gpgIdFile)};
  if (!gpgIdSigFile.isEmpty()) {
    paths << pgit(gpgIdSigFile);
  }
  // add stages new and modified files alike; already-clean ones are a no-op.
  int rc = run(QStringList{"add", "--"} + paths);
  if (rc != 0) {
    return rc;
  }
  // Re-initialising with the same recipients changes nothing; that is not a
  // failure, and `git commit` would make it one. diff --quiet: 0 nothing
  // staged, 1 something staged, anything else is git failing.
  rc = run(QStringList{"diff", "--cached", "--quiet", "--"} + paths);
  if (rc == 0) {
    return 0;
  }
  if (rc != 1) {
    return rc;
  }
  QString commitPath = gpgIdFile;
  commitPath.replace(Util::endsWithGpg(), "");
  return run(QStringList{"commit", "-m",
                         "Added " + commitPath + " using QtPass.", "--"} +
             paths);
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
  // The .gpg-id is written as path + ".gpg-id": without the trailing
  // separator (the context menu hands over a cleaned path) that would be a
  // file beside the folder, not the folder's own list.
  path = Util::normalizeFolderPath(path);
  // A linked folder, or a link planted under the .gpg-id or .gpg-id.sig
  // name, is not this store's: refused up front, and the writes below go
  // through staged files and renames so that one planted afterwards is
  // replaced as an entry, not written through.
  const QString folder = QDir::cleanPath(path);
  if (refuseLinkedPath(path) ||
      refuseLinkedPath(folder + QStringLiteral("/.gpg-id")) ||
      refuseLinkedPath(folder + QStringLiteral("/.gpg-id.sig"))) {
    return;
  }
  const GpgIdSigner signer = gpgIdSigner();
  const QString gpgIdSigFile = path + ".gpg-id.sig";
  if (signer.enabled() && !signer.haveSecretKey()) {
    emit critical(tr("No signing key!"),
                  tr("None of the secret signing keys is available.\n"
                     "You will not be able to change the user list!"));
    return;
  }

  const bool useGit = gitReady();
  const QString gpgIdFile = path + ".gpg-id";
  QByteArray written;
  if (!writeGpgIdFile(gpgIdFile, users, &written)) {
    return;
  }

  QString sigToCommit;
  if (signer.enabled()) {
    if (!signGpgIdFile(gpgIdFile, written)) {
      return;
    }
    sigToCommit = gpgIdSigFile;
  } else if (QFile::exists(gpgIdSigFile)) {
    // Signing was switched off: a signature of the previous list must not
    // stay behind, where pass and other clients would reject the new list
    // under it. Its removal goes into the same commit.
    const bool tracked = useGit && gitTracks(gpgIdSigFile);
    if (!QFile::remove(gpgIdSigFile)) {
      emit critical(
          tr("Cannot update"),
          tr("Failed to remove the old signature %1.").arg(gpgIdSigFile));
      return;
    }
    if (tracked) {
      sigToCommit = gpgIdSigFile;
    }
  }

  if (useGit && m_settings.addGPGId) {
    // The .gpg-id (and its signature) must be in the repository before any
    // entry is re-encrypted to it: the backup commit before re-encryption
    // only picks up tracked files (#1685), and MainWindow::addFolder writes
    // a folder's .gpg-id without staging it, so without this the
    // re-encrypted entries were pushed without the recipients file they
    // were encrypted to (#1682). When the commit fails the re-encryption
    // does not start, or the working tree would follow a recipient list the
    // repository does not have.
    QString gitOut;
    QString gitErr;
    const int gitExit = gitAddGpgId(gpgIdFile, sigToCommit, &gitOut, &gitErr);
    if (gitExit != 0) {
      Pass::finished(PASS_INIT, gitExit, gitOut, gitErr);
      return;
    }
    reencryptPath(path);
    // Same contract the asynchronous add/commit transaction used to provide:
    // finishedInit once the .gpg-id landed in git.
    Pass::finished(PASS_INIT, 0, gitOut, gitErr);
    return;
  }
  reencryptPath(path);
  if (useGit) {
    Pass::finished(PASS_INIT, 0, QString(), QString());
  }
}

auto ImitatePass::loadVerifiedRecipients(const QString &gpgIdFile,
                                         QStringList *recipients, QString *why)
    -> bool {
  recipients->clear();
  if (why != nullptr) {
    *why = tr("Signature for %1 is invalid.").arg(gpgIdFile);
  }
  // A link under the .gpg-id name is not the store's list: with signing
  // off it reads as missing, with signing on verifyFile() refuses it.
  if (Util::isLinkedFolder(gpgIdFile)) {
    return !gpgIdSigner().enabled();
  }
  QByteArray contents;
  const GpgIdSigner signer = gpgIdSigner();
  if (signer.enabled()) {
    if (!signer.verifyFile(gpgIdFile, &contents)) {
      return false;
    }
    // Authentic and unmodified is not the same as current: an older signed
    // list put back into the store is refused once this device has seen a
    // newer one.
    QString reason;
    if (GpgIdGeneration::accept(gpgIdFile, contents, m_settings.passStore,
                                &reason) !=
        GpgIdGeneration::Verdict::Accepted) {
      if (why != nullptr) {
        *why = reason;
      }
      return false;
    }
  } else {
    QFile file(gpgIdFile);
    if (!file.open(QIODevice::ReadOnly)) {
      // No file is not a bad signature; the empty list says it all.
      return true;
    }
    contents = file.readAll();
  }
  *recipients = Pass::parseRecipients(contents, gpgIdFile);
  return true;
}

auto ImitatePass::recoverReencryptLeftovers(const QString &dir) -> bool {
  // What a crashed run can have left in the store, and what each means
  // (a temporary modified within the hour is left alone: see below).
  // Today's writers stage every file as .qtpass-XXXXXX.tmp next to its
  // destination and rename it into place in one step; QtPass 1.8.x wrote
  // X.gpg.reencrypt.tmp and replaced the entry through X.gpg.reencrypt.bak
  // in two renames, and builds between 1.8.x and 2.0 wrote X.gpg.XXXXXX.tmp.
  //   .qtpass-XXXXXX.tmp, X.gpg.reencrypt.tmp, X.gpg.XXXXXX.tmp
  //                      a file that was being written or never verified;
  //                      it is never a source of truth: delete.
  //   X.gpg.reencrypt.bak with no X.gpg
  //                      a 1.8.x crash between its two renames; the backup
  //                      is the only copy of the entry: put it back.
  //   X.gpg.reencrypt.bak next to an X.gpg
  //                      that run finished but the backup could not be
  //                      removed, or X.gpg was recreated since; both are
  //                      valid ciphertexts and it is not for QtPass to pick
  //                      one: report and leave both.
  bool clean = true;
  // Only regular files are walked and listed. A symlink, junction or special
  // file under a leftover's name is handed back separately: QtPass never
  // made one, and renaming a link into an entry's place would make the run
  // decrypt and re-encrypt whatever it points to, inside the store or not.
  // Linked directories are handed back too; they are not leftovers and are
  // left to reencryptFiles() to mention.
  const QStringList leftoverNames{QStringLiteral(".qtpass-??????.tmp"),
                                  QStringLiteral("*.gpg.reencrypt.tmp"),
                                  QStringLiteral("*.gpg.??????.tmp"),
                                  QStringLiteral("*.gpg.reencrypt.bak")};
  QStringList skipped;
  // Hidden files included: the staged name starts with a dot.
  const QStringList leftovers = Util::regularFilesUnder(
      QDir::cleanPath(dir), leftoverNames, &skipped, true);
  for (const QString &path : skipped) {
    if (!QDir::match(leftoverNames, QFileInfo(path).fileName())) {
      continue;
    }
    if (path.endsWith(QStringLiteral(".tmp"))) {
      // Removing a link removes the link, never what it points to; a
      // junction or a directory symlink on Windows is a directory entry and
      // goes with rmdir.
      if (!QFile::remove(path) && !QDir().rmdir(path)) {
        qCWarning(lcQtPass) << "Could not remove stale temporary" << path;
      }
      continue;
    }
    emit critical(tr("Leftover from an earlier re-encryption"),
                  tr("%1 is not a regular file and was not restored. Look "
                     "at it and remove it, then re-encrypt again.")
                      .arg(path));
    clean = false;
  }
  // A temporary younger than this is taken for a write in progress: another
  // QtPass on a shared store stages its files under the same names, and
  // removing one from under it would fail that write for nothing. A crash
  // leaves its temporary behind for good; the next run after an hour picks
  // it up.
  const QDateTime inFlightSince = QDateTime::currentDateTime().addSecs(-3600);
  for (const QString &path : leftovers) {
    if (path.endsWith(QStringLiteral(".tmp"))) {
      if (QFileInfo(path).lastModified() > inFlightSince) {
        qCDebug(lcQtPass) << "Leaving a recent temporary alone:" << path;
        continue;
      }
      if (!QFile::remove(path)) {
        qCWarning(lcQtPass) << "Could not remove stale temporary" << path;
      }
      continue;
    }
    // Belt and braces: the walker only lists regular files.
    const QFileInfo backup(path);
    if (backup.isSymLink() || !backup.isFile()) {
      emit critical(tr("Leftover from an earlier re-encryption"),
                    tr("%1 is not a regular file and was not restored. Look "
                       "at it and remove it, then re-encrypt again.")
                        .arg(path));
      clean = false;
      continue;
    }
    const QString original =
        path.chopped(QStringLiteral(".reencrypt.bak").size());
    if (QFileInfo::exists(original)) {
      emit critical(tr("Leftover from an earlier re-encryption"),
                    tr("%1 exists next to %2. Both are encrypted copies of the "
                       "entry; check which one you want and delete the other, "
                       "then re-encrypt again.")
                        .arg(path, original));
      clean = false;
      continue;
    }
    if (QFile::rename(path, original)) {
      qCWarning(lcQtPass) << "Restored" << original << "from" << path;
      emit statusMsg(tr("Restored %1 from the backup an interrupted "
                        "re-encryption left behind.")
                         .arg(original),
                     5000);
    } else {
      emit critical(tr("Leftover from an earlier re-encryption"),
                    tr("%1 is missing and its backup %2 could not be renamed "
                       "back. Rename it by hand, then re-encrypt again.")
                        .arg(original, path));
      clean = false;
    }
  }
  return clean;
}

/**
 * @brief ImitatePass::reencryptPath reencrypt all files under the chosen
 * directory
 *
 * This is still quite experimental..
 * @param dir
 */
auto ImitatePass::verifyGpgIdForDir(const QString &file,
                                    QHash<QString, QStringList> &verified,
                                    QStringList &gpgId) -> bool {
  const QString gpgIdPath = Pass::getGpgIdPath(file, m_settings.passStore);
  // The cache maps each .gpg-id to the recipients its verified bytes held,
  // so every file under it is encrypted to exactly the list the signature
  // covered; a second directory sharing the .gpg-id gets the same list, a
  // different .gpg-id is read and verified on its own.
  const auto cached = verified.constFind(gpgIdPath);
  if (cached != verified.constEnd()) {
    gpgId = cached.value();
    return true;
  }
  QStringList recipients;
  QString why;
  if (!loadVerifiedRecipients(gpgIdPath, &recipients, &why)) {
    // An interrupted gpg is a cancel, not a bad signature.
    if (!m_reencryptCancel.load())
      emit critical(tr("Check .gpg-id file signature!"), why);
    return false;
  }
  recipients.sort();
  verified.insert(gpgIdPath, recipients);
  gpgId = recipients;
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
      "--list-only", "--keyid-format=long", "--",
      pgpg(fileName)};
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
 * QString why;
 * bool result = ImitatePass::reencryptSingleFile(fileName, recipients, &why);
 * std::cout << result << std::endl; // Expected output: true on success, false
 * on failure
 *
 * @param const QString &fileName - Path to the encrypted file to re-encrypt.
 * @param const QStringList &recipients - List of recipient keys to encrypt the
 * file to.
 * @param QString *why - Receives the reason the new ciphertext could not be
 * put under the entry's name, for the run's summary; left empty for a gpg
 * failure, which is logged.
 * @return bool - True if the file was successfully decrypted, re-encrypted,
 * verified, and replaced; otherwise false.
 */
auto ImitatePass::reencryptSingleFile(const QString &fileName,
                                      const QStringList &recipients,
                                      QString *why) -> bool {
  qCDebug(lcQtPass) << "reencrypt" << fileName << "for" << recipients.size()
                    << "recipients";
  QString local_lastDecrypt;
  QStringList args = {"-d",      "--quiet",     "--yes", "--no-encrypt-to",
                      "--batch", "--use-agent", "--",    pgpg(fileName)};
  int result = execBlocking(m_settings.gpgExecutable, args, &local_lastDecrypt);

  if (result != 0 || local_lastDecrypt.isEmpty()) {
    qCDebug(lcQtPass) << "Decrypt error on re-encrypt for:" << fileName;
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

  // gpg writes the new ciphertext outside the store, into a directory of
  // QtPass's own (0700, an unguessable name): nothing a co-writer of the
  // store can pre-create or swap for a link before gpg opens it by name.
  // placeEncryptedFile() then brings the bytes into the store the way
  // Insert() does, replacing the entry in one rename.
  QTemporaryDir scratch;
  if (!scratch.isValid()) {
    qCDebug(lcQtPass) << "Cannot create a scratch directory for re-encrypting"
                      << fileName;
    return false;
  }
  const QString tempPath = scratch.filePath(QStringLiteral("reencrypted.tmp"));
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
    qCDebug(lcQtPass) << "Encrypt error on re-encrypt for:" << fileName;
    return false;
  }

  // Verify encryption worked by attempting to decrypt the temp file
  QString verifyOutput;
  args = QStringList{"-d",          "--quiet", "--batch",
                     "--use-agent", "--",      pgpg(tempPath)};
  result = execBlocking(m_settings.gpgExecutable, args, &verifyOutput);
  if (result != 0 || verifyOutput.isEmpty()) {
    qCDebug(lcQtPass) << "Verification failed for:" << tempPath;
    return false;
  }
  // Verify content matches original decrypted content (defense in depth)
  if (verifyOutput.trimmed() != local_lastDecrypt.trimmed()) {
    qCDebug(lcQtPass) << "Verification content mismatch for:" << tempPath;
    return false;
  }

  // Another client may have removed the entry while gpg ran; the rename
  // would bring it back under its old name. Best effort: the check and the
  // replace are two steps, as pass's own write is.
  const QFileInfo still(fileName);
  if (!still.exists() && !still.isSymLink()) {
    qCDebug(lcQtPass) << "Entry vanished before it could be replaced:"
                      << fileName;
    return false;
  }
  if (!placeEncryptedFile(tempPath, fileName, true, why)) {
    // Reported once, in the run's summary, with the others: a folder that
    // cannot be written fails every entry in it the same way. The entry
    // keeps its old ciphertext, unless what failed was the check after the
    // rename, where the swapped-in object is under the name and said so.
    if (why != nullptr && !why->contains(fileName)) {
      // Every reason but the copy's names the entry; that one names the
      // file gpg wrote, in a scratch directory the user never sees.
      *why = tr("%1 could not be re-encrypted: %2").arg(fileName, *why);
    }
    return false;
  }

  if (gitConfigured()) {
    // -C the store so git runs there rather than in QtPass's launch directory
    // (executeBlocking sets no working directory).
    const QString store = pgit(m_settings.passStore);
    if (execBlocking(m_settings.gitExecutable,
                     {"-C", store, "add", "--", pgit(fileName)}) != 0) {
      qCDebug(lcQtPass) << "git add failed after re-encrypting:" << fileName;
      // The file on disk is re-encrypted correctly; only the repository is
      // now behind. Report it so the caller counts this file as failed and
      // the run is not pushed.
      return false;
    }
    QString path = QDir(m_settings.passStore).relativeFilePath(fileName);
    path.replace(Util::endsWithGpg(), "");
    if (execBlocking(m_settings.gitExecutable,
                     {"-C", store, "commit", "-m",
                      "Re-encrypt for " + path + " using QtPass.", "--",
                      pgit(fileName)}) != 0) {
      qCDebug(lcQtPass) << "git commit failed after re-encrypting:" << fileName;
      return false;
    }
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
  QStringList failed;     ///< One line per file that could not be
                          ///< re-encrypted: its path, or the reason (which
                          ///< names the path) when there is one to give.
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
  // The walk follows its starting point (the store root is allowed to be a
  // link); a folder inside the store that is, or lies behind, a link is not
  // part of it. MainWindow refuses such a pick already; this holds for every
  // caller, Init() included.
  if (Util::isUnderLink(dir, m_settings.passStore)) {
    emit critical(tr("Not a folder of the store"),
                  tr("%1 is, or lies behind, a symbolic link or junction. What "
                     "that points to is not part of the password store and "
                     "was not re-encrypted.")
                      .arg(QDir::toNativeSeparators(QDir::cleanPath(dir))));
    emit endReencryptPath();
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
    if (execBlocking(m_settings.gitExecutable,
                     {"-C", pgit(m_settings.passStore), "pull"}) != 0) {
      // A pull that could not reach the remote leaves the store as it was;
      // one that stopped in a merge leaves conflict markers and an unmerged
      // index, and re-encrypting on top of that would commit the mess.
      QString unmerged;
      execBlocking(m_settings.gitExecutable,
                   {"-C", pgit(m_settings.passStore), "ls-files", "--unmerged"},
                   &unmerged);
      if (!unmerged.trimmed().isEmpty()) {
        emit critical(tr("Git pull failed"),
                      tr("The pull left the store with unmerged files. Resolve "
                         "the conflict before re-encrypting."));
        result.aborted = true;
        return result;
      }
      emit statusMsg(tr("Git pull failed, re-encrypting the store as it is"),
                     5000);
    }
  }

  // Leftovers of an interrupted run first: a restored entry then goes into
  // the backup commit like everything else, and a stale temporary does not.
  if (!recoverReencryptLeftovers(dir)) {
    result.aborted = true;
    return result;
  }

  // Create backup before re-encryption - abort if it fails
  if (!createBackupCommit()) {
    if (m_reencryptCancel.load())
      result.cancelled = true;
    else
      result.aborted = true;
    return result;
  }

  // Regular files only: a symlink or junction is not a password entry, and
  // following one would decrypt and rewrite something outside the store.
  QStringList skipped;
  const QStringList files =
      Util::regularFilesUnder(dir, QStringList() << "*.gpg", &skipped);
  if (!skipped.isEmpty()) {
    emit statusMsg(tr("%n entr(y/ies) skipped: a symlink, junction or special "
                      "file is not part of the store.",
                      "", static_cast<int>(skipped.size())),
                   5000);
  }
  result.total = files.size();
  emit reencryptProgress(0, result.total);

  QString currentDir;
  QHash<QString, QStringList> gpgIdFilesVerified;
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
      QString why;
      if (reencryptSingleFile(fileName, gpgId, &why)) {
        result.reencrypted++;
      } else if (m_reencryptCancel.load()) {
        // Interrupted by the cancel: the file is untouched, not failed.
        result.cancelled = true;
        break;
      } else {
        // The reason names the entry itself when there is one to give.
        result.failed << (why.isEmpty() ? fileName : why);
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
        qCDebug(lcQtPass) << "Destination file already exists";
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
      qCDebug(lcQtPass) << "Destination is a file";
      return {};
    } else {
      destFile = dest;
    }
  } else {
    qCDebug(lcQtPass) << "Source file does not exist";
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
  args << "--" << pgit(src) << pgit(destFile);
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
  if (refuseLinkedPath(src) || refuseLinkedPath(dest)) {
    return;
  }
  TransactionHelper trans(&m_transaction, PASS_MOVE);
  QString destFile = resolveMoveDestination(src, dest, force);
  if (destFile.isEmpty()) {
    return;
  }
  if (refuseLinkedPath(destFile)) {
    return;
  }

  qCDebug(lcQtPass) << "Move Source: " << src;
  qCDebug(lcQtPass) << "Move Destination: " << destFile;

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
  // QFile::copy reads through a link: the target's bytes would become an
  // entry of the store.
  if (refuseLinkedPath(src) || refuseLinkedPath(dest)) {
    return;
  }
  TransactionHelper trans(&m_transaction, PASS_COPY);
  // Like `pass cp`, dest may be an existing folder (a drag-and-drop copy hands
  // over the folder, not the new file name). Resolve the real target the same
  // way Move does: into the folder, .gpg appended, no clobbering without force.
  QString destFile = resolveMoveDestination(src, dest, force);
  if (destFile.isEmpty()) {
    emit critical(tr("Copy failed"),
                  tr("Could not copy %1 to %2.").arg(src, dest));
    return;
  }
  // dest may have been a folder; the file that ends up written is destFile,
  // and a link planted under that name (dangling ones pass exists()) is not
  // an entry to write.
  if (refuseLinkedPath(destFile)) {
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
  // when using git, stage the new path afterwards. The copy is synchronous
  // and replaces the destination atomically (Util::copyFileReplacing: the
  // source read as the regular file it is, the bytes staged next to the
  // destination, the rename following nothing), so it exists before the
  // re-encryption below runs and an entry being overwritten with force
  // survives a copy that fails half-way. Without force nothing under the
  // name is replaced, also nothing that appeared since the check above.
  QString why;
  if (!Util::copyFileReplacing(src, destFile, force, &why)) {
    emit critical(tr("Copy failed"),
                  tr("Could not copy %1 to %2.").arg(src, destFile) + "\n" +
                      why);
    return;
  }
  // QFileInfo caches; the comparison above may have looked at a path that did
  // not exist yet, so re-read it before deciding what to re-encrypt.
  destFileInfo.refresh();
  if (gitReady()) {
    executeGit(GIT_COPY, {"add", "--", pgit(destFile)});
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
  qCDebug(lcQtPass) << "Imitate Pass";
  QString error = err;
  if (id == PASS_INSERT && !m_pendingInserts.isEmpty()) {
    // The gpg step of an Insert(): its ciphertext goes into the store now,
    // before the git steps queued behind it run (the executor starts the
    // next item only after this returns). When it cannot, the insert failed
    // like a gpg error would have: the git steps are cancelled below and
    // the reason reaches the interface through the failed-operation path.
    // Nothing is shown from here: a dialog would spin the event loop and
    // let the queued git steps run first. The scratch directory goes with
    // the pending entry.
    const PendingInsert pending = m_pendingInserts.dequeue();
    if (exitCode == 0 && !placeEncryptedFile(pending.output, pending.file,
                                             pending.overwrite, &error)) {
      exitCode = 1;
    }
  }
  PROCESS pid = m_transaction.transactionIsOver(static_cast<PROCESS>(id));
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
        qCDebug(lcQtPass) << "No such transaction!";
        return;
      }
      pid = m_transaction.transactionIsOver(static_cast<PROCESS>(id));
    }
  }
  Pass::finished(pid, exitCode, m_transactionOutput, error);
  m_transactionOutput.clear();
}

auto ImitatePass::placeEncryptedFile(const QString &output, const QString &file,
                                     bool overwrite, QString *error) -> bool {
  if (QFileInfo(output).size() <= 0) {
    // gpg reported success and wrote nothing: not an entry.
    *error = tr("gpg wrote no ciphertext for %1.").arg(file);
    return false;
  }
  // Staged next to the entry, written by the temporary's own handle, then
  // the operating system's rename: the entry under the name is replaced as
  // an entry, a link planted since the check included; without overwrite,
  // anything that appeared under the name since the check fails the add.
  return Util::copyFileReplacing(output, file, overwrite, error);
}

/**
 * @brief Register a transaction before each wrapped execution.
 *
 * Native mode treats every git/gpg invocation as a transaction; the base
 * Pass::executeWrapper calls this hook just before dispatching.
 * @param id Process identifier of the command about to run.
 */
void ImitatePass::beforeExecute(PROCESS id) {
  m_transaction.transactionAdd(id);
}

/**
 * @brief Search all password content by GPG-decrypting each .gpg file.
 *
 * The pattern is evaluated with `QRegularExpression` (**PCRE**), which differs
 * from the POSIX BRE dialect of the `pass` backend — see Pass::Grep for the
 * cross-backend caveat. The work happens on a NativeGrep thread; its result
 * arrives on this object's thread as finishedGrep.
 */
void ImitatePass::Grep(QString pattern, bool caseInsensitive) {
  m_grep.search(pattern, caseInsensitive, m_settings.gpgExecutable,
                m_settings.passStore, exec.environment());
}
