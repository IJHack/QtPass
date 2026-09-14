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
 * and destroying the ImitatePass, interrupt the process in progress instead
 * of waiting for it.
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
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

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
  /// `exec` so the signal a cancel sends lands on the sleep itself rather than
  /// on a shell that would leave it behind. Returns the script path, or an
  /// empty string on failure.
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
      // cancel signals; sleep runs as a child and is left to exit on its own.
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
 * @brief A process that ignores the polite terminate must still be ended by
 * the delayed forced kill after the grace period. The fake gpg traps SIGTERM,
 * so only SIGKILL can end it; the run must finish well before the 60 s sleep.
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
 * the object's members), so it has to interrupt the process and then join.
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

QTEST_MAIN(tst_imitatepass)
#include "tst_imitatepass.moc"
