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
#include <QMap>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>
#include <algorithm>
#ifndef Q_OS_WIN
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "../../../src/gpgidgeneration.h"
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
        << "  sign) cat >/dev/null; printf 'sig' > \"$outfile\";;\n"
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

  /// A fake gpg like writeSigningGpg() whose behaviour for one or more of the
  /// modes decrypt, encrypt, verify, sign and seckeys is replaced by the
  /// shell in @p overrides (the rest keep the cooperating default). The
  /// shell sees $outfile (the --output argument) and $last (the last
  /// argument: the file being decrypted, say). Returns the script path, or
  /// an empty string on failure.
  static QString writeCustomGpg(const QString &dir, const QString &logPath,
                                const QMap<QString, QString> &overrides) {
    const QString script = QDir(dir).filePath(QStringLiteral("custom-gpg.sh"));
    QFile f(script);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
      return {};
    QMap<QString, QString> body{
        {QStringLiteral("decrypt"), QStringLiteral("printf 'plaintext\\n'")},
        {QStringLiteral("encrypt"),
         QStringLiteral(
             "cat >/dev/null; printf 'ciphertext\\n' > \"$outfile\"")},
        {QStringLiteral("sign"),
         QStringLiteral("cat >/dev/null; printf 'sig' > \"$outfile\"")},
        {QStringLiteral("seckeys"),
         QStringLiteral("printf '[GNUPG:] KEY_CONSIDERED %1 0\\n'")
             .arg(kSigner)},
        {QStringLiteral("verify"),
         QStringLiteral("cat >/dev/null; printf '[GNUPG:] VALIDSIG %1 "
                        "2026-09-19 1758200000 0 4 0 1 10 00 %1\\n'")
             .arg(kSigner)}};
    for (auto it = overrides.cbegin(); it != overrides.cend(); ++it)
      body[it.key()] = it.value();
    QTextStream out(&f);
    out << "#!/bin/sh\n"
        << "printf '%s\\n' \"$*\" >> '" << logPath << "'\n"
        << "mode=''; outfile=''; prev=''; last=''\n"
        << "for a in \"$@\"; do\n"
        << "  case \"$a\" in -d) mode=decrypt;; -eq) mode=encrypt;; "
           "--verify) mode=verify;; --detach-sign) mode=sign;; "
           "--list-secret-keys) mode=seckeys;; esac\n"
        << "  [ \"$prev\" = '--output' ] && outfile=\"$a\"\n"
        << "  prev=\"$a\"; last=\"$a\"\n"
        << "done\n"
        << "case \"$mode\" in\n";
    for (auto it = body.cbegin(); it != body.cend(); ++it)
      out << "  " << it.key() << ") " << it.value() << ";;\n";
    out << "esac\n"
        << "exit 0\n";
    out.flush();
    f.close();
    if (!QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                           QFile::ExeOwner))
      return {};
    return script;
  }

  /// A fake git that logs argv, runs the shell `case` arms in @p arms against
  /// "$*" (each arm may print or `exit`) and otherwise exits 0. Returns the
  /// script path, or an empty string on failure.
  static QString writeScriptedGit(const QString &dir, const QString &logPath,
                                  const QString &arms) {
    const QString script =
        QDir(dir).filePath(QStringLiteral("scripted-git.sh"));
    QFile f(script);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
      return {};
    QTextStream out(&f);
    out << "#!/bin/sh\n"
        << "printf '%s\\n' \"$*\" >> '" << logPath << "'\n"
        << "case \"$*\" in\n"
        << arms << "\n"
        << "esac\n"
        << "exit 0\n";
    out.flush();
    f.close();
    if (!QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                           QFile::ExeOwner))
      return {};
    return script;
  }

  /// An enabled recipient with the given key, with or without a secret key.
  static UserInfo user(const QString &keyId, bool haveSecret = true) {
    UserInfo u;
    u.key_id = keyId;
    u.enabled = true;
    u.have_secret = haveSecret;
    return u;
  }

  /// Read a whole file; empty when it cannot be opened.
  static QByteArray contentsOf(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
      return {};
    return f.readAll();
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

  /**
   * @brief Run reencryptPath() on a one-entry store with the fake gpg at
   * @p fakeGpg and check the failure contract: the entry is reported as failed
   * in the aggregated dialog, the summary counts one failure, and no temporary
   * or backup is left. The recorded signals and gpg calls are handed back.
   */
  static void expectSingleFailureLeavesEntryIntact(const QString &storeDir,
                                                   const QString &fakeGpg,
                                                   QStringList *criticals,
                                                   QStringList *statusMessages,
                                                   QList<QStringList> *gpgCalls,
                                                   const QString &logPath) {
    ImitatePass pass;
    pass.init(settingsFor(storeDir, fakeGpg));
    QObject ctx;
    Recorder rec;
    record(pass, ctx, rec);
    QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
    pass.reencryptPath(storeDir);
    QVERIFY(endSpy.count() > 0 || endSpy.wait(15000));
    QCoreApplication::processEvents();
    *criticals = rec.criticals;
    *statusMessages = rec.statusMessages;
    *gpgCalls = loggedCalls(logPath);
    const QStringList leftovers = QDir(storeDir).entryList(
        {QStringLiteral("*.tmp"), QStringLiteral("*.bak")},
        QDir::Files | QDir::System | QDir::Hidden);
    QVERIFY2(leftovers.isEmpty(), qPrintable(leftovers.join(' ')));
    QVERIFY2(std::any_of(criticals->cbegin(), criticals->cend(),
                         [](const QString &m) {
                           return m.contains("could not be re-encrypted") &&
                                  m.contains("entry0.gpg");
                         }),
             qPrintable(criticals->join(" | ")));
    QVERIFY2(!statusMessages->isEmpty() &&
                 statusMessages->last().contains(QStringLiteral("1 failed")),
             qPrintable(statusMessages->join(" | ")));
  }

  /// Re-encrypt @p storeDir with the recording fake gpg and the fake git at
  /// @p fakeGit, autoPush on (and autoPull as given), and hand back what was
  /// recorded: the signals, every git call and the number of encrypt calls.
  /// With @p expectPush the push that closes a clean run is waited for too,
  /// so it is among the git calls; without it a push that must not happen
  /// gets a grace period to show up. Returns false when the run (or the
  /// expected push) did not finish in time.
  static bool runReencryptWithGit(const QString &storeDir,
                                  const QString &fakeGit, Recorder &rec,
                                  QList<QStringList> *gitCalls, int *encrypts,
                                  bool autoPull = false,
                                  bool expectPush = false) {
    const QString gpgLog = QDir(storeDir).filePath("gpg-argv.log");
    const QString gitLog = QDir(storeDir).filePath("git-argv.log");
    const QString fakeGpg = writeRecordingGpg(storeDir, gpgLog);
    if (fakeGpg.isEmpty())
      return false;
    ImitatePass pass;
    AppSettings s = settingsFor(storeDir, fakeGpg);
    s.useGit = true;
    s.gitExecutable = fakeGit;
    s.autoPush = true;
    s.autoPull = autoPull;
    pass.init(s);
    QObject ctx;
    record(pass, ctx, rec);
    QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
    QSignalSpy pushSpy(&pass, &Pass::finishedGitPush);
    pass.reencryptPath(storeDir);
    bool finished = endSpy.count() > 0 || endSpy.wait(15000);
    if (expectPush) {
      // GitPush() is queued on the executor just before the run ends and
      // reaches the fake git a little later.
      finished = finished && (pushSpy.count() > 0 || pushSpy.wait(5000));
    } else {
      // A push that must not happen gets the time it would need to show up.
      QTest::qWait(300);
    }
    QCoreApplication::processEvents();
    *gitCalls = loggedCalls(gitLog);
    *encrypts = encryptCalls(loggedCalls(gpgLog)).size();
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
  void insertEncryptsIntoATemporaryAndMovesItIntoPlace();
  void insertDoesNotWriteThroughALinkPlantedAfterTheCheck();
  void insertDoesNotReplaceAnEntryThatAppearedWhileAdding();
  void insertRemovesTheTemporaryWhenGpgFails();
  void copyWritesTheBytesThroughAStagedFileAndKeepsAnUnforcedTarget();
  void anOlderSignedGpgIdIsRefusedUntilSavedAgain();
  void aDifferentListOfTheSameGenerationIsAConflict();
  void aSignedGpgIdCopiedIntoAnotherFolderIsRefused();
  void aPlantedGenerationDoesNotMoveTheCounter();
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
  void gitInitAndPullRunThroughTheExecutor();
  void gitPullBlockingReportsAFailedPull();
  void insertRefusesWhenTheStoreHasNoRecipientList();
  void insertReportsAnUnusableTemporaryLocation();
  void removeEntryWithGitAsksGitToRemoveAndCommit();
  void removeReportsALinkItCannotUnlink();
  void initRefusesAFolderOutsideTheStoreWhenSigning();
  void initReportsAFailedSignature();
  void initRefusesASignatureThatDoesNotVerify();
  void initRefusesWhenTheSigningKeyHasNoSecret();
  void initReportsAnOldSignatureItCannotRemove();
  void initCommitsTheRemovalOfATrackedOldSignature();
  void initWithGitButWithoutAddGpgIdStillFinishes();
  void recoveryRemovesALinkUnderATemporaryName();
  void reencryptStopsWhenTheFolderHasNoRecipients();
  void reencryptRemovesItsTemporaryWhenEncryptionFails();
  void reencryptDiscardsACiphertextThatDoesNotDecrypt();
  void reencryptDiscardsACiphertextWhoseContentDiffers();
  void reencryptFailsWhenTheOriginalVanishesBeforeTheSwap();
  void reencryptLeavesTheOriginalWhenTheCiphertextVanishes();
  void reencryptFailsWhenNoTemporaryCanBeMadeNextToTheEntry();
  void reencryptCountsAFailedGitAddAsAFailure();
  void reencryptCountsAFailedGitCommitAsAFailure();
  void reencryptAbortsWhenGitStatusFails();
  void reencryptAbortsWhenTheBackupCommitFails();
  void reencryptCancelledBeforeTheWorkerStartsEndsAtOnce();
  void reencryptPullsFirstWhenAutoPullIsOn();
  void reencryptGoesOnWhenThePullFailsCleanly();
  void reencryptAbortsWhenThePullLeavesConflicts();
  void failureDialogListsAtMostFifteenFiles();
  void moveFolderResolvesItsDestination();
  void moveWithForceReplacesTheDestinationWithoutGit();
  void moveWithGitUsesGitMvAndCommits();
  void copyFailsWhenTheDestinationFolderDoesNotExist();
  void insertFailsWhenGpgWritesNoCiphertext();
  void insertFailsWhenTheEntryFolderVanishes();
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
 * @brief gpg never opens a path in the store for output: it writes into a
 *        directory of QtPass's own outside the store, from where the bytes
 *        go into the store through a temporary next to the entry and a
 *        rename; the scratch is gone afterwards, the entry holds the
 *        ciphertext, no temporary is left, and an overwrite replaces an
 *        existing entry the same way. The dialog names a new entry relative
 *        to the store, and that lands in the store, not in the working
 *        directory.
 */
void tst_imitatepass::insertEncryptsIntoATemporaryAndMovesItIntoPlace() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString fakeGpg = writeRecordingGpg(storeDir.path(), logPath);
  QVERIFY(!fakeGpg.isEmpty());
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), fakeGpg));
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy criticalSpy(&pass, &Pass::critical);

  const QString entry = QDir(storeDir.path()).filePath("new.gpg");
  pass.Insert(QStringLiteral("new"), QStringLiteral("s\n"), false);
  QVERIFY(insertSpy.count() > 0 || insertSpy.wait(15000));
  QCOMPARE(criticalSpy.count(), 0);
  QVERIFY2(!QFileInfo::exists(QDir::current().filePath("new.gpg")),
           "a store-relative name is not resolved against the working "
           "directory");
  const QList<QStringList> enc = encryptCalls(loggedCalls(logPath));
  QCOMPARE(enc.size(), 1);
  const QString output = enc.first().at(enc.first().indexOf("--output") + 1);
  QVERIFY2(output != entry, qPrintable(output));
  QVERIFY2(!output.startsWith(storeDir.path()),
           qPrintable("gpg must not write into the store: " + output));
  QVERIFY2(!QFileInfo::exists(QFileInfo(output).path()),
           "the scratch directory is gone once the entry is in place");
  QFile written(entry);
  QVERIFY(written.open(QIODevice::ReadOnly));
  QCOMPARE(written.readAll(), QByteArray("ciphertext\n"));
  QVERIFY(!QFileInfo(entry).isSymLink());
  QVERIFY(!QFile::exists(entry + QStringLiteral(".insert.bak")));

  // Adding over an existing entry is refused before gpg runs; editing it
  // replaces its bytes.
  QFile::remove(logPath);
  pass.Insert(QDir(storeDir.path()).filePath("entry0"), QStringLiteral("s\n"),
              false);
  QTest::qWait(200);
  QCOMPARE(criticalSpy.count(), 1);
  QVERIFY(criticalSpy.takeFirst().at(1).toString().contains("already exists"));
  QVERIFY(loggedCalls(logPath).isEmpty());
  insertSpy.clear();
  pass.Insert(QDir(storeDir.path()).filePath("entry0"), QStringLiteral("s\n"),
              true);
  QVERIFY(insertSpy.count() > 0 || insertSpy.wait(15000));
  QCOMPARE(criticalSpy.count(), 0);
  QFile edited(QDir(storeDir.path()).filePath("entry0.gpg"));
  QVERIFY(edited.open(QIODevice::ReadOnly));
  QCOMPARE(edited.readAll(), QByteArray("ciphertext\n"));
  QVERIFY(
      !QFile::exists(QDir(storeDir.path()).filePath("entry0.gpg.insert.bak")));
  QCOMPARE(
      QDir(storeDir.path())
          .entryList({QStringLiteral(".*.tmp")}, QDir::Files | QDir::Hidden)
          .size(),
      0);
#endif
}

/**
 * @brief The race #1864 could not close: the entry becomes a link to a file
 *        outside the store between the check and gpg's open. gpg now writes
 *        elsewhere, and the link goes aside as an entry when the ciphertext
 *        moves in; the file it pointed at is untouched.
 */
void tst_imitatepass::insertDoesNotWriteThroughALinkPlantedAfterTheCheck() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QTemporaryDir outsideDir;
  QVERIFY(storeDir.isValid() && outsideDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString victim = QDir(outsideDir.path()).filePath("important");
  {
    QFile f(victim);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("do not touch");
  }
  const QString entry = QDir(storeDir.path()).filePath("entry0.gpg");
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  // A fake gpg that, when asked to encrypt, first makes the entry a link to
  // the victim (the attacker winning the race), then writes its output
  // where told.
  const QString script = QDir(storeDir.path()).filePath("racing-gpg.sh");
  {
    QFile f(script);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
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
        << "  encrypt) cat >/dev/null; rm -f '" << entry << "'; ln -s '"
        << victim << "' '" << entry
        << "'; printf 'ciphertext\\n' > "
           "\"$outfile\";;\n"
        << "esac\n"
        << "exit 0\n";
    out.flush();
    f.close();
    QVERIFY(QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                              QFile::ExeOwner));
  }
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), script));
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  pass.Insert(QDir(storeDir.path()).filePath("entry0"), QStringLiteral("s\n"),
              true);
  QVERIFY(insertSpy.count() > 0 || insertSpy.wait(15000));
  QCOMPARE(criticalSpy.count(), 0);
  QFile v(victim);
  QVERIFY(v.open(QIODevice::ReadOnly));
  QCOMPARE(v.readAll(), QByteArray("do not touch"));
  QVERIFY2(!QFileInfo(entry).isSymLink(), "the planted link went aside");
  QFile written(entry);
  QVERIFY(written.open(QIODevice::ReadOnly));
  QCOMPARE(written.readAll(), QByteArray("ciphertext\n"));
  QVERIFY(!QFile::exists(entry + QStringLiteral(".insert.bak")));
#endif
}

/**
 * @brief Adding (not editing) an entry whose name gains a regular file
 *        while gpg runs: the rename refuses to replace it, the add fails
 *        through the ordinary failed-operation path, the file that
 *        appeared is kept, and the git steps queued behind gpg do not run.
 */
void tst_imitatepass::insertDoesNotReplaceAnEntryThatAppearedWhileAdding() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  QVERIFY(QDir(storeDir.path()).mkpath(QStringLiteral(".git")));
  const QString entry = QDir(storeDir.path()).filePath("new.gpg");
  const QString logPath = QDir(storeDir.path()).filePath("argv.log");
  const QString script = QDir(storeDir.path()).filePath("appearing-gpg.sh");
  {
    QFile f(script);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&f);
    out << "#!/bin/sh\n"
        << "printf 'gpg %s\\n' \"$*\" >> '" << logPath << "'\n"
        << "mode=''; outfile=''; prev=''\n"
        << "for a in \"$@\"; do\n"
        << "  case \"$a\" in -d) mode=decrypt;; -eq) mode=encrypt;; esac\n"
        << "  [ \"$prev\" = '--output' ] && outfile=\"$a\"\n"
        << "  prev=\"$a\"\n"
        << "done\n"
        << "case \"$mode\" in\n"
        << "  decrypt) printf 'plaintext\\n';;\n"
        << "  encrypt) cat >/dev/null; printf 'theirs' > '" << entry
        << "'; printf 'ciphertext\\n' > \"$outfile\";;\n"
        << "esac\n"
        << "exit 0\n";
    out.flush();
    f.close();
    QVERIFY(QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                              QFile::ExeOwner));
  }
  const QString fakeGit = writeRecordingGit(storeDir.path(), logPath);
  QVERIFY(!fakeGit.isEmpty());
  ImitatePass pass;
  AppSettings s = settingsFor(storeDir.path(), script);
  s.useGit = true;
  s.gitExecutable = fakeGit;
  pass.init(s);
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  pass.Insert(QStringLiteral("new"), QStringLiteral("s\n"), false);
  QVERIFY(errorSpy.count() > 0 || errorSpy.wait(15000));
  QCOMPARE(insertSpy.count(), 0);
  QVERIFY2(errorSpy.first().at(1).toString().contains("already exists"),
           qPrintable(errorSpy.first().at(1).toString()));
  QFile kept(entry);
  QVERIFY(kept.open(QIODevice::ReadOnly));
  QCOMPARE(kept.readAll(), QByteArray("theirs"));
  QTest::qWait(300);
  const QList<QStringList> calls = loggedCalls(logPath);
  QVERIFY2(std::none_of(calls.cbegin(), calls.cend(),
                        [](const QStringList &c) {
                          return c.first() == QStringLiteral("git") &&
                                 (c.contains(QStringLiteral("add")) ||
                                  c.contains(QStringLiteral("commit")));
                        }),
           "no git step of a failed add may run");
  QCOMPARE(
      QDir(storeDir.path())
          .entryList({QStringLiteral(".*.tmp")}, QDir::Files | QDir::Hidden)
          .size(),
      0);
#endif
}

/**
 * @brief When gpg fails, the temporary it was to write is removed, the
 *        entry is left as it was, and the failure is reported.
 */
void tst_imitatepass::insertRemovesTheTemporaryWhenGpgFails() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString script = QDir(storeDir.path()).filePath("failing-gpg.sh");
  {
    QFile f(script);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(QStringLiteral("#!/bin/sh\nprintf '%s\\n' \"$*\" >> '%1'\ncat "
                           ">/dev/null\nexit 2\n")
                .arg(logPath)
                .toUtf8());
    f.close();
    QVERIFY(QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                              QFile::ExeOwner));
  }
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), script));
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QDir(storeDir.path()).filePath("entry0"), QStringLiteral("s\n"),
              true);
  QVERIFY(errorSpy.count() > 0 || errorSpy.wait(15000));
  QCOMPARE(insertSpy.count(), 0);
  QFile kept(QDir(storeDir.path()).filePath("entry0.gpg"));
  QVERIFY(kept.open(QIODevice::ReadOnly));
  QCOMPARE(kept.readAll(), QByteArray("not really encrypted"));
  const QList<QStringList> enc = encryptCalls(loggedCalls(logPath));
  QCOMPARE(enc.size(), 1);
  const QString output = enc.first().at(enc.first().indexOf("--output") + 1);
  QVERIFY2(!QFileInfo::exists(QFileInfo(output).path()),
           "the scratch directory goes with the failed insert");
  QCOMPARE(
      QDir(storeDir.path())
          .entryList({QStringLiteral(".*.tmp")}, QDir::Files | QDir::Hidden)
          .size(),
      0);
#endif
}

/**
 * @brief Copy reads the source pinned as a regular file and writes the
 *        copy through a staged sibling and a rename: the destination holds
 *        the source's bytes and mode, no temporary is left, an existing
 *        destination survives an unforced copy and is replaced by a forced
 *        one.
 */
void tst_imitatepass::
    copyWritesTheBytesThroughAStagedFileAndKeepsAnUnforcedTarget() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QDir root(storeDir.path());
  const QString logPath = root.filePath(QStringLiteral("gpg-argv.log"));
  const QString fakeGpg = writeRecordingGpg(storeDir.path(), logPath);
  QVERIFY(!fakeGpg.isEmpty());
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), fakeGpg));
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
  const QString src = root.filePath(QStringLiteral("entry0.gpg"));
  const QString dst = root.filePath(QStringLiteral("copy.gpg"));
  pass.Copy(src, dst, false);
  QVERIFY(endSpy.count() > 0 || endSpy.wait(15000));
  endSpy.clear();
  QVERIFY2(criticalSpy.isEmpty(),
           qPrintable(criticalSpy.isEmpty()
                          ? QString()
                          : criticalSpy.first().at(1).toString()));
  QFile copied(dst);
  QVERIFY(copied.open(QIODevice::ReadOnly));
  {
    const QByteArray bytes = copied.readAll();
    QVERIFY2(bytes == QByteArray("not really encrypted") ||
                 bytes == QByteArray("ciphertext\n"),
             bytes.constData());
  }
  QCOMPARE(
      root.entryList({QStringLiteral(".*.tmp")}, QDir::Files | QDir::Hidden)
          .size(),
      0);
  // Unforced onto an existing entry: refused, the entry as it was.
  {
    QFile f(dst);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("other");
  }
  pass.Copy(src, dst, false);
  QTest::qWait(300);
  QCOMPARE(criticalSpy.count(), 1);
  QFile kept(dst);
  QVERIFY(kept.open(QIODevice::ReadOnly));
  QCOMPARE(kept.readAll(), QByteArray("other"));
  kept.close();
  pass.Copy(src, dst, true);
  QVERIFY(endSpy.count() > 0 || endSpy.wait(15000));
  QCOMPARE(criticalSpy.count(), 1);
  QFile replaced(dst);
  QVERIFY(replaced.open(QIODevice::ReadOnly));
  // The re-encryption pass rewrites entries whose recipients differ; the
  // fake gpg turns every one into "ciphertext", so the copy read from the
  // source is one of the two.
  const QByteArray bytes = replaced.readAll();
  QVERIFY2(bytes == QByteArray("not really encrypted") ||
               bytes == QByteArray("ciphertext\n"),
           bytes.constData());
  QVERIFY(!QFileInfo(dst).isSymLink());
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
  QVERIFY2(gen1.startsWith(
               "# QtPass-GpgId-Generation: 1\n# QtPass-GpgId-Folder: .\n"),
           gen1.constData());
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

/**
 * @brief Two devices both write generation 2 from 1 (Git's conflict); the
 *        device that wrote one of them meets the other, genuinely signed,
 *        with its own generation number: not a rollback, a conflict. Nothing
 *        is encrypted to it until a holder of the signing key saves again,
 *        which writes generation 3.
 */
void tst_imitatepass::aDifferentListOfTheSameGenerationIsAConflict() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QString gpgIdFile = QDir(storeDir.path()).filePath(".gpg-id");
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
  const QString bob =
      QStringLiteral("89ABCDEF0123456789ABCDEF0123456789ABCDEF");
  const QString store = QDir(storeDir.path()).path() + QLatin1Char('/');

  // Generations 1 and 2, this device's.
  pass.Init(store, {alice});
  QVERIFY(endSpy.count() > 0 || endSpy.wait(15000));
  endSpy.clear();
  pass.Init(store, {alice});
  QVERIFY(endSpy.count() > 0 || endSpy.wait(15000));
  endSpy.clear();
  {
    QFile g2(gpgIdFile);
    QVERIFY(g2.open(QIODevice::ReadOnly));
    QVERIFY(g2.readAll().startsWith("# QtPass-GpgId-Generation: 2\n"));
  }
  QCOMPARE(criticalSpy.count(), 0);

  // The other device's generation 2, Bob included, arrives with the pull
  // (the fake gpg verifies it, as the real one would: it is signed).
  {
    QFile g(gpgIdFile);
    QVERIFY(g.open(QIODevice::WriteOnly | QIODevice::Truncate));
    g.write(GpgIdGeneration::withHeader(
        2, QStringLiteral("."), (kSigner + "\n" + bob + "\n").toUtf8()));
  }
  QFile::remove(logPath);
  pass.Insert(QDir(storeDir.path()).filePath("entry"),
              QStringLiteral("secret\n"), false);
  QTest::qWait(500);
  QCOMPARE(insertSpy.count(), 0);
  QCOMPARE(criticalSpy.count(), 1);
  const QString why = criticalSpy.takeFirst().at(1).toString();
  QVERIFY2(why.contains(QStringLiteral("generation 2")) &&
               why.contains(QStringLiteral("same generation")) &&
               why.contains(QStringLiteral("Users")),
           qPrintable(why));
  QVERIFY2(encryptCalls(loggedCalls(logPath)).isEmpty(),
           "nothing may be encrypted to a list this device did not accept");

  // The way through: a save, which writes generation 3 over the conflict.
  pass.Init(store, {alice});
  QVERIFY(endSpy.count() > 0 || endSpy.wait(15000));
  {
    QFile g3(gpgIdFile);
    QVERIFY(g3.open(QIODevice::ReadOnly));
    QVERIFY(g3.readAll().startsWith("# QtPass-GpgId-Generation: 3\n"));
  }
  QFile::remove(logPath);
  pass.Insert(QDir(storeDir.path()).filePath("entry"),
              QStringLiteral("secret\n"), false);
  QVERIFY(insertSpy.count() > 0 || insertSpy.wait(15000));
  QCOMPARE(criticalSpy.count(), 0);
  QCOMPARE(recipientsOf(encryptCalls(loggedCalls(logPath)).first()),
           QStringList{kSigner});
#endif
}

/**
 * @brief Relocation: the root's signed pair (still naming Bob) copied into
 *        a folder that never had a list of its own. The folder's record is
 *        empty, so first sight would accept it; the folder line says it was
 *        written for ".", and nothing is encrypted to it.
 */
void tst_imitatepass::aSignedGpgIdCopiedIntoAnotherFolderIsRefused() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QDir root(storeDir.path());
  const QString logPath = root.filePath("gpg-argv.log");
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
  const QString store = root.path() + QLatin1Char('/');
  pass.Init(store, {alice, bob});
  QVERIFY(endSpy.count() > 0 || endSpy.wait(15000));
  endSpy.clear();
  pass.Init(store, {alice});
  QVERIFY(endSpy.count() > 0 || endSpy.wait(15000));
  QCOMPARE(criticalSpy.count(), 0);
  // The attacker kept the generation-1 pair and plants it in a new folder.
  QVERIFY(root.mkpath(QStringLiteral("team")));
  QVERIFY(QFile::copy(root.filePath(".gpg-id"), root.filePath("team/.gpg-id")));
  QVERIFY(QFile::copy(root.filePath(".gpg-id.sig"),
                      root.filePath("team/.gpg-id.sig")));
  {
    // ... the old one, with Bob: rewrite team/.gpg-id from the gen-1 shape.
    QFile f(root.filePath("team/.gpg-id"));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(GpgIdGeneration::withHeader(
        1, QStringLiteral("."),
        (alice.key_id + "\n" + bob.key_id + "\n").toUtf8()));
  }
  QFile::remove(logPath);
  pass.Insert(root.filePath("team/entry"), QStringLiteral("secret\n"), false);
  QTest::qWait(500);
  QCOMPARE(insertSpy.count(), 0);
  QCOMPARE(criticalSpy.count(), 1);
  const QString why = criticalSpy.takeFirst().at(1).toString();
  QVERIFY2(why.contains(QStringLiteral("\".\"")) &&
               why.contains(QStringLiteral("\"team\"")),
           qPrintable(why));
  QVERIFY2(encryptCalls(loggedCalls(logPath)).isEmpty(),
           "nothing may be encrypted to a relocated list");
#endif
}

/**
 * @brief A planted, unsigned header at the top of the grammar must not
 *        drive the counter: the save after it still writes a small number
 *        (the list on disk did not verify, so its number is not taken), and
 *        the store keeps working.
 */
void tst_imitatepass::aPlantedGenerationDoesNotMoveTheCounter() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QDir root(storeDir.path());
  const QString gpgIdFile = root.filePath(".gpg-id");
  const QString logPath = root.filePath("gpg-argv.log");
  // This fake reports VALIDSIG for anything, so make the planted list fail
  // the read some other way: the folder line names another folder.
  const QString fakeGpg = writeSigningGpg(storeDir.path(), logPath);
  QVERIFY(!fakeGpg.isEmpty());
  ImitatePass pass;
  AppSettings s = settingsFor(storeDir.path(), fakeGpg);
  s.passSigningKey = kSigner;
  pass.init(s);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  UserInfo alice;
  alice.key_id = kSigner;
  alice.enabled = true;
  alice.have_secret = true;
  const QString store = root.path() + QLatin1Char('/');
  pass.Init(store, {alice});
  QVERIFY(endSpy.count() > 0 || endSpy.wait(15000));
  endSpy.clear();
  {
    QFile f(gpgIdFile);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("# QtPass-GpgId-Generation: 999999999999999999\n"
            "# QtPass-GpgId-Folder: elsewhere\n" +
            alice.key_id.toUtf8() + "\n");
  }
  // Insert is refused (the list is not this folder's) ...
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  pass.Insert(root.filePath("entry"), QStringLiteral("secret\n"), false);
  QTest::qWait(500);
  QCOMPARE(insertSpy.count(), 0);
  QVERIFY(criticalSpy.count() >= 1);
  criticalSpy.clear();
  // ... and the save that follows writes generation 2, not 10^18.
  pass.Init(store, {alice});
  QVERIFY(endSpy.count() > 0 || endSpy.wait(15000));
  QCOMPARE(criticalSpy.count(), 0);
  QFile g(gpgIdFile);
  QVERIFY(g.open(QIODevice::ReadOnly));
  const QByteArray written = g.readAll();
  QVERIFY2(written.startsWith("# QtPass-GpgId-Generation: 2\n"),
           written.constData());
  QVERIFY(GpgIdGeneration::parse(written).has_value());
  QFile::remove(logPath);
  pass.Insert(root.filePath("entry"), QStringLiteral("secret\n"), false);
  QVERIFY(insertSpy.count() > 0 || insertSpy.wait(15000));
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
 * @brief The new ciphertext goes to a file in a directory QtPass created
 *        itself, outside the store. A file (or symlink) planted at the old
 *        predictable <file>.reencrypt.tmp is never written through, and the
 *        scratch file is gone afterwards.
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
  QVERIFY2(!output.startsWith(storeDir.path()),
           qPrintable("gpg must write outside the store: " + output));
  QVERIFY2(!QFile::exists(output), "the scratch file must not be left behind");
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
  // A real folder with a link planted under the .gpg-id name: not this
  // store's list, refused up front (and a write would replace it as an
  // entry, never through it).
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

/**
 * @brief GitInit() and GitPull() are thin executor wrappers: `git init
 *        <store>` and `git pull` run and complete through the ordinary
 *        finishedGitInit / finishedGitPull path.
 */
void tst_imitatepass::gitInitAndPullRunThroughTheExecutor() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a recording fake git");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  const QString gitLog = QDir(storeDir.path()).filePath("git-argv.log");
  const QString fakeGit = writeRecordingGit(storeDir.path(), gitLog);
  QVERIFY(!fakeGit.isEmpty());
  AppSettings s = settingsFor(storeDir.path(), unstartableGpg());
  s.useGit = true;
  s.gitExecutable = fakeGit;
  ImitatePass pass;
  pass.init(s);
  QSignalSpy initSpy(&pass, &Pass::finishedGitInit);
  QSignalSpy pullSpy(&pass, &Pass::finishedGitPull);
  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);

  pass.GitInit();
  QVERIFY2(initSpy.count() > 0 || initSpy.wait(5000),
           "git init must complete through finishedGitInit");
  pass.GitPull();
  QVERIFY2(pullSpy.count() > 0 || pullSpy.wait(5000),
           "git pull must complete through finishedGitPull");
  QCOMPARE(errorSpy.count(), 0);
  const QList<QStringList> calls = loggedCalls(gitLog);
  QCOMPARE(calls.size(), 2);
  QCOMPARE(calls.at(0).first(), QStringLiteral("init"));
  QCOMPARE(QDir::cleanPath(calls.at(0).last()),
           QDir::cleanPath(storeDir.path()));
  QCOMPARE(calls.at(1), QStringList{QStringLiteral("pull")});
#endif
}

/**
 * @brief GitPull_b() blocks, runs git in the store (-C) and only speaks up
 *        when the pull fails, quoting git's stderr in the status bar.
 */
void tst_imitatepass::gitPullBlockingReportsAFailedPull() {
#ifdef Q_OS_WIN
  QSKIP("uses shell scripts as fake git");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  const QString gitLog = QDir(storeDir.path()).filePath("git-argv.log");
  const QString okGit = writeRecordingGit(storeDir.path(), gitLog);
  const QString failingGit = writeScriptedGit(
      storeDir.path(), gitLog,
      QStringLiteral("  *' pull') printf 'fatal: no remote\\n' >&2; exit 1;;"));
  QVERIFY(!okGit.isEmpty() && !failingGit.isEmpty());
  AppSettings s = settingsFor(storeDir.path(), unstartableGpg());
  s.useGit = true;
  s.gitExecutable = okGit;
  ImitatePass pass;
  pass.init(s);
  QSignalSpy statusSpy(&pass, &Pass::statusMsg);

  pass.GitPull_b();
  QVERIFY2(statusSpy.isEmpty(), "a pull that worked has nothing to say");
  QList<QStringList> calls = loggedCalls(gitLog);
  QCOMPARE(calls.size(), 1);
  QCOMPARE(calls.first().at(0), QStringLiteral("-C"));
  QCOMPARE(QDir::cleanPath(calls.first().at(1)),
           QDir::cleanPath(storeDir.path()));
  QCOMPARE(calls.first().last(), QStringLiteral("pull"));

  s.gitExecutable = failingGit;
  pass.init(s);
  pass.GitPull_b();
  QCOMPARE(statusSpy.count(), 1);
  const QString msg = statusSpy.first().at(0).toString();
  QVERIFY2(msg.contains(QStringLiteral("Git pull failed")) &&
               msg.contains(QStringLiteral("fatal: no remote")),
           qPrintable(msg));

  // With git enabled but no executable configured nothing runs at all.
  s.gitExecutable.clear();
  pass.init(s);
  statusSpy.clear();
  pass.GitPull_b();
  QCOMPARE(loggedCalls(gitLog).size(), 2);
  QCOMPARE(statusSpy.count(), 1);
  QVERIFY2(statusSpy.first().at(0).toString().contains("not configured"),
           qPrintable(statusSpy.first().at(0).toString()));
#endif
}

/**
 * @brief A store with no .gpg-id (signing off) has nobody to encrypt to:
 *        Insert() says so and never starts gpg.
 */
void tst_imitatepass::insertRefusesWhenTheStoreHasNoRecipientList() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a recording fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString fakeGpg = writeRecordingGpg(storeDir.path(), logPath);
  QVERIFY(!fakeGpg.isEmpty());
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), fakeGpg));
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);

  pass.Insert(QStringLiteral("entry"), QStringLiteral("secret\n"), false);
  QTest::qWait(200);
  QCOMPARE(insertSpy.count(), 0);
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(criticalSpy.first().at(0).toString(),
           QStringLiteral("Can not edit"));
  QVERIFY2(criticalSpy.first().at(1).toString().contains(
               QStringLiteral("Could not read encryption key")),
           qPrintable(criticalSpy.first().at(1).toString()));
  QVERIFY2(loggedCalls(logPath).isEmpty(), "gpg must not have been run");
  QVERIFY2(!QFile::exists(QDir(storeDir.path()).filePath("entry.gpg")),
           "no entry may appear when the add was refused");
#endif
}

/**
 * @brief gpg writes into a private temporary directory; when none can be
 *        made (TMPDIR unusable) the add is refused up front with the reason,
 *        rather than gpg being pointed into the store.
 */
void tst_imitatepass::insertReportsAnUnusableTemporaryLocation() {
#ifdef Q_OS_WIN
  QSKIP("redirects TMPDIR and uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString fakeGpg = writeRecordingGpg(storeDir.path(), logPath);
  QVERIFY(!fakeGpg.isEmpty());
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), fakeGpg));
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);

  const QByteArray oldTmp = qgetenv("TMPDIR");
  const bool hadTmp = qEnvironmentVariableIsSet("TMPDIR");
  qputenv("TMPDIR", QByteArrayLiteral("/nonexistent/qtpass-test-tmp"));
  pass.Insert(QStringLiteral("entry"), QStringLiteral("secret\n"), false);
  if (hadTmp)
    qputenv("TMPDIR", oldTmp);
  else
    qunsetenv("TMPDIR");
  QTest::qWait(200);

  QCOMPARE(insertSpy.count(), 0);
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(criticalSpy.first().at(0).toString(),
           QStringLiteral("Cannot write"));
  QVERIFY2(criticalSpy.first().at(1).toString().contains(
               QStringLiteral("temporary directory")),
           qPrintable(criticalSpy.first().at(1).toString()));
  QVERIFY2(loggedCalls(logPath).isEmpty(), "gpg must not have been run");
  QVERIFY2(!QFile::exists(QDir(storeDir.path()).filePath("entry.gpg")),
           "no entry may appear when the add was refused");
#endif
}

/**
 * @brief With git on, deleting an entry is `git rm -f -- <file>` followed by
 *        a commit naming the entry, and the interface hears finishedRemove.
 */
void tst_imitatepass::removeEntryWithGitAsksGitToRemoveAndCommit() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a recording fake git");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString gitLog = QDir(storeDir.path()).filePath("git-argv.log");
  const QString fakeGit = writeRecordingGit(storeDir.path(), gitLog);
  QVERIFY(!fakeGit.isEmpty());
  AppSettings s = settingsFor(storeDir.path(), unstartableGpg());
  s.useGit = true;
  s.gitExecutable = fakeGit;
  ImitatePass pass;
  pass.init(s);
  QSignalSpy removeSpy(&pass, &Pass::finishedRemove);
  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);

  pass.Remove(QStringLiteral("entry0"), false);
  QVERIFY2(removeSpy.count() > 0 || removeSpy.wait(5000),
           "finishedRemove must follow the commit");
  QCOMPARE(errorSpy.count(), 0);
  const QList<QStringList> calls = loggedCalls(gitLog);
  QCOMPARE(calls.size(), 2);
  const QString entry = QDir(storeDir.path()).filePath("entry0.gpg");
  QCOMPARE(calls.at(0).mid(0, 3),
           (QStringList{QStringLiteral("rm"), QStringLiteral("-f"),
                        QStringLiteral("--")}));
  QCOMPARE(QDir::cleanPath(calls.at(0).last()), QDir::cleanPath(entry));
  QVERIFY2(calls.at(1).first() == QStringLiteral("commit") &&
               calls.at(1).contains(QStringLiteral("Remove")) &&
               calls.at(1).contains(QStringLiteral("entry0")) &&
               QDir::cleanPath(calls.at(1).last()) == QDir::cleanPath(entry),
           qPrintable(calls.at(1).join(' ')));
#endif
}

/**
 * @brief A link the store cannot unlink (its folder is read-only) is
 *        reported as a failed delete; nothing is asked of git and the
 *        link's target is untouched.
 */
void tst_imitatepass::removeReportsALinkItCannotUnlink() {
#ifdef Q_OS_WIN
  QSKIP("uses a symlink and directory permissions");
#else
  if (geteuid() == 0)
    QSKIP("root ignores directory permissions");
  QTemporaryDir storeDir;
  QTemporaryDir outsideDir;
  QVERIFY(storeDir.isValid() && outsideDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QDir root(storeDir.path());
  QVERIFY(root.mkpath(QStringLiteral("sub")));
  const QString sub = root.filePath(QStringLiteral("sub"));
  QVERIFY(QFile::link(outsideDir.path(),
                      root.filePath(QStringLiteral("sub/shared"))));
  const QString gitLog = root.filePath("git-argv.log");
  const QString fakeGit = writeRecordingGit(storeDir.path(), gitLog);
  QVERIFY(!fakeGit.isEmpty());
  AppSettings s = settingsFor(storeDir.path(), unstartableGpg());
  s.useGit = true;
  s.gitExecutable = fakeGit;
  ImitatePass pass;
  pass.init(s);
  QSignalSpy criticalSpy(&pass, &Pass::critical);

  QVERIFY(QFile::setPermissions(sub, QFile::ReadOwner | QFile::ExeOwner));
  pass.Remove(QStringLiteral("sub/shared"), true);
  const bool stillLinked =
      QFileInfo(root.filePath(QStringLiteral("sub/shared"))).isSymLink();
  QVERIFY(QFile::setPermissions(sub, QFile::ReadOwner | QFile::WriteOwner |
                                         QFile::ExeOwner));
  QTest::qWait(200);
  QVERIFY2(stillLinked, "the link must still be there when unlinking failed");
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(criticalSpy.first().at(0).toString(),
           QStringLiteral("Delete failed"));
  QVERIFY2(criticalSpy.first().at(1).toString().contains(
               QStringLiteral("Could not remove the link")),
           qPrintable(criticalSpy.first().at(1).toString()));
  QVERIFY2(!QFile::exists(gitLog),
           "git must not be asked about a failed unlink");
  QVERIFY(QFileInfo(outsideDir.path()).isDir());
#endif
}

/**
 * @brief With a signing key the recipient list carries a folder line, and a
 *        folder that is not inside the store has no such line to give: Init
 *        refuses and writes nothing there.
 */
void tst_imitatepass::initRefusesAFolderOutsideTheStoreWhenSigning() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QTemporaryDir outsideDir;
  QVERIFY(storeDir.isValid() && outsideDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString fakeGpg = writeSigningGpg(storeDir.path(), logPath);
  QVERIFY(!fakeGpg.isEmpty());
  ImitatePass pass;
  AppSettings s = settingsFor(storeDir.path(), fakeGpg);
  s.passSigningKey = kSigner;
  pass.init(s);
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  QSignalSpy startSpy(&pass, &ImitatePass::startReencryptPath);

  pass.Init(outsideDir.path() + QLatin1Char('/'), {user(kSigner)});
  QTest::qWait(200);
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(criticalSpy.first().at(0).toString(),
           QStringLiteral("Cannot update"));
  QVERIFY2(criticalSpy.first().at(1).toString().contains(
               QStringLiteral("not inside the password store")),
           qPrintable(criticalSpy.first().at(1).toString()));
  QVERIFY2(!QFile::exists(QDir(outsideDir.path()).filePath(".gpg-id")),
           "no recipient list may be written outside the store");
  QVERIFY2(!QFile::exists(QDir(outsideDir.path()).filePath(".gpg-id.sig")),
           "no signature may be written outside the store");
  QCOMPARE(startSpy.count(), 0);
#endif
}

/**
 * @brief When gpg cannot make the signature, Init stops there: the failure
 *        is reported with gpg's own words, no signature file appears and no
 *        re-encryption to the new list starts.
 */
void tst_imitatepass::initReportsAFailedSignature() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QDir root(storeDir.path());
  const QString logPath = root.filePath("gpg-argv.log");
  const QString fakeGpg = writeCustomGpg(
      storeDir.path(), logPath,
      {{QStringLiteral("sign"),
        QStringLiteral("cat >/dev/null; printf 'gpg: signing failed: No "
                       "secret key\\n' >&2; exit 2")}});
  QVERIFY(!fakeGpg.isEmpty());
  ImitatePass pass;
  AppSettings s = settingsFor(storeDir.path(), fakeGpg);
  s.passSigningKey = kSigner;
  pass.init(s);
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  QSignalSpy startSpy(&pass, &ImitatePass::startReencryptPath);

  pass.Init(root.path() + QLatin1Char('/'), {user(kSigner)});
  QTest::qWait(300);
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(criticalSpy.first().at(0).toString(),
           QStringLiteral("GPG signing failed!"));
  const QString why = criticalSpy.first().at(1).toString();
  QVERIFY2(why.contains(QStringLiteral("Failed to sign")) &&
               why.contains(QStringLiteral("No secret key")),
           qPrintable(why));
  QVERIFY2(!QFile::exists(root.filePath(".gpg-id.sig")),
           "a failed signing leaves no signature file");
  QCOMPARE(startSpy.count(), 0);
  QVERIFY2(encryptCalls(loggedCalls(logPath)).isEmpty(),
           "nothing may be encrypted to an unsigned list");

  // Without a word from gpg the message still names the file.
  const QString silentGpg =
      writeCustomGpg(storeDir.path(), logPath,
                     {{QStringLiteral("sign"), QStringLiteral("exit 2")}});
  s.gpgExecutable = silentGpg;
  pass.init(s);
  criticalSpy.clear();
  pass.Init(root.path() + QLatin1Char('/'), {user(kSigner)});
  QTest::qWait(300);
  QCOMPARE(criticalSpy.count(), 1);
  QVERIFY2(
      criticalSpy.first().at(1).toString().endsWith(
          QStringLiteral("Failed to sign %1.").arg(root.filePath(".gpg-id"))),
      qPrintable(criticalSpy.first().at(1).toString()));
#endif
}

/**
 * @brief A signature gpg made but does not verify afterwards (no VALIDSIG
 *        by a configured key) is not trusted: Init reports the invalid
 *        signature and no re-encryption starts.
 */
void tst_imitatepass::initRefusesASignatureThatDoesNotVerify() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QDir root(storeDir.path());
  const QString logPath = root.filePath("gpg-argv.log");
  const QString fakeGpg = writeCustomGpg(
      storeDir.path(), logPath,
      {{QStringLiteral("verify"), QStringLiteral("cat >/dev/null")}});
  QVERIFY(!fakeGpg.isEmpty());
  ImitatePass pass;
  AppSettings s = settingsFor(storeDir.path(), fakeGpg);
  s.passSigningKey = kSigner;
  pass.init(s);
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  QSignalSpy startSpy(&pass, &ImitatePass::startReencryptPath);

  pass.Init(root.path() + QLatin1Char('/'), {user(kSigner)});
  QTest::qWait(300);
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(criticalSpy.first().at(0).toString(),
           QStringLiteral("Check .gpg-id file signature!"));
  QVERIFY2(criticalSpy.first().at(1).toString().contains(
               QStringLiteral("Signature for")) &&
               criticalSpy.first().at(1).toString().contains(
                   QStringLiteral("is invalid")),
           qPrintable(criticalSpy.first().at(1).toString()));
  QVERIFY2(QFile::exists(root.filePath(".gpg-id.sig")),
           "the signature gpg wrote is on disk, it just does not verify");
  QCOMPARE(startSpy.count(), 0);
  QVERIFY2(encryptCalls(loggedCalls(logPath)).isEmpty(),
           "nothing may be encrypted to a list whose signature is bad");
#endif
}

/**
 * @brief Without the secret half of the signing key the list could be
 *        written but never signed, and then refused everywhere: Init stops
 *        before writing anything.
 */
void tst_imitatepass::initRefusesWhenTheSigningKeyHasNoSecret() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  const QDir root(storeDir.path());
  const QString logPath = root.filePath("gpg-argv.log");
  const QString fakeGpg =
      writeCustomGpg(storeDir.path(), logPath,
                     {{QStringLiteral("seckeys"), QStringLiteral("exit 0")}});
  QVERIFY(!fakeGpg.isEmpty());
  ImitatePass pass;
  AppSettings s = settingsFor(storeDir.path(), fakeGpg);
  s.passSigningKey = kSigner;
  pass.init(s);
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  QSignalSpy startSpy(&pass, &ImitatePass::startReencryptPath);

  pass.Init(root.path() + QLatin1Char('/'), {user(kSigner)});
  QTest::qWait(200);
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(criticalSpy.first().at(0).toString(),
           QStringLiteral("No signing key!"));
  QVERIFY2(!QFile::exists(root.filePath(".gpg-id")),
           "no list may be written that cannot be signed");
  QCOMPARE(startSpy.count(), 0);
  const QList<QStringList> calls = loggedCalls(logPath);
  QCOMPARE(calls.size(), 1);
  QVERIFY2(calls.first().contains(QStringLiteral("--list-secret-keys")),
           qPrintable("the one gpg call is the secret-key check: " +
                      calls.first().join(' ')));
#endif
}

/**
 * @brief Signing switched off and the old signature cannot be removed (a
 *        directory sits under its name): Init reports it instead of leaving
 *        the new list under a stale signature pass would reject.
 */
void tst_imitatepass::initReportsAnOldSignatureItCannotRemove() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QDir root(storeDir.path());
  QVERIFY(root.mkpath(QStringLiteral(".gpg-id.sig")));
  const QString logPath = root.filePath("gpg-argv.log");
  const QString fakeGpg = writeRecordingGpg(storeDir.path(), logPath);
  QVERIFY(!fakeGpg.isEmpty());
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), fakeGpg));
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  QSignalSpy startSpy(&pass, &ImitatePass::startReencryptPath);

  pass.Init(root.path() + QLatin1Char('/'),
            {user(QStringLiteral("0123456789ABCDEF"))});
  QTest::qWait(200);
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(criticalSpy.first().at(0).toString(),
           QStringLiteral("Cannot update"));
  QVERIFY2(criticalSpy.first().at(1).toString().contains(
               QStringLiteral("Failed to remove the old signature")),
           qPrintable(criticalSpy.first().at(1).toString()));
  QVERIFY2(QFileInfo(root.filePath(".gpg-id.sig")).isDir(),
           "what sits under the signature's name is left alone");
  QCOMPARE(startSpy.count(), 0);
#endif
}

/**
 * @brief Signing switched off in a git store: the old, tracked signature is
 *        removed and its removal goes into the same commit as the new list,
 *        so no revision holds the list under a signature that is not its own.
 */
void tst_imitatepass::initCommitsTheRemovalOfATrackedOldSignature() {
#ifdef Q_OS_WIN
  QSKIP("uses shell scripts as fake gpg and git");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QDir root(storeDir.path());
  const QString sig = root.filePath(".gpg-id.sig");
  {
    QFile f(sig);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("old signature");
  }
  const QString gpgLog = root.filePath("gpg-argv.log");
  const QString gitLog = root.filePath("git-argv.log");
  const QString fakeGpg = writeRecordingGpg(storeDir.path(), gpgLog);
  // ls-files says "tracked", diff --cached says "something staged".
  const QString fakeGit = writeGitFailingOn(storeDir.path(), gitLog, QString());
  QVERIFY(!fakeGpg.isEmpty() && !fakeGit.isEmpty());
  ImitatePass pass;
  AppSettings s = settingsFor(storeDir.path(), fakeGpg);
  s.useGit = true;
  s.addGPGId = true;
  s.gitExecutable = fakeGit;
  pass.init(s);
  QSignalSpy initSpy(&pass, &Pass::finishedInit);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
  QSignalSpy criticalSpy(&pass, &Pass::critical);

  pass.Init(root.path() + QLatin1Char('/'),
            {user(QStringLiteral("0123456789ABCDEF"))});
  QVERIFY2(endSpy.count() > 0 || endSpy.wait(15000),
           "the re-encryption Init starts must finish");
  QVERIFY2(initSpy.count() > 0 || initSpy.wait(5000),
           "finishedInit must follow the .gpg-id commit");
  QCOMPARE(criticalSpy.count(), 0);
  QVERIFY2(!QFile::exists(sig), "the old signature must be gone");
  const QList<QStringList> git = loggedCalls(gitLog);
  QList<QStringList> commits;
  for (const QStringList &c : git)
    if (c.contains(QStringLiteral("commit")))
      commits << c;
  QVERIFY2(!commits.isEmpty(), "the new list must have been committed");
  const QStringList &first = commits.first();
  const QStringList paths =
      first.mid(first.lastIndexOf(QStringLiteral("--")) + 1);
  QVERIFY2(paths.size() == 2 && paths.first().endsWith(".gpg-id") &&
               paths.last().endsWith(".gpg-id.sig"),
           qPrintable("the removed signature goes into the .gpg-id commit: " +
                      first.join(' ')));
#endif
}

/**
 * @brief Git on but "add .gpg-id" off: the list is written and the folder
 *        re-encrypted, git is never asked about the .gpg-id, and the
 *        interface still gets its finishedInit.
 */
void tst_imitatepass::initWithGitButWithoutAddGpgIdStillFinishes() {
#ifdef Q_OS_WIN
  QSKIP("uses shell scripts as fake gpg and git");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QDir root(storeDir.path());
  const QString gpgLog = root.filePath("gpg-argv.log");
  const QString gitLog = root.filePath("git-argv.log");
  const QString fakeGpg = writeRecordingGpg(storeDir.path(), gpgLog);
  const QString fakeGit = writeRecordingGit(storeDir.path(), gitLog);
  QVERIFY(!fakeGpg.isEmpty() && !fakeGit.isEmpty());
  ImitatePass pass;
  AppSettings s = settingsFor(storeDir.path(), fakeGpg);
  s.useGit = true;
  s.addGPGId = false;
  s.gitExecutable = fakeGit;
  pass.init(s);
  QSignalSpy initSpy(&pass, &Pass::finishedInit);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
  QSignalSpy criticalSpy(&pass, &Pass::critical);

  pass.Init(root.path() + QLatin1Char('/'),
            {user(QStringLiteral("FEDCBA9876543210"))});
  QVERIFY2(endSpy.count() > 0 || endSpy.wait(15000),
           "the re-encryption Init starts must finish");
  QVERIFY2(initSpy.count() > 0 || initSpy.wait(5000),
           "finishedInit must be emitted with git on and add .gpg-id off");
  QCOMPARE(criticalSpy.count(), 0);
  QCOMPARE(contentsOf(root.filePath(".gpg-id")),
           QByteArrayLiteral("FEDCBA9876543210\n"));
  QCOMPARE(encryptCalls(loggedCalls(gpgLog)).size(), 1);
  for (const QStringList &c : loggedCalls(gitLog))
    QVERIFY2(!c.join(' ').contains(QStringLiteral(".gpg-id")),
             qPrintable("git must not see the .gpg-id: " + c.join(' ')));
#endif
}

/**
 * @brief A symlink under a stale temporary's name is a leftover QtPass never
 *        made; the link is removed (never what it points to) and the run
 *        goes on.
 */
void tst_imitatepass::recoveryRemovesALinkUnderATemporaryName() {
#ifdef Q_OS_WIN
  QSKIP("uses a symlink and a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QTemporaryDir outsideDir;
  QVERIFY(storeDir.isValid() && outsideDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString outside = QDir(outsideDir.path()).filePath("victim");
  {
    QFile f(outside);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("precious");
  }
  const QString stale = QDir(storeDir.path()).filePath("entry0.gpg.aB3xYz.tmp");
  QVERIFY(QFile::link(outside, stale));
  Recorder rec;
  int encrypts = 0;
  bool aborted = true;
  QVERIFY2(runReencrypt(storeDir.path(), rec, &encrypts, &aborted),
           "re-encryption must finish");
  QVERIFY2(!aborted, qPrintable(rec.criticals.join("; ")));
  QVERIFY2(!QFileInfo(stale).isSymLink() && !QFileInfo::exists(stale),
           "the link under the temporary's name must be gone");
  QCOMPARE(contentsOf(outside), QByteArrayLiteral("precious"));
  QCOMPARE(encrypts, 1);
#endif
}

/**
 * @brief A folder whose (unsigned) .gpg-id is missing has nobody to encrypt
 *        to: the run stops with a verification failure before any file is
 *        touched.
 */
void tst_imitatepass::reencryptStopsWhenTheFolderHasNoRecipients() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  QVERIFY(QFile::remove(QDir(storeDir.path()).filePath(".gpg-id")));
  Recorder rec;
  int encrypts = 0;
  bool aborted = false;
  QVERIFY2(runReencrypt(storeDir.path(), rec, &encrypts, &aborted),
           "re-encryption must finish");
  QVERIFY2(aborted, "a folder without recipients must abort the run");
  QCOMPARE(rec.criticals.size(), 1);
  QVERIFY2(rec.criticals.first().contains(
               QStringLiteral("Could not verify .gpg-id")),
           qPrintable(rec.criticals.first()));
  QCOMPARE(encrypts, 0);
  QVERIFY2(
      std::none_of(rec.statusMessages.cbegin(), rec.statusMessages.cend(),
                   [](const QString &m) { return m.contains("completed"); }),
      "an aborted run is not summarised as completed");
  QCOMPARE(contentsOf(QDir(storeDir.path()).filePath("entry0.gpg")),
           QByteArrayLiteral("not really encrypted"));
#endif
}

/**
 * @brief gpg fails to encrypt: the temporary it was to write is removed, the
 *        entry keeps its bytes and is counted as failed.
 */
void tst_imitatepass::reencryptRemovesItsTemporaryWhenEncryptionFails() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString fakeGpg = writeCustomGpg(
      storeDir.path(), logPath,
      {{QStringLiteral("encrypt"), QStringLiteral("cat >/dev/null; exit 2")}});
  QVERIFY(!fakeGpg.isEmpty());
  QStringList criticals;
  QStringList status;
  QList<QStringList> calls;
  expectSingleFailureLeavesEntryIntact(storeDir.path(), fakeGpg, &criticals,
                                       &status, &calls, logPath);
  if (QTest::currentTestFailed())
    return;
  QCOMPARE(contentsOf(QDir(storeDir.path()).filePath("entry0.gpg")),
           QByteArrayLiteral("not really encrypted"));
  // Decrypt, encrypt; no verification of a ciphertext that was never written.
  QCOMPARE(encryptCalls(calls).size(), 1);
  int decrypts = 0;
  for (const QStringList &c : calls)
    if (c.first() == QStringLiteral("-d"))
      ++decrypts;
  QCOMPARE(decrypts, 1);
#endif
}

/**
 * @brief The new ciphertext must decrypt before it replaces the entry: one
 *        that does not is discarded and the entry stays as it was.
 */
void tst_imitatepass::reencryptDiscardsACiphertextThatDoesNotDecrypt() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString fakeGpg = writeCustomGpg(
      storeDir.path(), logPath,
      {{QStringLiteral("decrypt"),
        QStringLiteral("case \"$last\" in *.tmp) exit 2;; *) printf "
                       "'plaintext\\n';; esac")}});
  QVERIFY(!fakeGpg.isEmpty());
  QStringList criticals;
  QStringList status;
  QList<QStringList> calls;
  expectSingleFailureLeavesEntryIntact(storeDir.path(), fakeGpg, &criticals,
                                       &status, &calls, logPath);
  if (QTest::currentTestFailed())
    return;
  QCOMPARE(contentsOf(QDir(storeDir.path()).filePath("entry0.gpg")),
           QByteArrayLiteral("not really encrypted"));
  QVERIFY2(std::any_of(calls.cbegin(), calls.cend(),
                       [](const QStringList &c) {
                         return c.first() == QStringLiteral("-d") &&
                                c.last().endsWith(QStringLiteral(".tmp"));
                       }),
           "the new ciphertext must have been test-decrypted");
#endif
}

/**
 * @brief A ciphertext that decrypts to something other than the plaintext
 *        that went in is discarded as well (defence in depth against a gpg
 *        that encrypted the wrong bytes).
 */
void tst_imitatepass::reencryptDiscardsACiphertextWhoseContentDiffers() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString fakeGpg = writeCustomGpg(
      storeDir.path(), logPath,
      {{QStringLiteral("decrypt"),
        QStringLiteral("case \"$last\" in *.tmp) printf 'tampered\\n';; *) "
                       "printf 'plaintext\\n';; esac")}});
  QVERIFY(!fakeGpg.isEmpty());
  QStringList criticals;
  QStringList status;
  QList<QStringList> calls;
  expectSingleFailureLeavesEntryIntact(storeDir.path(), fakeGpg, &criticals,
                                       &status, &calls, logPath);
  if (QTest::currentTestFailed())
    return;
  QCOMPARE(contentsOf(QDir(storeDir.path()).filePath("entry0.gpg")),
           QByteArrayLiteral("not really encrypted"));
  QVERIFY2(std::any_of(calls.cbegin(), calls.cend(),
                       [](const QStringList &c) {
                         return c.first() == QStringLiteral("-d") &&
                                c.last().endsWith(QStringLiteral(".tmp"));
                       }),
           "the new ciphertext must have been test-decrypted and compared");
#endif
}

/**
 * @brief The entry disappears while gpg encrypts (another client deleted
 *        it): the new ciphertext is dropped and the file counted as failed;
 *        nothing is invented under its name.
 */
void tst_imitatepass::reencryptFailsWhenTheOriginalVanishesBeforeTheSwap() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString entry = QDir(storeDir.path()).filePath("entry0.gpg");
  const QString fakeGpg = writeCustomGpg(
      storeDir.path(), logPath,
      {{QStringLiteral("encrypt"),
        QStringLiteral("cat >/dev/null; rm -f '%1'; printf 'ciphertext\\n' > "
                       "\"$outfile\"")
            .arg(entry)}});
  QVERIFY(!fakeGpg.isEmpty());
  QStringList criticals;
  QStringList status;
  QList<QStringList> calls;
  expectSingleFailureLeavesEntryIntact(storeDir.path(), fakeGpg, &criticals,
                                       &status, &calls, logPath);
  if (QTest::currentTestFailed())
    return;
  QVERIFY2(!QFileInfo::exists(entry),
           "a deleted entry must not come back as the new ciphertext");
#endif
}

/**
 * @brief The new ciphertext is gone by the time it should take the entry's
 *        place: the entry is untouched and the user is told. Also pins the
 *        newline the decrypted text gets when gpg prints none.
 */
void tst_imitatepass::reencryptLeavesTheOriginalWhenTheCiphertextVanishes() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  // The test-decrypt of the scratch file answers correctly, then the file
  // is gone before it is placed; the entry's own decrypt has no trailing
  // newline.
  const QString fakeGpg = writeCustomGpg(
      storeDir.path(), logPath,
      {{QStringLiteral("decrypt"),
        QStringLiteral("case \"$last\" in *.tmp) printf 'plaintext\\n'; rm -f "
                       "\"$last\";; *) printf 'plaintext';; esac")}});
  QVERIFY(!fakeGpg.isEmpty());
  QStringList criticals;
  QStringList status;
  QList<QStringList> calls;
  expectSingleFailureLeavesEntryIntact(storeDir.path(), fakeGpg, &criticals,
                                       &status, &calls, logPath);
  if (QTest::currentTestFailed())
    return;
  QCOMPARE(contentsOf(QDir(storeDir.path()).filePath("entry0.gpg")),
           QByteArrayLiteral("not really encrypted"));
  QVERIFY2(std::any_of(criticals.cbegin(), criticals.cend(),
                       [](const QString &m) {
                         return m.contains("gpg wrote no ciphertext") &&
                                m.contains("entry0.gpg");
                       }),
           qPrintable(criticals.join(" | ")));
#endif
}

/**
 * @brief The new ciphertext is staged next to the entry; when nothing can be
 *        created there (the folder is read-only) the file is counted as
 *        failed and left exactly as it was, the user told why, and no
 *        temporary is left anywhere.
 */
void tst_imitatepass::reencryptFailsWhenNoTemporaryCanBeMadeNextToTheEntry() {
#ifdef Q_OS_WIN
  QSKIP("uses directory permissions and a shell script as a fake gpg");
#else
  if (geteuid() == 0)
    QSKIP("root ignores directory permissions");
  QTemporaryDir storeDir;
  QTemporaryDir toolDir;
  QVERIFY(storeDir.isValid() && toolDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  // The fake gpg and its log live outside the store, which is about to
  // become read-only.
  const QString logPath = QDir(toolDir.path()).filePath("gpg-argv.log");
  const QString fakeGpg = writeCustomGpg(toolDir.path(), logPath, {});
  QVERIFY(!fakeGpg.isEmpty());
  QVERIFY(QFile::setPermissions(storeDir.path(),
                                QFile::ReadOwner | QFile::ExeOwner));
  QStringList criticals;
  QStringList status;
  QList<QStringList> calls;
  expectSingleFailureLeavesEntryIntact(storeDir.path(), fakeGpg, &criticals,
                                       &status, &calls, logPath);
  QVERIFY(QFile::setPermissions(
      storeDir.path(), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
  if (QTest::currentTestFailed())
    return;
  QCOMPARE(contentsOf(QDir(storeDir.path()).filePath("entry0.gpg")),
           QByteArrayLiteral("not really encrypted"));
  // The scratch file is outside the store, so gpg did encrypt; only the
  // placing failed.
  QCOMPARE(encryptCalls(calls).size(), 1);
  QVERIFY2(std::any_of(criticals.cbegin(), criticals.cend(),
                       [](const QString &m) {
                         return m.contains("temporary file next to") &&
                                m.contains("entry0.gpg");
                       }),
           qPrintable(criticals.join(" | ")));
#endif
}

/**
 * @brief The entry is re-encrypted on disk but `git add` fails: the
 *        repository is behind, so the file counts as failed and the run is
 *        not pushed.
 */
void tst_imitatepass::reencryptCountsAFailedGitAddAsAFailure() {
#ifdef Q_OS_WIN
  QSKIP("uses shell scripts as fake gpg and git");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString gitLog = QDir(storeDir.path()).filePath("git-argv.log");
  const QString fakeGit =
      writeGitFailingOn(storeDir.path(), gitLog, QStringLiteral("add"));
  QVERIFY(!fakeGit.isEmpty());
  Recorder rec;
  QList<QStringList> git;
  int encrypts = 0;
  QVERIFY(runReencryptWithGit(storeDir.path(), fakeGit, rec, &git, &encrypts));
  QCOMPARE(encrypts, 1);
  QCOMPARE(contentsOf(QDir(storeDir.path()).filePath("entry0.gpg")),
           QByteArrayLiteral("ciphertext\n"));
  QVERIFY2(std::any_of(rec.criticals.cbegin(), rec.criticals.cend(),
                       [](const QString &m) {
                         return m.contains("could not be re-encrypted") &&
                                m.contains("entry0.gpg");
                       }),
           qPrintable(rec.criticals.join(" | ")));
  QVERIFY2(
      std::any_of(rec.statusMessages.cbegin(), rec.statusMessages.cend(),
                  [](const QString &m) { return m.contains("Not pushing"); }),
      qPrintable(rec.statusMessages.join(" | ")));
  QVERIFY2(!hasPush(git),
           "a store the repository does not match is not pushed");
  QVERIFY2(std::none_of(git.cbegin(), git.cend(),
                        [](const QStringList &c) {
                          return c.contains(QStringLiteral("commit"));
                        }),
           "nothing is committed after a failed add");
#endif
}

/**
 * @brief Like a failed add, a failed per-file commit counts the file as
 *        failed and keeps the run from being pushed.
 */
void tst_imitatepass::reencryptCountsAFailedGitCommitAsAFailure() {
#ifdef Q_OS_WIN
  QSKIP("uses shell scripts as fake gpg and git");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString gitLog = QDir(storeDir.path()).filePath("git-argv.log");
  const QString fakeGit =
      writeGitFailingOn(storeDir.path(), gitLog, QStringLiteral("commit"));
  QVERIFY(!fakeGit.isEmpty());
  Recorder rec;
  QList<QStringList> git;
  int encrypts = 0;
  QVERIFY(runReencryptWithGit(storeDir.path(), fakeGit, rec, &git, &encrypts));
  QCOMPARE(encrypts, 1);
  QVERIFY2(std::any_of(rec.criticals.cbegin(), rec.criticals.cend(),
                       [](const QString &m) {
                         return m.contains("could not be re-encrypted") &&
                                m.contains("entry0.gpg");
                       }),
           qPrintable(rec.criticals.join(" | ")));
  QVERIFY2(std::any_of(git.cbegin(), git.cend(),
                       [](const QStringList &c) {
                         return c.contains(QStringLiteral("add")) &&
                                c.last().endsWith("entry0.gpg");
                       }),
           "the file was added before the commit failed");
  QVERIFY2(std::any_of(git.cbegin(), git.cend(),
                       [](const QStringList &c) {
                         return c.contains(QStringLiteral("commit")) &&
                                c.contains(QStringLiteral("Re-encrypt"));
                       }),
           "the per-file commit was attempted");
  QVERIFY2(!hasPush(git), "a run with a failed file is not pushed");
#endif
}

/**
 * @brief The backup commit needs `git status`; when that fails the run is
 *        aborted with a dialog before any file is touched.
 */
void tst_imitatepass::reencryptAbortsWhenGitStatusFails() {
#ifdef Q_OS_WIN
  QSKIP("uses shell scripts as fake gpg and git");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString gitLog = QDir(storeDir.path()).filePath("git-argv.log");
  const QString fakeGit =
      writeGitFailingOn(storeDir.path(), gitLog, QStringLiteral("status"));
  QVERIFY(!fakeGit.isEmpty());
  Recorder rec;
  QList<QStringList> git;
  int encrypts = 0;
  QVERIFY(runReencryptWithGit(storeDir.path(), fakeGit, rec, &git, &encrypts));
  QCOMPARE(encrypts, 0);
  QCOMPARE(rec.criticals.size(), 1);
  QVERIFY2(rec.criticals.first().contains(
               QStringLiteral("Could not inspect git status")),
           qPrintable(rec.criticals.first()));
  QCOMPARE(contentsOf(QDir(storeDir.path()).filePath("entry0.gpg")),
           QByteArrayLiteral("not really encrypted"));
  QVERIFY2(
      std::none_of(rec.statusMessages.cbegin(), rec.statusMessages.cend(),
                   [](const QString &m) { return m.contains("completed"); }),
      qPrintable(rec.statusMessages.join(" | ")));
  QVERIFY2(!hasPush(git), "an aborted run is not pushed");
#endif
}

/**
 * @brief A dirty tree is committed (tracked files only, `add -u`) before
 *        re-encryption; when that commit fails there is no safe state to fall
 *        back to, so the run is aborted with a dialog.
 */
void tst_imitatepass::reencryptAbortsWhenTheBackupCommitFails() {
#ifdef Q_OS_WIN
  QSKIP("uses shell scripts as fake gpg and git");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString gitLog = QDir(storeDir.path()).filePath("git-argv.log");
  const QString fakeGit = writeScriptedGit(
      storeDir.path(), gitLog,
      QStringLiteral("  *' status '*) printf ' M entry0.gpg\\n';;\n"
                     "  *' commit '*) exit 1;;"));
  QVERIFY(!fakeGit.isEmpty());
  Recorder rec;
  QList<QStringList> git;
  int encrypts = 0;
  QVERIFY(runReencryptWithGit(storeDir.path(), fakeGit, rec, &git, &encrypts));
  QCOMPARE(encrypts, 0);
  QCOMPARE(rec.criticals.size(), 1);
  QVERIFY2(rec.criticals.first().contains(
               QStringLiteral("git backup could not be created")),
           qPrintable(rec.criticals.first()));
  QVERIFY2(std::any_of(git.cbegin(), git.cend(),
                       [](const QStringList &c) {
                         return c.contains(QStringLiteral("add")) &&
                                c.contains(QStringLiteral("-u"));
                       }),
           "only tracked changes are staged for the backup");
  QVERIFY2(std::any_of(git.cbegin(), git.cend(),
                       [](const QStringList &c) {
                         return c.contains(QStringLiteral("commit")) &&
                                c.contains(QStringLiteral("Backup"));
                       }),
           "the backup commit was attempted");
  QVERIFY2(!hasPush(git), "an aborted run is not pushed");
#endif
}

/**
 * @brief A cancel that lands while the run is still waiting for the executor
 *        queue to drain (a Show is running) ends the run without ever
 *        starting the worker: no file is looked at and the summary says so.
 */
void tst_imitatepass::reencryptCancelledBeforeTheWorkerStartsEndsAtOnce() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a slow fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 2));
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  // Every call takes a second: long enough for the cancel to land while the
  // Show below still occupies the executor.
  const QString fakeGpg = writeCustomGpg(
      storeDir.path(), logPath,
      {{QStringLiteral("decrypt"), QStringLiteral("sleep 1; printf 'pw\\n'")}});
  QVERIFY(!fakeGpg.isEmpty());
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), fakeGpg));
  QObject ctx;
  Recorder rec;
  record(pass, ctx, rec);
  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  QSignalSpy startSpy(&pass, &ImitatePass::startReencryptPath);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);

  pass.Show(QStringLiteral("entry0"));
  pass.reencryptPath(storeDir.path());
  QCOMPARE(startSpy.count(), 1);
  pass.cancelReencryptPath();
  QVERIFY2(endSpy.count() > 0 || endSpy.wait(5000),
           "the cancelled run must end without waiting for the worker");
  QCoreApplication::processEvents();
  QVERIFY2(rec.progress.isEmpty(), "no file may have been looked at");
  QVERIFY2(rec.criticals.isEmpty(), qPrintable(rec.criticals.join(" | ")));
  QVERIFY2(!rec.statusMessages.isEmpty() &&
               rec.statusMessages.last().contains(
                   QStringLiteral("cancelled: 0 of 0")),
           qPrintable(rec.statusMessages.join(" | ")));
  QVERIFY2(showSpy.count() > 0 || showSpy.wait(5000),
           "the Show that held the executor must still complete");
  QVERIFY2(encryptCalls(loggedCalls(logPath)).isEmpty(),
           "nothing may be re-encrypted after the cancel");
#endif
}

/**
 * @brief With autoPull on the run pulls first, telling the user so, and a
 *        pull that works changes nothing else about the run.
 */
void tst_imitatepass::reencryptPullsFirstWhenAutoPullIsOn() {
#ifdef Q_OS_WIN
  QSKIP("uses shell scripts as fake gpg and git");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString gitLog = QDir(storeDir.path()).filePath("git-argv.log");
  const QString fakeGit = writeRecordingGit(storeDir.path(), gitLog);
  QVERIFY(!fakeGit.isEmpty());
  Recorder rec;
  QList<QStringList> git;
  int encrypts = 0;
  QVERIFY2(runReencryptWithGit(storeDir.path(), fakeGit, rec, &git, &encrypts,
                               true, true),
           "the run and its push must finish");
  QVERIFY2(rec.criticals.isEmpty(), qPrintable(rec.criticals.join(" | ")));
  QCOMPARE(encrypts, 1);
  QVERIFY2(!git.isEmpty(), "git must have been asked to pull");
  QVERIFY2(git.first().first() == QStringLiteral("-C") &&
               git.first().last() == QStringLiteral("pull"),
           qPrintable("the pull comes first, in the store: " +
                      git.first().join(' ')));
  // Pull first, then the backup commit: the "Updating" notice precedes it.
  const int updating = rec.statusMessages.indexOf(
      QRegularExpression(QStringLiteral("^Updating password-store")));
  const int backup = rec.statusMessages.indexOf(
      QRegularExpression(QStringLiteral("^Creating backup commit")));
  QVERIFY2(updating >= 0 && backup > updating,
           qPrintable(rec.statusMessages.join(" | ")));
  QVERIFY2(hasPush(git), "a clean run with autoPush on is pushed");
#endif
}

/**
 * @brief A pull that fails without leaving a merge behind (no remote, say)
 *        leaves the store as it was: the user is told and the run goes on.
 */
void tst_imitatepass::reencryptGoesOnWhenThePullFailsCleanly() {
#ifdef Q_OS_WIN
  QSKIP("uses shell scripts as fake gpg and git");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString gitLog = QDir(storeDir.path()).filePath("git-argv.log");
  const QString fakeGit = writeScriptedGit(
      storeDir.path(), gitLog, QStringLiteral("  *' pull') exit 1;;"));
  QVERIFY(!fakeGit.isEmpty());
  Recorder rec;
  QList<QStringList> git;
  int encrypts = 0;
  QVERIFY2(runReencryptWithGit(storeDir.path(), fakeGit, rec, &git, &encrypts,
                               true, true),
           "the run and its push must finish");
  QVERIFY2(rec.criticals.isEmpty(), qPrintable(rec.criticals.join(" | ")));
  QCOMPARE(encrypts, 1);
  QVERIFY2(std::any_of(rec.statusMessages.cbegin(), rec.statusMessages.cend(),
                       [](const QString &m) {
                         return m.contains("Git pull failed") &&
                                m.contains("as it is");
                       }),
           qPrintable(rec.statusMessages.join(" | ")));
  QVERIFY2(std::any_of(git.cbegin(), git.cend(),
                       [](const QStringList &c) {
                         return c.contains(QStringLiteral("ls-files")) &&
                                c.contains(QStringLiteral("--unmerged"));
                       }),
           "the run must check for an unfinished merge");
  QVERIFY2(std::any_of(rec.statusMessages.cbegin(), rec.statusMessages.cend(),
                       [](const QString &m) {
                         return m.contains("completed") &&
                                m.contains("1 files re-encrypted");
                       }),
           qPrintable(rec.statusMessages.join(" | ")));
  QVERIFY2(hasPush(git), "a failed pull alone does not stop the push");
#endif
}

/**
 * @brief A pull that stopped in a merge leaves conflict markers and an
 *        unmerged index; re-encrypting on top of that would commit the mess,
 *        so the run is aborted with a dialog and nothing is encrypted.
 */
void tst_imitatepass::reencryptAbortsWhenThePullLeavesConflicts() {
#ifdef Q_OS_WIN
  QSKIP("uses shell scripts as fake gpg and git");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString gitLog = QDir(storeDir.path()).filePath("git-argv.log");
  const QString fakeGit = writeScriptedGit(
      storeDir.path(), gitLog,
      QStringLiteral("  *' pull') exit 1;;\n"
                     "  *' ls-files --unmerged') printf '100644 0123abcd 1\\t"
                     "entry0.gpg\\n';;"));
  QVERIFY(!fakeGit.isEmpty());
  Recorder rec;
  QList<QStringList> git;
  int encrypts = 0;
  QVERIFY(runReencryptWithGit(storeDir.path(), fakeGit, rec, &git, &encrypts,
                              true));
  QCOMPARE(encrypts, 0);
  QCOMPARE(rec.criticals.size(), 1);
  QVERIFY2(rec.criticals.first().contains(QStringLiteral("unmerged")),
           qPrintable(rec.criticals.first()));
  QCOMPARE(contentsOf(QDir(storeDir.path()).filePath("entry0.gpg")),
           QByteArrayLiteral("not really encrypted"));
  QVERIFY2(!hasPush(git), "an aborted run is not pushed");
  QVERIFY2(std::none_of(git.cbegin(), git.cend(),
                        [](const QStringList &c) {
                          return c.contains(QStringLiteral("status"));
                        }),
           "no backup commit is attempted on an unmerged tree");
#endif
}

/**
 * @brief The failure dialog lists at most fifteen files and says how many
 *        more there are, so a store of thousands does not produce a dialog
 *        taller than the screen.
 */
void tst_imitatepass::failureDialogListsAtMostFifteenFiles() {
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  const int fileCount = 17;
  QVERIFY(populateStore(storeDir.path(), fileCount));
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), unstartableGpg()));
  QObject ctx;
  Recorder rec;
  record(pass, ctx, rec);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);

  pass.reencryptPath(storeDir.path());
  QVERIFY2(endSpy.count() > 0 || endSpy.wait(30000),
           "re-encryption must finish");
  QCoreApplication::processEvents();
  QCOMPARE(rec.criticals.size(), 1);
  const QString dialog = rec.criticals.first();
  QVERIFY2(dialog.startsWith(QStringLiteral("17 file")) &&
               dialog.contains(QStringLiteral("could not be re-encrypted")),
           qPrintable(dialog));
  int listed = 0;
  for (int i = 0; i < fileCount; ++i)
    if (dialog.contains(QStringLiteral("entry%1.gpg").arg(i)))
      ++listed;
  QCOMPARE(listed, 15);
  QVERIFY2(dialog.contains(QStringLiteral("and 2 more")), qPrintable(dialog));
  QVERIFY2(!rec.statusMessages.isEmpty() &&
               rec.statusMessages.last().contains(QStringLiteral("17 failed")),
           qPrintable(rec.statusMessages.last()));
}

/**
 * @brief Moving a folder without git: into an existing folder it keeps its
 *        name, onto a file it is refused, and to a new name it is renamed.
 */
void tst_imitatepass::moveFolderResolvesItsDestination() {
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QDir root(storeDir.path());
  for (const QString &folder :
       {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"),
        QStringLiteral("target")}) {
    QVERIFY(root.mkpath(folder));
  }
  for (const QString &folder :
       {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}) {
    QFile f(root.filePath(folder + QStringLiteral("/x.gpg")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(folder.toUtf8());
  }
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), unstartableGpg()));
  QSignalSpy criticalSpy(&pass, &Pass::critical);

  // Into an existing folder.
  pass.Move(root.filePath(QStringLiteral("a")),
            root.filePath(QStringLiteral("target")), false);
  QVERIFY2(!QFileInfo::exists(root.filePath(QStringLiteral("a"))),
           "the folder must have moved into the destination folder");
  QCOMPARE(contentsOf(root.filePath(QStringLiteral("target/a/x.gpg"))),
           QByteArrayLiteral("a"));
  // Onto a file: refused, nothing moves.
  pass.Move(root.filePath(QStringLiteral("b")),
            root.filePath(QStringLiteral("entry0.gpg")), false);
  QCOMPARE(contentsOf(root.filePath(QStringLiteral("b/x.gpg"))),
           QByteArrayLiteral("b"));
  QCOMPARE(contentsOf(root.filePath(QStringLiteral("entry0.gpg"))),
           QByteArrayLiteral("not really encrypted"));
  // To a new name.
  pass.Move(root.filePath(QStringLiteral("c")),
            root.filePath(QStringLiteral("renamed")), false);
  QVERIFY2(!QFileInfo::exists(root.filePath(QStringLiteral("c"))),
           "the folder must have been renamed");
  QCOMPARE(contentsOf(root.filePath(QStringLiteral("renamed/x.gpg"))),
           QByteArrayLiteral("c"));
  QCOMPARE(criticalSpy.count(), 0);
}

/**
 * @brief Without git a forced move over an existing entry replaces it on
 *        the filesystem: the destination holds the source's bytes and the
 *        source is gone; unforced, the clash is refused and both stay.
 */
void tst_imitatepass::moveWithForceReplacesTheDestinationWithoutGit() {
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 2));
  const QDir root(storeDir.path());
  const QString src = root.filePath(QStringLiteral("entry0.gpg"));
  const QString dst = root.filePath(QStringLiteral("entry1.gpg"));
  {
    QFile f(src);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("moved bytes");
  }
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), unstartableGpg()));

  pass.Move(src, dst, false);
  QCOMPARE(contentsOf(src), QByteArrayLiteral("moved bytes"));
  QCOMPARE(contentsOf(dst), QByteArrayLiteral("not really encrypted"));
  pass.Move(src, dst, true);
  QVERIFY2(!QFileInfo::exists(src), "the source must be gone after the move");
  QCOMPARE(contentsOf(dst), QByteArrayLiteral("moved bytes"));
}

/**
 * @brief With git on, Move is `git mv` (with -f when forced) and one commit
 *        naming both entries relative to the store; the interface hears
 *        finishedMove once the commit is done.
 */
void tst_imitatepass::moveWithGitUsesGitMvAndCommits() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a recording fake git");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QDir root(storeDir.path());
  const QString gitLog = root.filePath("git-argv.log");
  const QString fakeGit = writeRecordingGit(storeDir.path(), gitLog);
  QVERIFY(!fakeGit.isEmpty());
  AppSettings s = settingsFor(storeDir.path(), unstartableGpg());
  s.useGit = true;
  s.gitExecutable = fakeGit;
  ImitatePass pass;
  pass.init(s);
  QSignalSpy moveSpy(&pass, &Pass::finishedMove);
  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);
  const QString src = root.filePath(QStringLiteral("entry0.gpg"));
  const QString dst = root.filePath(QStringLiteral("renamed.gpg"));

  pass.Move(src, dst, false);
  QVERIFY2(moveSpy.count() > 0 || moveSpy.wait(5000),
           "finishedMove must follow the commit");
  QCOMPARE(errorSpy.count(), 0);
  QList<QStringList> calls = loggedCalls(gitLog);
  QCOMPARE(calls.size(), 2);
  QCOMPARE(calls.at(0).mid(0, 2),
           (QStringList{QStringLiteral("mv"), QStringLiteral("--")}));
  QCOMPARE(QDir::cleanPath(calls.at(0).at(2)), QDir::cleanPath(src));
  QCOMPARE(QDir::cleanPath(calls.at(0).at(3)), QDir::cleanPath(dst));
  QVERIFY2(calls.at(1).first() == QStringLiteral("commit") &&
               calls.at(1).join(' ').contains(
                   QStringLiteral("Moved for entry0 to renamed using QtPass.")),
           qPrintable(calls.at(1).join(' ')));

  // Forced: git gets -f.
  QFile::remove(gitLog);
  moveSpy.clear();
  pass.Move(src, dst, true);
  QVERIFY2(moveSpy.count() > 0 || moveSpy.wait(5000),
           "finishedMove must follow the forced move's commit");
  calls = loggedCalls(gitLog);
  QCOMPARE(calls.size(), 2);
  QCOMPARE(calls.at(0).mid(0, 3),
           (QStringList{QStringLiteral("mv"), QStringLiteral("-f"),
                        QStringLiteral("--")}));
  QCOMPARE(errorSpy.count(), 0);
#endif
}

/**
 * @brief Copying into a folder that does not exist cannot stage the bytes
 *        next to the destination: the copy fails with a dialog, nothing is
 *        created and no re-encryption starts.
 */
void tst_imitatepass::copyFailsWhenTheDestinationFolderDoesNotExist() {
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QDir root(storeDir.path());
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), unstartableGpg()));
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  QSignalSpy startSpy(&pass, &ImitatePass::startReencryptPath);
  const QString dst = root.filePath(QStringLiteral("nowhere/new.gpg"));

  pass.Copy(root.filePath(QStringLiteral("entry0.gpg")), dst, false);
  QTest::qWait(200);
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(criticalSpy.first().at(0).toString(), QStringLiteral("Copy failed"));
  QVERIFY2(criticalSpy.first().at(1).toString().contains(dst),
           qPrintable(criticalSpy.first().at(1).toString()));
  QVERIFY2(!QFileInfo::exists(dst),
           "nothing may be created at the destination");
  QVERIFY2(!QFileInfo::exists(root.filePath(QStringLiteral("nowhere"))),
           "the missing folder must not be created");
  QCOMPARE(startSpy.count(), 0);
  QCOMPARE(
      root.entryList({QStringLiteral(".*.tmp")}, QDir::Files | QDir::Hidden)
          .size(),
      0);
}

/**
 * @brief gpg exits 0 without writing a ciphertext: that is not an entry. The
 *        add fails through the failed-operation path with a message that
 *        says so, and nothing appears under the entry's name.
 */
void tst_imitatepass::insertFailsWhenGpgWritesNoCiphertext() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QDir root(storeDir.path());
  const QString logPath = root.filePath("gpg-argv.log");
  const QString fakeGpg = writeCustomGpg(
      storeDir.path(), logPath,
      {{QStringLiteral("encrypt"), QStringLiteral("cat >/dev/null")}});
  QVERIFY(!fakeGpg.isEmpty());
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), fakeGpg));
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);

  pass.Insert(QStringLiteral("new"), QStringLiteral("s\n"), false);
  QVERIFY2(errorSpy.count() > 0 || errorSpy.wait(15000),
           "the add must fail through processErrorExit");
  QCOMPARE(insertSpy.count(), 0);
  QVERIFY2(errorSpy.first().at(1).toString().contains(
               QStringLiteral("wrote no ciphertext")),
           qPrintable(errorSpy.first().at(1).toString()));
  QVERIFY2(!QFileInfo::exists(root.filePath(QStringLiteral("new.gpg"))),
           "an empty ciphertext must not become an entry");
  QCOMPARE(
      root.entryList({QStringLiteral(".*.tmp")}, QDir::Files | QDir::Hidden)
          .size(),
      0);
  QCOMPARE(encryptCalls(loggedCalls(logPath)).size(), 1);
#endif
}

/**
 * @brief The entry's folder disappears while gpg runs (deleted by another
 *        client): the ciphertext cannot be staged next to the entry, the add
 *        fails with the reason, and the folder is not recreated.
 */
void tst_imitatepass::insertFailsWhenTheEntryFolderVanishes() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QDir root(storeDir.path());
  QVERIFY(root.mkpath(QStringLiteral("sub")));
  const QString logPath = root.filePath("gpg-argv.log");
  const QString fakeGpg = writeCustomGpg(
      storeDir.path(), logPath,
      {{QStringLiteral("encrypt"),
        QStringLiteral("cat >/dev/null; rm -rf '%1'; printf 'ciphertext\\n' > "
                       "\"$outfile\"")
            .arg(root.filePath(QStringLiteral("sub")))}});
  QVERIFY(!fakeGpg.isEmpty());
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), fakeGpg));
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);

  pass.Insert(QStringLiteral("sub/new"), QStringLiteral("s\n"), false);
  QVERIFY2(errorSpy.count() > 0 || errorSpy.wait(15000),
           "the add must fail through processErrorExit");
  QCOMPARE(insertSpy.count(), 0);
  QVERIFY2(errorSpy.first().at(1).toString().contains(
               QStringLiteral("Cannot create a temporary file next to")),
           qPrintable(errorSpy.first().at(1).toString()));
  QVERIFY2(!QFileInfo::exists(root.filePath(QStringLiteral("sub"))),
           "the vanished folder must not be recreated");
  QCOMPARE(encryptCalls(loggedCalls(logPath)).size(), 1);
#endif
}

QTEST_MAIN(tst_imitatepass)
#include "tst_imitatepass.moc"
