// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "../../../src/appsettings.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/realpass.h"
#include "../../../src/userinfo.h"
#include "../testpass.h"
#include "../testsettings.h"

/**
 * @brief The argv RealPass hands to `pass`, checked against a stand-in pass
 *        script that records its arguments, stdin, working directory and
 *        PASSWORD_STORE_DIR. These are the destructive backend commands for
 *        every pass user and had no assertions at all.
 */
class tst_realpass : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void gitCommands();
  void showAndGrep();
  void grepDecryptsRealEntriesOnlyNeverThroughALink();
  void insertPipesTheValue();
  void removeFileAndFolder();
  void initWritesEnabledKeysRelativeToTheStore();
  void loggedArgvRedactsSecretShapedValues();
  void genericOutputSignalIsAnAllowList();
  void moveAndCopyUseStoreRelativeNamesWithoutGpg();
  void moveBetweenExistingFilesNeedsForce();
  void linkedEntriesAndFoldersAreRefusedBeforePassRuns();
  void gitPullBlockingReturnsAfterPassRanWhateverItsExitCode();
  void linkedFolderThatCannotBeUnlinkedReportsDeleteFailed();
  void linkedFolderKnownToGitIsForgottenByGitAlone();

private:
  struct Call {
    QStringList args;
    QString stdinData;
    QString cwd;
    QString storeEnv;
  };
  auto waitForCall() -> Call;
  auto makePass() -> RealPass *;

  QTemporaryDir m_dir;
  QString m_store;
  QString m_log;
  AppSettings m_settings;
};

void tst_realpass::initTestCase() {
  isolateTestSettings();
#ifdef Q_OS_WIN
  QSKIP("the stand-in pass is a shell script");
#endif
  QVERIFY(m_dir.isValid());
  m_store =
      QDir(m_dir.path()).filePath(QStringLiteral("store")) + QLatin1Char('/');
  QVERIFY(QDir().mkpath(m_store + QStringLiteral("folder")));
  m_log = QDir(m_dir.path()).filePath(QStringLiteral("call.log"));

  const QString script = QDir(m_dir.path()).filePath(QStringLiteral("pass"));
  QFile f(script);
  QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
  // One record per invocation; written to a temp name and renamed so the
  // test never reads a half-written log.
  f.write("#!/bin/sh\n");
  f.write(QStringLiteral("LOG='%1'\n").arg(m_log).toUtf8());
  f.write("{\n  printf 'ARGS\\n'; for a in \"$@\"; do printf '%s\\n' \"$a\"; "
          "done\n");
  f.write("  printf 'STDIN\\n'; cat\n  printf '\\nCWD\\n'; pwd\n");
  f.write(
      "  printf 'STORE\\n%s\\n' \"$PASSWORD_STORE_DIR\"\n} > \"$LOG.tmp\"\n");
  f.write("mv \"$LOG.tmp\" \"$LOG\"\nexit 0\n");
  f.close();
  QVERIFY(
      f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

  m_settings = QtPassSettings::load();
  m_settings.passExecutable = script;
  m_settings.passStore = m_store;
  m_settings.usePass = true;
  QtPassSettings::save(m_settings);
}

void tst_realpass::init() { QFile::remove(m_log); }

auto tst_realpass::makePass() -> RealPass * {
  auto *pass = new RealPass;
  pass->init(m_settings);
  pass->updateEnv();
  return pass;
}

// QTRY_* macros return from a void function; this variant returns a value.
auto tst_realpass::waitForCall() -> Call {
  Call call;
  QTRY_VERIFY_WITH_TIMEOUT_RETURN(QFile::exists(m_log), 5000, call);
  QFile f(m_log);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return call;
  }
  const QString text = QString::fromUtf8(f.readAll());
  const QString args = text.section(QStringLiteral("ARGS\n"), 1)
                           .section(QStringLiteral("STDIN\n"), 0, 0);
  call.args = args.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  call.stdinData = text.section(QStringLiteral("STDIN\n"), 1)
                       .section(QStringLiteral("\nCWD\n"), 0, 0);
  call.cwd = text.section(QStringLiteral("CWD\n"), 1)
                 .section(QStringLiteral("STORE\n"), 0, 0)
                 .trimmed();
  call.storeEnv = text.section(QStringLiteral("STORE\n"), 1).trimmed();
  return call;
}

void tst_realpass::gitCommands() {
  QScopedPointer<RealPass> pass(makePass());
  pass->GitInit();
  Call call = waitForCall();
  QCOMPARE(call.args,
           (QStringList{QStringLiteral("git"), QStringLiteral("init")}));
  QCOMPARE(QDir(call.cwd).canonicalPath(), QDir(m_store).canonicalPath());
  QCOMPARE(QDir(call.storeEnv).canonicalPath(), QDir(m_store).canonicalPath());

  QFile::remove(m_log);
  pass->GitPull();
  QCOMPARE(waitForCall().args,
           (QStringList{QStringLiteral("git"), QStringLiteral("pull")}));
  QFile::remove(m_log);
  pass->GitPush();
  QCOMPARE(waitForCall().args,
           (QStringList{QStringLiteral("git"), QStringLiteral("push")}));
}

void tst_realpass::showAndGrep() {
  QScopedPointer<RealPass> pass(makePass());
  pass->Show(QStringLiteral("folder/entry"));
  QCOMPARE(waitForCall().args, (QStringList{QStringLiteral("show"),
                                            QStringLiteral("folder/entry")}));
  QFile::remove(m_log);
  // Search does not go through pass any more (its `find -L` follows links);
  // with no GPG executable it answers empty instead.
  AppSettings noGpg = m_settings;
  noGpg.gpgExecutable.clear();
  pass->init(noGpg);
  QSignalSpy grepSpy(pass.data(), &Pass::finishedGrep);
  pass->Grep(QStringLiteral("-needle"), true);
  QCOMPARE(grepSpy.count(), 1);
  QTest::qWait(200);
  QVERIFY2(!QFile::exists(m_log), "pass grep must not run");
  pass->init(m_settings);
}

/**
 * @brief The pass backend's search decrypts the store's real .gpg files with
 *        gpg and nothing behind a link: `pass grep` runs `find -L`, which
 *        would have followed Bank.gpg -> outside and searched a file that is
 *        not the store's.
 */
void tst_realpass::grepDecryptsRealEntriesOnlyNeverThroughALink() {
  QTemporaryDir outsideDir;
  QVERIFY(outsideDir.isValid());
  const QString outsideSecret =
      QDir(outsideDir.path()).filePath(QStringLiteral("secret.gpg"));
  for (const QString &path :
       {m_store + QStringLiteral("folder/real.gpg"), outsideSecret}) {
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("ciphertext");
  }
  const QString bank = m_store + QStringLiteral("Bank.gpg");
  QVERIFY(QFile::link(outsideSecret, bank));
  // A gpg that "decrypts" by echoing which file it was given.
  const QString gpgLog = QDir(m_dir.path()).filePath(QStringLiteral("gpg.log"));
  const QString fakeGpg = QDir(m_dir.path()).filePath(QStringLiteral("gpg"));
  {
    QFile f(fakeGpg);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("#!/bin/sh\n");
    f.write(QStringLiteral("for a in \"$@\"; do last=\"$a\"; done\n"
                           "printf '%s\\n' \"$last\" >> '%1'\n"
                           "printf 'needle in %s\\n' \"$last\"\n")
                .arg(gpgLog)
                .toUtf8());
    f.close();
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                             QFile::ExeOwner));
  }
  const auto cleanup = qScopeGuard([&] {
    QFile::remove(bank);
    QFile::remove(m_store + QStringLiteral("folder/real.gpg"));
    QFile::remove(gpgLog);
    QFile::remove(fakeGpg);
  });
  AppSettings withGpg = m_settings;
  withGpg.gpgExecutable = fakeGpg;
  QScopedPointer<RealPass> pass(makePass());
  pass->init(withGpg);
  QSignalSpy grepSpy(pass.data(), &Pass::finishedGrep);
  pass->Grep(QStringLiteral("needle"), false);
  QVERIFY(grepSpy.count() > 0 || grepSpy.wait(10000));
  const auto results =
      grepSpy.takeFirst().at(0).value<QList<QPair<QString, QStringList>>>();
  QStringList entries;
  for (const auto &r : results) {
    entries << r.first;
  }
  QCOMPARE(entries, QStringList{QStringLiteral("folder/real")});
  QFile log(gpgLog);
  QVERIFY(log.open(QIODevice::ReadOnly));
  const QString decrypted = QString::fromUtf8(log.readAll());
  QVERIFY2(!decrypted.contains(QStringLiteral("Bank.gpg")) &&
               !decrypted.contains(outsideSecret),
           qPrintable("gpg was handed: " + decrypted));
  QVERIFY2(!QFile::exists(m_log), "pass itself is not involved in a search");
}

void tst_realpass::insertPipesTheValue() {
  QScopedPointer<RealPass> pass(makePass());
  pass->Insert(QStringLiteral("folder/new"),
               QStringLiteral("hunter2\nuser: me\n"), false);
  Call call = waitForCall();
  QCOMPARE(call.args,
           (QStringList{QStringLiteral("insert"), QStringLiteral("-m"),
                        QStringLiteral("folder/new")}));
  QCOMPARE(call.stdinData, QStringLiteral("hunter2\nuser: me\n"));

  QFile::remove(m_log);
  pass->Insert(QStringLiteral("folder/new"), QStringLiteral("x"), true);
  QCOMPARE(waitForCall().args,
           (QStringList{QStringLiteral("insert"), QStringLiteral("-m"),
                        QStringLiteral("-f"), QStringLiteral("folder/new")}));
}

void tst_realpass::removeFileAndFolder() {
  QScopedPointer<RealPass> pass(makePass());
  pass->Remove(QStringLiteral("folder/entry"), false);
  QCOMPARE(waitForCall().args,
           (QStringList{QStringLiteral("rm"), QStringLiteral("-f"),
                        QStringLiteral("folder/entry")}));
  QFile::remove(m_log);
  pass->Remove(QStringLiteral("folder"), true);
  QCOMPARE(waitForCall().args,
           (QStringList{QStringLiteral("rm"), QStringLiteral("-rf"),
                        QStringLiteral("folder")}));
}

void tst_realpass::initWritesEnabledKeysRelativeToTheStore() {
  QScopedPointer<RealPass> pass(makePass());
  UserInfo alice;
  alice.key_id = QStringLiteral("AAAA");
  alice.enabled = true;
  UserInfo bob;
  bob.key_id = QStringLiteral("BBBB");
  UserInfo carol;
  carol.key_id = QStringLiteral("CCCC");
  carol.enabled = true;
  pass->Init(m_store + QStringLiteral("folder/"), {alice, bob, carol});
  QCOMPARE(waitForCall().args,
           (QStringList{QStringLiteral("init"), QStringLiteral("--path=folder"),
                        QStringLiteral("AAAA"), QStringLiteral("CCCC")}));
  QFile::remove(m_log);
  pass->Init(m_store, {alice});
  QCOMPARE(waitForCall().args,
           (QStringList{QStringLiteral("init"), QStringLiteral("--path="),
                        QStringLiteral("AAAA")}));
  // A directory whose name merely starts with the store's is not inside it;
  // the old prefix test turned /store-other into --path=-other.
  QFile::remove(m_log);
  const QString sibling = QDir::cleanPath(m_store) + QStringLiteral("-other");
  QVERIFY(QDir().mkpath(sibling));
  pass->Init(sibling, {alice});
  QCOMPARE(waitForCall().args, (QStringList{QStringLiteral("init"),
                                            QStringLiteral("--path=") + sibling,
                                            QStringLiteral("AAAA")}));
}

/**
 * @brief The debug log shows argv, minus anything shaped like a secret.
 */
void tst_realpass::loggedArgvRedactsSecretShapedValues() {
  QCOMPARE(Pass::loggableArgs({QStringLiteral("--batch"), QStringLiteral("-r"),
                               QStringLiteral("ABCD")}),
           (QStringList{QStringLiteral("--batch"), QStringLiteral("-r"),
                        QStringLiteral("ABCD")}));
  QCOMPARE(
      Pass::loggableArgs({QStringLiteral("--passphrase"),
                          QStringLiteral("hunter2"), QStringLiteral("-d")}),
      (QStringList{QStringLiteral("--passphrase"), QStringLiteral("<redacted>"),
                   QStringLiteral("-d")}));
  QCOMPARE(Pass::loggableArgs({QStringLiteral("--passphrase=hunter2")}),
           QStringList{QStringLiteral("--passphrase=<redacted>")});
  QCOMPARE(Pass::loggableArgs({QStringLiteral("otpauth://totp/x?secret=S")}),
           QStringList{QStringLiteral("<redacted>")});
}

/**
 * @brief Only process kinds whose output cannot hold a secret reach the
 *        generic finishedAnyWithPid listeners; the decrypt, grep and insert
 *        kinds never do, and a kind nobody has classified stays silent.
 */
void tst_realpass::genericOutputSignalIsAnAllowList() {
  QScopedPointer<RealPass> pass(makePass());
  QSignalSpy any(pass.data(), &Pass::finishedAnyWithPid);
  const auto deliver = [&](Enums::PROCESS pid) -> qsizetype {
    any.clear();
    if (!QMetaObject::invokeMethod(
            pass.data(), "finished", Qt::DirectConnection, Q_ARG(int, pid),
            Q_ARG(int, 0), Q_ARG(QString, QStringLiteral("secret")),
            Q_ARG(QString, QString()))) {
      return -1;
    }
    return any.count();
  };
  QCOMPARE(deliver(Enums::PASS_SHOW), 0);
  QCOMPARE(deliver(Enums::PASS_GREP), 0);
  QCOMPARE(deliver(Enums::PASS_INSERT), 0);
  QCOMPARE(deliver(Enums::INVALID), 0);
  QCOMPARE(deliver(Enums::PROCESS_COUNT), 0);
  QCOMPARE(deliver(Enums::GIT_PUSH), 1);
  QCOMPARE(deliver(Enums::PASS_REMOVE), 1);
  QCOMPARE(deliver(Enums::GPG_GENKEYS), 1);
}

void tst_realpass::moveAndCopyUseStoreRelativeNamesWithoutGpg() {
  QScopedPointer<RealPass> pass(makePass());
  QFile src(m_store + QStringLiteral("folder/a.gpg"));
  QVERIFY(src.open(QIODevice::WriteOnly));
  src.close();
  pass->Move(m_store + QStringLiteral("folder/a.gpg"),
             m_store + QStringLiteral("b.gpg"), true);
  QCOMPARE(waitForCall().args,
           (QStringList{QStringLiteral("mv"), QStringLiteral("-f"),
                        QStringLiteral("folder/a"), QStringLiteral("b")}));
  QFile::remove(m_log);
  pass->Copy(m_store + QStringLiteral("folder"),
             m_store + QStringLiteral("copy"), false);
  QCOMPARE(waitForCall().args,
           (QStringList{QStringLiteral("cp"), QStringLiteral("folder"),
                        QStringLiteral("copy")}));
}

void tst_realpass::moveBetweenExistingFilesNeedsForce() {
  QScopedPointer<RealPass> pass(makePass());
  for (const char *name : {"folder/x.gpg", "folder/y.gpg"}) {
    QFile f(m_store + QString::fromLatin1(name));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.close();
  }
  pass->Move(m_store + QStringLiteral("folder/x.gpg"),
             m_store + QStringLiteral("folder/y.gpg"), false);
  QTest::qWait(300);
  QVERIFY2(!QFile::exists(m_log),
           "moving onto an existing file without force must not call pass");
  pass->Move(m_store + QStringLiteral("folder/x.gpg"),
             m_store + QStringLiteral("folder/y.gpg"), true);
  QCOMPARE(
      waitForCall().args,
      (QStringList{QStringLiteral("mv"), QStringLiteral("-f"),
                   QStringLiteral("folder/x"), QStringLiteral("folder/y")}));
}

/**
 * @brief pass follows links as readily as gpg does, so the guard sits in
 *        front of it: show, insert, mv, cp and init on a linked entry or a
 *        folder behind a link never reach the stand-in; rm of the link itself
 *        does, rm of something behind one does not.
 */
void tst_realpass::linkedEntriesAndFoldersAreRefusedBeforePassRuns() {
  QTemporaryDir outsideDir;
  QVERIFY(outsideDir.isValid());
  QVERIFY(QDir(outsideDir.path()).mkpath(QStringLiteral("sub")));
  {
    QFile f(QDir(outsideDir.path()).filePath(QStringLiteral("secret.gpg")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
  }
  const QString bank = m_store + QStringLiteral("Bank.gpg");
  const QString shared = m_store + QStringLiteral("shared");
  QVERIFY(QFile::link(
      QDir(outsideDir.path()).filePath(QStringLiteral("secret.gpg")), bank));
  QVERIFY(QFile::link(outsideDir.path(), shared));
  const auto cleanup = qScopeGuard([&] {
    QFile::remove(bank);
    QFile::remove(shared);
  });
  QScopedPointer<RealPass> pass(makePass());
  QSignalSpy criticalSpy(pass.data(), &Pass::critical);
  pass->Show(QStringLiteral("Bank"));
  pass->Show(QStringLiteral("shared/sub/deep"));
  pass->Insert(QStringLiteral("Bank"), QStringLiteral("v"), true);
  pass->Insert(QStringLiteral("shared/fresh"), QStringLiteral("v"), false);
  pass->Move(bank, m_store + QStringLiteral("Moved.gpg"), false);
  pass->Move(m_store + QStringLiteral("folder/entry.gpg"), shared, false);
  pass->Copy(bank, m_store + QStringLiteral("Copied.gpg"), false);
  pass->Remove(QStringLiteral("shared/sub/deep"), false);
  UserInfo alice;
  alice.key_id = QStringLiteral("AAAA");
  alice.enabled = true;
  pass->Init(shared + QLatin1Char('/'), {alice});
  // A real folder with a link under the .gpg-id name: pass init would write
  // through it.
  QVERIFY(QDir(m_store).mkpath(QStringLiteral("team")));
  const QString plantedId = m_store + QStringLiteral("team/.gpg-id");
  QVERIFY(QFile::link(
      QDir(outsideDir.path()).filePath(QStringLiteral("secret.gpg")),
      plantedId));
  pass->Init(m_store + QStringLiteral("team/"), {alice});
  // A drop copies onto the folder; pass cp writes <folder>/<name>, and a
  // link planted there is what cp would write through.
  const QString plantedEntry = m_store + QStringLiteral("team/entry.gpg");
  QVERIFY(QFile::link(
      QDir(outsideDir.path()).filePath(QStringLiteral("secret.gpg")),
      plantedEntry));
  pass->Copy(m_store + QStringLiteral("folder/entry.gpg"),
             m_store + QStringLiteral("team"), false);
  QTest::qWait(300);
  QCOMPARE(criticalSpy.count(), 11);
  QVERIFY2(!QFile::exists(m_log), "the stand-in pass must not have run");
  QVERIFY(QFile::remove(plantedId) && QFile::remove(plantedEntry));
  QVERIFY(QDir(m_store).rmdir(QStringLiteral("team")));
  // A linked folder is unlinked here, never handed to pass rm (which would
  // rm -rf "<link>/" and empty the target). With git on, pass is asked
  // whether git knew the link; the stand-in prints nothing, so it did not,
  // and no rm --cached or commit follows.
  AppSettings withGit = m_settings;
  withGit.useGit = true;
  pass->init(withGit);
  QSignalSpy removedSpy(pass.data(), &Pass::finishedRemove);
  pass->Remove(QStringLiteral("shared/"), true);
  QCOMPARE(waitForCall().args,
           (QStringList{QStringLiteral("git"), QStringLiteral("ls-files"),
                        QStringLiteral("--"), QStringLiteral("shared")}));
  QTest::qWait(300);
  QVERIFY(!QFileInfo(shared).isSymLink());
  QVERIFY2(removedSpy.count() == 1,
           "a removal done locally still finishes like one pass did");
  QVERIFY(QFile::exists(
      QDir(outsideDir.path()).filePath(QStringLiteral("secret.gpg"))));
  QCOMPARE(criticalSpy.count(), 11);
  pass->init(m_settings);
  QFile::remove(m_log);
  // Unlinking the link itself is a store operation.
  pass->Remove(QStringLiteral("Bank"), false);
  QCOMPARE(waitForCall().args,
           (QStringList{QStringLiteral("rm"), QStringLiteral("-f"),
                        QStringLiteral("Bank")}));
  QCOMPARE(criticalSpy.count(), 11);
}

/**
 * @brief GitPull_b blocks: when it returns, the stand-in has already run
 *        `git pull` (the log exists without waiting), and a non-zero exit
 *        is explained in the debug log only, never turned into a signal or
 *        a second attempt.
 */
void tst_realpass::gitPullBlockingReturnsAfterPassRanWhateverItsExitCode() {
  QScopedPointer<RealPass> pass(makePass());
  QSignalSpy criticalSpy(pass.data(), &Pass::critical);
  QSignalSpy errorSpy(pass.data(), &Pass::processErrorExit);
  pass->GitPull_b();
  QVERIFY2(QFile::exists(m_log),
           "GitPull_b must not return before the stand-in pass finished");
  QCOMPARE(waitForCall().args,
           (QStringList{QStringLiteral("git"), QStringLiteral("pull")}));

  // A pass that fails: counts its run, then the same stand-in, then exit 3.
  QFile::remove(m_log);
  const QString attempts =
      QDir(m_dir.path()).filePath(QStringLiteral("attempts.log"));
  const QString failing =
      QDir(m_dir.path()).filePath(QStringLiteral("pass-fail"));
  {
    QFile f(failing);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("#!/bin/sh\n");
    f.write(QStringLiteral("printf 'run\\n' >> '%1'\n'%2' \"$@\"\nexit 3\n")
                .arg(attempts, m_settings.passExecutable)
                .toUtf8());
    f.close();
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                             QFile::ExeOwner));
  }
  const auto cleanup = qScopeGuard([&] {
    QLoggingCategory::setFilterRules(QStringLiteral("qtpass.debug=false"));
    QFile::remove(failing);
    QFile::remove(attempts);
  });
  AppSettings failingPass = m_settings;
  failingPass.passExecutable = failing;
  pass->init(failingPass);
  // The exit code goes to the debug log; enable the category so the message
  // is emitted and checked (the guard restores the default, info and up).
  QLoggingCategory::setFilterRules(QStringLiteral("qtpass.debug=true"));
  QTest::ignoreMessage(QtDebugMsg, "Git pull failed with code: 3");
  pass->GitPull_b();
  QLoggingCategory::setFilterRules(QStringLiteral("qtpass.debug=false"));
  QVERIFY2(QFile::exists(m_log), "the failing pass must still have run");
  QCOMPARE(waitForCall().args,
           (QStringList{QStringLiteral("git"), QStringLiteral("pull")}));
  QFile runs(attempts);
  QVERIFY2(runs.open(QIODevice::ReadOnly | QIODevice::Text),
           "the failing pass must have counted its run");
  QCOMPARE(QString::fromUtf8(runs.readAll()), QStringLiteral("run\n"));
  QTest::qWait(100);
  QCOMPARE(criticalSpy.count(), 0);
  QCOMPARE(errorSpy.count(), 0);
  pass->init(m_settings);
}

/**
 * @brief A linked folder is unlinked here rather than handed to pass rm; when
 *        that unlink fails (the parent is read-only) the failure is reported
 *        as a delete error naming the link, nothing finishes as removed and
 *        pass is never asked anything.
 */
void tst_realpass::linkedFolderThatCannotBeUnlinkedReportsDeleteFailed() {
  QTemporaryDir outsideDir;
  QVERIFY(outsideDir.isValid());
  const QString locked = m_store + QStringLiteral("locked");
  QVERIFY(QDir().mkpath(locked));
  const QString shared = locked + QStringLiteral("/shared");
  QVERIFY(QFile::link(outsideDir.path(), shared));
  const QFile::Permissions writable = QFile::permissions(locked);
  QVERIFY(QFile::setPermissions(locked, QFile::ReadOwner | QFile::ExeOwner));
  const auto cleanup = qScopeGuard([&] {
    QFile::setPermissions(locked, writable);
    QFile::remove(shared);
    QDir(m_store).rmdir(QStringLiteral("locked"));
  });
  // Unlinking needs write access to the parent, as creating does: where a
  // probe can still be created (root, a file system without permission bits)
  // the failure under test cannot happen.
  {
    QFile probe(locked + QStringLiteral("/probe"));
    if (probe.open(QIODevice::WriteOnly)) {
      probe.close();
      QFile::remove(probe.fileName());
      QSKIP("the read-only directory is still writable here");
    }
  }
  AppSettings withGit = m_settings;
  withGit.useGit = true;
  QScopedPointer<RealPass> pass(makePass());
  pass->init(withGit);
  QSignalSpy criticalSpy(pass.data(), &Pass::critical);
  QSignalSpy errorSpy(pass.data(), &Pass::processErrorExit);
  QSignalSpy removedSpy(pass.data(), &Pass::finishedRemove);
  pass->Remove(QStringLiteral("locked/shared"), true);
  QTest::qWait(300);
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(criticalSpy.first().at(0).toString(),
           QStringLiteral("Delete failed"));
  QVERIFY2(
      criticalSpy.first().at(1).toString().contains(QDir::cleanPath(shared)),
      qPrintable("message does not name the link: " +
                 criticalSpy.first().at(1).toString()));
  QCOMPARE(errorSpy.count(), 1);
  QCOMPARE(errorSpy.first().at(0).toInt(), 1);
  QCOMPARE(errorSpy.first().at(1).toString(),
           criticalSpy.first().at(1).toString());
  QCOMPARE(removedSpy.count(), 0);
  QVERIFY2(QFileInfo(shared).isSymLink(), "the link must still be there");
  QVERIFY2(!QFile::exists(m_log), "pass must not have been asked anything");
}

/**
 * @brief When git knew the linked folder, unlinking it locally is followed
 *        by `git rm --cached` and one `git commit` for it through pass, each
 *        run with the store's PASSWORD_STORE_DIR, and that commit is the
 *        PASS_REMOVE that finishes (finishedRemove carries its output, not
 *        the empty local one); the link's target is untouched.
 */
void tst_realpass::linkedFolderKnownToGitIsForgottenByGitAlone() {
  QTemporaryDir outsideDir;
  QVERIFY(outsideDir.isValid());
  const QString marker =
      QDir(outsideDir.path()).filePath(QStringLiteral("keep.gpg"));
  {
    QFile f(marker);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
  }
  const QString shared = m_store + QStringLiteral("shared");
  QVERIFY(QFile::link(outsideDir.path(), shared));
  // A stand-in that appends every invocation (its PASSWORD_STORE_DIR first,
  // then argv), answers ls-files with the path so git "tracks" it, and says
  // so on stdout when it commits.
  const QString calls =
      QDir(m_dir.path()).filePath(QStringLiteral("calls.log"));
  const QString gitPass =
      QDir(m_dir.path()).filePath(QStringLiteral("pass-git"));
  {
    QFile f(gitPass);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("#!/bin/sh\n");
    f.write(QStringLiteral("{ printf 'CALL\\nSTORE=%s\\n' "
                           "\"$PASSWORD_STORE_DIR\"; for a in \"$@\"; do "
                           "printf '%s\\n' \"$a\"; done; } >> '%1'\n")
                .arg(calls)
                .toUtf8());
    f.write("cat > /dev/null\n");
    f.write("if [ \"$2\" = ls-files ]; then printf 'shared\\n'; fi\n");
    f.write("if [ \"$2\" = commit ]; then printf 'committed\\n'; fi\n");
    f.write("exit 0\n");
    f.close();
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                             QFile::ExeOwner));
  }
  const auto cleanup = qScopeGuard([&] {
    QFile::remove(shared);
    QFile::remove(gitPass);
    QFile::remove(calls);
  });
  AppSettings withGit = m_settings;
  withGit.useGit = true;
  withGit.passExecutable = gitPass;
  QScopedPointer<RealPass> pass(makePass());
  pass->init(withGit);
  pass->updateEnv();
  QSignalSpy criticalSpy(pass.data(), &Pass::critical);
  QSignalSpy removedSpy(pass.data(), &Pass::finishedRemove);
  pass->Remove(QStringLiteral("shared/"), true);
  QVERIFY2(removedSpy.count() == 1 || removedSpy.wait(5000),
           "the git commit must finish as the one PASS_REMOVE");
  QCOMPARE(removedSpy.count(), 1);
  QVERIFY2(removedSpy.first().at(0).toString().trimmed() ==
               QStringLiteral("committed"),
           qPrintable("finishedRemove did not come from the commit: " +
                      removedSpy.first().at(0).toString()));
  QCOMPARE(criticalSpy.count(), 0);
  QVERIFY2(!QFileInfo(shared).isSymLink() && !QFile::exists(shared),
           "the link must be gone");
  QVERIFY2(QFile::exists(marker), "the link's target must be untouched");
  QFile f(calls);
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
           "the stand-in must have logged its invocations");
  const QStringList invocations =
      QString::fromUtf8(f.readAll())
          .split(QStringLiteral("CALL\n"), Qt::SkipEmptyParts);
  QCOMPARE(invocations.size(), 3);
  QList<QStringList> argv;
  for (const QString &record : invocations) {
    const QStringList lines =
        record.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    const QString store = lines.value(0);
    QVERIFY2(store.startsWith(QStringLiteral("STORE=")),
             qPrintable("record without the store first: " + record));
    QCOMPARE(QDir(store.mid(6)).canonicalPath(), QDir(m_store).canonicalPath());
    argv << lines.mid(1);
  }
  QCOMPARE(argv.at(0),
           (QStringList{QStringLiteral("git"), QStringLiteral("ls-files"),
                        QStringLiteral("--"), QStringLiteral("shared")}));
  QCOMPARE(argv.at(1),
           (QStringList{QStringLiteral("git"), QStringLiteral("rm"),
                        QStringLiteral("-q"), QStringLiteral("--cached"),
                        QStringLiteral("--"), QStringLiteral("shared")}));
  QCOMPARE(argv.at(2),
           (QStringList{QStringLiteral("git"), QStringLiteral("commit"),
                        QStringLiteral("-q"), QStringLiteral("-m"),
                        QStringLiteral("Remove for shared using QtPass."),
                        QStringLiteral("--"), QStringLiteral("shared")}));
}

QTEST_MAIN(tst_realpass)
#include "tst_realpass.moc"
