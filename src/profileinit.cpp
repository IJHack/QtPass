// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "profileinit.h"
#include "appsettings.h"
#include "executor.h"
#include "gpgidsigner.h"
#include "userinfo.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

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
  if (!folder.exists() && !QDir().mkpath(folder.absolutePath())) {
    out = tr("Could not create %1.").arg(folder.absolutePath());
    return false;
  }
  const QString gpgIdFile = folder.filePath(QStringLiteral(".gpg-id"));
  if (!writeGpgId(gpgIdFile, users, &out)) {
    return false;
  }
  QString sigFile;
  if (!s.passSigningKey.trimmed().isEmpty()) {
    if (!signGpgId(gpgIdFile, s, &out)) {
      return false;
    }
    sigFile = gpgIdFile + QStringLiteral(".sig");
  }
  if (useGit &&
      !commitGpgId(folder.absolutePath(), gpgIdFile, sigFile, s, &out)) {
    return false;
  }

  const QStringList existing = folder.entryList(
      {QStringLiteral("*.gpg")}, QDir::Files | QDir::NoDotAndDotDot);
  if (!existing.isEmpty()) {
    out = tr("%1 already contains %n encrypted file(s); they were not "
             "re-encrypted. Switch to the profile and open Users to do that.",
             nullptr, static_cast<int>(existing.size()))
              .arg(folder.absolutePath());
  }
  return true;
}

auto ProfileInit::writeGpgId(const QString &gpgIdFile,
                             const QList<UserInfo> &users, QString *note)
    -> bool {
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
  QFile file(gpgIdFile);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text) ||
      file.write((ids.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8()) <
          0) {
    *note = tr("Could not write %1: %2").arg(gpgIdFile, file.errorString());
    return false;
  }
  file.close();
  // Same as ImitatePass::writeGpgIdFile: the recipient list leaks which keys
  // the store is encrypted to, so keep it owner-only where that means
  // anything.
  QFile::setPermissions(gpgIdFile, QFile::ReadOwner | QFile::WriteOwner);
  return true;
}

auto ProfileInit::signGpgId(const QString &gpgIdFile, const AppSettings &s,
                            QString *note) -> bool {
  const GpgIdSigner signer(s.gpgExecutable,
                           GpgIdSigner::keysFromSetting(s.passSigningKey));
  QString err;
  if (!signer.sign(gpgIdFile, &err)) {
    *note = tr("Could not sign %1 with %2: %3")
                .arg(gpgIdFile, signer.keys().first(), err.trimmed());
    return false;
  }
  return true;
}

auto ProfileInit::commitGpgId(const QString &dir, const QString &gpgIdFile,
                              const QString &sigFile, const AppSettings &s,
                              QString *note) -> bool {
  // A process of our own in the profile directory: nothing here may touch
  // the shared Executor or PASSWORD_STORE_DIR of the active store.
  QProcess git;
  git.setWorkingDirectory(dir);
  auto run = [&](const QStringList &args) -> bool {
    QString err;
    const int rc = Executor::executeBlocking(git, s.gitExecutable, args,
                                             QString(), nullptr, &err);
    if (rc != 0) {
      *note =
          tr("git %1 failed in %2: %3").arg(args.first(), dir, err.trimmed());
      return false;
    }
    return true;
  };
  QStringList files{QFileInfo(gpgIdFile).fileName()};
  if (!sigFile.isEmpty()) {
    files << QFileInfo(sigFile).fileName();
  }
  const QStringList add =
      QStringList{QStringLiteral("add"), QStringLiteral("--")} + files;
  const QStringList commit =
      QStringList{QStringLiteral("commit"), QStringLiteral("-m"),
                  QStringLiteral("Added .gpg-id using QtPass."),
                  QStringLiteral("--")} +
      files;
  return run({QStringLiteral("init")}) && run(add) && run(commit);
}

auto ProfileInit::initGit(const QString &dir, const AppSettings &s,
                          QString *note) -> bool {
  QProcess git;
  git.setWorkingDirectory(dir);
  auto run = [&](const QStringList &args) -> bool {
    QString err;
    const int rc = Executor::executeBlocking(git, s.gitExecutable, args,
                                             QString(), nullptr, &err);
    if (rc != 0) {
      *note =
          tr("git %1 failed in %2: %3").arg(args.first(), dir, err.trimmed());
      return false;
    }
    return true;
  };
  // Only what belongs to a store is staged: an existing folder may hold
  // exports, editor swap files or other plaintext that must not enter the
  // history (the same rule the re-encryption backup commit follows).
  QStringList files;
  QDirIterator it(dir,
                  {QStringLiteral("*.gpg"), QStringLiteral(".gpg-id"),
                   QStringLiteral(".gpg-id.sig")},
                  QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
  const QDir base(dir);
  while (it.hasNext()) {
    const QString rel = base.relativeFilePath(it.next());
    if (!rel.startsWith(QStringLiteral(".git/"))) {
      files << rel;
    }
  }
  if (!run({QStringLiteral("init")})) {
    return false;
  }
  if (!files.isEmpty() &&
      !run(QStringList{QStringLiteral("add"), QStringLiteral("--")} + files)) {
    return false;
  }
  return run({QStringLiteral("commit"), QStringLiteral("--allow-empty"),
              QStringLiteral("-m"),
              QStringLiteral("Added password store using QtPass.")});
}

auto ProfileInit::gitIdentityConfigured(const QString &dir,
                                        const AppSettings &s) -> bool {
  QProcess git;
  git.setWorkingDirectory(dir);
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
