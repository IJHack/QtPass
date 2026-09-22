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
#include "../../../src/gpgidgeneration.h"
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
  void initialiseFailsWhenTheDirectoryCannotBeCreated();
  void initialiseRefusesWhenNoGenerationCanBeReserved();
  void initialiseSignsAndCommitsTheSignature();
  void initialiseReportsAFailingGit();
  void initGitStagesOnlyWhatBelongsToTheStore();
  void initGitCommitsAnEmptyStore();
  void initGitReportsAFailingInit();
  void initGitReportsAFailingAdd();
  void gitIdentityConfiguredWithNameAndEmail();
  void gitIdentityConfiguredFalseWithoutAnEmail();
  void gitIdentityConfiguredFalseWhenGitCannotRun();

private:
  static auto twoUsers() -> QList<UserInfo>;
  static auto gitSettings() -> AppSettings;
  static void useTestGitIdentity();
  static auto writeScript(const QString &path, const QByteArray &body) -> bool;
  static auto writeFile(const QString &path, const QByteArray &body) -> bool;
  static auto gitOutput(const AppSettings &s, const QString &dir,
                        const QStringList &args) -> QString;
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

/// A fresh repository needs an identity to commit with; do not depend on
/// the machine's git config. Its global ignore and attributes files are
/// kept out too: a `*.gpg` or `.gpg-id` in ~/.config/git/ignore would make
/// `git add` refuse the very files these tests expect staged.
void tst_profileinit::useTestGitIdentity() {
  qputenv("GIT_AUTHOR_NAME", "tst_profileinit");
  qputenv("GIT_AUTHOR_EMAIL", "tst@example.invalid");
  qputenv("GIT_COMMITTER_NAME", "tst_profileinit");
  qputenv("GIT_COMMITTER_EMAIL", "tst@example.invalid");
  qputenv("GIT_CONFIG_GLOBAL", "/dev/null");
  qputenv("GIT_CONFIG_SYSTEM", "/dev/null");
  qputenv("GIT_CONFIG_COUNT", "2");
  qputenv("GIT_CONFIG_KEY_0", "core.excludesFile");
  qputenv("GIT_CONFIG_VALUE_0", "/dev/null");
  qputenv("GIT_CONFIG_KEY_1", "core.attributesFile");
  qputenv("GIT_CONFIG_VALUE_1", "/dev/null");
}

auto tst_profileinit::writeFile(const QString &path, const QByteArray &body)
    -> bool {
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly)) {
    return false;
  }
  return f.write(body) == body.size();
}

auto tst_profileinit::writeScript(const QString &path, const QByteArray &body)
    -> bool {
  if (!writeFile(path, "#!/bin/sh\n" + body)) {
    return false;
  }
  return QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner |
                                         QFile::ExeOwner);
}

/// stdout of a git command run in @p dir, empty when it fails.
auto tst_profileinit::gitOutput(const AppSettings &s, const QString &dir,
                                const QStringList &args) -> QString {
  QProcess git;
  git.setWorkingDirectory(dir);
  git.start(s.gitExecutable, args);
  if (!git.waitForFinished(10000) || git.exitCode() != 0) {
    return QString();
  }
  return QString::fromUtf8(git.readAllStandardOutput());
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
  const QString gpgIdFile =
      QDir(dir.path()).filePath(QStringLiteral(".gpg-id"));
  QFile gpgId(gpgIdFile);
  QVERIFY(gpgId.open(QIODevice::ReadOnly));
  const QByteArray written = gpgId.readAll();
  QCOMPARE(QString::fromUtf8(written),
           QStringLiteral("# QtPass-GpgId-Generation: 1\n"
                          "# QtPass-GpgId-Folder: .\n"
                          "AAAA1111AAAA1111AAAA1111AAAA1111AAAA1111\n"));
  // The bytes written are the ones this device knows for generation 1:
  // another list of that generation is a conflict, not the same list.
  QCOMPARE(GpgIdGeneration::accept(gpgIdFile, written, dir.path()),
           GpgIdGeneration::Verdict::Accepted);
  QCOMPARE(GpgIdGeneration::accept(
               gpgIdFile,
               GpgIdGeneration::withHeader(1, QStringLiteral("."), "BBBB\n"),
               dir.path()),
           GpgIdGeneration::Verdict::Conflict);
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
  useTestGitIdentity();

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

/**
 * @brief A directory that cannot be made (its parent is a regular file) is
 *        reported as such and nothing else is attempted: no recipients
 *        error, no .gpg-id anywhere.
 */
void tst_profileinit::initialiseFailsWhenTheDirectoryCannotBeCreated() {
  QTemporaryDir base;
  QVERIFY(base.isValid());
  const QString blocker = QDir(base.path()).filePath(QStringLiteral("file"));
  QVERIFY(writeFile(blocker, "not a directory"));
  const QString dir = QDir(blocker).filePath(QStringLiteral("store"));
  QString note = QStringLiteral("stale");
  QVERIFY2(
      !ProfileInit::initialise(dir, twoUsers(), AppSettings(), false, &note),
      "a store cannot be created under a regular file");
  QVERIFY2(note.contains(QStringLiteral("Could not create")) &&
               note.contains(QStringLiteral("store")),
           qPrintable("the note must name the folder: " + note));
  QVERIFY2(QFileInfo(blocker).isFile(), "the blocking file is left alone");
}

/**
 * @brief With a signing key the list needs a generation number reserved
 *        first; when none is left for that path (the record already holds
 *        the highest one), initialise() fails with the record's reason and
 *        writes no .gpg-id, so no unnumbered list can be signed.
 */
void tst_profileinit::initialiseRefusesWhenNoGenerationCanBeReserved() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString gpgIdFile =
      QDir(dir.path()).filePath(QStringLiteral(".gpg-id"));
  QCOMPARE(GpgIdGeneration::reserveNext(gpgIdFile,
                                        GpgIdGeneration::kMaxGeneration - 1),
           std::optional<qint64>(GpgIdGeneration::kMaxGeneration));
  AppSettings s;
  s.passSigningKey = QStringLiteral("AAAA1111AAAA1111AAAA1111AAAA1111AAAA1111");
  s.gpgExecutable = QStringLiteral("/nonexistent/gpg");
  QString note;
  QVERIFY2(!ProfileInit::initialise(dir.path(), twoUsers(), s, false, &note),
           "no generation left means no list");
  QVERIFY2(note.contains(QStringLiteral("highest there is")),
           qPrintable("the record's reason is the note: " + note));
  QVERIFY2(!QFile::exists(gpgIdFile), "no .gpg-id without a generation");
}

/**
 * @brief When gpg signs (a stand-in that writes whatever --output names),
 *        the signature lands next to the list, covers exactly the bytes
 *        written, and the first commit holds both files.
 */
void tst_profileinit::initialiseSignsAndCommitsTheSignature() {
#ifdef Q_OS_WIN
  QSKIP("shell-script stand-in for gpg");
#else
  AppSettings s = gitSettings();
  if (s.gitExecutable.isEmpty())
    QSKIP("git not found in PATH");
  useTestGitIdentity();
  QTemporaryDir base;
  QVERIFY(base.isValid());
  const QString signedLog =
      QDir(base.path()).filePath(QStringLiteral("signed.txt"));
  const QString fakeGpg = QDir(base.path()).filePath(QStringLiteral("gpg"));
  // Remember what gpg was asked to sign, then "sign" it.
  QVERIFY(writeScript(
      fakeGpg,
      QStringLiteral("out=''\nprev=''\n"
                     "for a in \"$@\"; do\n"
                     "  if [ \"$prev\" = --output ]; then out=\"$a\"; fi\n"
                     "  prev=\"$a\"\n"
                     "done\n"
                     "cat > '%1'\n"
                     "printf 'FAKESIG' > \"$out\"\n"
                     "exit 0\n")
          .arg(signedLog)
          .toUtf8()));
  s.gpgExecutable = fakeGpg;
  s.passSigningKey = QStringLiteral("AAAA1111AAAA1111AAAA1111AAAA1111AAAA1111");

  const QString dir = QDir(base.path()).filePath(QStringLiteral("store"));
  QString note;
  QVERIFY2(ProfileInit::initialise(dir, twoUsers(), s, true, &note),
           qPrintable(note));
  QVERIFY2(note.isEmpty(), qPrintable("quiet success expected: " + note));

  QFile sig(QDir(dir).filePath(QStringLiteral(".gpg-id.sig")));
  QVERIFY2(sig.open(QIODevice::ReadOnly), ".gpg-id.sig must be in place");
  QCOMPARE(sig.readAll(), QByteArray("FAKESIG"));
  QFile gpgId(QDir(dir).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::ReadOnly));
  QFile signedBytes(signedLog);
  QVERIFY(signedBytes.open(QIODevice::ReadOnly));
  QCOMPARE(signedBytes.readAll(), gpgId.readAll());

  const QString files =
      gitOutput(s, dir, {QStringLiteral("ls-files")}).trimmed();
  QCOMPARE(
      files.split(QLatin1Char('\n')),
      (QStringList{QStringLiteral(".gpg-id"), QStringLiteral(".gpg-id.sig")}));
  QCOMPARE(gitOutput(s, dir,
                     {QStringLiteral("rev-list"), QStringLiteral("--count"),
                      QStringLiteral("HEAD")})
               .trimmed(),
           QStringLiteral("1"));
#endif
}

/**
 * @brief A git that cannot run makes initialise() fail after the .gpg-id is
 *        written, with a note naming the git command and the folder.
 */
void tst_profileinit::initialiseReportsAFailingGit() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  AppSettings s;
  s.gitExecutable = QStringLiteral("/nonexistent/git");
  QString note;
  QVERIFY2(!ProfileInit::initialise(dir.path(), twoUsers(), s, true, &note),
           "no repository means failure when one was asked for");
  QVERIFY2(note.contains(QStringLiteral("git init failed in ")) &&
               note.contains(dir.path()),
           qPrintable("the note must name the command and folder: " + note));
  QVERIFY2(QFile::exists(QDir(dir.path()).filePath(QStringLiteral(".gpg-id"))),
           "the recipients were written before git was tried");
}

/**
 * @brief initGit() stages the .gpg entries (in subfolders too) and the
 *        .gpg-id files, but not the other plaintext lying around, not
 *        links, and nothing under hidden directories; one commit results.
 */
void tst_profileinit::initGitStagesOnlyWhatBelongsToTheStore() {
  const AppSettings s = gitSettings();
  if (s.gitExecutable.isEmpty())
    QSKIP("git not found in PATH");
  useTestGitIdentity();
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QDir base(dir.path());
  QVERIFY(base.mkpath(QStringLiteral("sub/deeper")));
  QVERIFY(base.mkpath(QStringLiteral(".hidden")));
  QVERIFY(writeFile(base.filePath(QStringLiteral(".gpg-id")), "AAAA\n"));
  QVERIFY(writeFile(base.filePath(QStringLiteral(".gpg-id.sig")), "sig"));
  QVERIFY(writeFile(base.filePath(QStringLiteral("top.gpg")), "x"));
  QVERIFY(writeFile(base.filePath(QStringLiteral("sub/deeper/low.gpg")), "x"));
  QVERIFY(writeFile(base.filePath(QStringLiteral("notes.txt")), "plain"));
  QVERIFY(writeFile(base.filePath(QStringLiteral("sub/export.csv")), "plain"));
  QVERIFY(writeFile(base.filePath(QStringLiteral(".hidden/secret.gpg")), "x"));
#ifndef Q_OS_WIN
  QVERIFY(QFile::link(base.filePath(QStringLiteral("notes.txt")),
                      base.filePath(QStringLiteral("link.gpg"))));
#endif

  QString note;
  QVERIFY2(ProfileInit::initGit(dir.path(), s, &note), qPrintable(note));

  QStringList staged = gitOutput(s, dir.path(), {QStringLiteral("ls-files")})
                           .trimmed()
                           .split(QLatin1Char('\n'));
  staged.sort();
  QCOMPARE(staged, (QStringList{QStringLiteral(".gpg-id"),
                                QStringLiteral(".gpg-id.sig"),
                                QStringLiteral("sub/deeper/low.gpg"),
                                QStringLiteral("top.gpg")}));
  const QString log = gitOutput(
      s, dir.path(), {QStringLiteral("log"), QStringLiteral("--format=%s")});
  QCOMPARE(log.trimmed(), QStringLiteral("Added password store using QtPass."));
  QVERIFY2(QFile::exists(base.filePath(QStringLiteral("notes.txt"))),
           "nothing is removed from the folder");
}

/**
 * @brief A folder with nothing to stage still becomes a repository with a
 *        first (empty) commit, so a later push has a history to send.
 */
void tst_profileinit::initGitCommitsAnEmptyStore() {
  const AppSettings s = gitSettings();
  if (s.gitExecutable.isEmpty())
    QSKIP("git not found in PATH");
  useTestGitIdentity();
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QString note;
  QVERIFY2(ProfileInit::initGit(dir.path(), s, &note), qPrintable(note));
  QVERIFY2(QDir(dir.path()).exists(QStringLiteral(".git")),
           "git init must have run");
  QCOMPARE(gitOutput(s, dir.path(),
                     {QStringLiteral("rev-list"), QStringLiteral("--count"),
                      QStringLiteral("HEAD")})
               .trimmed(),
           QStringLiteral("1"));
  QVERIFY2(gitOutput(s, dir.path(), {QStringLiteral("ls-files")})
               .trimmed()
               .isEmpty(),
           "nothing was there to stage");
}

/**
 * @brief When git itself cannot start, initGit() fails at `init` with a
 *        note naming the command and the folder, and no repository appears.
 */
void tst_profileinit::initGitReportsAFailingInit() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  AppSettings s;
  s.gitExecutable = QStringLiteral("/nonexistent/git");
  QString note;
  QVERIFY2(!ProfileInit::initGit(dir.path(), s, &note),
           "a git that does not run cannot make a repository");
  QVERIFY2(note.contains(QStringLiteral("git init failed in ")) &&
               note.contains(dir.path()),
           qPrintable("the note must name the command and folder: " + note));
  QVERIFY2(!QDir(dir.path()).exists(QStringLiteral(".git")),
           "no repository without git");
}

/**
 * @brief A failing `git add` (a stand-in that refuses it, and hands
 *        everything else to the real git) stops initGit() before the commit,
 *        with the stand-in's stderr in the note.
 */
void tst_profileinit::initGitReportsAFailingAdd() {
#ifdef Q_OS_WIN
  QSKIP("shell-script stand-in for git");
#else
  AppSettings s = gitSettings();
  if (s.gitExecutable.isEmpty())
    QSKIP("git not found in PATH");
  useTestGitIdentity();
  QTemporaryDir base;
  QVERIFY(base.isValid());
  const QString fakeGit = QDir(base.path()).filePath(QStringLiteral("git"));
  QVERIFY(writeScript(fakeGit, QStringLiteral("if [ \"$1\" = add ]; then\n"
                                              "  echo 'index locked' >&2\n"
                                              "  exit 128\n"
                                              "fi\n"
                                              "exec '%1' \"$@\"\n")
                                   .arg(s.gitExecutable)
                                   .toUtf8()));
  const AppSettings real = s;
  s.gitExecutable = fakeGit;
  const QString dir = QDir(base.path()).filePath(QStringLiteral("store"));
  QVERIFY(QDir().mkpath(dir));
  QVERIFY(writeFile(QDir(dir).filePath(QStringLiteral(".gpg-id")), "AAAA\n"));

  QString note;
  QVERIFY2(!ProfileInit::initGit(dir, s, &note),
           "a refused add must fail the whole thing");
  QVERIFY2(note.contains(QStringLiteral("git add failed in ")) &&
               note.contains(QStringLiteral("index locked")),
           qPrintable("the note must carry git's stderr: " + note));
  QVERIFY2(QDir(dir).exists(QStringLiteral(".git")), "init had already run");
  QVERIFY2(gitOutput(real, dir,
                     {QStringLiteral("rev-list"), QStringLiteral("--count"),
                      QStringLiteral("HEAD")})
               .isEmpty(),
           "no commit was made");
#endif
}

/**
 * @brief A repository whose own config names both user.name and user.email
 *        has an identity to commit with: gitIdentityConfigured() says so,
 *        with the machine's global and system config kept out of it.
 */
void tst_profileinit::gitIdentityConfiguredWithNameAndEmail() {
  const AppSettings s = gitSettings();
  if (s.gitExecutable.isEmpty())
    QSKIP("git not found in PATH");
  useTestGitIdentity();
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QProcess init;
  init.setWorkingDirectory(dir.path());
  init.start(s.gitExecutable, {QStringLiteral("init"), QStringLiteral("-q")});
  QVERIFY(init.waitForFinished(10000));
  QCOMPARE(init.exitCode(), 0);
  // Inside the fresh repository, so a checkout above the temp folder cannot
  // lend it an identity: its empty config plus the nulled global and system
  // ones is no identity at all.
  QVERIFY2(!ProfileInit::gitIdentityConfigured(dir.path(), s),
           "no config anywhere means no identity");
  QProcess name;
  name.setWorkingDirectory(dir.path());
  name.start(s.gitExecutable,
             {QStringLiteral("config"), QStringLiteral("user.name"),
              QStringLiteral("tst_profileinit")});
  QVERIFY(name.waitForFinished(10000));
  QCOMPARE(name.exitCode(), 0);
  QProcess email;
  email.setWorkingDirectory(dir.path());
  email.start(s.gitExecutable,
              {QStringLiteral("config"), QStringLiteral("user.email"),
               QStringLiteral("tst@example.invalid")});
  QVERIFY(email.waitForFinished(10000));
  QCOMPARE(email.exitCode(), 0);
  QVERIFY2(ProfileInit::gitIdentityConfigured(dir.path(), s),
           "user.name and user.email in the repository's config count");
}

/**
 * @brief Half an identity is none: a repository with only user.name (the
 *        e-mail missing, as on a fresh machine) makes gitIdentityConfigured()
 *        false, so the wizard can warn before a commit would fail.
 */
void tst_profileinit::gitIdentityConfiguredFalseWithoutAnEmail() {
  const AppSettings s = gitSettings();
  if (s.gitExecutable.isEmpty())
    QSKIP("git not found in PATH");
  useTestGitIdentity();
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QProcess init;
  init.setWorkingDirectory(dir.path());
  init.start(s.gitExecutable, {QStringLiteral("init"), QStringLiteral("-q")});
  QVERIFY(init.waitForFinished(10000));
  QCOMPARE(init.exitCode(), 0);
  QProcess name;
  name.setWorkingDirectory(dir.path());
  name.start(s.gitExecutable,
             {QStringLiteral("config"), QStringLiteral("user.name"),
              QStringLiteral("tst_profileinit")});
  QVERIFY(name.waitForFinished(10000));
  QCOMPARE(name.exitCode(), 0);
  QCOMPARE(gitOutput(s, dir.path(),
                     {QStringLiteral("config"), QStringLiteral("--get"),
                      QStringLiteral("user.name")})
               .trimmed(),
           QStringLiteral("tst_profileinit"));
  QVERIFY2(!ProfileInit::gitIdentityConfigured(dir.path(), s),
           "a name without an e-mail is not an identity git commits with");
}

/**
 * @brief A git that cannot be started answers nothing, which counts as no
 *        identity rather than crashing or claiming one.
 */
void tst_profileinit::gitIdentityConfiguredFalseWhenGitCannotRun() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  AppSettings s;
  s.gitExecutable = QStringLiteral("/nonexistent/git");
  QVERIFY2(!ProfileInit::gitIdentityConfigured(dir.path(), s),
           "no git, no identity");
}

QTEST_MAIN(tst_profileinit)
#include "tst_profileinit.moc"
