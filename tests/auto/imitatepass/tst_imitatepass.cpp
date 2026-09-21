// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @brief Tests for ImitatePass::reencryptPath() running on a worker thread.
 *
 * No real gpg is needed: the gpg executable points at a path that cannot be
 * started (or, on Unix, at a shell script that sleeps and fails), so every
 * file "fails" to re-encrypt in a controlled way. That is enough to check the
 * threading contract: startReencryptPath() is emitted synchronously, the loop
 * runs off the calling thread, progress is reported per file, per-file
 * failures are aggregated into a single critical(), a cancel stops between
 * files, and endReencryptPath() always closes the run.
 *
 * A third pair uses a fake gpg that blocks (sleeps) to check that a cancel,
 * and destroying the ImitatePass, make the worker end the process in
 * progress instead of waiting for it: the cancel only sets a flag, the
 * worker's wait loop terminates and, if need be, kills its own child.
 *
 * A second group swaps in a recording fake gpg to check the argv QtPass hands
 * to gpg on encryption: every encrypt call must carry --no-encrypt-to (and
 * --compress-algo=none, as pass(1) does) so a user's gpg.conf cannot add a
 * recipient that the .gpg-id does not list.
 *
 * A third group swaps in a recording fake git as well, with autoPush on, to
 * check that a run in which every file re-encrypted is pushed while a run with
 * a failed file is not: pushing the partial store would publish recipient
 * metadata that does not match all encrypted files.
 */

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>
#include <algorithm>
#ifndef Q_OS_WIN
#include <sys/stat.h>
#endif

#include "../../../src/imitatepass.h"
#include "../testsettings.h"

class tst_imitatepass : public QObject {
  Q_OBJECT

  /// A gpg path that can never be started, so every gpg call fails fast.
  static QString unstartableGpg() {
    return QStringLiteral("/nonexistent/qtpass-test-no-such-gpg");
  }

  /// Create @p count dummy `.gpg` files plus a `.gpg-id` in @p storeDir.
  static bool populateStore(const QString &storeDir, int count) {
    QFile gpgId(QDir(storeDir).filePath(QStringLiteral(".gpg-id")));
    if (!gpgId.open(QIODevice::WriteOnly | QIODevice::Text))
      return false;
    if (gpgId.write("0123456789ABCDEF\n") <= 0)
      return false;
    gpgId.close();
    for (int i = 0; i < count; ++i) {
      QFile f(QDir(storeDir).filePath(QStringLiteral("entry%1.gpg").arg(i)));
      if (!f.open(QIODevice::WriteOnly))
        return false;
      if (f.write("not really encrypted") <= 0)
        return false;
    }
    return true;
  }

  /// Write a fake gpg that touches @p marker and then blocks for @p seconds.
  /// `exec` so the SIGTERM the worker's terminate() sends lands on the sleep
  /// itself rather than on a shell that would leave it behind. Returns the
  /// script path, or an empty string on failure.
  static QString writeBlockingGpg(const QString &dir, const QString &marker,
                                  int seconds, bool ignoreTerm = false) {
    const QString script = QDir(dir).filePath("blocking-gpg.sh");
    QFile f(script);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
      return {};
    QTextStream out(&f);
    out << "#!/bin/sh\n"
        << ": > '" << marker << "'\n";
    if (ignoreTerm) {
      // Stay in the shell (no exec) so the trap applies to the process the
      // worker terminate()s; sleep runs as a child and is left to exit on its
      // own.
      out << "trap '' TERM\n"
          << "sleep " << seconds << "\n";
    } else {
      out << "exec sleep " << seconds << "\n";
    }
    out.flush();
    f.close();
    if (!QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                           QFile::ExeOwner))
      return {};
    return script;
  }

  static AppSettings settingsFor(const QString &storeDir, const QString &gpg) {
    AppSettings s;
    s.passStore = QDir::cleanPath(storeDir) + QLatin1Char('/');
    s.gpgExecutable = gpg;
    s.useGit = false;
    s.addGPGId = false;
    return s;
  }

  /// Write a fake gpg that appends every argv it receives (one call per line,
  /// arguments space-separated) to @p logPath and then behaves just enough
  /// like gpg for Insert() and reencryptSingleFile() to succeed: decrypts
  /// (-d) print a fixed plaintext, encrypts (-eq) write a dummy ciphertext to
  /// the --output path. Returns the script path, or an empty string.
  static QString writeRecordingGpg(const QString &dir, const QString &logPath) {
    const QString script = QDir(dir).filePath(QStringLiteral("record-gpg.sh"));
    QFile f(script);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
      return {};
    QTextStream out(&f);
    out << "#!/bin/sh\n"
        << "printf '%s\\n' \"$*\" >> '" << logPath << "'\n"
        << "mode=''; outfile=''; prev=''\n"
        << "for a in \"$@\"; do\n"
        << "  case \"$a\" in -d) mode=decrypt;; -eq) mode=encrypt;; esac\n"
        << "  [ \"$prev\" = '--output' ] && outfile=\"$a\"\n"
        << "  prev=\"$a\"\n"
        << "done\n"
        << "case \"$mode\" in\n"
        << "  decrypt) printf 'plaintext\\n';;\n"
        << "  encrypt) cat >/dev/null; printf 'ciphertext\\n' > "
           "\"$outfile\";;\n"
        << "esac\n"
        << "exit 0\n";
    out.flush();
    f.close();
    if (!QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                           QFile::ExeOwner))
      return {};
    return script;
  }

  /// Write a fake git that appends every argv it receives to @p logPath and
  /// succeeds silently, so `status --porcelain` reports a clean tree and no
  /// backup commit is attempted. Returns the script path, or an empty string.
  static QString writeRecordingGit(const QString &dir, const QString &logPath) {
    const QString script = QDir(dir).filePath(QStringLiteral("record-git.sh"));
    QFile f(script);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
      return {};
    QTextStream out(&f);
    out << "#!/bin/sh\n"
        << "printf '%s\\n' \"$*\" >> '" << logPath << "'\n"
        << "exit 0\n";
    out.flush();
    f.close();
    if (!QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                           QFile::ExeOwner))
      return {};
    return script;
  }

  static const inline QString kSigner =
      QStringLiteral("0123456789ABCDEF0123456789ABCDEF01234567");

  /// A fake gpg like writeRecordingGpg() that also answers `--verify` with a
  /// VALIDSIG by kSigner and, if @p swapGpgIdTo is not empty, overwrites the
  /// store's .gpg-id with that recipient while "verifying": the attacker who
  /// wins the race between the signature check and the read.
  static QString writeSigningGpg(const QString &dir, const QString &logPath,
                                 const QString &swapGpgIdTo = QString()) {
    const QString script = QDir(dir).filePath(QStringLiteral("sign-gpg.sh"));
    QFile f(script);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
      return {};
    QTextStream out(&f);
    out << "#!/bin/sh\n"
        << "printf '%s\\n' \"$*\" >> '" << logPath << "'\n"
        << "mode=''; outfile=''; prev=''\n"
        << "for a in \"$@\"; do\n"
        << "  case \"$a\" in -d) mode=decrypt;; -eq) mode=encrypt;; "
           "--verify) mode=verify;; --detach-sign) mode=sign;; "
           "--list-secret-keys) mode=seckeys;; esac\n"
        << "  [ \"$prev\" = '--output' ] && outfile=\"$a\"\n"
        << "  prev=\"$a\"\n"
        << "done\n"
        << "case \"$mode\" in\n"
        << "  decrypt) printf 'plaintext\\n';;\n"
        << "  encrypt) cat >/dev/null; printf 'ciphertext\\n' > "
           "\"$outfile\";;\n"
        << "  sign) : > \"" << QDir(dir).filePath(".gpg-id.sig") << "\";;\n"
        << "  seckeys) printf '[GNUPG:] KEY_CONSIDERED " << kSigner
        << " 0\\n';;\n"
        << "  verify) cat >/dev/null\n";
    if (!swapGpgIdTo.isEmpty()) {
      out << "    printf '" << swapGpgIdTo << "\\n' > '"
          << QDir(dir).filePath(".gpg-id") << "'\n";
    }
    out << "    printf '[GNUPG:] VALIDSIG " << kSigner
        << " 2026-09-19 1758200000 0 4 0 1 10 00 " << kSigner << "\\n';;\n"
        << "esac\n"
        << "exit 0\n";
    out.flush();
    f.close();
    if (!QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                           QFile::ExeOwner))
      return {};
    return script;
  }

  /// A fake git that logs argv and exits with @p failOn's code when the
  /// subcommand matches, 0 otherwise; `diff --cached --quiet` reports "there
  /// is something to commit" (exit 1) so the commit path is taken.
  static QString writeGitFailingOn(const QString &dir, const QString &logPath,
                                   const QString &failOn) {
    const QString script = QDir(dir).filePath(QStringLiteral("fail-git.sh"));
    QFile f(script);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
      return {};
    QTextStream out(&f);
    out << "#!/bin/sh\n"
        << "printf '%s\\n' \"$*\" >> '" << logPath << "'\n"
        << "case \"$*\" in\n"
        << "  *' diff --cached --quiet'*) exit 1;;\n";
    if (!failOn.isEmpty())
      out << "  *' " << failOn << " '*|*' " << failOn << "') exit 128;;\n";
    out << "esac\n"
        << "exit 0\n";
    out.flush();
    f.close();
    if (!QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                           QFile::ExeOwner))
      return {};
    return script;
  }

  /// Every logged gpg call, one QStringList of arguments per call.
  static QList<QStringList> loggedCalls(const QString &logPath) {
    QList<QStringList> calls;
    QFile f(logPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
      return calls;
    for (const QString &line :
         QString::fromUtf8(f.readAll()).split(QLatin1Char('\n')))
      if (!line.isEmpty())
        calls << line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    return calls;
  }

  /// Whether any of @p calls is a `git push`.
  static bool hasPush(const QList<QStringList> &calls) {
    for (const QStringList &c : calls)
      if (c.contains(QStringLiteral("push")))
        return true;
    return false;
  }

  /// Every value that follows a -r in @p argv, in order.
  static QStringList recipientsOf(const QStringList &argv) {
    QStringList out;
    for (int i = 0; i + 1 < argv.size(); ++i)
      if (argv.at(i) == QStringLiteral("-r"))
        out << argv.at(i + 1);
    return out;
  }

  /// Only the encrypt (-eq) calls out of @p calls.
  static QList<QStringList> encryptCalls(const QList<QStringList> &calls) {
    QList<QStringList> out;
    for (const QStringList &c : calls)
      if (c.contains(QStringLiteral("-eq")))
        out << c;
    return out;
  }

  /// Signals emitted from the worker are recorded through queued connections
  /// on the test thread, so the lists are only touched from one thread.
  struct Recorder {
    QList<QPair<int, int>> progress;
    QStringList criticals;
    QStringList statusMessages;
  };
  static void record(ImitatePass &pass, QObject &ctx, Recorder &rec) {
    connect(
        &pass, &ImitatePass::reencryptProgress, &ctx,
        [&rec](int cur, int total) { rec.progress.append({cur, total}); },
        Qt::QueuedConnection);
    connect(
        &pass, &Pass::critical, &ctx,
        [&rec](const QString &, const QString &msg) { rec.criticals << msg; },
        Qt::QueuedConnection);
    connect(
        &pass, &Pass::statusMsg, &ctx,
        [&rec](const QString &msg, int) { rec.statusMessages << msg; },
        Qt::QueuedConnection);
  }

  /// Run reencryptPath() on @p storeDir with the recording fake gpg and
  /// collect what it reported.
  /// Returns false when the run did not finish in time.
  static bool runReencrypt(const QString &storeDir, Recorder &rec,
                           int *encrypts, bool *aborted) {
    const QString logPath = QDir(storeDir).filePath("gpg-argv.log");
    const QString fakeGpg = writeRecordingGpg(storeDir, logPath);
    ImitatePass pass;
    pass.init(settingsFor(storeDir, fakeGpg));
    QObject ctx;
    record(pass, ctx, rec);
    QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
    pass.reencryptPath(storeDir);
    const bool finished = endSpy.count() > 0 || endSpy.wait(15000);
    QCoreApplication::processEvents();
    *encrypts = encryptCalls(loggedCalls(logPath)).size();
    *aborted = !rec.criticals.isEmpty();
    return finished;
  }

private Q_SLOTS:
  void initTestCase();
  void reencryptPathEmitsStartSynchronouslyAndEndLater();
  void reencryptPathAggregatesFailuresIntoOneCritical();
  void reencryptPathCancelStopsBetweenFiles();
  void reencryptPathCancelInterruptsActiveProcess();
  void reencryptPathCancelKillsProcessIgnoringTerminate();
  void destructorInterruptsActiveReencryptProcess();
  void insertEncryptArgvCarriesNoEncryptTo();
  void reencryptEncryptArgvCarriesNoEncryptTo();
  void reencryptPathPushesWhenAllFilesSucceed();
  void reencryptPathDoesNotPushAfterFailures();
  void insertRunsNoGitWhenGitIsDisabled();
  void insertEncryptsToTheRecipientsWhoseSignatureWasChecked();
  void anOlderSignedGpgIdIsRefusedUntilSavedAgain();
  void reencryptUsesTheRecipientsWhoseSignatureWasChecked();
  void initCommitsGpgIdAndSignatureTogether();
  void initDoesNotReencryptWhenTheGpgIdCommitFails();
  void initRemovesTheOldSignatureWhenSigningIsOff();
  void reencryptWritesThroughItsOwnTemporaryFileOnly();
  void reencryptRestoresABackupWhoseOriginalIsMissing();
  void reencryptStopsWhenBackupAndOriginalBothExist();
  void reencryptRemovesStaleTemporaries();
  void recoveryDoesNotPromoteASymlinkToAnEntry();
  void recoveryDoesNotPromoteASpecialFileToAnEntry();
  void reencryptSkipsSymlinkedEntries();
  void removeFolderWithoutGitLeavesLinkTargetsAlone();
  void reencryptRefusesAFolderBehindALink();
  void operationsRefuseLinkedEntriesAndFoldersButRemoveUnlinks();
  void linkedGpgIdIsNotARecipientList();
  void removeLinkedFolderWithGitUnlinksAndForgets();
};

void tst_imitatepass::initTestCase() { isolateTestSettings(); }

/**
 * @brief The loop runs off the calling thread: startReencryptPath() fires
 * before reencryptPath() returns, endReencryptPath() only afterwards.
 */
void tst_imitatepass::reencryptPathEmitsStartSynchronouslyAndEndLater() {
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));

  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), unstartableGpg()));
  QSignalSpy startSpy(&pass, &ImitatePass::startReencryptPath);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);

  pass.reencryptPath(storeDir.path());
  QCOMPARE(startSpy.count(), 1);
  QVERIFY2(endSpy.isEmpty(),
           "endReencryptPath must not be emitted synchronously: the run "
           "belongs on a worker thread");

  QVERIFY2(endSpy.wait(15000), "endReencryptPath must arrive when the worker "
                               "has finished");
  QCOMPARE(endSpy.count(), 1);
}

/**
 * @brief Every file fails (gpg cannot start): the failures are reported once,
 * listing all files, with one progress step per file, and the run ends.
 */
void tst_imitatepass::reencryptPathAggregatesFailuresIntoOneCritical() {
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  const int fileCount = 3;
  QVERIFY(populateStore(storeDir.path(), fileCount));

  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), unstartableGpg()));
  QObject ctx;
  Recorder rec;
  record(pass, ctx, rec);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);

  pass.reencryptPath(storeDir.path());
  QVERIFY(endSpy.wait(15000));
  // Queued signals emitted before the completion are already delivered, but
  // flush once more so nothing posted later is missed.
  QCoreApplication::processEvents();

  QCOMPARE(rec.criticals.size(), 1);
  for (int i = 0; i < fileCount; ++i) {
    const QString name = QStringLiteral("entry%1.gpg").arg(i);
    QVERIFY2(rec.criticals.first().contains(name),
             qPrintable(QStringLiteral("aggregated failure dialog must list "
                                       "%1, got: %2")
                            .arg(name, rec.criticals.first())));
  }

  QVERIFY2(!rec.progress.isEmpty(), "progress must be reported");
  QCOMPARE(rec.progress.first(), qMakePair(0, fileCount));
  QCOMPARE(rec.progress.last(), qMakePair(fileCount, fileCount));
  // One initial (0, total) plus one step per file.
  QCOMPARE(rec.progress.size(), fileCount + 1);

  QVERIFY2(!rec.statusMessages.isEmpty(), "a summary status must be shown");
  QVERIFY2(rec.statusMessages.last().contains(QStringLiteral("3 failed")),
           qPrintable(rec.statusMessages.last()));
}

/**
 * @brief Cancelling stops the run: fewer files are checked than exist, the
 * ones that were checked before the cancel are still reported (and the one
 * the cancel interrupted is not), and a second reencryptPath() during the run
 * is ignored.
 */
void tst_imitatepass::reencryptPathCancelStopsBetweenFiles() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a slow fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  const int fileCount = 6;
  QVERIFY(populateStore(storeDir.path(), fileCount));

  // A gpg stand-in that takes a noticeable time per call and always fails, so
  // each file costs a few hundred milliseconds and ends up in the failure
  // list. The delay is what lets the cancel land while files remain.
  const QString fakeGpg = QDir(storeDir.path()).filePath("slow-gpg.sh");
  {
    QFile script(fakeGpg);
    QVERIFY(script.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(script.write("#!/bin/sh\nsleep 0.3\nexit 2\n") > 0);
    script.close();
    QVERIFY(QFile::setPermissions(
        fakeGpg, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
  }

  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), fakeGpg));
  QObject ctx;
  Recorder rec;
  record(pass, ctx, rec);
  QSignalSpy startSpy(&pass, &ImitatePass::startReencryptPath);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);

  // Cancel as soon as the first file has been checked.
  bool cancelled = false;
  connect(
      &pass, &ImitatePass::reencryptProgress, &ctx,
      [&pass, &cancelled](int cur, int) {
        if (cur >= 1 && !cancelled) {
          cancelled = true;
          pass.reencryptPath(QStringLiteral("/ignored")); // must be a no-op
          pass.cancelReencryptPath();
        }
      },
      Qt::QueuedConnection);

  pass.reencryptPath(storeDir.path());
  QVERIFY(endSpy.wait(30000));
  QCoreApplication::processEvents();

  QVERIFY(cancelled);
  QCOMPARE(startSpy.count(), 1); // the nested call did not start a second run
  QCOMPARE(endSpy.count(), 1);

  const int checked = rec.progress.last().first;
  QVERIFY2(checked >= 1, "at least the file in progress must be finished");
  QVERIFY2(checked < fileCount,
           qPrintable(QStringLiteral("cancel must stop before all %1 files "
                                     "are checked, got %2")
                          .arg(fileCount)
                          .arg(checked)));

  // Exactly the checked files failed and are reported once.
  QCOMPARE(rec.criticals.size(), 1);
  int listed = 0;
  for (int i = 0; i < fileCount; ++i) {
    if (rec.criticals.first().contains(QStringLiteral("entry%1.gpg").arg(i)))
      ++listed;
  }
  QCOMPARE(listed, checked);

  QVERIFY2(!rec.statusMessages.isEmpty(), "a summary status must be shown");
  QVERIFY2(rec.statusMessages.last().contains(QStringLiteral("cancelled")),
           qPrintable(rec.statusMessages.last()));
#endif
}

/**
 * @brief cancelReencryptPath() must interrupt the gpg in progress rather than
 * wait for it (the worker used to poll the flag only between files, so a gpg
 * stuck on pinentry pinned the run). The fake gpg blocks for far longer than
 * the bound; the run must end promptly, with no file counted or reported and
 * no failure dialog for the interrupted call.
 */
void tst_imitatepass::reencryptPathCancelInterruptsActiveProcess() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a blocking fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 3));
  const QString marker = QDir(storeDir.path()).filePath("gpg-started");
  const QString fakeGpg = writeBlockingGpg(storeDir.path(), marker, 60);
  QVERIFY2(!fakeGpg.isEmpty(), "failed to write the blocking fake gpg");

  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), fakeGpg));
  QObject ctx;
  Recorder rec;
  record(pass, ctx, rec);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);

  pass.reencryptPath(storeDir.path());
  // Only cancel once the first gpg is really blocked in its sleep.
  QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(marker), 10000);
  QElapsedTimer elapsed;
  elapsed.start();
  pass.cancelReencryptPath();
  QVERIFY2(endSpy.count() == 1 || endSpy.wait(5000),
           "cancel must end the run within seconds, not after the 60 s gpg");
  QVERIFY2(
      elapsed.elapsed() < 5000,
      qPrintable(QStringLiteral("cancel took %1 ms").arg(elapsed.elapsed())));
  QCoreApplication::processEvents();

  QCOMPARE(endSpy.count(), 1);
  QVERIFY2(rec.criticals.isEmpty(),
           qPrintable(QStringLiteral("no failure dialog expected, got: %1")
                          .arg(rec.criticals.join(QStringLiteral(" | ")))));
  QVERIFY(!rec.progress.isEmpty());
  QCOMPARE(rec.progress.last().first, 0); // the interrupted file is not counted
  QVERIFY2(!rec.statusMessages.isEmpty(), "a summary status must be shown");
  QVERIFY2(rec.statusMessages.last().contains(QStringLiteral("cancelled")),
           qPrintable(rec.statusMessages.last()));
  // The store is untouched: no temp or backup file was left behind.
  const QStringList leftovers =
      QDir(storeDir.path())
          .entryList({QStringLiteral("*.reencrypt.*")}, QDir::Files);
  QVERIFY2(leftovers.isEmpty(), qPrintable(leftovers.join(' ')));
#endif
}

/**
 * @brief A process that ignores the polite terminate() must still be ended by
 * the kill() fallback of the worker's wait loop once the grace period is
 * over. The fake gpg traps SIGTERM, so only SIGKILL can end it; the run must
 * finish well before the 60 s sleep.
 */
void tst_imitatepass::reencryptPathCancelKillsProcessIgnoringTerminate() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a blocking fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 2));
  const QString marker = QDir(storeDir.path()).filePath("gpg-started");
  const QString fakeGpg = writeBlockingGpg(storeDir.path(), marker, 60, true);
  QVERIFY2(!fakeGpg.isEmpty(), "failed to write the blocking fake gpg");

  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), fakeGpg));
  QObject ctx;
  Recorder rec;
  record(pass, ctx, rec);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);

  pass.reencryptPath(storeDir.path());
  QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(marker), 10000);
  QElapsedTimer elapsed;
  elapsed.start();
  pass.cancelReencryptPath();
  QVERIFY2(endSpy.count() == 1 || endSpy.wait(10000),
           "the forced kill must end the run, not the 60 s gpg");
  QVERIFY2(
      elapsed.elapsed() < 10000,
      qPrintable(QStringLiteral("cancel took %1 ms").arg(elapsed.elapsed())));
  QCoreApplication::processEvents();

  QCOMPARE(endSpy.count(), 1);
  QVERIFY2(rec.criticals.isEmpty(),
           qPrintable(QStringLiteral("no failure dialog expected, got: %1")
                          .arg(rec.criticals.join(QStringLiteral(" | ")))));
  QVERIFY(!rec.progress.isEmpty());
  QCOMPARE(rec.progress.last().first, 0);
  QVERIFY2(!rec.statusMessages.isEmpty(), "a summary status must be shown");
  QVERIFY2(rec.statusMessages.last().contains(QStringLiteral("cancelled")),
           qPrintable(rec.statusMessages.last()));
#endif
}

/**
 * @brief Destroying the ImitatePass while its worker is blocked in gpg must
 * not hang: the destructor cannot just time out the join (the worker touches
 * the object's members), so it sets the cancel flag, on which the worker ends
 * its own gpg, and then joins.
 */
void tst_imitatepass::destructorInterruptsActiveReencryptProcess() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a blocking fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 3));
  const QString marker = QDir(storeDir.path()).filePath("gpg-started");
  const QString fakeGpg = writeBlockingGpg(storeDir.path(), marker, 60);
  QVERIFY2(!fakeGpg.isEmpty(), "failed to write the blocking fake gpg");

  auto *pass = new ImitatePass;
  pass->init(settingsFor(storeDir.path(), fakeGpg));
  pass->reencryptPath(storeDir.path());
  QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(marker), 10000);

  QElapsedTimer elapsed;
  elapsed.start();
  delete pass; // joins the worker; must not wait for the 60 s gpg
  QVERIFY2(
      elapsed.elapsed() < 5000,
      qPrintable(
          QStringLiteral("destruction took %1 ms").arg(elapsed.elapsed())));
  // Let the worker thread's deleteLater and the queued completion (which must
  // notice the object is gone) run without touching freed memory.
  QCoreApplication::processEvents();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QCoreApplication::processEvents();
#endif
}

/**
 * @brief Insert() must encrypt with --no-encrypt-to (and --compress-algo=none,
 * as pass does): without it a `encrypt-to` line in gpg.conf silently adds a
 * recipient the .gpg-id never listed. Negative check: the flag is present on
 * the encrypt argv; positive check: the recipient from .gpg-id is still
 * passed and the insert completes.
 */
void tst_imitatepass::insertEncryptArgvCarriesNoEncryptTo() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a recording fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString fakeGpg = writeRecordingGpg(storeDir.path(), logPath);
  QVERIFY2(!fakeGpg.isEmpty(), "failed to write the recording fake gpg");

  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), fakeGpg));
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);

  pass.Insert(QDir(storeDir.path()).filePath("entry"),
              QStringLiteral("secret\n"), false);
  QVERIFY2(insertSpy.count() > 0 || insertSpy.wait(15000),
           "finishedInsert must be emitted when gpg exits 0");
  QCOMPARE(errorSpy.count(), 0);

  const QList<QStringList> enc = encryptCalls(loggedCalls(logPath));
  QCOMPARE(enc.size(), 1);
  const QStringList &argv = enc.first();
  QVERIFY2(argv.contains(QStringLiteral("--no-encrypt-to")),
           qPrintable(QStringLiteral("encrypt argv lacks --no-encrypt-to: %1")
                          .arg(argv.join(' '))));
  QVERIFY2(argv.contains(QStringLiteral("--compress-algo=none")),
           qPrintable(QStringLiteral("encrypt argv lacks --compress-algo=none: "
                                     "%1")
                          .arg(argv.join(' '))));
  // The .gpg-id recipient is still the one (and only) -r.
  QCOMPARE(argv.count(QStringLiteral("-r")), 1);
  const int r = argv.indexOf(QStringLiteral("-r"));
  QCOMPARE(argv.value(r + 1), QStringLiteral("0123456789ABCDEF"));
  // --no-encrypt-to must be an option, i.e. precede the "-" stdin marker.
  QVERIFY(argv.indexOf(QStringLiteral("--no-encrypt-to")) <
          argv.lastIndexOf(QStringLiteral("-")));
#endif
}

/**
 * @brief The per-file re-encryption in the worker is the second place QtPass
 * encrypts; it must carry the same flags. The decrypt (-d) calls around it
 * are left as they were.
 */
void tst_imitatepass::reencryptEncryptArgvCarriesNoEncryptTo() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a recording fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  const int fileCount = 2;
  QVERIFY(populateStore(storeDir.path(), fileCount));
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString fakeGpg = writeRecordingGpg(storeDir.path(), logPath);
  QVERIFY2(!fakeGpg.isEmpty(), "failed to write the recording fake gpg");

  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), fakeGpg));
  QObject ctx;
  Recorder rec;
  record(pass, ctx, rec);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);

  pass.reencryptPath(storeDir.path());
  QVERIFY(endSpy.wait(30000));
  QCoreApplication::processEvents();

  // Positive: with a cooperating gpg every file is re-encrypted, none fails.
  QVERIFY2(rec.criticals.isEmpty(),
           qPrintable(rec.criticals.join(QStringLiteral(" | "))));
  QVERIFY(!rec.statusMessages.isEmpty());
  QVERIFY2(rec.statusMessages.last().contains(
               QStringLiteral("%1 files re-encrypted").arg(fileCount)),
           qPrintable(rec.statusMessages.last()));

  const QList<QStringList> enc = encryptCalls(loggedCalls(logPath));
  QCOMPARE(enc.size(), fileCount);
  for (const QStringList &argv : enc) {
    QVERIFY2(argv.contains(QStringLiteral("--no-encrypt-to")),
             qPrintable(QStringLiteral("encrypt argv lacks --no-encrypt-to: %1")
                            .arg(argv.join(' '))));
    QVERIFY2(argv.contains(QStringLiteral("--compress-algo=none")),
             qPrintable(QStringLiteral("encrypt argv lacks "
                                       "--compress-algo=none: %1")
                            .arg(argv.join(' '))));
    QCOMPARE(argv.count(QStringLiteral("-r")), 1);
    const int r = argv.indexOf(QStringLiteral("-r"));
    QCOMPARE(argv.value(r + 1), QStringLiteral("0123456789ABCDEF"));
  }
#endif
}

/**
 * @brief Positive control for the auto-push gate: with git configured,
 * autoPush on and a cooperating gpg, the run ends with a `git push`.
 */
void tst_imitatepass::reencryptPathPushesWhenAllFilesSucceed() {
#ifdef Q_OS_WIN
  QSKIP("uses shell scripts as recording fake gpg and git");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 2));
  const QString gpgLog = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString gitLog = QDir(storeDir.path()).filePath("git-argv.log");
  const QString fakeGpg = writeRecordingGpg(storeDir.path(), gpgLog);
  const QString fakeGit = writeRecordingGit(storeDir.path(), gitLog);
  QVERIFY2(!fakeGpg.isEmpty(), "failed to write the recording fake gpg");
  QVERIFY2(!fakeGit.isEmpty(), "failed to write the recording fake git");

  AppSettings settings = settingsFor(storeDir.path(), fakeGpg);
  settings.useGit = true;
  settings.gitExecutable = fakeGit;
  settings.autoPush = true;
  ImitatePass pass;
  pass.init(settings);
  QObject ctx;
  Recorder rec;
  record(pass, ctx, rec);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);

  pass.reencryptPath(storeDir.path());
  QVERIFY(endSpy.wait(30000));
  QCoreApplication::processEvents();

  QVERIFY2(rec.criticals.isEmpty(),
           qPrintable(rec.criticals.join(QStringLiteral(" | "))));
  // The push goes through the asynchronous Executor queue, so it may land a
  // little after endReencryptPath().
  QTRY_VERIFY_WITH_TIMEOUT(hasPush(loggedCalls(gitLog)), 15000);
#endif
}

/**
 * @brief With Git disabled, Insert() must touch git at all: gitReady() returns
 * m_settings.useGit, so nothing may be staged or committed even when a git
 * executable is configured.
 */
void tst_imitatepass::insertRunsNoGitWhenGitIsDisabled() {
#ifdef Q_OS_WIN
  QSKIP("uses shell scripts as recording fake gpg and git");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QString gpgLog = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString gitLog = QDir(storeDir.path()).filePath("git-argv.log");
  const QString fakeGpg = writeRecordingGpg(storeDir.path(), gpgLog);
  const QString fakeGit = writeRecordingGit(storeDir.path(), gitLog);
  QVERIFY2(!fakeGpg.isEmpty(), "failed to write the recording fake gpg");
  QVERIFY2(!fakeGit.isEmpty(), "failed to write the recording fake git");

  AppSettings settings = settingsFor(storeDir.path(), fakeGpg);
  settings.useGit = false;
  settings.gitExecutable = fakeGit;
  ImitatePass pass;
  pass.init(settings);
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);

  pass.Insert(QDir(storeDir.path()).filePath("entry"),
              QStringLiteral("secret\n"), false);
  QVERIFY2(insertSpy.count() > 0 || insertSpy.wait(15000),
           "finishedInsert must be emitted when gpg exits 0");

  // Insert() queues its git calls right after the gpg one, and the Executor
  // runs a single FIFO queue, so a second Insert finishing proves that
  // everything queued by the first has already run. No sleeping involved.
  pass.Insert(QDir(storeDir.path()).filePath("second"),
              QStringLiteral("secret\n"), false);
  QTRY_COMPARE_WITH_TIMEOUT(insertSpy.count(), 2, 15000);

  QVERIFY2(!QFile::exists(gitLog),
           "Insert must not run git when Use Git is off");
#endif
}

/**
 * @brief When a file failed to re-encrypt the store is not pushed even with
 * autoPush on: the remote must not receive recipient metadata that does not
 * match every encrypted file. The user is told why the push was skipped.
 */
void tst_imitatepass::reencryptPathDoesNotPushAfterFailures() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a recording fake git");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 2));
  const QString gitLog = QDir(storeDir.path()).filePath("git-argv.log");
  const QString fakeGit = writeRecordingGit(storeDir.path(), gitLog);
  QVERIFY2(!fakeGit.isEmpty(), "failed to write the recording fake git");

  AppSettings settings = settingsFor(storeDir.path(), unstartableGpg());
  settings.useGit = true;
  settings.gitExecutable = fakeGit;
  settings.autoPush = true;
  ImitatePass pass;
  pass.init(settings);
  QObject ctx;
  Recorder rec;
  record(pass, ctx, rec);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);

  pass.reencryptPath(storeDir.path());
  QVERIFY(endSpy.wait(30000));
  // Give a wrongly queued push time to reach the fake git before checking.
  QTest::qWait(500);

  QCOMPARE(rec.criticals.size(), 1);
  const QList<QStringList> gitCalls = loggedCalls(gitLog);
  QVERIFY2(!gitCalls.isEmpty(),
           "the fake git must have been used for the backup status check");
  QVERIFY2(!hasPush(gitCalls),
           "a run with failed files must not be pushed to the remote");
  QVERIFY(!rec.statusMessages.isEmpty());
  QVERIFY2(rec.statusMessages.last().contains(QStringLiteral("Not pushing")),
           qPrintable(rec.statusMessages.last()));
#endif
}

/**
 * @brief #1842: a signed .gpg-id used to be verified by path and then read
 *        again; whoever could write the store in between chose the
 *        recipients. The bytes that pass verification are the bytes that are
 *        parsed, so the swap changes nothing.
 */
void tst_imitatepass::insertEncryptsToTheRecipientsWhoseSignatureWasChecked() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString fakeGpg = writeSigningGpg(storeDir.path(), logPath,
                                          QStringLiteral("MALLORY0MALLORY0"));
  QVERIFY(!fakeGpg.isEmpty());

  ImitatePass pass;
  AppSettings s = settingsFor(storeDir.path(), fakeGpg);
  s.passSigningKey = kSigner;
  pass.init(s);
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  pass.Insert(QDir(storeDir.path()).filePath("entry"),
              QStringLiteral("secret\n"), false);
  QVERIFY2(insertSpy.count() > 0 || insertSpy.wait(15000),
           "the signature is valid, so the insert must go through");
  QCOMPARE(criticalSpy.count(), 0);

  const QList<QStringList> calls = loggedCalls(logPath);
  QVERIFY2(std::any_of(calls.cbegin(), calls.cend(),
                       [](const QStringList &c) {
                         return c.contains(QStringLiteral("--verify")) &&
                                c.last() == QStringLiteral("-");
                       }),
           "the signature must be checked against stdin, not a path");
  const QList<QStringList> enc = encryptCalls(calls);
  QCOMPARE(enc.size(), 1);
  QCOMPARE(recipientsOf(enc.first()),
           QStringList{QStringLiteral("0123456789ABCDEF")});
#endif
}

/**
 * @brief Rollback: a genuinely signed recipient list that names a member
 *        since removed is put back into the store. The signature is valid,
 *        so before generations it was accepted and the next entry encrypted
 *        to the removed member. Now: Init writes generation 1 and then 2;
 *        the generation-1 pair restored is refused with a message naming
 *        both numbers, nothing is encrypted; saving the list again writes 3
 *        and the store works again.
 */
void tst_imitatepass::anOlderSignedGpgIdIsRefusedUntilSavedAgain() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QString gpgIdFile = QDir(storeDir.path()).filePath(".gpg-id");
  const QString sigFile = gpgIdFile + ".sig";
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString fakeGpg = writeSigningGpg(storeDir.path(), logPath);
  QVERIFY(!fakeGpg.isEmpty());
  ImitatePass pass;
  AppSettings s = settingsFor(storeDir.path(), fakeGpg);
  s.passSigningKey = kSigner;
  pass.init(s);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  UserInfo alice;
  alice.key_id = kSigner;
  alice.enabled = true;
  alice.have_secret = true;
  UserInfo bob;
  bob.key_id = QStringLiteral("89ABCDEF0123456789ABCDEF0123456789ABCDEF");
  bob.enabled = true;
  const QString store = QDir(storeDir.path()).path() + QLatin1Char('/');

  // Generation 1: Alice and Bob.
  pass.Init(store, {alice, bob});
  QVERIFY(endSpy.count() > 0 || endSpy.wait(15000));
  endSpy.clear();
  QFile g1(gpgIdFile);
  QVERIFY(g1.open(QIODevice::ReadOnly));
  const QByteArray gen1 = g1.readAll();
  g1.close();
  QVERIFY2(gen1.startsWith("# QtPass-GpgId-Generation: 1\n"), gen1.constData());
  QVERIFY(gen1.contains(bob.key_id.toUtf8()));
  QFile s1(sigFile);
  QVERIFY(s1.open(QIODevice::ReadOnly));
  const QByteArray sig1 = s1.readAll();
  s1.close();

  // Generation 2: Bob is out.
  pass.Init(store, {alice});
  QVERIFY(endSpy.count() > 0 || endSpy.wait(15000));
  endSpy.clear();
  {
    QFile g2(gpgIdFile);
    QVERIFY(g2.open(QIODevice::ReadOnly));
    const QByteArray gen2 = g2.readAll();
    QVERIFY2(gen2.startsWith("# QtPass-GpgId-Generation: 2\n"),
             gen2.constData());
    QVERIFY(!gen2.contains(bob.key_id.toUtf8()));
  }
  QCOMPARE(criticalSpy.count(), 0);

  // The attacker puts the generation-1 pair back. Its signature is genuine.
  {
    QFile g(gpgIdFile);
    QVERIFY(g.open(QIODevice::WriteOnly | QIODevice::Truncate));
    g.write(gen1);
    QFile sg(sigFile);
    QVERIFY(sg.open(QIODevice::WriteOnly | QIODevice::Truncate));
    sg.write(sig1);
  }
  QFile::remove(logPath);
  pass.Insert(QDir(storeDir.path()).filePath("entry"),
              QStringLiteral("secret\n"), false);
  QTest::qWait(500);
  QCOMPARE(insertSpy.count(), 0);
  QCOMPARE(criticalSpy.count(), 1);
  const QString why = criticalSpy.takeFirst().at(1).toString();
  QVERIFY2(why.contains(QStringLiteral("generation 1")) &&
               why.contains(QStringLiteral("generation 2")) &&
               why.contains(QStringLiteral("Users")),
           qPrintable(why));
  QVERIFY2(encryptCalls(loggedCalls(logPath)).isEmpty(),
           "nothing may be encrypted to the rolled-back list");

  // The way through: save the list again, which writes generation 3.
  pass.Init(store, {alice});
  QVERIFY(endSpy.count() > 0 || endSpy.wait(15000));
  QFile g3(gpgIdFile);
  QVERIFY(g3.open(QIODevice::ReadOnly));
  QVERIFY(g3.readAll().startsWith("# QtPass-GpgId-Generation: 3\n"));
  g3.close();
  QFile::remove(logPath);
  pass.Insert(QDir(storeDir.path()).filePath("entry"),
              QStringLiteral("secret\n"), false);
  QVERIFY(insertSpy.count() > 0 || insertSpy.wait(15000));
  QCOMPARE(criticalSpy.count(), 0);
  QCOMPARE(recipientsOf(encryptCalls(loggedCalls(logPath)).first()),
           QStringList{kSigner});
#endif
}

void tst_imitatepass::reencryptUsesTheRecipientsWhoseSignatureWasChecked() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 2));
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString fakeGpg = writeSigningGpg(storeDir.path(), logPath,
                                          QStringLiteral("MALLORY0MALLORY0"));
  QVERIFY(!fakeGpg.isEmpty());

  ImitatePass pass;
  AppSettings s = settingsFor(storeDir.path(), fakeGpg);
  s.passSigningKey = kSigner;
  pass.init(s);
  QObject ctx;
  Recorder rec;
  record(pass, ctx, rec);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
  pass.reencryptPath(storeDir.path());
  QVERIFY(endSpy.count() > 0 || endSpy.wait(15000));
  QCoreApplication::processEvents();
  QVERIFY2(rec.criticals.isEmpty(), qPrintable(rec.criticals.join("; ")));

  const QList<QStringList> enc = encryptCalls(loggedCalls(logPath));
  QCOMPARE(enc.size(), 2);
  for (const QStringList &argv : enc) {
    QCOMPARE(recipientsOf(argv),
             QStringList{QStringLiteral("0123456789ABCDEF")});
  }
#endif
}

/**
 * @brief The recipient list and its signature go into one commit; two
 *        commits left the repository with a new list under the old signature
 *        in between, for good when the second one failed.
 */
void tst_imitatepass::initCommitsGpgIdAndSignatureTogether() {
#ifdef Q_OS_WIN
  QSKIP("uses shell scripts as fake gpg and git");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString gpgLog = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString gitLog = QDir(storeDir.path()).filePath("git-argv.log");
  const QString fakeGpg = writeSigningGpg(storeDir.path(), gpgLog);
  const QString fakeGit = writeGitFailingOn(storeDir.path(), gitLog, QString());
  QVERIFY(!fakeGpg.isEmpty() && !fakeGit.isEmpty());

  ImitatePass pass;
  AppSettings s = settingsFor(storeDir.path(), fakeGpg);
  s.passSigningKey = kSigner;
  s.useGit = true;
  s.addGPGId = true;
  s.gitExecutable = fakeGit;
  pass.init(s);
  QSignalSpy initSpy(&pass, &Pass::finishedInit);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
  UserInfo alice;
  alice.key_id = QStringLiteral("0123456789ABCDEF");
  alice.enabled = true;
  pass.Init(QDir(storeDir.path()).path() + QLatin1Char('/'), {alice});
  QVERIFY(endSpy.count() > 0 || endSpy.wait(15000));
  QVERIFY2(initSpy.count() > 0 || initSpy.wait(5000),
           "finishedInit must follow a successful commit");

  const QList<QStringList> git = loggedCalls(gitLog);
  QList<QStringList> commits;
  for (const QStringList &c : git)
    if (c.contains(QStringLiteral("commit")))
      commits << c;
  QVERIFY2(!commits.isEmpty(), "the .gpg-id must be committed");
  const QStringList &first = commits.first();
  const QStringList paths =
      first.mid(first.lastIndexOf(QStringLiteral("--")) + 1);
  QVERIFY2(paths.size() == 2 && paths.first().endsWith(".gpg-id") &&
               paths.last().endsWith(".gpg-id.sig"),
           qPrintable("one commit with both files, got: " + first.join(' ')));
  // The re-encryption's own commits come after, never before, that commit.
  const QList<QStringList> enc = encryptCalls(loggedCalls(gpgLog));
  QCOMPARE(enc.size(), 1);
#endif
}

void tst_imitatepass::initDoesNotReencryptWhenTheGpgIdCommitFails() {
#ifdef Q_OS_WIN
  QSKIP("uses shell scripts as fake gpg and git");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString gpgLog = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString gitLog = QDir(storeDir.path()).filePath("git-argv.log");
  const QString fakeGpg = writeSigningGpg(storeDir.path(), gpgLog);
  const QString fakeGit =
      writeGitFailingOn(storeDir.path(), gitLog, QStringLiteral("commit"));
  QVERIFY(!fakeGpg.isEmpty() && !fakeGit.isEmpty());

  ImitatePass pass;
  AppSettings s = settingsFor(storeDir.path(), fakeGpg);
  s.passSigningKey = kSigner;
  s.useGit = true;
  s.addGPGId = true;
  s.gitExecutable = fakeGit;
  pass.init(s);
  QSignalSpy initSpy(&pass, &Pass::finishedInit);
  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);
  QSignalSpy startSpy(&pass, &ImitatePass::startReencryptPath);
  UserInfo alice;
  alice.key_id = QStringLiteral("0123456789ABCDEF");
  alice.enabled = true;
  pass.Init(QDir(storeDir.path()).path() + QLatin1Char('/'), {alice});
  QVERIFY2(errorSpy.count() > 0 || errorSpy.wait(5000),
           "a failed commit must be reported");
  QCOMPARE(initSpy.count(), 0);
  QTest::qWait(500);
  QCOMPARE(startSpy.count(), 0);
  QVERIFY2(encryptCalls(loggedCalls(gpgLog)).isEmpty(),
           "nothing may be re-encrypted to a list the repository lacks");
#endif
}

/**
 * @brief Switching signing off must not leave the previous list's signature
 *        next to the new list, where pass would reject it.
 */
void tst_imitatepass::initRemovesTheOldSignatureWhenSigningIsOff() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QString sig = QDir(storeDir.path()).filePath(".gpg-id.sig");
  {
    QFile f(sig);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("old signature");
  }
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString fakeGpg = writeRecordingGpg(storeDir.path(), logPath);
  QVERIFY(!fakeGpg.isEmpty());
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), fakeGpg)); // no signing key
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
  UserInfo alice;
  alice.key_id = QStringLiteral("0123456789ABCDEF");
  alice.enabled = true;
  pass.Init(QDir(storeDir.path()).path() + QLatin1Char('/'), {alice});
  QVERIFY(endSpy.count() > 0 || endSpy.wait(15000));
  QVERIFY2(!QFile::exists(sig), "the stale .gpg-id.sig must be gone");
#endif
}

/**
 * @brief The new ciphertext goes to a file QtPass created itself. A file (or
 *        symlink) planted at the old predictable <file>.reencrypt.tmp is
 *        never written through, and the temporary is gone afterwards.
 */
void tst_imitatepass::reencryptWritesThroughItsOwnTemporaryFileOnly() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg and a symlink");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString entry = QDir(storeDir.path()).filePath("entry0.gpg");
  const QString canary = QDir(storeDir.path()).filePath("canary");
  {
    QFile f(canary);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("untouched");
  }
  // The attacker's guess at the old name, pointing at something they want
  // overwritten.
  QVERIFY(QFile::link(canary, entry + ".reencrypt.tmp"));
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString fakeGpg = writeRecordingGpg(storeDir.path(), logPath);
  QVERIFY(!fakeGpg.isEmpty());

  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), fakeGpg));
  QObject ctx;
  Recorder rec;
  record(pass, ctx, rec);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
  pass.reencryptPath(storeDir.path());
  QVERIFY(endSpy.count() > 0 || endSpy.wait(15000));
  QCoreApplication::processEvents();
  QVERIFY2(rec.criticals.isEmpty(), qPrintable(rec.criticals.join("; ")));

  QFile check(canary);
  QVERIFY(check.open(QIODevice::ReadOnly));
  QCOMPARE(check.readAll(), QByteArrayLiteral("untouched"));
  const QList<QStringList> enc = encryptCalls(loggedCalls(logPath));
  QCOMPARE(enc.size(), 1);
  const QString output =
      enc.first().value(enc.first().indexOf(QStringLiteral("--output")) + 1);
  QVERIFY2(output != entry + ".reencrypt.tmp" &&
               output.startsWith(entry + ".") && output.endsWith(".tmp"),
           qPrintable("gpg must write to QtPass's own temporary: " + output));
  QVERIFY2(!QFile::exists(output), "the temporary must not be left behind");
  QFile result(entry);
  QVERIFY(result.open(QIODevice::ReadOnly));
  QCOMPARE(result.readAll(), QByteArrayLiteral("ciphertext\n"));
  const QStringList leftovers =
      QDir(storeDir.path())
          .entryList({QStringLiteral("*.tmp"), QStringLiteral("*.bak")},
                     QDir::Files | QDir::System);
  QCOMPARE(leftovers, QStringList{QStringLiteral("entry0.gpg.reencrypt.tmp")});
#endif
}

/**
 * @brief A crash between the two renames leaves X.gpg.reencrypt.bak and no
 *        X.gpg: the backup is the only copy, so the next run puts it back
 *        first and then re-encrypts it like any other entry.
 */
void tst_imitatepass::reencryptRestoresABackupWhoseOriginalIsMissing() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString entry = QDir(storeDir.path()).filePath("entry0.gpg");
  QVERIFY(QFile::rename(entry, entry + ".reencrypt.bak"));
  Recorder rec;
  int encrypts = 0;
  bool aborted = true;
  QVERIFY2(runReencrypt(storeDir.path(), rec, &encrypts, &aborted),
           "re-encryption must finish");
  QVERIFY2(!aborted, qPrintable(rec.criticals.join("; ")));
  QVERIFY2(QFile::exists(entry), "the entry must be back under its name");
  QVERIFY(!QFile::exists(entry + ".reencrypt.bak"));
  QCOMPARE(encrypts, 1);
  QVERIFY2(std::any_of(rec.statusMessages.cbegin(), rec.statusMessages.cend(),
                       [](const QString &m) { return m.contains("Restored"); }),
           "the user is told what happened");
#endif
}

/**
 * @brief A backup next to a present original: two valid ciphertexts, and
 *        QtPass must not pick one. It reports and does nothing.
 */
void tst_imitatepass::reencryptStopsWhenBackupAndOriginalBothExist() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString entry = QDir(storeDir.path()).filePath("entry0.gpg");
  QVERIFY(QFile::copy(entry, entry + ".reencrypt.bak"));
  Recorder rec;
  int encrypts = 0;
  bool aborted = false;
  QVERIFY2(runReencrypt(storeDir.path(), rec, &encrypts, &aborted),
           "re-encryption must finish");
  QVERIFY(aborted);
  QVERIFY(rec.criticals.first().contains(".reencrypt.bak"));
  QCOMPARE(encrypts, 0);
  QVERIFY(QFile::exists(entry) && QFile::exists(entry + ".reencrypt.bak"));
#endif
}

/**
 * @brief A temporary gpg was writing when the process died is never a
 *        source of truth; it is removed before the run.
 */
void tst_imitatepass::reencryptRemovesStaleTemporaries() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString stale = QDir(storeDir.path()).filePath("entry0.gpg.aB3xYz.tmp");
  {
    QFile f(stale);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("half a ciphertext");
  }
  Recorder rec;
  int encrypts = 0;
  bool aborted = true;
  QVERIFY2(runReencrypt(storeDir.path(), rec, &encrypts, &aborted),
           "re-encryption must finish");
  QVERIFY2(!aborted, qPrintable(rec.criticals.join("; ")));
  QVERIFY(!QFile::exists(stale));
  QCOMPARE(encrypts, 1);
  QCOMPARE(QDir(storeDir.path())
               .entryList({QStringLiteral("*.tmp"), QStringLiteral("*.bak")},
                          QDir::Files),
           QStringList());
#endif
}

/**
 * @brief A symlink under a backup's name is not a backup QtPass made:
 *        renaming it into place would make a symlink the entry and the run
 *        would re-encrypt whatever it points to. It is reported, not used.
 */
void tst_imitatepass::recoveryDoesNotPromoteASymlinkToAnEntry() {
#ifdef Q_OS_WIN
  QSKIP("uses a symlink and a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QTemporaryDir outsideDir;
  QVERIFY(storeDir.isValid() && outsideDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QString outside = QDir(outsideDir.path()).filePath("secret.gpg");
  {
    QFile f(outside);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("somebody else's ciphertext");
  }
  const QString entry = QDir(storeDir.path()).filePath("entry0.gpg");
  QVERIFY(QFile::link(outside, entry + ".reencrypt.bak"));
  Recorder rec;
  int encrypts = 0;
  bool aborted = false;
  QVERIFY2(runReencrypt(storeDir.path(), rec, &encrypts, &aborted),
           "re-encryption must finish");
  QVERIFY(aborted);
  QVERIFY(rec.criticals.first().contains("not a regular file"));
  QVERIFY2(!QFileInfo::exists(entry) && !QFileInfo(entry).isSymLink(),
           "no entry may appear under the backup's name");
  QCOMPARE(encrypts, 0);
  QFile check(outside);
  QVERIFY(check.open(QIODevice::ReadOnly));
  QCOMPARE(check.readAll(), QByteArrayLiteral("somebody else's ciphertext"));
#endif
}

/**
 * @brief A FIFO under a backup's name is no backup either: reported, not
 *        renamed into place, and the run stops for the user to look.
 */
void tst_imitatepass::recoveryDoesNotPromoteASpecialFileToAnEntry() {
#ifdef Q_OS_WIN
  QSKIP("uses mkfifo and a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QString entry = QDir(storeDir.path()).filePath("entry0.gpg");
  QCOMPARE(
      mkfifo(QFile::encodeName(entry + ".reencrypt.bak").constData(), 0600), 0);
  Recorder rec;
  int encrypts = 0;
  bool aborted = false;
  QVERIFY2(runReencrypt(storeDir.path(), rec, &encrypts, &aborted),
           "re-encryption must finish");
  QVERIFY(aborted);
  QVERIFY(rec.criticals.first().contains("not a regular file"));
  QVERIFY2(!QFileInfo::exists(entry), "no entry may appear under the name");
  QCOMPARE(encrypts, 0);
#endif
}

/**
 * @brief A symlinked .gpg is not a password entry: the run leaves it alone,
 *        re-encrypts the regular files, and says how many it skipped.
 */
void tst_imitatepass::reencryptSkipsSymlinkedEntries() {
#ifdef Q_OS_WIN
  QSKIP("uses a symlink and a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QTemporaryDir outsideDir;
  QVERIFY(storeDir.isValid() && outsideDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString outside = QDir(outsideDir.path()).filePath("secret.gpg");
  {
    QFile f(outside);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("somebody else's ciphertext");
  }
  QVERIFY(QFile::link(outside, QDir(storeDir.path()).filePath("escape.gpg")));
  // A symlinked directory must not be descended into either.
  QVERIFY(QFile::link(outsideDir.path(),
                      QDir(storeDir.path()).filePath("elsewhere")));
  Recorder rec;
  int encrypts = 0;
  bool aborted = true;
  QVERIFY2(runReencrypt(storeDir.path(), rec, &encrypts, &aborted),
           "re-encryption must finish");
  QVERIFY2(!aborted, qPrintable(rec.criticals.join("; ")));
  QCOMPARE(encrypts, 1);
  QFile check(outside);
  QVERIFY(check.open(QIODevice::ReadOnly));
  QCOMPARE(check.readAll(), QByteArrayLiteral("somebody else's ciphertext"));
  QVERIFY2(std::any_of(rec.statusMessages.cbegin(), rec.statusMessages.cend(),
                       [](const QString &m) { return m.contains("symlink"); }),
           "the user is told an entry was skipped");
#endif
}

/**
 * @brief Deleting a folder without git must not reach through a link: a link
 *        inside the folder goes as an entry, and a folder that is itself a
 *        link (the tree shows those) is unlinked, not emptied.
 *        QDir::removeRecursively() would have listed through the top-level
 *        link and deleted the target's files.
 */
void tst_imitatepass::removeFolderWithoutGitLeavesLinkTargetsAlone() {
#ifdef Q_OS_WIN
  QSKIP("creating a symlink needs a privilege a CI runner may lack");
#else
  QTemporaryDir storeDir;
  QTemporaryDir outsideDir;
  QVERIFY(storeDir.isValid() && outsideDir.isValid());
  const QDir root(storeDir.path());
  const QDir outside(outsideDir.path());
  QVERIFY(root.mkpath(QStringLiteral("folder")));
  for (const QString &path : {root.filePath(QStringLiteral("folder/a.gpg")),
                              outside.filePath(QStringLiteral("secret.gpg"))}) {
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
  }
  QVERIFY(QFile::link(outsideDir.path(),
                      root.filePath(QStringLiteral("folder/elsewhere"))));
  QVERIFY(QFile::link(outsideDir.path(),
                      root.filePath(QStringLiteral("linked-folder"))));
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), QStringLiteral("/nonexistent/gpg")));
  pass.Remove(QStringLiteral("folder"), true);
  QVERIFY(!QFileInfo::exists(root.filePath(QStringLiteral("folder"))));
  QVERIFY2(QFile::exists(outside.filePath(QStringLiteral("secret.gpg"))),
           "the link's target must keep its files");
  // The tree names a folder with a trailing separator; that must not turn
  // the link into its target.
  pass.Remove(QStringLiteral("linked-folder/"), true);
  QVERIFY2(
      !QFileInfo(root.filePath(QStringLiteral("linked-folder"))).isSymLink(),
      "the link itself is what gets removed");
  QVERIFY2(QFile::exists(outside.filePath(QStringLiteral("secret.gpg"))),
           "a folder that is a link must not have its target emptied");
#endif
}

/**
 * @brief A run started on a linked folder, or on a folder behind one, is
 *        refused before anything is walked: the files behind the link are
 *        not the store's, whatever .gpg-id a lexical walk up would find.
 *        The store root itself may be a link and is not refused.
 */
void tst_imitatepass::reencryptRefusesAFolderBehindALink() {
#ifdef Q_OS_WIN
  QSKIP("creating a symlink needs a privilege a CI runner may lack");
#else
  QTemporaryDir storeDir;
  QTemporaryDir outsideDir;
  QVERIFY(storeDir.isValid() && outsideDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QDir outside(outsideDir.path());
  QVERIFY(outside.mkpath(QStringLiteral("sub")));
  {
    QFile f(outside.filePath(QStringLiteral("sub/secret.gpg")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("somebody else's ciphertext");
  }
  QVERIFY(
      QFile::link(outsideDir.path(),
                  QDir(storeDir.path()).filePath(QStringLiteral("shared"))));
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString fakeGpg = writeRecordingGpg(storeDir.path(), logPath);
  QVERIFY(!fakeGpg.isEmpty());
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), fakeGpg));
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  for (const QString &pick :
       {QDir(storeDir.path()).filePath(QStringLiteral("shared")),
        QDir(storeDir.path()).filePath(QStringLiteral("shared/sub/"))}) {
    pass.reencryptPath(pick);
    QCOMPARE(endSpy.count(), 1);
    QCOMPARE(criticalSpy.count(), 1);
    QVERIFY(criticalSpy.takeFirst().at(1).toString().contains("link"));
    endSpy.clear();
  }
  QVERIFY2(encryptCalls(loggedCalls(logPath)).isEmpty(),
           "nothing behind the link may be touched");
  QFile check(outside.filePath(QStringLiteral("sub/secret.gpg")));
  QVERIFY(check.open(QIODevice::ReadOnly));
  QCOMPARE(check.readAll(), QByteArrayLiteral("somebody else's ciphertext"));
#endif
}

/**
 * @brief Show, edit (Insert), Move and Copy refuse an entry that is a link or
 *        lies behind a linked folder, before gpg or git is asked anything;
 *        Remove of the link itself unlinks it and leaves the target alone,
 *        Remove of something behind a link is refused. Init on a linked
 *        folder is refused as well.
 */
void tst_imitatepass::
    operationsRefuseLinkedEntriesAndFoldersButRemoveUnlinks() {
#ifdef Q_OS_WIN
  QSKIP("uses symlinks and a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QTemporaryDir outsideDir;
  QVERIFY(storeDir.isValid() && outsideDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QDir root(storeDir.path());
  const QDir outside(outsideDir.path());
  QVERIFY(outside.mkpath(QStringLiteral("sub")));
  for (const QString &name :
       {QStringLiteral("secret.gpg"), QStringLiteral("sub/deep.gpg")}) {
    QFile f(outside.filePath(name));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("somebody else's ciphertext");
  }
  QVERIFY(QFile::link(outside.filePath(QStringLiteral("secret.gpg")),
                      root.filePath(QStringLiteral("Bank.gpg"))));
  QVERIFY(
      QFile::link(outsideDir.path(), root.filePath(QStringLiteral("shared"))));
  const QString logPath = root.filePath("gpg-argv.log");
  const QString fakeGpg = writeRecordingGpg(storeDir.path(), logPath);
  QVERIFY(!fakeGpg.isEmpty());
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), fakeGpg));
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);
  QSignalSpy showSpy(&pass, &Pass::finishedShow);

  pass.Show(QStringLiteral("Bank"));
  pass.Show(QStringLiteral("shared/sub/deep"));
  pass.Insert(QStringLiteral("Bank"), QStringLiteral("new secret"), true);
  pass.Insert(QStringLiteral("shared/fresh"), QStringLiteral("new"), false);
  pass.Move(root.filePath(QStringLiteral("Bank.gpg")),
            root.filePath(QStringLiteral("Moved.gpg")), false);
  pass.Move(root.filePath(QStringLiteral("entry0.gpg")),
            root.filePath(QStringLiteral("shared/")), false);
  pass.Copy(root.filePath(QStringLiteral("Bank.gpg")),
            root.filePath(QStringLiteral("Copied.gpg")), false);
  pass.Copy(root.filePath(QStringLiteral("entry0.gpg")),
            root.filePath(QStringLiteral("shared/sub/")), false);
  pass.Remove(QStringLiteral("shared/sub/deep"), false);
  pass.Remove(QStringLiteral("shared/sub"), true);
  UserInfo alice;
  alice.key_id = QStringLiteral("0123456789ABCDEF");
  alice.enabled = true;
  pass.Init(root.filePath(QStringLiteral("shared/")), {alice});
  // A real folder with a link planted under the .gpg-id name: QSaveFile
  // would write the recipient list through it.
  QVERIFY(root.mkpath(QStringLiteral("team")));
  {
    QFile f(outside.filePath(QStringLiteral("victim.txt")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("precious");
  }
  QVERIFY(QFile::link(outside.filePath(QStringLiteral("victim.txt")),
                      root.filePath(QStringLiteral("team/.gpg-id"))));
  pass.Init(root.filePath(QStringLiteral("team/")), {alice});
  // A drop copies onto the folder; the file written is <folder>/<name>, and
  // a dangling link there passes exists().
  QVERIFY(root.mkpath(QStringLiteral("other")));
  QVERIFY(QFile::link(outside.filePath(QStringLiteral("not-yet.gpg")),
                      root.filePath(QStringLiteral("other/entry0.gpg"))));
  pass.Copy(root.filePath(QStringLiteral("entry0.gpg")),
            root.filePath(QStringLiteral("other")), false);
  QTest::qWait(300);
  QCOMPARE(criticalSpy.count(), 13);
  QVERIFY2(errorSpy.count() == 13,
           "every refusal ends the operation for the interface as well");
  {
    QFile check(outside.filePath(QStringLiteral("victim.txt")));
    QVERIFY(check.open(QIODevice::ReadOnly));
    QCOMPARE(check.readAll(), QByteArrayLiteral("precious"));
  }
  QVERIFY(!QFileInfo::exists(outside.filePath(QStringLiteral("not-yet.gpg"))));
  for (const QList<QVariant> &sig : criticalSpy) {
    QVERIFY2(sig.at(1).toString().contains(QStringLiteral("link")),
             qPrintable(sig.at(1).toString()));
  }
  QCOMPARE(showSpy.count(), 0);
  QVERIFY2(loggedCalls(logPath).isEmpty(), "gpg must not have been run");
  QVERIFY(!QFileInfo::exists(root.filePath(QStringLiteral("Moved.gpg"))));
  QVERIFY(!QFileInfo::exists(root.filePath(QStringLiteral("Copied.gpg"))));
  QVERIFY(!QFileInfo::exists(outside.filePath(QStringLiteral("entry0.gpg"))));
  QVERIFY(!QFileInfo::exists(outside.filePath(QStringLiteral("fresh.gpg"))));
  QVERIFY(!QFileInfo::exists(outside.filePath(QStringLiteral(".gpg-id"))));
  QVERIFY(QFile::exists(outside.filePath(QStringLiteral("sub/deep.gpg"))));

  // The link itself may be removed; what it pointed to stays.
  pass.Remove(QStringLiteral("Bank"), false);
  pass.Remove(QStringLiteral("shared"), true);
  QVERIFY(!QFileInfo(root.filePath(QStringLiteral("Bank.gpg"))).isSymLink());
  QVERIFY(!QFileInfo(root.filePath(QStringLiteral("shared"))).isSymLink());
  QCOMPARE(criticalSpy.count(), 13);
  QFile check(outside.filePath(QStringLiteral("secret.gpg")));
  QVERIFY(check.open(QIODevice::ReadOnly));
  QCOMPARE(check.readAll(), QByteArrayLiteral("somebody else's ciphertext"));
  QVERIFY(QFile::exists(outside.filePath(QStringLiteral("sub/deep.gpg"))));

  // A real entry still works: the guard is not a blanket refusal.
  pass.Show(QStringLiteral("entry0"));
  QVERIFY(showSpy.count() > 0 || showSpy.wait(5000));
#endif
}

/**
 * @brief With Git on, deleting a linked folder named the way the tree names
 *        it ("shared/") unlinks it and asks git only to forget it; a
 *        recursive "git rm" on "shared/" used to fail on the pathspec and
 *        leave the link.
 */
void tst_imitatepass::removeLinkedFolderWithGitUnlinksAndForgets() {
#ifdef Q_OS_WIN
  QSKIP("uses symlinks and a shell script as a fake git");
#else
  QTemporaryDir storeDir;
  QTemporaryDir outsideDir;
  QVERIFY(storeDir.isValid() && outsideDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QDir root(storeDir.path());
  {
    QFile f(QDir(outsideDir.path()).filePath(QStringLiteral("secret.gpg")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
  }
  QVERIFY(
      QFile::link(outsideDir.path(), root.filePath(QStringLiteral("shared"))));
  const QString gitLog = root.filePath("git-argv.log");
  const QString fakeGit =
      writeGitFailingOn(storeDir.path(), gitLog, QStringLiteral("never"));
  QVERIFY(!fakeGit.isEmpty());
  ImitatePass pass;
  AppSettings s = settingsFor(storeDir.path(), QStringLiteral("/nonexistent"));
  s.useGit = true;
  s.gitExecutable = fakeGit;
  pass.init(s);
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);
  pass.Remove(QStringLiteral("shared/"), true);
  // ls-files (tracked? the fake says yes), rm --cached, commit.
  QTRY_VERIFY_WITH_TIMEOUT(loggedCalls(gitLog).size() >= 3, 5000);
  QCOMPARE(criticalSpy.count(), 0);
  QVERIFY(!QFileInfo(root.filePath(QStringLiteral("shared"))).isSymLink());
  QVERIFY(QFile::exists(
      QDir(outsideDir.path()).filePath(QStringLiteral("secret.gpg"))));
  const QList<QStringList> calls = loggedCalls(gitLog);
  QVERIFY2(calls.at(0).contains(QStringLiteral("ls-files")),
           qPrintable(calls.at(0).join(' ')));
  QVERIFY2(calls.at(1).contains(QStringLiteral("rm")) &&
               calls.at(1).contains(QStringLiteral("--cached")) &&
               !calls.at(1).last().endsWith(QLatin1Char('/')),
           qPrintable(calls.at(1).join(' ')));
  QVERIFY2(calls.at(2).contains(QStringLiteral("commit")),
           qPrintable(calls.at(2).join(' ')));
  QTest::qWait(200);
  QCOMPARE(errorSpy.count(), 0);

  // A link git never knew (synced or planted, not committed): unlinked, and
  // git is not asked to commit a pathspec that matches nothing.
  QVERIFY(
      QFile::link(outsideDir.path(), root.filePath(QStringLiteral("stray"))));
  QFile::remove(gitLog);
  const QString refusingGit =
      writeGitFailingOn(storeDir.path(), gitLog, QStringLiteral("ls-files"));
  s.gitExecutable = refusingGit;
  pass.init(s);
  pass.Remove(QStringLiteral("stray"), true);
  QTRY_VERIFY_WITH_TIMEOUT(loggedCalls(gitLog).size() >= 1, 5000);
  QTest::qWait(300);
  QCOMPARE(loggedCalls(gitLog).size(), 1);
  QVERIFY(!QFileInfo(root.filePath(QStringLiteral("stray"))).isSymLink());
  QCOMPARE(criticalSpy.count(), 0);
  QCOMPARE(errorSpy.count(), 0);
#endif
}

/**
 * @brief A link under the .gpg-id name is not the folder's recipient list:
 *        the parent's list applies, and a linked list at the root reads as
 *        missing.
 */
void tst_imitatepass::linkedGpgIdIsNotARecipientList() {
#ifdef Q_OS_WIN
  QSKIP("uses symlinks");
#else
  QTemporaryDir storeDir;
  QTemporaryDir outsideDir;
  QVERIFY(storeDir.isValid() && outsideDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QDir root(storeDir.path());
  QVERIFY(root.mkpath(QStringLiteral("team")));
  {
    QFile f(QDir(outsideDir.path()).filePath(QStringLiteral(".gpg-id")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("EVIL\n");
  }
  QVERIFY(
      QFile::link(QDir(outsideDir.path()).filePath(QStringLiteral(".gpg-id")),
                  root.filePath(QStringLiteral("team/.gpg-id"))));
  const QString store = storeDir.path() + QLatin1Char('/');
  QCOMPARE(QDir::cleanPath(Pass::getGpgIdPath(
               root.filePath(QStringLiteral("team/x.gpg")), store)),
           QDir::cleanPath(root.filePath(QStringLiteral(".gpg-id"))));
  QVERIFY(!Pass::getRecipientList(root.filePath(QStringLiteral("team/x.gpg")),
                                  store)
               .contains(QStringLiteral("EVIL")));
  // The root list itself as a link: nothing to encrypt to.
  QVERIFY(QFile::remove(root.filePath(QStringLiteral(".gpg-id"))));
  QVERIFY(
      QFile::link(QDir(outsideDir.path()).filePath(QStringLiteral(".gpg-id")),
                  root.filePath(QStringLiteral(".gpg-id"))));
  QVERIFY(
      Pass::getRecipientList(root.filePath(QStringLiteral("team/x.gpg")), store)
          .isEmpty());
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), QStringLiteral("/nonexistent/gpg")));
  QStringList recipients{QStringLiteral("stale")};
  QVERIFY(pass.loadVerifiedRecipients(root.filePath(QStringLiteral(".gpg-id")),
                                      &recipients));
  QVERIFY(recipients.isEmpty());
#endif
}

QTEST_MAIN(tst_imitatepass)
#include "tst_imitatepass.moc"
