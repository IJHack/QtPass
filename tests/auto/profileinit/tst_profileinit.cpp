// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#ifndef Q_OS_WIN
#include <unistd.h>
#endif

#include "../../../src/appsettings.h"
#include "../../../src/profileinit.h"
#include "../../../src/userinfo.h"
#include "../testsettings.h"

class tst_profileinit : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase() { isolateTestSettings(); }
  void needsInitFalseForNonexistentPath();
  void initialiseWithASigningKeyWritesTheGenerationHeader();
  void needsInitFalseForEmptyPath();
  void needsInitTrueForDirWithoutGpgId();
  void needsInitFalseForDirWithGpgId();
  void needsInitFalseAfterGpgIdAdded();
  void initialiseWritesEnabledKeysOnly();
  void initialiseRefusesWithoutRecipients();
  void initialiseCreatesTheDirectory();
  void initialiseCommitsUnderGit();
  void initialiseWarnsAboutExistingEntries();
  void initialiseLeavesNoHalfWrittenGpgId();

private:
  static auto twoUsers() -> QList<UserInfo>;
  static auto gitSettings() -> AppSettings;
};

auto tst_profileinit::twoUsers() -> QList<UserInfo> {
  UserInfo alice;
  alice.key_id = QStringLiteral("AAAA1111AAAA1111AAAA1111AAAA1111AAAA1111");
  alice.enabled = true;
  UserInfo bob;
  bob.key_id = QStringLiteral("BBBB2222BBBB2222BBBB2222BBBB2222BBBB2222");
  bob.enabled = false;
  return {alice, bob};
}

auto tst_profileinit::gitSettings() -> AppSettings {
  AppSettings s;
  s.gitExecutable = QStandardPaths::findExecutable(QStringLiteral("git"));
  return s;
}

void tst_profileinit::needsInitFalseForNonexistentPath() {
  // Build a guaranteed-absent path inside the system temp tree.
  QTemporaryDir base;
  QVERIFY2(base.isValid(), "base temp dir must be created");
  const QString absent =
      QDir(base.path()).filePath(QStringLiteral("nonexistent_subdir"));
  QVERIFY2(!ProfileInit::needsInit(absent),
           "needsInit must return false for a path that does not exist");
}

void tst_profileinit::needsInitFalseForEmptyPath() {
  QVERIFY2(!ProfileInit::needsInit(QString()),
           "needsInit must return false for an empty path");
}

void tst_profileinit::needsInitTrueForDirWithoutGpgId() {
  QTemporaryDir dir;
  QVERIFY2(dir.isValid(), "temp dir must be created");
  QVERIFY2(ProfileInit::needsInit(dir.path()),
           "needsInit must return true for a directory without .gpg-id");
}

void tst_profileinit::needsInitFalseForDirWithGpgId() {
  QTemporaryDir dir;
  QVERIFY2(dir.isValid(), "temp dir must be created");

  QFile gpgId(QDir(dir.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY2(gpgId.open(QIODevice::WriteOnly), ".gpg-id must be writable");
  gpgId.close();

  QVERIFY2(!ProfileInit::needsInit(dir.path()),
           "needsInit must return false for a directory that has .gpg-id");
}

void tst_profileinit::needsInitFalseAfterGpgIdAdded() {
  QTemporaryDir dir;
  QVERIFY2(dir.isValid(), "temp dir must be created");

  QVERIFY2(ProfileInit::needsInit(dir.path()),
           "needsInit must return true before .gpg-id is created");

  QFile gpgId(QDir(dir.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY2(gpgId.open(QIODevice::WriteOnly), ".gpg-id must be writable");
  gpgId.close();

  QVERIFY2(!ProfileInit::needsInit(dir.path()),
           "needsInit must return false after .gpg-id is created");
}

/**
 * @brief The recipients file lists exactly the enabled keys, one per line,
 *        readable by its owner only.
 */
void tst_profileinit::initialiseWritesEnabledKeysOnly() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QString note;
  QVERIFY2(ProfileInit::initialise(dir.path(), twoUsers(), AppSettings(), false,
                                   &note),
           qPrintable(note));
  QVERIFY2(note.isEmpty(), qPrintable("quiet success expected: " + note));
  QFile gpgId(QDir(dir.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::ReadOnly | QIODevice::Text));
  // No signing key, so a plain list: nothing checks its freshness and every
  // client reads it.
  QCOMPARE(QString::fromUtf8(gpgId.readAll()),
           QStringLiteral("AAAA1111AAAA1111AAAA1111AAAA1111AAAA1111\n"));
  QVERIFY(!ProfileInit::needsInit(dir.path()));
#ifndef Q_OS_WIN
  const auto perms = QFileInfo(gpgId).permissions();
  QVERIFY2(!(perms & (QFile::ReadGroup | QFile::ReadOther)),
           ".gpg-id must be owner-only");
#endif
}

/**
 * @brief The .gpg-id is put in place whole or not at all: when the folder
 *        cannot be written to, initialise() fails with a note and leaves no
 *        file behind for a signing step or a later run to take for the list.
 */
void tst_profileinit::initialiseLeavesNoHalfWrittenGpgId() {
#ifdef Q_OS_WIN
  QSKIP("a read-only directory does not stop writes on Windows");
#else
  if (::geteuid() == 0) {
    QSKIP("root writes into read-only directories");
  }
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QVERIFY(
      QFile::setPermissions(dir.path(), QFile::ReadOwner | QFile::ExeOwner));
  const auto restore = qScopeGuard([&dir] {
    QFile::setPermissions(dir.path(), QFile::ReadOwner | QFile::WriteOwner |
                                          QFile::ExeOwner);
  });
  QString note;
  QVERIFY(!ProfileInit::initialise(dir.path(), twoUsers(), AppSettings(), false,
                                   &note));
  QVERIFY2(note.contains(QStringLiteral(".gpg-id")), qPrintable(note));
  QVERIFY2(
      QDir(dir.path())
          .entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)
          .isEmpty(),
      "nothing may be left behind, no temporary either");
#endif
}

/**
 * @brief With a signing key configured the list carries the generation and
 *        folder header the store's lists are checked against; signing itself
 *        fails here (no gpg), which is reported, but the file is what a
 *        signed store's first list looks like.
 */
void tst_profileinit::initialiseWithASigningKeyWritesTheGenerationHeader() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  AppSettings s;
  s.passSigningKey = QStringLiteral("AAAA1111AAAA1111AAAA1111AAAA1111AAAA1111");
  s.gpgExecutable = QStringLiteral("/nonexistent/gpg");
  QString note;
  ProfileInit::initialise(dir.path(), twoUsers(), s, false, &note);
  QFile gpgId(QDir(dir.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::ReadOnly | QIODevice::Text));
  QCOMPARE(QString::fromUtf8(gpgId.readAll()),
           QStringLiteral("# QtPass-GpgId-Generation: 1\n"
                          "# QtPass-GpgId-Folder: .\n"
                          "AAAA1111AAAA1111AAAA1111AAAA1111AAAA1111\n"));
}

void tst_profileinit::initialiseRefusesWithoutRecipients() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QList<UserInfo> nobody = twoUsers();
  nobody[0].enabled = false;
  QString note;
  QVERIFY(!ProfileInit::initialise(dir.path(), nobody, AppSettings(), false,
                                   &note));
  QVERIFY(!note.isEmpty());
  QVERIFY2(!QFile::exists(QDir(dir.path()).filePath(QStringLiteral(".gpg-id"))),
           "no recipients means no .gpg-id");
}

void tst_profileinit::initialiseCreatesTheDirectory() {
  QTemporaryDir base;
  QVERIFY(base.isValid());
  const QString dir = QDir(base.path()).filePath(QStringLiteral("new/store"));
  QString note;
  QVERIFY2(
      ProfileInit::initialise(dir, twoUsers(), AppSettings(), false, &note),
      qPrintable(note));
  QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral(".gpg-id"))));
}

/**
 * @brief With git requested the folder becomes a repository whose first
 *        commit holds the .gpg-id — all of it blocking, inside that folder.
 */
void tst_profileinit::initialiseCommitsUnderGit() {
  const AppSettings s = gitSettings();
  if (s.gitExecutable.isEmpty())
    QSKIP("git not found in PATH");
  // A fresh repository needs an identity to commit with; do not depend on
  // the machine's git config.
  qputenv("GIT_AUTHOR_NAME", "tst_profileinit");
  qputenv("GIT_AUTHOR_EMAIL", "tst@example.invalid");
  qputenv("GIT_COMMITTER_NAME", "tst_profileinit");
  qputenv("GIT_COMMITTER_EMAIL", "tst@example.invalid");
  qputenv("GIT_CONFIG_GLOBAL", "/dev/null");

  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QString note;
  QVERIFY2(ProfileInit::initialise(dir.path(), twoUsers(), s, true, &note),
           qPrintable(note));

  QProcess git;
  git.setWorkingDirectory(dir.path());
  git.start(s.gitExecutable,
            {QStringLiteral("log"), QStringLiteral("--oneline"),
             QStringLiteral("--name-only")});
  QVERIFY(git.waitForFinished(10000));
  const QString log = QString::fromUtf8(git.readAllStandardOutput());
  QCOMPARE(git.exitCode(), 0);
  QVERIFY2(log.contains(QStringLiteral(".gpg-id")),
           qPrintable("the first commit must contain .gpg-id: " + log));
  QCOMPARE(log.count(QLatin1Char('\n')), 2); // one commit, one file
}

void tst_profileinit::initialiseWarnsAboutExistingEntries() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QFile old(QDir(dir.path()).filePath(QStringLiteral("old.gpg")));
  QVERIFY(old.open(QIODevice::WriteOnly));
  old.write("x");
  old.close();
  QString note;
  QVERIFY2(ProfileInit::initialise(dir.path(), twoUsers(), AppSettings(), false,
                                   &note),
           qPrintable(note));
  QVERIFY2(note.contains(QStringLiteral("not re-encrypted")),
           qPrintable("existing entries must be pointed out: " + note));
}

QTEST_MAIN(tst_profileinit)
#include "tst_profileinit.moc"
