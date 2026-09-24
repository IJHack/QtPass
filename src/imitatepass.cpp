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

ImitatePass::ImitatePass() : m_grep(this) {
  connect(&m_grep, &NativeGrep::finished, this, &ImitatePass::finishedGrep);
}

ImitatePass::~ImitatePass() {
  // Grep workers wind down meanwhile; m_grep's destructor waits for them.
  m_grep.cancel();
  // The re-encryption worker calls members, so join it with no timeout; it
  // ends its own blocked process on the flag (see execBlocking()).
  if (m_reencryptThread && m_reencryptThread->isRunning()) {
    m_reencryptCancel.store(true);
    m_reencryptThread->wait();
  }
}

// The helpers are shared by the owning thread and the worker; thread affinity
// tells them apart. Only the worker touches its QProcess, so a cancel can
// never hit an exited process or a reused pid.
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

void ImitatePass::GitInit() {
  executeGit(GIT_INIT, {"init", pgit(m_settings.passStore)});
}

void ImitatePass::GitPull() {
  if (gitReady()) {
    executeGit(GIT_PULL, {"pull"});
  }
}

// Blocks until git finishes.
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

void ImitatePass::GitPush() {
  if (gitReady()) {
    executeGit(GIT_PUSH, {"push"});
  }
}

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

namespace {

/**
 * @brief The arguments of every gpg encrypt call: @p leading, then encrypt
 * to exactly @p recipients from stdin into @p output. --no-encrypt-to keeps
 * an `encrypt-to` line in the user's gpg.conf from adding a recipient the
 * (possibly signed) .gpg-id does not list; --compress-algo=none mirrors
 * pass(1).
 */
auto encryptArgs(QStringList leading, const QString &output,
                 const QStringList &recipients) -> QStringList {
  leading << "-eq" << "--compress-algo=none" << "--no-encrypt-to" << "--output"
          << output;
  for (const QString &recipient : recipients) {
    leading << "-r" << recipient;
  }
  leading << "-";
  return leading;
}

} // namespace

auto ImitatePass::recipientsForEntry(const QString &file,
                                     QStringList *recipients) -> bool {
  QString why;
  if (!loadVerifiedRecipients(Pass::getGpgIdPath(file, m_settings.passStore),
                              recipients, &why)) {
    emit critical(tr("Check .gpg-id file signature!"), why);
    return false;
  }
  if (recipients->isEmpty()) {
    emit critical(tr("Can not edit"),
                  tr("Could not read encryption key to use, .gpg-id "
                     "file missing or invalid."));
    return false;
  }
  return true;
}

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
  QStringList recipients;
  if (!recipientsForEntry(file, &recipients)) {
    return;
  }
  // gpg never opens a store path for output: a co-writer could make it a link
  // after refuseLinkedPath(). gpg writes into a 0700 directory of our own and
  // placeEncryptedFile() brings the bytes in via an open handle and a rename.
  auto scratch = std::make_shared<QTemporaryDir>();
  if (!scratch->isValid()) {
    emit critical(tr("Cannot write"),
                  tr("Cannot create a temporary directory: %1")
                      .arg(scratch->errorString()));
    return;
  }
  const QString output = scratch->filePath(QStringLiteral("entry.gpg"));
  TransactionHelper trans(&m_transaction, PASS_INSERT);
  const QStringList args =
      encryptArgs({"--batch", "--status-fd", "2"}, pgpg(output), recipients);
  m_pendingInserts.enqueue({std::move(scratch), output, file, overwrite});
  executeGpg(PASS_INSERT, args, newValue);
  if (!gitReady()) {
    return;
  }
  if (!overwrite) {
    executeGit(GIT_ADD, {"add", "--", pgit(file)});
  }
  QString path = QDir(m_settings.passStore).relativeFilePath(file);
  path.replace(Util::endsWithGpg(), "");
  gitCommit(file, QString(overwrite ? "Edit" : "Add") + " for " + path +
                      " using QtPass.");
}

void ImitatePass::gitCommit(const QString &file, const QString &msg) {
  if (file.isEmpty()) {
    executeGit(GIT_COMMIT, {"commit", "-m", msg});
  } else {
    executeGit(GIT_COMMIT, {"commit", "-m", msg, "--", pgit(file)});
  }
}

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
    // Only when git knew it: a commit matching nothing (a link never
    // committed) exits 1 and reads as a failed removal. ls-files reads the
    // index, so it still answers now that the link is gone.
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

auto ImitatePass::gpgIdSigner() -> GpgIdSigner {
  return {m_settings.gpgExecutable,
          GpgIdSigner::keysFromSetting(m_settings.passSigningKey),
          [this](const QString &app, const QStringList &args,
                 const QString &input, QString *out, QString *err) {
            return execBlocking(app, args, input, out, err);
          }};
}

auto ImitatePass::addGenerationHeader(const QString &gpgIdFile,
                                      const GpgIdSigner &signer,
                                      QByteArray *contents) -> bool {
  const std::optional<QString> folder =
      GpgIdGeneration::folderOf(gpgIdFile, m_settings.passStore);
  if (!folder) {
    emit critical(tr("Cannot update"),
                  tr("%1 is not inside the password store.").arg(gpgIdFile));
    return false;
  }
  // The list on disk counts only if it is this folder's own, verified list:
  // a signature vouches for the bytes, the folder line for the place.
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
  *contents = GpgIdGeneration::withHeader(*generation, *folder, *contents);
  return true;
}

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
  // Without a signing key the plain list stays (GpgIdGeneration).
  const GpgIdSigner signer = gpgIdSigner();
  if (signer.enabled() && !addGenerationHeader(gpgIdFile, signer, &contents)) {
    return false;
  }
  // Whole or not at all, owner-only (it names the store's keys); a link
  // planted since Init's check is replaced, not written through.
  QString writeError;
  if (!Util::writeFileReplacing(gpgIdFile, contents, true, &writeError)) {
    emit critical(tr("Cannot update"), writeError);
    return false;
  }
  if (written != nullptr) {
    *written = contents;
  }
  if (signer.enabled()) {
    // From now on these bytes are this generation. The list is signed either
    // way (unsigned is refused everywhere); if the record could not take them
    // (another writer, record busy), the user is told to save once more.
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

// Synchronous on purpose: queued on the async executor, the add/commit raced
// reencryptPath()'s blocking backup commit for index.lock and either left the
// .gpg-id untracked or found nothing to commit.
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

auto ImitatePass::gitTracks(const QString &file) -> bool {
  return Executor::executeBlocking(m_settings.gitExecutable,
                                   {"-C", pgit(m_settings.passStore),
                                    "ls-files", "--error-unmatch", "--",
                                    pgit(file)}) == 0;
}

auto ImitatePass::refuseLinkedGpgIdFolder(const QString &path) -> bool {
  // A link planted later is replaced, not written through: the writes stage
  // and rename.
  const QString folder = QDir::cleanPath(path);
  return refuseLinkedPath(path) ||
         refuseLinkedPath(folder + QStringLiteral("/.gpg-id")) ||
         refuseLinkedPath(folder + QStringLiteral("/.gpg-id.sig"));
}

auto ImitatePass::settleSignature(const QString &gpgIdFile,
                                  const QByteArray &written,
                                  const GpgIdSigner &signer, bool useGit,
                                  QString *sigToCommit) -> bool {
  const QString gpgIdSigFile = gpgIdFile + ".sig";
  if (signer.enabled()) {
    if (!signGpgIdFile(gpgIdFile, written)) {
      return false;
    }
    *sigToCommit = gpgIdSigFile;
    return true;
  }
  if (!QFile::exists(gpgIdSigFile)) {
    return true;
  }
  // Signing was switched off; the removal goes into the same commit.
  const bool tracked = useGit && gitTracks(gpgIdSigFile);
  if (!QFile::remove(gpgIdSigFile)) {
    emit critical(
        tr("Cannot update"),
        tr("Failed to remove the old signature %1.").arg(gpgIdSigFile));
    return false;
  }
  if (tracked) {
    *sigToCommit = gpgIdSigFile;
  }
  return true;
}

void ImitatePass::Init(QString path, const QList<UserInfo> &users) {
  // The .gpg-id is written as path + ".gpg-id": without the trailing
  // separator (the context menu hands over a cleaned path) that would be a
  // file beside the folder, not the folder's own list.
  path = Util::normalizeFolderPath(path);
  if (refuseLinkedGpgIdFolder(path)) {
    return;
  }
  const GpgIdSigner signer = gpgIdSigner();
  if (signer.enabled() && !signer.haveSecretKey()) {
    emit critical(tr("No signing key!"),
                  tr("None of the secret signing keys is available.\n"
                     "You will not be able to change the user list!"));
    return;
  }

  const bool useGit = gitReady();
  const QString gpgIdFile = path + ".gpg-id";
  QByteArray written;
  QString sigToCommit;
  if (!writeGpgIdFile(gpgIdFile, users, &written) ||
      !settleSignature(gpgIdFile, written, signer, useGit, &sigToCommit)) {
    return;
  }

  QString gitOut;
  QString gitErr;
  if (useGit && m_settings.addGPGId) {
    // Commit the .gpg-id (and .sig) before re-encrypting to it: the backup
    // commit only takes tracked files (#1685) and addFolder does not stage
    // it, so entries were pushed without their recipients file (#1682). No
    // commit, no re-encryption.
    const int gitExit = gitAddGpgId(gpgIdFile, sigToCommit, &gitOut, &gitErr);
    if (gitExit != 0) {
      Pass::finished(PASS_INIT, gitExit, gitOut, gitErr);
      return;
    }
  }
  reencryptPath(path);
  // finishedInit once the .gpg-id landed in git, as the async path did.
  if (useGit) {
    Pass::finished(PASS_INIT, 0, gitOut, gitErr);
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

void ImitatePass::reportUnrestorable(const QString &path) {
  emit critical(tr("Leftover from an earlier re-encryption"),
                tr("%1 is not a regular file and was not restored. Look "
                   "at it and remove it, then re-encrypt again.")
                    .arg(path));
}

void ImitatePass::dropStaleTemporary(const QString &path) {
  // A crash's temporary is picked up by a run an hour later.
  if (QFileInfo(path).lastModified() >
      QDateTime::currentDateTime().addSecs(-3600)) {
    qCDebug(lcQtPass) << "Leaving a recent temporary alone:" << path;
    return;
  }
  if (!QFile::remove(path)) {
    qCWarning(lcQtPass) << "Could not remove stale temporary" << path;
  }
}

auto ImitatePass::restoreBackup(const QString &path) -> bool {
  // Belt and braces: the walker only lists regular files.
  const QFileInfo backup(path);
  if (backup.isSymLink() || !backup.isFile()) {
    reportUnrestorable(path);
    return false;
  }
  const QString original =
      path.chopped(QStringLiteral(".reencrypt.bak").size());
  if (QFileInfo::exists(original)) {
    emit critical(tr("Leftover from an earlier re-encryption"),
                  tr("%1 exists next to %2. Both are encrypted copies of the "
                     "entry; check which one you want and delete the other, "
                     "then re-encrypt again.")
                      .arg(path, original));
    return false;
  }
  if (!QFile::rename(path, original)) {
    emit critical(tr("Leftover from an earlier re-encryption"),
                  tr("%1 is missing and its backup %2 could not be renamed "
                     "back. Rename it by hand, then re-encrypt again.")
                      .arg(original, path));
    return false;
  }
  qCWarning(lcQtPass) << "Restored" << original << "from" << path;
  emit statusMsg(tr("Restored %1 from the backup an interrupted "
                    "re-encryption left behind.")
                     .arg(original),
                 5000);
  return true;
}

auto ImitatePass::recoverReencryptLeftovers(const QString &dir) -> bool {
  // .qtpass-XXXXXX.tmp is today's staging name; 1.8.x replaced entries via
  // X.gpg.reencrypt.tmp and .bak in two renames, builds up to 2.0 wrote
  // X.gpg.XXXXXX.tmp. A .tmp is never a source of truth; a .bak is the only
  // copy when X.gpg is missing, and one of two valid ones when it is not.
  //
  // Regular files only: renaming a link under a leftover name into an entry's
  // place would re-encrypt whatever it points to. Links come back in
  // `skipped`; linked directories are left to reencryptFiles() to mention.
  const QStringList leftoverNames{QStringLiteral(".qtpass-??????.tmp"),
                                  QStringLiteral("*.gpg.reencrypt.tmp"),
                                  QStringLiteral("*.gpg.??????.tmp"),
                                  QStringLiteral("*.gpg.reencrypt.bak")};
  const auto isTemporary = [](const QString &path) {
    return path.endsWith(QStringLiteral(".tmp"));
  };
  QStringList skipped;
  // Hidden files included: the staged name starts with a dot.
  const QStringList leftovers = Util::regularFilesUnder(
      QDir::cleanPath(dir), leftoverNames, &skipped, true);
  bool clean = true;
  for (const QString &path : skipped) {
    if (!QDir::match(leftoverNames, QFileInfo(path).fileName())) {
      continue;
    }
    // Removing a link removes the link, never what it points to; a junction
    // or a directory symlink on Windows is a directory entry and goes with
    // rmdir.
    if (isTemporary(path)) {
      if (!QFile::remove(path) && !QDir().rmdir(path)) {
        qCWarning(lcQtPass) << "Could not remove stale temporary" << path;
      }
      continue;
    }
    reportUnrestorable(path);
    clean = false;
  }
  for (const QString &path : leftovers) {
    if (isTemporary(path)) {
      dropStaleTemporary(path);
    } else {
      clean = restoreBackup(path) && clean;
    }
  }
  return clean;
}

auto ImitatePass::verifyGpgIdForDir(const QString &file,
                                    QHash<QString, QStringList> &verified,
                                    QStringList &gpgId) -> bool {
  const QString gpgIdPath = Pass::getGpgIdPath(file, m_settings.passStore);
  // Keyed by .gpg-id, so every file under it gets exactly the list its
  // signature covered.
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

auto ImitatePass::decryptEntry(const QString &fileName, QString *plaintext)
    -> bool {
  const QStringList args = {
      "-d",      "--quiet",     "--yes", "--no-encrypt-to",
      "--batch", "--use-agent", "--",    pgpg(fileName)};
  if (execBlocking(m_settings.gpgExecutable, args, plaintext) != 0 ||
      plaintext->isEmpty()) {
    qCDebug(lcQtPass) << "Decrypt error on re-encrypt for:" << fileName;
    return false;
  }
  if (!plaintext->endsWith(u'\n')) {
    plaintext->append(u'\n');
  }
  return true;
}

auto ImitatePass::encryptFor(const QString &output,
                             const QStringList &recipients,
                             const QString &plaintext) -> bool {
  const QStringList args =
      encryptArgs({"--yes", "--batch"}, pgpg(output), recipients);
  if (execBlocking(m_settings.gpgExecutable, args, plaintext) != 0) {
    qCDebug(lcQtPass) << "Encrypt error on re-encrypt, output:" << output;
    return false;
  }
  return true;
}

auto ImitatePass::ciphertextHolds(const QString &ciphertext,
                                  const QString &plaintext) -> bool {
  QString decrypted;
  const QStringList args{"-d",          "--quiet", "--batch",
                         "--use-agent", "--",      pgpg(ciphertext)};
  if (execBlocking(m_settings.gpgExecutable, args, &decrypted) != 0 ||
      decrypted.isEmpty()) {
    qCDebug(lcQtPass) << "Verification failed for:" << ciphertext;
    return false;
  }
  // Defence in depth: gpg said it encrypted, and this is what comes back.
  if (decrypted.trimmed() != plaintext.trimmed()) {
    qCDebug(lcQtPass) << "Verification content mismatch for:" << ciphertext;
    return false;
  }
  return true;
}

auto ImitatePass::commitReencrypted(const QString &fileName) -> bool {
  if (!gitConfigured()) {
    return true;
  }
  // -C the store so git runs there rather than in QtPass's launch directory
  // (executeBlocking sets no working directory).
  const QString store = pgit(m_settings.passStore);
  if (execBlocking(m_settings.gitExecutable,
                   {"-C", store, "add", "--", pgit(fileName)}) != 0) {
    // The file on disk is re-encrypted correctly; only the repository is
    // now behind. Report it so the caller counts this file as failed and
    // the run is not pushed.
    qCDebug(lcQtPass) << "git add failed after re-encrypting:" << fileName;
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
  return true;
}

auto ImitatePass::reencryptSingleFile(const QString &fileName,
                                      const QStringList &recipients,
                                      QString *why) -> bool {
  qCDebug(lcQtPass) << "reencrypt" << fileName << "for" << recipients.size()
                    << "recipients";
  if (recipients.isEmpty()) {
    emit critical(tr("Can not edit"),
                  tr("Could not read encryption key to use, .gpg-id "
                     "file missing or invalid."));
    return false;
  }
  QString plaintext;
  if (!decryptEntry(fileName, &plaintext)) {
    return false;
  }

  // gpg writes outside the store, into a 0700 directory with an unguessable
  // name no co-writer can pre-create or link; placeEncryptedFile() then
  // replaces the entry in one rename, as in Insert().
  QTemporaryDir scratch;
  if (!scratch.isValid()) {
    qCDebug(lcQtPass) << "Cannot create a scratch directory for re-encrypting"
                      << fileName;
    return false;
  }
  const QString tempPath = scratch.filePath(QStringLiteral("reencrypted.tmp"));
  if (!encryptFor(tempPath, recipients, plaintext) ||
      !ciphertextHolds(tempPath, plaintext)) {
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
    // Reported once, in the run's summary: an unwritable folder fails every
    // entry alike. The old ciphertext stays unless the post-rename check
    // failed, and then `why` says so.
    if (why != nullptr && !why->contains(fileName)) {
      // Every reason but the copy's names the entry; that one names the
      // file gpg wrote, in a scratch directory the user never sees.
      *why = tr("%1 could not be re-encrypted: %2").arg(fileName, *why);
    }
    return false;
  }
  return commitReencrypted(fileName);
}

auto ImitatePass::createBackupCommit() -> bool {
  if (!gitConfigured()) {
    return true;
  }
  emit statusMsg(tr("Creating backup commit"), 2000);
  const QString git = m_settings.gitExecutable;
  // -C the store: executeBlocking sets no working directory, so git would
  // run in QtPass's launch directory, possibly an unrelated repository.
  const QString store = pgit(m_settings.passStore);
  // Tracked files only: an untracked plaintext export or swap file must not
  // be swept into a commit that autoPush sends to the shared remote.
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

void ImitatePass::reencryptPath(const QString &dir) {
  if (m_reencryptActive) {
    emit statusMsg(tr("A re-encryption is already running"), 3000);
    return;
  }
  // The store root may be a link; a folder inside that is, or lies behind,
  // one is not part of the store. MainWindow refuses it too; this covers
  // Init().
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

// The worker's kill() after the grace period is needed: terminate() is only
// SIGTERM, or WM_CLOSE on Windows, which a console gpg ignores.
void ImitatePass::cancelReencryptPath() {
  if (!m_reencryptActive)
    return;
  m_reencryptCancel.store(true);
}

// Init(), Move() and Copy() queue async git commands right before
// reencryptPath(); the worker's blocking git would fight them for the index
// lock, so wait for the queue to drain.
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

auto ImitatePass::pullBeforeReencrypt() -> bool {
  if (!m_settings.autoPull || !gitConfigured()) {
    return true;
  }
  emit statusMsg(tr("Updating password-store"), 2000);
  if (execBlocking(m_settings.gitExecutable,
                   {"-C", pgit(m_settings.passStore), "pull"}) == 0) {
    return true;
  }
  // A pull that could not reach the remote leaves the store as it was; one
  // that stopped in a merge leaves conflict markers and an unmerged index,
  // and re-encrypting on top of that would commit the mess.
  QString unmerged;
  execBlocking(m_settings.gitExecutable,
               {"-C", pgit(m_settings.passStore), "ls-files", "--unmerged"},
               &unmerged);
  if (!unmerged.trimmed().isEmpty()) {
    emit critical(tr("Git pull failed"),
                  tr("The pull left the store with unmerged files. Resolve "
                     "the conflict before re-encrypting."));
    return false;
  }
  emit statusMsg(tr("Git pull failed, re-encrypting the store as it is"), 5000);
  return true;
}

auto ImitatePass::entriesToReencrypt(const QString &dir) -> QStringList {
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
  return files;
}

auto ImitatePass::recipientsForDir(const QString &fileName,
                                   QHash<QString, QStringList> &verified,
                                   QStringList &gpgId, ReencryptResult &result)
    -> bool {
  if (!verifyGpgIdForDir(fileName, verified, gpgId)) {
    if (m_reencryptCancel.load()) {
      result.cancelled = true;
    } else {
      result.aborted = true;
    }
    return false;
  }
  if (gpgId.isEmpty() && !verified.isEmpty()) {
    emit critical(tr("GPG ID verification failed"),
                  tr("Could not verify .gpg-id for directory."));
    result.aborted = true;
    return false;
  }
  return true;
}

// Worker thread: only blocking helpers, nothing touching `exec` or the
// transaction state; signals reach the GUI queued. A helper failing while the
// cancel flag is set was interrupted: its file is neither checked nor failed.
auto ImitatePass::reencryptFiles(const QString &dir) -> ReencryptResult {
  ReencryptResult result;
  if (!pullBeforeReencrypt()) {
    result.aborted = true;
    return result;
  }

  // Leftovers of an interrupted run first: a restored entry then goes into
  // the backup commit like everything else, and a stale temporary does not.
  if (!recoverReencryptLeftovers(dir)) {
    result.aborted = true;
    return result;
  }

  if (!createBackupCommit()) {
    if (m_reencryptCancel.load()) {
      result.cancelled = true;
    } else {
      result.aborted = true;
    }
    return result;
  }

  const QStringList files = entriesToReencrypt(dir);
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
      if (!recipientsForDir(fileName, gpgIdFilesVerified, gpgId, result)) {
        return result;
      }
      currentDir = fileDir;
    }
    if (getKeysFromFile(fileName) != gpgId) {
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

// No push after a cancel, abort or per-file failure: a partially re-encrypted
// store must not reach the remote before the user inspects it.
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

auto ImitatePass::copyDestination(const QString &src, const QString &dest,
                                  bool force) -> QString {
  const auto refuse = [this, &src](const QString &to) {
    emit critical(tr("Copy failed"),
                  tr("Could not copy %1 to %2.").arg(src, to));
    return QString();
  };
  // Like `pass cp`, dest may be an existing folder (a drag-and-drop copy hands
  // over the folder, not the new file name). Resolve the real target the same
  // way Move does: into the folder, .gpg appended, no clobbering without force.
  const QString destFile = resolveMoveDestination(src, dest, force);
  if (destFile.isEmpty()) {
    return refuse(dest);
  }
  // A link planted under that name (dangling ones pass exists()) is not an
  // entry to write.
  if (refuseLinkedPath(destFile)) {
    return {};
  }
  const QFileInfo destFileInfo(destFile);
  // A folder destination that is the source's own folder resolves to the
  // source itself; with force that would replace the only copy with itself.
  // And resolveMoveDestination only sees a clash when dest names the file.
  if (QFileInfo(src) == destFileInfo || (!force && destFileInfo.exists())) {
    return refuse(destFile);
  }
  return destFile;
}

void ImitatePass::Copy(const QString src, const QString dest,
                       const bool force) {
  // QFile::copy reads through a link: the target's bytes would become an
  // entry of the store.
  if (refuseLinkedPath(src) || refuseLinkedPath(dest)) {
    return;
  }
  TransactionHelper trans(&m_transaction, PASS_COPY);
  const QString destFile = copyDestination(src, dest, force);
  if (destFile.isEmpty()) {
    return;
  }
  // git has no "cp": copy on disk in both modes, then stage. Synchronous and
  // atomic (Util::copyFileReplacing follows no link), so it exists before the
  // re-encryption below and a forced overwrite survives a half-way failure;
  // without force nothing is replaced, not even what appeared since the check.
  QString why;
  if (!Util::copyFileReplacing(src, destFile, force, &why)) {
    emit critical(tr("Copy failed"),
                  tr("Could not copy %1 to %2.").arg(src, destFile) + "\n" +
                      why);
    return;
  }
  if (gitReady()) {
    executeGit(GIT_COPY, {"add", "--", pgit(destFile)});
    gitCommit("",
              QString("Copied from %1 to %2 using QtPass.").arg(src, destFile));
  }
  const QFileInfo written(destFile);
  if (written.isDir()) {
    reencryptPath(written.absoluteFilePath());
  } else if (written.isFile()) {
    reencryptPath(written.dir().path());
  }
}

void ImitatePass::executeGpg(PROCESS id, const QStringList &args, QString input,
                             bool readStdout, bool readStderr) {
  executeWrapper(id, m_settings.gpgExecutable, args, std::move(input),
                 readStdout, readStderr);
}

// useGit can be on with no gitExecutable (fresh setup, git removed later);
// handing that to the Executor used to wedge the queue (#1682).
auto ImitatePass::gitConfigured() const -> bool {
  return m_settings.useGit && !m_settings.gitExecutable.isEmpty();
}

// Tells the user once per operation why git was skipped, so the store
// silently drifting from git does not go unnoticed.
auto ImitatePass::gitReady() -> bool {
  if (m_settings.useGit && m_settings.gitExecutable.isEmpty()) {
    emit statusMsg(tr("Git executable not configured, skipping git"), 3000);
    return false;
  }
  return m_settings.useGit;
}

void ImitatePass::executeGit(PROCESS id, const QStringList &args, QString input,
                             bool readStdout, bool readStderr) {
  // Callers check gitReady() first; an empty executable that still gets here
  // is reported by the Executor instead of wedging the queue (#1682).
  executeWrapper(id, m_settings.gitExecutable, args, std::move(input),
                 readStdout, readStderr);
}

// Only PASS_* processes reach Pass::finished, so to the interface this looks
// the same as RealPass.
void ImitatePass::finished(int id, int exitCode, const QString &out,
                           const QString &err) {
  qCDebug(lcQtPass) << "Imitate Pass";
  QString error = err;
  if (id == PASS_INSERT && !m_pendingInserts.isEmpty()) {
    // Insert()'s gpg step: place the ciphertext before the queued git steps
    // run (the executor starts the next item only after this returns). On
    // failure it fails like a gpg error: git steps cancelled below.
    // No dialog here: it would spin the event loop and run git steps first.
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
  // A link planted since the check is replaced as an entry; without
  // overwrite, anything new under the name fails the add.
  return Util::copyFileReplacing(output, file, overwrite, error);
}

void ImitatePass::beforeExecute(PROCESS id) {
  m_transaction.transactionAdd(id);
}

// PCRE, not pass's POSIX BRE (see Pass::Grep). Runs on a NativeGrep thread;
// the result arrives on this thread as finishedGrep.
void ImitatePass::Grep(QString pattern, bool caseInsensitive) {
  m_grep.search(pattern, caseInsensitive, m_settings.gpgExecutable,
                m_settings.passStore, exec.environment());
}
