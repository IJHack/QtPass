// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QLoggingCategory>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

#include "../../../src/appsettings.h"
#include "../../../src/gpgidgeneration.h"
#include "../../../src/imitatepass.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/realpass.h"
#include "../../../src/userinfo.h"
#include "../testpass.h"
#include "../testsettings.h"

#ifndef Q_OS_WIN
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
/// A RealPass whose executor the test can reach, to drop a queued command.
class ExposedRealPass : public RealPass {
public:
  /// @return The backend's executor.
  auto executor() -> Executor & { return exec; }
};
} // namespace

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
  void insertWhoseCommitFailsSaysWhetherTheEntryWasWritten();
  void insertThatCannotStartIsNotCreditedWithTheNextWrite();
  void insertsPendingCountsTheInsertsYetToReport();
  void refusesToInsertDuringAnUnfinishedMerge();
  void refusesToInsertWhileAPullIsInFlight();
  void signedListIsCheckedAsTheNativeBackendDoes();
  void folderWithAnUnreadableNameIsRefusedBeforePassRuns();
  void removeFileAndFolder();
  void initWritesEnabledKeysRelativeToTheStore();
  void loggedArgvRedactsSecretShapedValues();
  void genericOutputSignalIsAnAllowList();
  void moveAndCopyUseStoreRelativeNamesWithoutGpg();
  void moveBetweenExistingFilesNeedsForce();
  void reencryptFailedJudgesTheStatusLines_data();
  void reencryptFailedJudgesTheStatusLines();
  void failedReencryptionHoldsAutomaticPushes();
  void reencryptFailureIsReportedNotFinished();
  void linkedEntriesAndFoldersAreRefusedBeforePassRuns();
  void gitPullBlockingReturnsAfterPassRanWhateverItsExitCode();
  void linkedFolderThatCannotBeUnlinkedReportsDeleteFailed();
  void linkedFolderKnownToGitIsForgottenByGitAlone();
  void linkedFolderRemovalSaysWhenTheCommitFails();
  void linkedFolderGitFailureIsReported();

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

  // An absolute name inside the store goes to pass relative to it: pass
  // would save <store>/<the whole path> for the root's recipients.
  QFile::remove(m_log);
  pass->Insert(m_store + QStringLiteral("folder/abs"), QStringLiteral("x"),
               false);
  QCOMPARE(waitForCall().args,
           (QStringList{QStringLiteral("insert"), QStringLiteral("-m"),
                        QStringLiteral("folder/abs")}));
}

/**
 * @brief pass writes the entry, then its git commit fails (one that cannot
 *        be signed, say) and pass exits non-zero: the entry is saved, for a
 *        new entry and for an edited one alike. When pass fails before it
 *        writes, or gpg breaks off part-way ("Password encryption
 *        aborted."), nothing was saved. Two saves of one entry queued back
 *        to back: the second compares against what the first left, not
 *        against the entry before either ran.
 */
void tst_realpass::insertWhoseCommitFailsSaysWhetherTheEntryWasWritten() {
  const QString entry = m_store + QStringLiteral("folder/edited.gpg");
  const auto writeEntry = [&entry](const QByteArray &bytes) {
    QFile f(entry);
    return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
  };
  // The value piped in says what this stand-in does: "ok" writes and
  // succeeds, "commit" writes and then fails as git does, "abort" leaves a
  // partial file as gpg breaking off does, anything else fails first.
  const QString script =
      QDir(m_dir.path()).filePath(QStringLiteral("pass-commit-fails"));
  {
    QFile f(script);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("#!/bin/sh\nmode=$(cat)\n"
            "for last; do :; done\n"
            "out=\"$PASSWORD_STORE_DIR/$last.gpg\"\n"
            "case \"$mode\" in\n"
            "ok) printf 'new %s\\n' \"$$\" > \"$out\"; exit 0 ;;\n"
            "commit) printf 'new %s\\n' \"$$\" > \"$out\"\n"
            "  echo 'error: gpg failed to sign the data' >&2 ;;\n"
            "abort) printf 'part' > \"$out\"\n"
            "  echo 'Password encryption aborted.' >&2 ;;\n"
            "*) echo 'gpg: no public key' >&2 ;;\n"
            "esac\nexit 1\n");
    f.close();
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                             QFile::ExeOwner));
  }
  AppSettings settings = m_settings;
  settings.passExecutable = script;
  // Runs the inserts in turn on one backend; what the (single) failure
  // said: whether the entry was written, and the box's title if any.
  const auto run =
      [&settings](const QString &name, bool overwrite,
                  const QStringList &modes) -> std::pair<bool, QString> {
    RealPass pass;
    pass.init(settings);
    pass.updateEnv();
    QSignalSpy failed(&pass, &Pass::insertFailed);
    QSignalSpy critical(&pass, &Pass::critical);
    for (const QString &mode : modes) {
      pass.Insert(name, mode, overwrite);
    }
    if (!failed.wait(5000) && failed.isEmpty()) {
      return {false, QStringLiteral("no insertFailed")};
    }
    const bool written = failed.first().at(1).toBool();
    const QString title =
        critical.isEmpty() ? QString() : critical.first().at(0).toString();
    return {written, title};
  };
  const QString edited = QStringLiteral("folder/edited");

  // An existing entry, rewritten before the commit fails.
  QVERIFY(writeEntry("old"));
  auto [written, title] = run(edited, true, {QStringLiteral("commit")});
  QVERIFY2(written, "the edited entry was written");
  QCOMPARE(title, QStringLiteral("Saved, not committed"));

  // An existing entry pass never got to: not written, no box.
  QVERIFY(writeEntry("old"));
  std::tie(written, title) = run(edited, true, {QStringLiteral("fail")});
  QVERIFY(!written);
  QVERIFY2(title.isEmpty(), qPrintable(title));

  // gpg broke off and left a partial file: changed, but no entry.
  QVERIFY(writeEntry("old"));
  std::tie(written, title) = run(edited, true, {QStringLiteral("abort")});
  QVERIFY2(!written, "a broken-off encryption is not a saved entry");
  QVERIFY2(title.isEmpty(), qPrintable(title));

  // Two saves queued: the first writes and succeeds, the second fails
  // before it writes. The second did not write, whatever the first did.
  QVERIFY(writeEntry("old"));
  std::tie(written, title) =
      run(edited, true, {QStringLiteral("ok"), QStringLiteral("fail")});
  QVERIFY2(!written, "the second save wrote nothing");
  QVERIFY2(title.isEmpty(), qPrintable(title));

  // A new entry, written before the commit fails.
  QFile::remove(m_store + QStringLiteral("folder/fresh.gpg"));
  std::tie(written, title) =
      run(QStringLiteral("folder/fresh"), false, {QStringLiteral("commit")});
  QVERIFY(written);
  QCOMPARE(title, QStringLiteral("Saved, not committed"));
}

/**
 * @brief An insert whose pass cannot start reports that later (the error is
 *        posted), and the next queued insert starts at once. If that one
 *        has written its entry by the time the error is handled, the failed
 *        insert is still judged by its own entry, not credited with it.
 */
void tst_realpass::insertThatCannotStartIsNotCreditedWithTheNextWrite() {
  const QString script =
      QDir(m_dir.path()).filePath(QStringLiteral("pass-late-start"));
  {
    QFile f(script);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    // stdin is not read: QProcess writes it from the event loop, which
    // this test holds off on purpose.
    f.write("#!/bin/sh\nfor last; do :; done\n"
            "printf 'new %s\\n' \"$$\" > \"$PASSWORD_STORE_DIR/$last.gpg\"\n"
            "exit 0\n");
    f.close();
    // Not executable yet: the first insert cannot start.
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner));
  }
  const QString first = m_store + QStringLiteral("folder/never.gpg");
  const QString second = m_store + QStringLiteral("folder/later.gpg");
  QFile::remove(first);
  QFile::remove(second);
  AppSettings settings = m_settings;
  settings.passExecutable = script;
  RealPass pass;
  pass.init(settings);
  pass.updateEnv();
  QSignalSpy failed(&pass, &Pass::insertFailed);
  QSignalSpy critical(&pass, &Pass::critical);
  QSignalSpy inserted(&pass, &Pass::finishedInsert);

  pass.Insert(QStringLiteral("folder/never"), QStringLiteral("v"), false);
  QVERIFY(QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                            QFile::ExeOwner));
  pass.Insert(QStringLiteral("folder/later"), QStringLiteral("v"), false);
  // The second pass runs on its own; give it time to write before the
  // event loop delivers the first insert's error.
  QElapsedTimer timer;
  timer.start();
  while (!QFile::exists(second) && timer.elapsed() < 5000) {
    QThread::msleep(10);
  }
  QThread::msleep(50);
  QVERIFY(QFile::exists(second));

  QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 5000);
  QVERIFY2(!failed.first().at(1).toBool(),
           "the insert that never started wrote nothing");
  QCOMPARE(critical.count(), 0);
  QVERIFY(!QFile::exists(first));
  QTRY_COMPARE_WITH_TIMEOUT(inserted.count(), 1, 5000);
}

/**
 * @brief insertsPending() counts every queued insert until it reports, so a
 *        PasswordDialog can tell an earlier insert's report from its own.
 */
void tst_realpass::insertsPendingCountsTheInsertsYetToReport() {
  RealPass pass;
  pass.init(m_settings);
  pass.updateEnv();
  QSignalSpy inserted(&pass, &Pass::finishedInsert);
  QCOMPARE(pass.insertsPending(), 0);
  // Queued synchronously, so neither has reported yet.
  pass.Insert(QStringLiteral("folder/one"), QStringLiteral("v"), false);
  pass.Insert(QStringLiteral("folder/two"), QStringLiteral("v"), false);
  QCOMPARE(pass.insertsPending(), 2);
  QTRY_COMPARE(inserted.count(), 2);
  QCOMPARE(pass.insertsPending(), 0);
}

/**
 * @brief An insert while git is mid-merge is refused before `pass` runs. The
 *        stand-in `pass git rev-parse --git-path` reports a MERGE_HEAD that
 *        exists in the store, as real git would during a stopped merge.
 */
void tst_realpass::refusesToInsertDuringAnUnfinishedMerge() {
  QVERIFY(QDir().mkpath(m_store + QStringLiteral(".git")));
  {
    QFile marker(m_store + QStringLiteral(".git/MERGE_HEAD"));
    QVERIFY(marker.open(QIODevice::WriteOnly | QIODevice::Text));
    marker.write("x\n");
  }
  const QString calls =
      QDir(m_dir.path()).filePath(QStringLiteral("merge-calls.log"));
  const auto cleanup = qScopeGuard([&] {
    QFile::remove(m_store + QStringLiteral(".git/MERGE_HEAD"));
    QDir(m_store).rmdir(QStringLiteral(".git"));
    QFile::remove(calls);
  });
  const QString script =
      QDir(m_dir.path()).filePath(QStringLiteral("merge-pass"));
  {
    QFile f(script);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    // For the rebase/merge probe, name the markers relative to the store;
    // MERGE_HEAD exists. Any other pass call is logged, to show none ran.
    f.write(QStringLiteral(
                "#!/bin/sh\n"
                "if [ \"$1\" = git ] && [ \"$2\" = rev-parse ]; then\n"
                "  printf '%s\\n' .git/rebase-merge .git/rebase-apply "
                ".git/MERGE_HEAD .git/CHERRY_PICK_HEAD .git/REVERT_HEAD\n"
                "  exit 0\n"
                "fi\n"
                "printf 'CALL\\n' >> '%1'\n"
                "exit 0\n")
                .arg(calls)
                .toUtf8());
    f.close();
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                             QFile::ExeOwner));
  }
  AppSettings s = m_settings;
  s.useGit = true;
  s.passExecutable = script;
  RealPass pass;
  pass.init(s);
  pass.updateEnv();
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  QSignalSpy inserted(&pass, &Pass::finishedInsert);
  pass.Insert(QStringLiteral("newsite"), QStringLiteral("v"), false);
  QTest::qWait(200);
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(criticalSpy.first().at(0).toString(),
           QStringLiteral("Unfinished Git operation"));
  QVERIFY2(
      criticalSpy.first().at(1).toString().contains(QStringLiteral("merge")),
      qPrintable(criticalSpy.first().at(1).toString()));
  QCOMPARE(inserted.count(), 0);
  QVERIFY2(!QFile::exists(calls),
           "pass insert must not run during the unfinished merge");
}

/**
 * @brief `pass insert` commits by itself, so there is no point to re-check
 *        after a queued pull; an insert is refused while a pull is still in
 *        flight, so its conflict cannot swallow the entry.
 */
void tst_realpass::refusesToInsertWhileAPullIsInFlight() {
  const QString script =
      QDir(m_dir.path()).filePath(QStringLiteral("sleeping-pull-pass"));
  const QString calls =
      QDir(m_dir.path()).filePath(QStringLiteral("pull-calls.log"));
  const auto cleanup = qScopeGuard([&] {
    QFile::remove(script);
    QFile::remove(calls);
  });
  {
    QFile f(script);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    // `git pull` sleeps so it is still in flight; `git rev-parse` answers with
    // paths that do not exist (no unfinished operation). Anything else is
    // logged, to show no insert ran.
    f.write(QStringLiteral(
                "#!/bin/sh\n"
                "if [ \"$1\" = git ] && [ \"$2\" = pull ]; then sleep 5; "
                "exit 0; fi\n"
                "if [ \"$1\" = git ] && [ \"$2\" = rev-parse ]; then\n"
                "  printf '%s\\n' .git/rebase-merge .git/rebase-apply "
                ".git/MERGE_HEAD .git/CHERRY_PICK_HEAD .git/REVERT_HEAD\n"
                "  exit 0\n"
                "fi\n"
                "printf 'CALL\\n' >> '%1'\n"
                "exit 0\n")
                .arg(calls)
                .toUtf8());
    f.close();
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                             QFile::ExeOwner));
  }
  AppSettings s = m_settings;
  s.useGit = true;
  s.passExecutable = script;
  RealPass pass;
  pass.init(s);
  pass.updateEnv();
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  QSignalSpy inserted(&pass, &Pass::finishedInsert);
  pass.GitPull();
  pass.Insert(QStringLiteral("folder/new"), QStringLiteral("v"), false);
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(criticalSpy.first().at(0).toString(),
           QStringLiteral("Update in progress"));
  QCOMPARE(inserted.count(), 0);
  QVERIFY2(!QFile::exists(calls),
           "pass insert must not run while a pull is in flight");
}

/**
 * @brief With a signing key, a move or copy of a folder holding a name that
 *        is not valid UTF-8 is refused before pass runs. Qt leaves such a
 *        name out of its listing, so a folder behind it, with a list of its
 *        own, would go unchecked while pass's find still re-encrypts it.
 */
void tst_realpass::folderWithAnUnreadableNameIsRefusedBeforePassRuns() {
#ifdef Q_OS_WIN
  QSKIP("Windows file names are UTF-16 and always readable");
#else
  const QString signer =
      QStringLiteral("0123456789ABCDEF0123456789ABCDEF01234567");
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  const QString store = QDir::cleanPath(storeDir.path());
  // A gpg that verifies anything as signed by `signer`.
  const QString gpg = QDir(m_dir.path()).filePath(QStringLiteral("utf8-gpg"));
  {
    QFile f(gpg);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(QStringLiteral(
                "#!/bin/sh\ncat >/dev/null\n"
                "printf '[GNUPG:] VALIDSIG %1 2026-09-19 1758200000 0 4 0 1 "
                "10 00 %1\\n'\nexit 0\n")
                .arg(signer)
                .toUtf8());
    f.close();
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                             QFile::ExeOwner));
  }
  {
    const QString list = QDir(store).filePath(QStringLiteral(".gpg-id"));
    QFile f(list);
    QFile sig(list + QStringLiteral(".sig"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    QVERIFY(f.write(GpgIdGeneration::withHeader(1, QStringLiteral("."),
                                                "ALICE\n")) > 0);
    QVERIFY(sig.open(QIODevice::WriteOnly));
    QCOMPARE(sig.write("sig"), 3);
  }
  QVERIFY(QDir(store).mkpath(QStringLiteral("team")));
  QVERIFY(QDir(store).mkpath(QStringLiteral("dest")));
  const QString team = QDir(store).filePath(QStringLiteral("team"));
  const QString dest = QDir(store).filePath(QStringLiteral("dest"));
  {
    QFile e(QDir(team).filePath(QStringLiteral("a.gpg")));
    QVERIFY(e.open(QIODevice::WriteOnly));
    e.write("x");
  }
  AppSettings settings = m_settings;
  settings.passStore = store;
  settings.gpgExecutable = gpg;
  settings.passSigningKey = signer;
  RealPass pass;
  pass.init(settings);
  pass.updateEnv();
  QSignalSpy critical(&pass, &Pass::critical);
  const auto passRan = [this]() {
    QTest::qWait(300);
    return QFile::exists(m_log);
  };

  QFile::remove(m_log);
  pass.Copy(team, dest, false);
  // A folder Qt can list completely goes through.
  QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(m_log), 5000);
  QCOMPARE(critical.count(), 0);

  // A folder named in Latin-1, as an old tool or another system may have
  // left it: Qt does not list it, pass would re-encrypt what is in it.
  const QByteArray bad = QFile::encodeName(team) + "/old\xff";
  if (::mkdir(bad.constData(), 0700) != 0) {
    QSKIP("this file system refuses names that are not UTF-8");
  }
  const auto cleanup = qScopeGuard([&bad] { ::rmdir(bad.constData()); });
  QFile::remove(m_log);
  pass.Move(team, dest, false);
  QVERIFY2(!passRan(), "pass never runs over what QtPass could not check");
  QCOMPARE(critical.count(), 1);
  QVERIFY2(critical.first().value(1).toString().contains(
               QDir::toNativeSeparators(team)),
           qPrintable(critical.first().value(1).toString()));
#endif
}

/**
 * @brief With a signing key set, pass is only asked to encrypt to a signed
 *        list QtPass accepts, as the native backend does (#2077): pass
 *        checks the signature but not the generation or folder lines, so an
 *        older authentic list put back, one signed for another folder, or a
 *        folder whose own list was replaced by a link is refused before pass
 *        runs. A current list goes through.
 */
void tst_realpass::signedListIsCheckedAsTheNativeBackendDoes() {
  const QString signer =
      QStringLiteral("0123456789ABCDEF0123456789ABCDEF01234567");
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  const QString store = QDir::cleanPath(storeDir.path());
  // A gpg that verifies anything as signed by `signer`.
  const QString gpg = QDir(m_dir.path()).filePath(QStringLiteral("sign-gpg"));
  {
    QFile f(gpg);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(QStringLiteral(
                "#!/bin/sh\ncat >/dev/null\n"
                "printf '[GNUPG:] VALIDSIG %1 2026-09-19 1758200000 0 4 0 1 "
                "10 00 %1\\n'\nexit 0\n")
                .arg(signer)
                .toUtf8());
    f.close();
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                             QFile::ExeOwner));
  }
  const QString gpgIdFile = QDir(store).filePath(QStringLiteral(".gpg-id"));
  const auto writeList = [&gpgIdFile](qint64 generation, const QString &folder,
                                      const QByteArray &keys) {
    QFile list(gpgIdFile);
    QFile sig(gpgIdFile + QStringLiteral(".sig"));
    return list.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           list.write(GpgIdGeneration::withHeader(generation, folder, keys)) >
               0 &&
           sig.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           sig.write("sig") == 3;
  };
  AppSettings settings = m_settings;
  settings.passStore = store;
  settings.gpgExecutable = gpg;
  settings.passSigningKey = signer;
  RealPass pass;
  pass.init(settings);
  pass.updateEnv();
  QSignalSpy critical(&pass, &Pass::critical);
  const auto passRan = [this]() {
    QTest::qWait(300);
    return QFile::exists(m_log);
  };

  // Generation 2, for the root: pass is asked, and this device remembers 2.
  QVERIFY(writeList(2, QStringLiteral("."), "ALICE\n"));
  QFile::remove(m_log);
  pass.Insert(QStringLiteral("entry"), QStringLiteral("v"), false);
  QVERIFY2(passRan(), "a current, bound list goes through");
  QCOMPARE(critical.count(), 0);

  // Generation 1 put back, authentic and naming Mallory: refused.
  QVERIFY(writeList(1, QStringLiteral("."), "ALICE\nMALLORY\n"));
  QFile::remove(m_log);
  pass.Insert(QStringLiteral("entry"), QStringLiteral("v"), true);
  QVERIFY2(!passRan(), "pass never sees the secret for a rolled-back list");
  QCOMPARE(critical.count(), 1);

  // A newer list, but signed for another folder: refused.
  QVERIFY(writeList(3, QStringLiteral("team"), "MALLORY\n"));
  QFile::remove(m_log);
  pass.Insert(QStringLiteral("entry"), QStringLiteral("v"), true);
  QVERIFY2(!passRan(), "a list signed for another folder is refused");
  QCOMPARE(critical.count(), 2);

  // A folder whose own list is a link: a move into it is refused, not
  // encrypted to the root's list.
#ifndef Q_OS_WIN
  QVERIFY(writeList(4, QStringLiteral("."), "ALICE\n"));
  QVERIFY(QDir(store).mkpath(QStringLiteral("shared")));
  {
    QFile other(QDir(m_dir.path()).filePath(QStringLiteral("elsewhere")));
    QVERIFY(other.open(QIODevice::WriteOnly));
    other.write("MALLORY\n");
  }
  QVERIFY(QFile::link(QDir(m_dir.path()).filePath(QStringLiteral("elsewhere")),
                      QDir(store).filePath(QStringLiteral("shared/.gpg-id"))));
  {
    QFile e(QDir(store).filePath(QStringLiteral("entry.gpg")));
    QVERIFY(e.open(QIODevice::WriteOnly));
    e.write("x");
  }
  QFile::remove(m_log);
  pass.Move(QDir(store).filePath(QStringLiteral("entry.gpg")),
            QDir(store).filePath(QStringLiteral("shared")), false);
  QVERIFY2(!passRan(), "no move under a list QtPass cannot vouch for");
  QCOMPARE(critical.count(), 3);
  QFile::remove(QDir(store).filePath(QStringLiteral("shared/.gpg-id")));
#endif

  // A folder with its own list, moved: pass re-encrypts its entries to that
  // list, which goes along. Checked where it lies now: a current one lets
  // the move through, a rolled-back one does not.
  const QString team = QDir(store).filePath(QStringLiteral("team"));
  QVERIFY(QDir(store).mkpath(QStringLiteral("team")));
  QVERIFY(QDir(store).mkpath(QStringLiteral("dest")));
  {
    QFile e(QDir(team).filePath(QStringLiteral("a.gpg")));
    QVERIFY(e.open(QIODevice::WriteOnly));
    e.write("x");
  }
  const QString teamList = QDir(team).filePath(QStringLiteral(".gpg-id"));
  const auto writeTeamList = [&teamList](qint64 generation,
                                         const QByteArray &keys) {
    QFile list(teamList);
    QFile sig(teamList + QStringLiteral(".sig"));
    return list.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           list.write(GpgIdGeneration::withHeader(
               generation, QStringLiteral("team"), keys)) > 0 &&
           sig.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           sig.write("sig") == 3;
  };
  QVERIFY(writeList(5, QStringLiteral("."), "ALICE\n"));
  QVERIFY(writeTeamList(2, "ALICE\n"));
  QFile::remove(m_log);
  pass.Copy(team, QDir(store).filePath(QStringLiteral("dest")), false);
  QVERIFY2(passRan(), "a folder whose own list is current goes through");
  const int before = static_cast<int>(critical.count());
  QVERIFY(writeTeamList(1, "ALICE\nMALLORY\n"));
  QFile::remove(m_log);
  pass.Move(team, QDir(store).filePath(QStringLiteral("dest")), false);
  QVERIFY2(!passRan(), "a rolled-back list inside the folder is refused");
  QCOMPARE(critical.count(), before + 1);

  // A list this device accepted at generation 2, then put back at 1.
  const auto rolledBack = [&store](const QString &folder) {
    const QString list =
        QDir(QDir(store).filePath(folder)).filePath(QStringLiteral(".gpg-id"));
    const QByteArray current =
        GpgIdGeneration::withHeader(2, folder, "ALICE\n");
    QFile f(list);
    QFile sig(list + QStringLiteral(".sig"));
    if (!QDir(store).mkpath(folder) ||
        !f.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        f.write(current) <= 0 ||
        !sig.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        sig.write("sig") != 3) {
      return false;
    }
    f.close();
    if (GpgIdGeneration::accept(list, current, store) !=
        GpgIdGeneration::Verdict::Accepted) {
      return false;
    }
    QFile old(list);
    return old.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           old.write(
               GpgIdGeneration::withHeader(1, folder, "ALICE\nMALLORY\n")) > 0;
  };

  // pass's walk takes hidden entries too: a folder holding only one, under
  // a rolled-back list, is refused.
  QVERIFY(rolledBack(QStringLiteral("hidden")));
  {
    QFile e(QDir(store).filePath(QStringLiteral("hidden/.secret.gpg")));
    QVERIFY(e.open(QIODevice::WriteOnly));
    e.write("x");
  }
  QFile::remove(m_log);
  pass.Move(QDir(store).filePath(QStringLiteral("hidden")),
            QDir(store).filePath(QStringLiteral("dest")), false);
  QVERIFY2(!passRan(), "a hidden entry's rolled-back list is refused");
  QCOMPARE(critical.count(), before + 2);

  // Writing into an existing folder re-encrypts all of it: an entry already
  // there, under a rolled-back list, makes the move refused.
  QVERIFY(rolledBack(QStringLiteral("dest/private")));
  {
    QFile e(QDir(store).filePath(QStringLiteral("dest/private/s.gpg")));
    QVERIFY(e.open(QIODevice::WriteOnly));
    e.write("x");
  }
  QFile::remove(m_log);
  pass.Move(QDir(store).filePath(QStringLiteral("entry.gpg")),
            QDir(store).filePath(QStringLiteral("dest")), false);
  QVERIFY2(!passRan(), "the destination's own entries count too");
  QCOMPARE(critical.count(), before + 3);

  // A copy merges into a folder of the same name: an incoming entry gets a
  // list already there that no entry uses yet. Rolled back: refused.
  // The rolled-back dest/private from above is gone again first.
  QVERIFY(QDir(QDir(store).filePath(QStringLiteral("dest/private")))
              .removeRecursively());
  QVERIFY(QDir(store).mkpath(QStringLiteral("src/sub")));
  {
    QFile e(QDir(store).filePath(QStringLiteral("src/sub/secret.gpg")));
    QVERIFY(e.open(QIODevice::WriteOnly));
    e.write("x");
  }
  QVERIFY(rolledBack(QStringLiteral("dest/src/sub")));
  QFile::remove(m_log);
  pass.Copy(QDir(store).filePath(QStringLiteral("src")),
            QDir(store).filePath(QStringLiteral("dest")), false);
  QVERIFY2(!passRan(), "a list waiting in the destination counts");
  QCOMPARE(critical.count(), before + 4);

  // The other way round: a source folder that holds only a rolled-back
  // list replaces the current one in the folder it merges into, whose entry
  // pass then re-encrypts to it. Refused.
  QVERIFY(QDir(QDir(store).filePath(QStringLiteral("dest/src")))
              .removeRecursively());
  QVERIFY(rolledBack(QStringLiteral("lists")));
  {
    const QString there = QStringLiteral("dest/lists");
    QVERIFY(QDir(store).mkpath(there));
    QFile list(QDir(QDir(store).filePath(there)).filePath(".gpg-id"));
    QVERIFY(list.open(QIODevice::WriteOnly));
    list.write(GpgIdGeneration::withHeader(7, there, "ALICE\n"));
    QFile sig(list.fileName() + QStringLiteral(".sig"));
    QVERIFY(sig.open(QIODevice::WriteOnly));
    sig.write("sig");
    QFile e(QDir(QDir(store).filePath(there)).filePath("kept.gpg"));
    QVERIFY(e.open(QIODevice::WriteOnly));
    e.write("x");
  }
  QFile::remove(m_log);
  pass.Copy(QDir(store).filePath(QStringLiteral("lists")),
            QDir(store).filePath(QStringLiteral("dest")), false);
  QVERIFY2(!passRan(), "a list the source brings counts, entries or not");
  QCOMPARE(critical.count(), before + 5);

  // Users (pass init) re-encrypts everything under its folder to each
  // entry's nearest list: a deeper rolled-back list is refused.
  UserInfo alice;
  alice.key_id = signer;
  alice.enabled = true;
  QVERIFY(rolledBack(QStringLiteral("outer/inner")));
  {
    QFile e(QDir(store).filePath(QStringLiteral("outer/inner/e.gpg")));
    QVERIFY(e.open(QIODevice::WriteOnly));
    e.write("x");
  }
  QFile::remove(m_log);
  pass.Init(QDir(store).filePath(QStringLiteral("outer")), {alice});
  QVERIFY2(!passRan(), "init does not re-encrypt under a rolled-back list");
  QCOMPARE(critical.count(), before + 6);
  // The same folder named relative to the store, as --path names it: the
  // walk looks in the store, not in the working directory.
  QFile::remove(m_log);
  pass.Init(QStringLiteral("outer"), {alice});
  QVERIFY2(!passRan(), "a relative folder is checked in the store");
  QCOMPARE(critical.count(), before + 7);
  // And spelled through the target of a store root reached by a link: the
  // walk is in the store's spelling, so the deeper list is still found.
  {
    QTemporaryDir links;
    QVERIFY(links.isValid());
    const QString linkedStore = QDir(links.path()).filePath("store");
    QVERIFY(QFile::link(QDir::cleanPath(store), linkedStore));
    AppSettings linked = settings;
    linked.passStore = linkedStore;
    RealPass viaLink;
    viaLink.init(linked);
    viaLink.updateEnv();
    QSignalSpy linkedCritical(&viaLink, &Pass::critical);
    QFile::remove(m_log);
    viaLink.Init(QDir(QFileInfo(store).canonicalFilePath())
                     .filePath(QStringLiteral("outer")),
                 {alice});
    QVERIFY2(!passRan(), "a folder spelled through the link target is "
                         "checked as the store names it");
    QCOMPARE(linkedCritical.count(), 1);
  }

  // Its own list is the one being written: a folder whose list was
  // removed is no refusal there, that is the way back.
  QVERIFY(rolledBack(QStringLiteral("gone")));
  QVERIFY(QFile::remove(QDir(store).filePath(QStringLiteral("gone/.gpg-id"))));
  {
    QFile e(QDir(store).filePath(QStringLiteral("gone/e.gpg")));
    QVERIFY(e.open(QIODevice::WriteOnly));
    e.write("x");
  }
  QFile::remove(m_log);
  pass.Init(QDir(store).filePath(QStringLiteral("gone")), {alice});
  QVERIFY2(passRan(), "re-initialising a folder whose list was removed");
  QCOMPARE(critical.count(), before + 7);

  // A target in a folder that does not exist yet gets the list above it,
  // as pass looks it up: a rolled-back one there is refused.
  QVERIFY(rolledBack(QStringLiteral("a")));
  QFile::remove(m_log);
  pass.Move(QDir(store).filePath(QStringLiteral("entry.gpg")),
            QDir(store).filePath(QStringLiteral("a/new/renamed.gpg")), false);
  QVERIFY2(!passRan(), "a target under a missing folder is checked against "
                       "the list above it");
  QCOMPARE(critical.count(), before + 8);
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
  // A store root reached through a link, the folder spelled through the
  // link's target: --path is still the folder's place in the store (a
  // lexical relative path made it ../../target/folder, which pass refuses).
  QTemporaryDir links;
  QVERIFY(links.isValid());
  const QString linkedStore = QDir(links.path()).filePath("store");
  QVERIFY(QFile::link(QDir::cleanPath(m_store), linkedStore));
  {
    AppSettings linked = m_settings;
    linked.passStore = linkedStore + QLatin1Char('/');
    RealPass viaLink;
    viaLink.init(linked);
    viaLink.updateEnv();
    // Any other operation handed that spelling breaks the path rule and
    // says so; Init() places it itself, so it does not.
    const QRegularExpression offRule(QStringLiteral("link target"));
    QTest::ignoreMessage(QtWarningMsg, offRule);
    QFile::remove(m_log);
    viaLink.Show(QFileInfo(m_store).canonicalFilePath() +
                 QStringLiteral("/folder/entry"));
    QCOMPARE(waitForCall().args.value(0), QStringLiteral("show"));
    QTest::failOnWarning(offRule);
    QFile::remove(m_log);
    viaLink.Init(QFileInfo(m_store).canonicalFilePath() +
                     QStringLiteral("/folder"),
                 {alice});
    QCOMPARE(waitForCall().args, (QStringList{QStringLiteral("init"),
                                              QStringLiteral("--path=folder"),
                                              QStringLiteral("AAAA")}));
  }
  // A folder outside the store is refused, as pass refuses `..`: pass
  // would take --path=/abs as a folder under the store. A directory whose
  // name merely starts with the store's is not inside it (the old prefix
  // test turned /store-other into --path=-other); neither is one reached
  // through `..`.
  QSignalSpy critical(pass.data(), &Pass::critical);
  QSignalSpy errorExit(pass.data(), &Pass::processErrorExit);
  const QString sibling = QDir::cleanPath(m_store) + QStringLiteral("-other");
  QVERIFY(QDir().mkpath(sibling));
  for (const QString &outside :
       {sibling, QStringLiteral("../") + QFileInfo(sibling).fileName()}) {
    QFile::remove(m_log);
    pass->Init(outside, {alice});
    QTest::qWait(300);
    QVERIFY2(!QFile::exists(m_log), qPrintable(outside + " reached pass"));
  }
  QCOMPARE(critical.count(), 2);
  QCOMPARE(errorExit.count(), 2);
  QVERIFY2(critical.at(0).at(1).toString().contains(
               QStringLiteral("not inside the password store")),
           qPrintable(critical.at(0).at(1).toString()));
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

void tst_realpass::reencryptFailedJudgesTheStatusLines_data() {
  QTest::addColumn<QString>("out");
  QTest::addColumn<QString>("err");
  QTest::addColumn<bool>("failed");
  const QString two =
      QStringLiteral("a: reencrypting to AAAA\nb: reencrypting to AAAA\n");
  QTest::newRow("all encrypted")
      << two
      << QStringLiteral("[GNUPG:] END_ENCRYPTION\n[GNUPG:] END_ENCRYPTION\n")
      << false;
  QTest::newRow("unusable recipient")
      << two
      << QStringLiteral(
             "gpg: CCCC: Ungeeigneter öffentlicher Schlüssel\n"
             "[GNUPG:] INV_RECP 0 CCCC\n"
             "[GNUPG:] FAILURE encrypt 53\n[GNUPG:] END_ENCRYPTION\n")
      << true;
  QTest::newRow("decrypt failed, pinentry cancelled")
      << two
      << QStringLiteral("[GNUPG:] DECRYPTION_FAILED\n"
                        "[GNUPG:] FAILURE decrypt 33554531\n")
      << true;
  QTest::newRow("one of two encrypted")
      << two << QStringLiteral("[GNUPG:] END_ENCRYPTION\r\n") << true;
  QTest::newRow("expired key, still encrypted")
      << QStringLiteral("a: reencrypting to AAAA\n")
      << QStringLiteral("[GNUPG:] KEYEXPIRED 1700000000\n"
                        "[GNUPG:] NO_SECKEY 1234\n[GNUPG:] END_ENCRYPTION\n")
      << false;
  // A pass that does not hand PASSWORD_STORE_GPG_OPTS on: nothing to go by.
  QTest::newRow("no status lines")
      << two << QStringLiteral("gpg: oops\n") << false;
  QTest::newRow("nothing re-encrypted") << QString() << QString() << false;
  QTest::newRow("an entry named like the line")
      << QStringLiteral("a: reencrypting to b: reencrypting to AAAA\n")
      << QStringLiteral("[GNUPG:] END_ENCRYPTION\n") << false;
}

/**
 * @brief After pass init reported entries it could not re-encrypt, the
 *        recipients change is committed locally: automatic pushes of the
 *        store are held, so a later edit's push does not publish it, until a
 *        clean init covers that folder. A clean init of another folder
 *        (a neighbour with the same prefix included) and a clean copy do
 *        not release it; holds of two folders are kept apart; the hold is
 *        shared by both backends.
 */
void tst_realpass::failedReencryptionHoldsAutomaticPushes() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as the fake pass");
#else
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString store = dir.path() + QStringLiteral("/store");
  for (const QString &folder :
       {QStringLiteral("a"), QStringLiteral("ab"), QStringLiteral("c")}) {
    QVERIFY(QDir().mkpath(QDir(store).filePath(folder)));
  }
  // Fails to re-encrypt in any folder named in FAIL (a file next to the
  // script, read at each run), exits 1 for one named with a "!" before it
  // (after re-encrypting) or a "?" (before), and reports a clean run
  // elsewhere.
  const QString failList = dir.filePath(QStringLiteral("FAIL"));
  const QString script = dir.filePath(QStringLiteral("pass"));
  {
    QFile f(script);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("#!/bin/sh\nfor a in \"$@\"; do case \"$a\" in --path=*) "
            "p=\"${a#--path=}\";; esac; last=\"$a\"; done\n"
            "if grep -qx \"?${p:-$last}\" \"");
    f.write(QFile::encodeName(failList));
    f.write("\" 2>/dev/null; then exit 1; fi\n"
            "echo \"x: reencrypting to AAAA\"\n"
            "if grep -qx \"!${p:-$last}\" \"");
    f.write(QFile::encodeName(failList));
    f.write("\" 2>/dev/null; then exit 1; fi\n"
            "if grep -qx \"${p:-$last}\" \"");
    f.write(QFile::encodeName(failList));
    f.write("\" 2>/dev/null; then\n"
            "  echo '[GNUPG:] INV_RECP 0 CCCC' >&2\n"
            "  echo '[GNUPG:] FAILURE encrypt 53' >&2\n"
            "else echo '[GNUPG:] END_ENCRYPTION' >&2; fi\nexit 0\n");
    f.close();
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                             QFile::ExeOwner));
  }
  const auto failFolders = [&failList](const QStringList &folders) {
    QFile f(failList);
    return f.open(QIODevice::WriteOnly) &&
           f.write((folders.join('\n') + '\n').toUtf8()) > 0;
  };
  AppSettings s = m_settings;
  s.passExecutable = script;
  s.passStore = store + QStringLiteral("/");
  s.useGit = false;
  RealPass pass;
  pass.init(s);
  pass.updateEnv();
  QSignalSpy done(&pass, &Pass::finishedInit);
  QSignalSpy copied(&pass, &Pass::finishedCopy);
  QSignalSpy errorExit(&pass, &Pass::processErrorExit);
  UserInfo alice;
  alice.key_id = QStringLiteral("AAAA");
  alice.enabled = true;
  // One init; true when it ran to an end, clean or reported failed.
  const auto init = [&](const QString &folder) {
    const qsizetype ends = done.count() + errorExit.count();
    pass.Init(folder, {alice});
    return QTest::qWaitFor(
        [&] { return done.count() + errorExit.count() > ends; }, 10000);
  };
  const auto release = qScopeGuard([&] {
    QVERIFY(failFolders({QStringLiteral("none")}));
    init(store);
  });

  QVERIFY(failFolders({QStringLiteral("a"), QStringLiteral("ab")}));
  QVERIFY(!pass.autoPushHeld());
  QVERIFY(init(QStringLiteral("a")));
  QVERIFY2(pass.autoPushHeld(), "a failed init holds the store");
  QVERIFY(init(QStringLiteral("ab")));
  QVERIFY(failFolders({QStringLiteral("none")}));
  QVERIFY(init(QStringLiteral("c")));
  QVERIFY2(pass.autoPushHeld(), "a clean init elsewhere releases nothing");
  QVERIFY(init(QStringLiteral("ab")));
  QVERIFY2(pass.autoPushHeld(), "folder a's failure is still outstanding");
  // Into folder a, the one still held: a copy is not taken as covering it.
  const qsizetype copies = copied.count();
  pass.Copy(QDir(store).filePath(QStringLiteral("c")),
            QDir(store).filePath(QStringLiteral("a")), false);
  QVERIFY(QTest::qWaitFor([&] { return copied.count() > copies; }, 10000));
  QVERIFY2(pass.autoPushHeld(), "a copy re-encrypts only its own entries");
  // Shared with the other backend: switching keeps the hold.
  {
    ImitatePass other;
    other.init(s);
    QVERIFY(other.autoPushHeld());
  }
  // Another store is not held.
  {
    AppSettings elsewhere = s;
    elsewhere.passStore = dir.filePath(QStringLiteral("other"));
    RealPass third;
    third.init(elsewhere);
    QVERIFY(!third.autoPushHeld());
  }
  // A clean init of a covers a, not its neighbour ab.
  QVERIFY(failFolders({QStringLiteral("ab")}));
  QVERIFY(init(QStringLiteral("ab")));
  QVERIFY(failFolders({QStringLiteral("none")}));
  QVERIFY(init(QStringLiteral("a")));
  QVERIFY2(pass.autoPushHeld(), "folder ab is not under folder a");
  QVERIFY(init(QStringLiteral("ab")));
  QVERIFY2(!pass.autoPushHeld(), "every failed folder covered again");
  // A copy that failed before re-encrypting anything (the destination
  // exists, a mistyped name) changed no recipients: not held. One that
  // failed while re-encrypting is.
  const auto copy = [&]() {
    const qsizetype ends = copied.count() + errorExit.count();
    pass.Copy(QDir(store).filePath(QStringLiteral("c")),
              QDir(store).filePath(QStringLiteral("a")), false);
    return QTest::qWaitFor(
        [&] { return copied.count() + errorExit.count() > ends; }, 10000);
  };
  QVERIFY(failFolders({QStringLiteral("?a")}));
  QVERIFY(copy());
  QVERIFY2(!pass.autoPushHeld(), "nothing was re-encrypted");
  QVERIFY(failFolders({QStringLiteral("!a")}));
  QVERIFY(copy());
  QVERIFY2(pass.autoPushHeld(), "it stopped while re-encrypting");
  QVERIFY(failFolders({QStringLiteral("none")}));
  QVERIFY(init(QStringLiteral("a")));
  QVERIFY(!pass.autoPushHeld());
  // An init that exits non-zero may have stopped after its commit.
  QVERIFY(failFolders({QStringLiteral("!c")}));
  QVERIFY(init(QStringLiteral("c")));
  QVERIFY2(pass.autoPushHeld(), "a failed init holds its folder");
  QVERIFY(failFolders({QStringLiteral("none")}));
  QVERIFY(init(QStringLiteral("c")));
  QVERIFY(!pass.autoPushHeld());
  // Also before any re-encryption: the new list may already be written.
  QVERIFY(failFolders({QStringLiteral("?c")}));
  QVERIFY(init(QStringLiteral("c")));
  QVERIFY2(pass.autoPushHeld(), "an init that failed early holds too");
  QVERIFY(failFolders({QStringLiteral("none")}));
  QVERIFY(init(QStringLiteral("c")));
  QVERIFY(!pass.autoPushHeld());
  // A failure, then a clean init of the whole store.
  QVERIFY(failFolders({QStringLiteral("c")}));
  QVERIFY(init(QStringLiteral("c")));
  QVERIFY(pass.autoPushHeld());
  QVERIFY(failFolders({QStringLiteral("none")}));
  QVERIFY(init(store));
  QVERIFY2(!pass.autoPushHeld(), "the whole store covers every folder");

  // A queued init the executor drops takes its folder along: the next
  // completion is not paired with it. Here init a is dropped once init c
  // has finished; the failing init ab must hold ab, and its clean rerun
  // release it again.
  {
    ExposedRealPass dropping;
    dropping.init(s);
    dropping.updateEnv();
    QSignalSpy initDone(&dropping, &Pass::finishedInit);
    QSignalSpy initError(&dropping, &Pass::processErrorExit);
    bool dropOnce = true;
    QObject::connect(&dropping.executor(), &Executor::finished, &dropping, [&] {
      if (dropOnce) {
        dropOnce = false;
        QVERIFY(dropping.executor().cancelNext() >= 0);
      }
    });
    const auto ended = [&](qsizetype ends) {
      return QTest::qWaitFor(
          [&] { return initDone.count() + initError.count() > ends; }, 10000);
    };
    QVERIFY(failFolders({QStringLiteral("ab")}));
    dropping.Init(QStringLiteral("c"), {alice});
    dropping.Init(QStringLiteral("a"), {alice});
    QVERIFY(ended(0));
    QVERIFY(!dropOnce);
    dropping.Init(QStringLiteral("ab"), {alice});
    QVERIFY(ended(1));
    QVERIFY2(dropping.autoPushHeld(), "the failing init ab holds");
    QVERIFY(failFolders({QStringLiteral("none")}));
    dropping.Init(QStringLiteral("ab"), {alice});
    QVERIFY(ended(2));
    QVERIFY2(!dropping.autoPushHeld(),
             "the hold was on ab, not on the dropped init's a");
  }

  // A push asked for while an init is queued or running waits for its
  // outcome: queued behind it, it went out before the hold was set. After
  // a failure an automatic one is dropped and the status line says why;
  // one asked for by hand goes. After a clean run either goes.
  {
    QSignalSpy pushed(&pass, &Pass::finishedGitPush);
    QSignalSpy status(&pass, &Pass::statusMsg);
    const auto ended = [&](qsizetype ends) {
      return QTest::qWaitFor(
          [&] { return done.count() + errorExit.count() > ends; }, 10000);
    };
    QVERIFY(failFolders({QStringLiteral("a")}));
    qsizetype ends = done.count() + errorExit.count();
    pass.Init(QStringLiteral("a"), {alice});
    pass.GitPushAutomatic();
    QVERIFY(ended(ends));
    QTest::qWait(300);
    QVERIFY2(pushed.isEmpty(), "held: the waiting automatic push is dropped");
    QVERIFY(pass.autoPushHeld());
    bool said = false;
    for (const QList<QVariant> &args : std::as_const(status)) {
      said = said || args.at(0).toString().startsWith(
                         QStringLiteral("Not pushing: an earlier"));
    }
    QVERIFY2(said, "the status line says why");
    // By hand, waiting behind a failing init: it goes all the same.
    ends = done.count() + errorExit.count();
    pass.Init(QStringLiteral("a"), {alice});
    pass.GitPush();
    QVERIFY(pushed.isEmpty());
    QVERIFY(ended(ends));
    QVERIFY2(pushed.count() == 1 || pushed.wait(10000),
             "a push by hand goes despite the hold");
    // Behind two inits, it waits for the second: here the one that fails.
    QVERIFY(failFolders({QStringLiteral("c")}));
    ends = done.count() + errorExit.count();
    pass.Init(QStringLiteral("a"), {alice});
    pass.Init(QStringLiteral("c"), {alice});
    pass.GitPushAutomatic();
    QVERIFY(QTest::qWaitFor(
        [&] { return done.count() + errorExit.count() > ends + 1; }, 10000));
    QTest::qWait(300);
    QCOMPARE(pushed.count(), 1);
    QVERIFY(pass.autoPushHeld());
    // Clean: the waiting automatic push goes.
    QVERIFY(failFolders({QStringLiteral("none")}));
    ends = done.count() + errorExit.count();
    pass.Init(QStringLiteral("c"), {alice});
    pass.GitPushAutomatic();
    QCOMPARE(pushed.count(), 1);
    QVERIFY(ended(ends));
    QVERIFY2(pushed.count() == 2 || pushed.wait(10000),
             "after a clean run the waiting push goes");
    QVERIFY(!pass.autoPushHeld());
  }
#endif
}

/**
 * @brief pass exits 0 when it could not re-encrypt an entry; gpg's status
 *        lines say so in every locale.
 */
void tst_realpass::reencryptFailedJudgesTheStatusLines() {
  QFETCH(QString, out);
  QFETCH(QString, err);
  QFETCH(bool, failed);
  QCOMPARE(RealPass::reencryptFailed(out, err), failed);
}

/**
 * @brief A `pass init` (Users ▸ OK), `mv` or `cp` that exits 0 having left
 *        an entry encrypted to the previous recipients is a failure: a
 *        critical message and processErrorExit, not finishedInit/Move, which
 *        pushed the store as it was. Only those commands get `--status-fd 2`.
 */
void tst_realpass::reencryptFailureIsReportedNotFinished() {
  const QString mode = QDir(m_dir.path()).filePath(QStringLiteral("mode"));
  const QString opts = QDir(m_dir.path()).filePath(QStringLiteral("opts"));
  const QString script =
      QDir(m_dir.path()).filePath(QStringLiteral("pass-reencrypt"));
  {
    QFile f(script);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(
        QStringLiteral(
            "#!/bin/sh\n"
            "printf '%s %s\\n' \"$1\" \"$PASSWORD_STORE_GPG_OPTS\" >> '%1'\n"
            "echo 'folder/a: reencrypting to AAAA'\n"
            "case \"$(cat '%2')\" in\n"
            "fail) echo 'gpg: CCCC: unusable public key' >&2\n"
            "  echo '[GNUPG:] INV_RECP 0 CCCC' >&2\n"
            "  echo '[GNUPG:] FAILURE encrypt 53' >&2 ;;\n"
            "*) echo '[GNUPG:] END_ENCRYPTION' >&2 ;;\n"
            "esac\nexit 0\n")
            .arg(opts, mode)
            .toUtf8());
    f.close();
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                             QFile::ExeOwner));
  }
  const auto setMode = [&mode](const char *value) {
    QFile f(mode);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(value);
  };
  AppSettings s = m_settings;
  s.passExecutable = script;
  RealPass pass;
  pass.init(s);
  pass.updateEnv();
  QSignalSpy inited(&pass, &Pass::finishedInit);
  QSignalSpy moved(&pass, &Pass::finishedMove);
  QSignalSpy critical(&pass, &Pass::critical);
  QSignalSpy errors(&pass, &Pass::processErrorExit);
  QSignalSpy output(&pass, &Pass::finishedAnyWithPid);
  UserInfo alice;
  alice.key_id = QStringLiteral("AAAA");
  alice.enabled = true;

  setMode("fail");
  pass.Init(m_store + QStringLiteral("folder/"), {alice});
  QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, 5000);
  QCOMPARE(inited.size(), 0);
  QCOMPARE(critical.size(), 1);
  QVERIFY2(critical.first().at(1).toString().contains(
               QStringLiteral("still encrypted to the previous recipients")),
           qPrintable(critical.first().at(1).toString()));
  QVERIFY(!output.isEmpty());
  QVERIFY2(!output.last().at(1).toString().contains(QStringLiteral("[GNUPG:]")),
           qPrintable(output.last().at(1).toString()));

  QFile a(m_store + QStringLiteral("folder/a.gpg"));
  QVERIFY(a.open(QIODevice::WriteOnly));
  a.close();
  pass.Move(m_store + QStringLiteral("folder/a.gpg"),
            m_store + QStringLiteral("b.gpg"), true);
  QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 2, 5000);
  QCOMPARE(moved.size(), 0);

  setMode("ok");
  pass.Init(m_store + QStringLiteral("folder/"), {alice});
  QTRY_COMPARE_WITH_TIMEOUT(inited.size(), 1, 5000);
  QCOMPARE(critical.size(), 2);
  QVERIFY2(!output.last().at(1).toString().contains(QStringLiteral("[GNUPG:]")),
           "status lines stay out of the output panel");
  QSignalSpy shown(&pass, &Pass::finishedShow);
  pass.Show(m_store + QStringLiteral("folder/a"));
  QTRY_COMPARE_WITH_TIMEOUT(shown.size(), 1, 5000);

  QFile log(opts);
  QVERIFY(log.open(QIODevice::ReadOnly | QIODevice::Text));
  const QStringList calls =
      QString::fromUtf8(log.readAll()).split('\n', Qt::SkipEmptyParts);
  QVERIFY2(calls.size() >= 4, qPrintable(calls.join(" | ")));
  for (const QString &call : calls) {
    const bool reencrypts = call.startsWith(QStringLiteral("init ")) ||
                            call.startsWith(QStringLiteral("mv "));
    QVERIFY2(call.endsWith(QStringLiteral("--status-fd 2")) == reencrypts,
             qPrintable(call));
  }
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
 *        `git pull` (the log exists without waiting), in the store's
 *        environment: without PASSWORD_STORE_DIR, pass pulled
 *        ~/.password-store instead of the configured store. A non-zero exit
 *        is said in the status bar and the debug log, never turned into an
 *        error signal or a second attempt.
 */
void tst_realpass::gitPullBlockingReturnsAfterPassRanWhateverItsExitCode() {
  QScopedPointer<RealPass> pass(makePass());
  QSignalSpy criticalSpy(pass.data(), &Pass::critical);
  QSignalSpy errorSpy(pass.data(), &Pass::processErrorExit);
  QSignalSpy statusSpy(pass.data(), &Pass::statusMsg);
  pass->GitPull_b();
  QVERIFY2(QFile::exists(m_log),
           "GitPull_b must not return before the stand-in pass finished");
  Call call = waitForCall();
  QCOMPARE(call.args,
           (QStringList{QStringLiteral("git"), QStringLiteral("pull")}));
  QCOMPARE(QDir(call.storeEnv).canonicalPath(), QDir(m_store).canonicalPath());
  QCOMPARE(statusSpy.count(), 0);

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
  call = waitForCall();
  QCOMPARE(call.args,
           (QStringList{QStringLiteral("git"), QStringLiteral("pull")}));
  QCOMPARE(QDir(call.storeEnv).canonicalPath(), QDir(m_store).canonicalPath());
  QCOMPARE(statusSpy.count(), 1);
  QVERIFY2(statusSpy.first().first().toString().contains(
               QStringLiteral("Git pull failed")),
           qPrintable(statusSpy.first().first().toString()));
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
  // The read-only rebase/merge probe (pass git rev-parse) may run; pass must
  // not be asked to change the store (rm or commit) for a failed unlink.
  if (QFile::exists(m_log)) {
    QFile log(m_log);
    QVERIFY(log.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString text = QString::fromUtf8(log.readAll());
    QVERIFY2(!text.contains(QLatin1String("\nrm\n")) &&
                 !text.contains(QLatin1String("\ncommit\n")),
             qPrintable("pass was asked to change the store: " + text));
  }
}

/**
 * @brief The link is removed first; when git then cannot forget it (here
 *        `git rm --cached` fails) the removal is not reported as clean: the
 *        user is told git still tracks it, and no commit of nothing runs.
 */
void tst_realpass::linkedFolderGitFailureIsReported() {
  QTemporaryDir outsideDir;
  QVERIFY(outsideDir.isValid());
  const QString shared = m_store + QStringLiteral("shared");
  QVERIFY(QFile::link(outsideDir.path(), shared));
  const QString calls =
      QDir(m_dir.path()).filePath(QStringLiteral("calls-fail.log"));
  const QString gitPass =
      QDir(m_dir.path()).filePath(QStringLiteral("pass-git-fail"));
  {
    QFile f(gitPass);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("#!/bin/sh\n");
    f.write(
        QStringLiteral("printf '%s\\n' \"$*\" >> '%1'\n").arg(calls).toUtf8());
    f.write("cat > /dev/null\n");
    f.write("if [ \"$2\" = ls-files ]; then printf 'shared\\n'; exit 0; fi\n");
    f.write("if [ \"$2\" = rm ]; then echo 'fatal: index.lock exists' >&2; "
            "exit 128; fi\n");
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
  QVERIFY(removedSpy.count() == 1 || removedSpy.wait(5000));
  QCOMPARE(criticalSpy.count(), 1);
  QVERIFY2(criticalSpy.first().at(1).toString().contains(
               QStringLiteral("index.lock")),
           qPrintable(criticalSpy.first().at(1).toString()));
  QVERIFY2(!QFileInfo(shared).isSymLink(), "the link is gone either way");
  QFile log(calls);
  QVERIFY(log.open(QIODevice::ReadOnly));
  QVERIFY2(!log.readAll().contains("commit"), "nothing may be committed");
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
  QList<QStringList> argv;
  for (const QString &record : invocations) {
    const QStringList lines =
        record.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    const QString store = lines.value(0);
    QVERIFY2(store.startsWith(QStringLiteral("STORE=")),
             qPrintable("record without the store first: " + record));
    QCOMPARE(QDir(store.mid(6)).canonicalPath(), QDir(m_store).canonicalPath());
    const QStringList call = lines.mid(1);
    // Skip the read-only rebase/merge probe Remove() runs first.
    if (call.contains(QStringLiteral("rev-parse"))) {
      continue;
    }
    argv << call;
  }
  // ls-files, rm --cached, the pass.signcommits query (as pass's own
  // git_commit() asks; the stand-in answers nothing, so no -S), commit, and
  // the check that nothing is left staged.
  QCOMPARE(argv.size(), 5);
  QCOMPARE(argv.at(0),
           (QStringList{QStringLiteral("git"), QStringLiteral("ls-files"),
                        QStringLiteral("--"), QStringLiteral("shared")}));
  QCOMPARE(argv.at(1),
           (QStringList{QStringLiteral("git"), QStringLiteral("rm"),
                        QStringLiteral("-q"), QStringLiteral("--cached"),
                        QStringLiteral("--"), QStringLiteral("shared")}));
  QCOMPARE(argv.at(2),
           (QStringList{QStringLiteral("git"), QStringLiteral("config"),
                        QStringLiteral("--bool"), QStringLiteral("--get"),
                        QStringLiteral("pass.signcommits")}));
  QCOMPARE(argv.at(3),
           (QStringList{QStringLiteral("git"), QStringLiteral("commit"),
                        QStringLiteral("-q"), QStringLiteral("-m"),
                        QStringLiteral("Remove for shared using QtPass."),
                        QStringLiteral("--"), QStringLiteral("shared")}));
  QCOMPARE(
      argv.at(4),
      (QStringList{QStringLiteral("git"), QStringLiteral("diff"),
                   QStringLiteral("--cached"), QStringLiteral("--name-only"),
                   QStringLiteral("--"), QStringLiteral("shared")}));
}

/**
 * @brief `pass git` exits 0 whatever git did. A commit that failed (here a
 * signer that would not sign) leaves the removal staged, and that is how
 * it is told: reported, not taken for a clean removal.
 */
void tst_realpass::linkedFolderRemovalSaysWhenTheCommitFails() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as the pass stand-in");
#else
  QTemporaryDir outsideDir;
  QVERIFY(outsideDir.isValid());
  const QString shared = m_store + QStringLiteral("shared");
  QVERIFY(QFile::link(outsideDir.path(), shared));
  const QString gitPass =
      QDir(m_dir.path()).filePath(QStringLiteral("pass-git-failing"));
  const QString stagedMark =
      QDir(m_dir.path()).filePath(QStringLiteral("still-staged"));
  {
    QFile f(gitPass);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    // The store asks for signed commits; a commit with -S fails to sign
    // (and leaves the removal staged), one without would go through: so
    // the test also fails when the -S goes missing.
    f.write("#!/bin/sh\ncat > /dev/null\n"
            "if [ \"$2\" = ls-files ]; then printf 'shared\\n'; fi\n"
            "if [ \"$2\" = config ]; then printf 'true\\n'; fi\n"
            "if [ \"$2\" = commit ]; then\n"
            "  case \" $* \" in *\" -S \"*)\n"
            "    echo 'error: gpg failed to sign the data' >&2\n"
            "    : > '%STAGED%' ;;\n"
            "  esac\n"
            "fi\n"
            "if [ \"$2\" = diff ] && [ -e '%STAGED%' ]; then "
            "printf 'shared\\n'; fi\n"
            "exit 0\n");
    f.close();
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                             QFile::ExeOwner));
  }
  // The script names the mark by a placeholder, filled in here.
  {
    QFile f(gitPass);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray script = f.readAll();
    f.close();
    script.replace("%STAGED%", QFile::encodeName(stagedMark));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(script);
  }
  const auto cleanup = qScopeGuard([&] {
    QFile::remove(shared);
    QFile::remove(gitPass);
    QFile::remove(stagedMark);
  });
  AppSettings withGit = m_settings;
  withGit.useGit = true;
  withGit.passExecutable = gitPass;
  QScopedPointer<RealPass> pass(makePass());
  pass->init(withGit);
  pass->updateEnv();
  QSignalSpy criticalSpy(pass.data(), &Pass::critical);
  QSignalSpy errorSpy(pass.data(), &Pass::processErrorExit);
  pass->Remove(QStringLiteral("shared/"), true);
  // Another profile while the commit runs: its pass says nothing is
  // staged, and must not be the one asked.
  const QString otherPass =
      QDir(m_dir.path()).filePath(QStringLiteral("pass-other-profile"));
  {
    QFile f(otherPass);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("#!/bin/sh\ncat > /dev/null\nexit 0\n");
    f.close();
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                             QFile::ExeOwner));
  }
  const auto removeOther = qScopeGuard([&] { QFile::remove(otherPass); });
  AppSettings other = withGit;
  other.passExecutable = otherPass;
  pass->init(other);
  pass->updateEnv();
  // The commit is queued: wait for its verdict.
  QVERIFY(criticalSpy.count() == 1 || criticalSpy.wait(5000));
  QCOMPARE(criticalSpy.count(), 1);
  QVERIFY2(criticalSpy.first().at(1).toString().contains(
               QStringLiteral("gpg failed to sign")),
           qPrintable(criticalSpy.first().at(1).toString()));
  QCOMPARE(errorSpy.count(), 1);
  QVERIFY2(!QFileInfo(shared).isSymLink(), "the link is gone all the same");
#endif
}

QTEST_MAIN(tst_realpass)
#include "tst_realpass.moc"
