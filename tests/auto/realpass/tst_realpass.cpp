// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
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
  void insertPipesTheValue();
  void removeFileAndFolder();
  void initWritesEnabledKeysRelativeToTheStore();
  void moveAndCopyUseStoreRelativeNamesWithoutGpg();
  void moveBetweenExistingFilesNeedsForce();

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
  pass->Grep(QStringLiteral("-needle"), true);
  QCOMPARE(waitForCall().args,
           (QStringList{QStringLiteral("grep"), QStringLiteral("-i"),
                        QStringLiteral("--"), QStringLiteral("-needle")}));
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

QTEST_MAIN(tst_realpass)
#include "tst_realpass.moc"
