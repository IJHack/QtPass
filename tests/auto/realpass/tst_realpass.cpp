// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "../../../src/appsettings.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/realpass.h"
#include "../../../src/userinfo.h"
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
#define QTRY_VERIFY_WITH_TIMEOUT_RETURN(expr, timeout, ret)                    \
  do {                                                                         \
    QElapsedTimer timer;                                                       \
    timer.start();                                                             \
    while (!(expr) && timer.elapsed() < (timeout)) {                           \
      QTest::qWait(20);                                                        \
    }                                                                          \
    if (!(expr)) {                                                             \
      QTest::qFail("stand-in pass was not invoked", __FILE__, __LINE__);       \
      return ret;                                                              \
    }                                                                          \
  } while (false)

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

QTEST_MAIN(tst_realpass)
#include "tst_realpass.moc"
