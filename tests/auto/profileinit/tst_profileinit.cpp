// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "../../../src/appsettings.h"
#include "../../../src/profileinit.h"
#include "../../../src/userinfo.h"

class tst_profileinit : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void needsInitFalseForNonexistentPath();
  void needsInitFalseForEmptyPath();
  void needsInitTrueForDirWithoutGpgId();
  void needsInitFalseForDirWithGpgId();
  void needsInitFalseAfterGpgIdAdded();
  void initialiseWritesEnabledKeysOnly();
  void initialiseRefusesWithoutRecipients();
  void initialiseCreatesTheDirectory();
  void initialiseCommitsUnderGit();
  void initialiseWarnsAboutExistingEntries();

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
  QCOMPARE(QString::fromUtf8(gpgId.readAll()),
           QStringLiteral("AAAA1111AAAA1111AAAA1111AAAA1111AAAA1111\n"));
  QVERIFY(!ProfileInit::needsInit(dir.path()));
#ifndef Q_OS_WIN
  const auto perms = QFileInfo(gpgId).permissions();
  QVERIFY2(!(perms & (QFile::ReadGroup | QFile::ReadOther)),
           ".gpg-id must be owner-only");
#endif
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
