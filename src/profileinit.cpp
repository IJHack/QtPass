// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Implements ProfileInit; the environments its gpg and git processes
 * run in are built by withGpgHome() and gitEnvironment() here.
 */
#include "profileinit.h"
#include "appsettings.h"
#include "executor.h"
#include "gpgidgeneration.h"
#include "gpgidsigner.h"
#include "qtpasslogging.h"
#include "userinfo.h"
#include "util.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QSet>
#include <algorithm>
#include <optional>

namespace {
/// @p env with the configured GPG home applied as Pass::init() applies it:
/// GNUPGHOME is that folder when it exists; otherwise the inherited one
/// stays. The new store's .gpg-id is signed, and a signed commit made, with
/// the keyring QtPass uses everywhere else, where the signing key may be
/// the only place it is.
auto withGpgHome(QProcessEnvironment env, const AppSettings &s)
    -> QProcessEnvironment {
  if (!s.gpgHome.isEmpty()) {
    QDir home(s.gpgHome);
    home.makeAbsolute();
    if (home.exists()) {
      env.insert(QStringLiteral("GNUPGHOME"), home.path());
    }
  }
  return env;
}

/// As Pass::updateEnv() sets it for the active store: git in @p dir does not
/// look above it, so neither the identity check nor a commit reads or uses a
/// repository the new store merely lies in (a dotfiles one in the home
/// folder), unless the user opted into that; and paths are names to it, not
/// patterns.
auto gitEnvironment(const QString &dir, const AppSettings &s)
    -> QProcessEnvironment {
  QProcessEnvironment env =
      withGpgHome(QProcessEnvironment::systemEnvironment(), s);
  // As for the active store: the paths handed to git are names, not
  // patterns (see Pass::updateEnv()).
  env.insert(QStringLiteral("GIT_LITERAL_PATHSPECS"), QStringLiteral("1"));
  if (!s.gitUseParentRepository) {
    for (const auto &[name, value] :
         Util::gitStoreVariables(dir, s.gitExecutable)) {
      env.insert(name, value);
    }
  }
  return env;
}
/// "commit", and "-S" when git config pass.signcommits is true for the
/// repository @p git runs in (a new one: so the user's own git config), as
/// pass signs its commits.
/// Time `git init` and `git add` may take while the window waits: the two
/// that read what lies in the folder. A commit is not bounded: git may sign
/// it (pass.signcommits or commit.gpgSign) and wait for the passphrase.
constexpr int kGitSetupTimeoutMs = 2 * 60 * 1000;

/**
 * @brief The first thing git would open while staging @p files in @p dir
 * that it could wait on for good, or empty.
 *
 * git add opens the .gitattributes and .gitignore of each folder it stages
 * from, by name: a FIFO there, planted in a shared folder, kept it waiting
 * in open() with the window waiting on it. A link to one counts as one:
 * older git follows it. A sub-folder with a .git of its own is a repository git
 * looks into (its HEAD, say).
 * @param dir The store.
 * @param files Paths relative to @p dir that git is to stage.
 * @return That path, or empty.
 */
auto blocksGit(const QString &dir, const QStringList &files) -> QString {
  QSet<QString> folders{QStringLiteral(".")};
  for (const QString &file : files) {
    for (QString folder = QFileInfo(file).path();
         folder != QLatin1String(".") && !folders.contains(folder);
         folder = QFileInfo(folder).path()) {
      folders.insert(folder);
    }
  }
  const QDir base(dir);
  for (const QString &folder : std::as_const(folders)) {
    const QDir here(base.filePath(folder));
    for (const char *name : {".gitattributes", ".gitignore"}) {
      const QFileInfo info(here.filePath(QLatin1String(name)));
      if (info.exists() && !info.isFile()) {
        return info.filePath();
      }
    }
    const QFileInfo nested(here.filePath(QStringLiteral(".git")));
    if (folder != QLatin1String(".") &&
        (nested.exists() || nested.isSymLink())) {
      return nested.filePath();
    }
  }
  return {};
}

/**
 * @brief The signer for the store set up with @p s: gpg runs in the
 * configured GPG home, as ImitatePass's signer runs in the store's
 * environment.
 * @param s The settings, with the signing key and gpg to use.
 * @return The signer.
 */
auto signerFor(const AppSettings &s) -> GpgIdSigner {
  return GpgIdSigner(
      s.gpgExecutable, GpgIdSigner::keysFromSetting(s.passSigningKey),
      [s](const QString &app, const QStringList &args, const QString &input,
          QString *out, QString *err) {
        QProcess gpg;
        gpg.setProcessEnvironment(
            withGpgHome(QProcessEnvironment::systemEnvironment(), s));
        return Executor::executeBlocking(gpg, app, args, input, out, err);
      });
}

auto commitArgs(QProcess &git, const AppSettings &s) -> QStringList {
  QString out;
  const bool sign =
      Executor::executeBlocking(
          git, s.gitExecutable,
          {QStringLiteral("config"), QStringLiteral("--bool"),
           QStringLiteral("--get"), QStringLiteral("pass.signcommits")},
          QString(), &out, nullptr) == 0 &&
      out.trimmed() == QLatin1String("true");
  return sign ? QStringList{QStringLiteral("commit"), QStringLiteral("-S")}
              : QStringList{QStringLiteral("commit")};
}
} // namespace

auto ProfileInit::gpgEnvironment(const AppSettings &s) -> QProcessEnvironment {
  return withGpgHome(QProcessEnvironment::systemEnvironment(), s);
}

auto ProfileInit::needsInit(const QString &path) -> bool {
  if (path.isEmpty()) {
    return false;
  }
  QDir dir(path);
  if (!dir.exists()) {
    return false;
  }
  return !dir.exists(".gpg-id");
}

auto ProfileInit::initialise(const QString &dir, const QList<UserInfo> &users,
                             const AppSettings &s, bool useGit, QString *note)
    -> bool {
  QString scratch;
  QString &out = note != nullptr ? *note : scratch;
  out.clear();

  const QDir folder(dir);
  if (!folder.exists() && !Util::mkpathForStore(folder.absolutePath())) {
    out = tr("Could not create %1.").arg(folder.absolutePath());
    return false;
  }
  // Before anything is written: a refusal later would leave a .gpg-id
  // behind, and a store that looks finished.
  if (useGit &&
      refusedForGit(folder.absolutePath(),
                    {QStringLiteral(".gpg-id"), QStringLiteral(".gpg-id.sig")},
                    &out)) {
    return false;
  }
  const QString gpgIdFile = folder.filePath(QStringLiteral(".gpg-id"));
  // Before the list is written, as ImitatePass::Init() does: writing it
  // takes the next generation, and a list that then cannot be signed left
  // this device's record one ahead of the store's own, refused afterwards
  // as a rollback.
  if (!s.passSigningKey.trimmed().isEmpty() && !signerFor(s).haveSecretKey()) {
    out = tr("The secret key of the signing key %1 is not available, so no "
             "recipient list was written.")
              .arg(GpgIdSigner::keysFromSetting(s.passSigningKey).first());
    // withGpgHome() fell back to the inherited home, as for signing below.
    if (!s.gpgHome.isEmpty()) {
      QDir home(s.gpgHome);
      home.makeAbsolute();
      if (!home.exists()) {
        out += QLatin1Char(' ') + tr("The configured GPG home %1 does not "
                                     "exist.")
                                      .arg(home.path());
      }
    }
    return false;
  }
  QByteArray written;
  if (!writeGpgId(gpgIdFile, users, !s.passSigningKey.trimmed().isEmpty(), &out,
                  &written)) {
    return false;
  }
  QString sigFile;
  if (!s.passSigningKey.trimmed().isEmpty()) {
    if (!signGpgId(gpgIdFile, written, s, &out)) {
      return false;
    }
    sigFile = gpgIdFile + QStringLiteral(".sig");
  }
  if (useGit &&
      !commitGpgId(folder.absolutePath(), gpgIdFile, sigFile, s, &out)) {
    return false;
  }

  // Entries in sub-folders too: a store taken over whole has most of them
  // there. Visible regular files only, not through links, .git not walked.
  // Not those under a sub-folder with a .gpg-id of its own: its recipients
  // did not change here, and Users on the profile's root leaves it alone,
  // as pass does.
  const QString root = QDir::cleanPath(folder.absolutePath());
  const bool viaPass = s.usePass;
  const QString rootList =
      QFileInfo(QDir(root).filePath(QStringLiteral(".gpg-id")))
          .canonicalFilePath();
  // What a folder's own .gpg-id decides, if it has one.
  const auto ownList = [&rootList,
                        viaPass](const QString &dir) -> std::optional<bool> {
    // A list of its own: a regular file. A linked .gpg-id is no list to
    // the native backend, as everywhere in QtPass, so its folder's entries
    // still count; pass follows the link (`-f`) and keeps that folder on
    // the linked list, so with it they do not, unless the link leads to the
    // root's own list: that folder follows the root, and Users on it
    // re-encrypts them.
    const QFileInfo list(QDir(dir).filePath(QStringLiteral(".gpg-id")));
    if (viaPass && list.isFile() && !rootList.isEmpty() &&
        list.canonicalFilePath() == rootList) {
      return false;
    }
    if (list.isFile() &&
        (viaPass || (!list.isSymLink() && !list.isJunction()))) {
      return true;
    }
    return std::nullopt;
  };
  // Each folder's verdict once: climbing from every entry to the root again
  // cost entries times depth, and a co-writer's deep chain of folders froze
  // Finish for tens of minutes.
  QHash<QString, bool> verdictOf;
  const auto underSubStore = [&root, &ownList,
                              &verdictOf](const QString &entry) {
    QStringList climbed;
    QString dir = QFileInfo(entry).absolutePath();
    bool verdict = false;
    while (dir != root && dir.startsWith(root + QLatin1Char('/'))) {
      if (const auto known = verdictOf.constFind(dir);
          known != verdictOf.constEnd()) {
        verdict = known.value();
        break;
      }
      climbed << dir;
      if (const std::optional<bool> own = ownList(dir)) {
        verdict = *own;
        break;
      }
      dir = QFileInfo(dir).absolutePath();
    }
    for (const QString &folder : std::as_const(climbed)) {
      verdictOf.insert(folder, verdict);
    }
    return verdict;
  };
  QStringList existing =
      Util::regularFilesUnder(root, {QStringLiteral("*.gpg")});
  existing.erase(
      std::remove_if(existing.begin(), existing.end(), underSubStore),
      existing.end());
  if (!existing.isEmpty()) {
    out = tr("%1 already contains %n encrypted file(s); they were not "
             "re-encrypted. Switch to the profile and open Users to do that.",
             nullptr, static_cast<int>(existing.size()))
              .arg(folder.absolutePath());
  }
  return true;
}

auto ProfileInit::writeGpgId(const QString &gpgIdFile,
                             const QList<UserInfo> &users, bool signed_,
                             QString *note, QByteArray *written) -> bool {
  QStringList ids;
  for (const UserInfo &user : users) {
    if (user.enabled) {
      ids << user.key_id;
    }
  }
  if (ids.isEmpty()) {
    *note = tr("No recipient selected; %1 was not written.").arg(gpgIdFile);
    return false;
  }
  // As ImitatePass::writeGpgIdFile: renamed into place whole, so an
  // interrupted write leaves no half .gpg-id to sign or use. Owner-only: the
  // list leaks which keys the store is encrypted to.
  QByteArray contents =
      (ids.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8();
  if (signed_) {
    // Same rule as ImitatePass::writeGpgIdFile: reserved and recorded
    // before the write, bound to the store root (GpgIdGeneration). A new
    // profile has no list on disk to take a generation from.
    QString why;
    const std::optional<QString> store = GpgIdGeneration::storeIdFor(
        QFileInfo(gpgIdFile).absolutePath(), std::nullopt, &why);
    if (!store) {
      *note = why;
      return false;
    }
    const std::optional<qint64> generation =
        GpgIdGeneration::reserveNext(gpgIdFile, std::nullopt, &why);
    if (!generation) {
      *note = why;
      return false;
    }
    contents = GpgIdGeneration::withHeader(*generation, QStringLiteral("."),
                                           contents, *store);
  }
  QString why;
  if (!Util::writeFileReplacing(gpgIdFile, contents, true, &why)) {
    *note = why;
    return false;
  }
  if (written != nullptr) {
    *written = contents;
  }
  if (signed_) {
    // Same as ImitatePass::writeGpgIdFile: the bytes written are the ones
    // recognised; not fatal, the first read digests them otherwise.
    QString why;
    if (!GpgIdGeneration::recordWritten(gpgIdFile, contents, &why)) {
      qCWarning(lcQtPass) << "Could not record the written .gpg-id:" << why;
    }
  }
  return true;
}

auto ProfileInit::signGpgId(const QString &gpgIdFile,
                            const QByteArray &contents, const AppSettings &s,
                            QString *note) -> bool {
  const GpgIdSigner signer = signerFor(s);
  QString err;
  if (!signer.sign(gpgIdFile, contents, &err)) {
    *note = err.trimmed().isEmpty()
                ? tr("Could not sign %1 with %2.")
                      .arg(gpgIdFile, signer.keys().first())
                : tr("Could not sign %1 with %2: %3")
                      .arg(gpgIdFile, signer.keys().first(), err.trimmed());
    // withGpgHome() fell back to the inherited home: gpg's "no secret key"
    // would not say that the configured one is missing.
    if (!s.gpgHome.isEmpty()) {
      QDir home(s.gpgHome);
      home.makeAbsolute();
      if (!home.exists()) {
        *note +=
            QLatin1Char(' ') +
            tr("The configured GPG home %1 does not exist.").arg(home.path());
      }
    }
    return false;
  }
  return true;
}

auto ProfileInit::runGit(QProcess &git, const AppSettings &s,
                         const QString &dir, const QStringList &args,
                         QString *note) -> bool {
  const bool bounded = args.first() == QLatin1String("init") ||
                       args.first() == QLatin1String("add");
  QElapsedTimer clock;
  clock.start();
  QString err;
  const int rc = Executor::executeBlocking(git, s.gitExecutable, args,
                                           QString(), nullptr, &err, nullptr,
                                           bounded ? kGitSetupTimeoutMs : -1);
  if (rc == 0) {
    return true;
  }
  // -1 is what a stopped run returns; git's own failures are positive.
  if (bounded && rc == -1 && clock.elapsed() >= kGitSetupTimeoutMs) {
    *note = tr("git %1 did not finish within %2 minutes in %3, and was "
               "stopped.")
                .arg(args.first())
                .arg(kGitSetupTimeoutMs / 60000)
                .arg(dir);
  } else {
    *note = tr("git %1 failed in %2: %3").arg(args.first(), dir, err.trimmed());
  }
  return false;
}

auto ProfileInit::refusedForGit(const QString &dir, const QStringList &files,
                                QString *note) -> bool {
  const QString path = blocksGit(dir, files);
  if (path.isEmpty()) {
    return false;
  }
  *note = QFileInfo(path).fileName() == QLatin1String(".git")
              ? tr("%1 is a Git repository inside the store. Move it out of "
                   "the store, then try again.")
                    .arg(QDir::toNativeSeparators(QFileInfo(path).path()))
              : tr("%1 is not a regular file, and Git could wait on it for "
                   "good. Remove it or make it a regular file, then try "
                   "again.")
                    .arg(QDir::toNativeSeparators(path));
  return true;
}

auto ProfileInit::commitGpgId(const QString &dir, const QString &gpgIdFile,
                              const QString &sigFile, const AppSettings &s,
                              QString *note) -> bool {
  // A process of our own in the profile directory: nothing here may touch
  // the shared Executor or PASSWORD_STORE_DIR of the active store.
  QProcess git;
  git.setWorkingDirectory(dir);
  git.setProcessEnvironment(gitEnvironment(dir, s));
  auto run = [&](const QStringList &args) -> bool {
    return runGit(git, s, dir, args, note);
  };
  QStringList files{QFileInfo(gpgIdFile).fileName()};
  if (!sigFile.isEmpty()) {
    files << QFileInfo(sigFile).fileName();
  }
  // initialise() checked before writing these; again here, as signing can
  // take a while and the folder is shared.
  if (refusedForGit(dir, files, note)) {
    return false;
  }
  const QStringList add =
      QStringList{QStringLiteral("add"), QStringLiteral("--")} + files;
  if (!run({QStringLiteral("init")}) || !run(add)) {
    return false;
  }
  return run(commitArgs(git, s) +
             QStringList{QStringLiteral("-m"),
                         QStringLiteral("Added .gpg-id using QtPass."),
                         QStringLiteral("--")} +
             files);
}

auto ProfileInit::initGit(const QString &dir, const AppSettings &s,
                          QString *note) -> bool {
  QProcess git;
  git.setWorkingDirectory(dir);
  git.setProcessEnvironment(gitEnvironment(dir, s));
  auto run = [&](const QStringList &args) -> bool {
    return runGit(git, s, dir, args, note);
  };
  // Stage only store files (as the re-encryption backup commit does): an
  // existing folder may hold plaintext exports or swap files. Regular files
  // only: a link or junction must not have its target staged.
  QStringList files;
  const QDir base(dir);
  // Hidden directories (.git among them) are not walked, hidden files are:
  // .gpg-id is one.
  const QStringList found = Util::regularFilesUnder(
      dir,
      {QStringLiteral("*.gpg"), QStringLiteral(".gpg-id"),
       QStringLiteral(".gpg-id.sig")},
      nullptr, true);
  for (const QString &path : found) {
    files << base.relativeFilePath(path);
  }
  if (refusedForGit(dir, files, note)) {
    return false;
  }
  if (!run({QStringLiteral("init")})) {
    return false;
  }
  if (!files.isEmpty() &&
      !run(QStringList{QStringLiteral("add"), QStringLiteral("--")} + files)) {
    return false;
  }
  return run(commitArgs(git, s) +
             QStringList{QStringLiteral("--allow-empty"), QStringLiteral("-m"),
                         QStringLiteral("Added password store using QtPass.")});
}

auto ProfileInit::gitIdentityConfigured(const QString &dir,
                                        const AppSettings &s) -> bool {
  QProcess git;
  git.setWorkingDirectory(dir);
  git.setProcessEnvironment(gitEnvironment(dir, s));
  for (const char *key : {"user.name", "user.email"}) {
    QString out;
    if (Executor::executeBlocking(git, s.gitExecutable,
                                  {QStringLiteral("config"),
                                   QStringLiteral("--get"),
                                   QString::fromLatin1(key)},
                                  QString(), &out, nullptr) != 0 ||
        out.trimmed().isEmpty()) {
      return false;
    }
  }
  return true;
}
