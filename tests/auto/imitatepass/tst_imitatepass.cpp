// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @brief Tests for the gpg argv ImitatePass builds when it encrypts.
 *
 * No real gpg is needed: a recording fake gpg logs every argv it receives and
 * behaves just enough like gpg for Insert() and reencryptSingleFile() to
 * succeed. Every encrypt call must carry --no-encrypt-to (and
 * --compress-algo=none, as pass(1) does) so a user's gpg.conf cannot add a
 * recipient that the .gpg-id does not list.
 */

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>
#ifndef Q_OS_WIN
#include <unistd.h>
#endif

#include "../../../src/imitatepass.h"
#include "../testsettings.h"

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

class tst_imitatepass : public QObject {
  Q_OBJECT

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

  /// The bytes of @p path, or an empty array when it cannot be read.
  static QByteArray contentsOf(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
      return {};
    return f.readAll();
  }

  /// Only the encrypt (-eq) calls out of @p calls.
  static QList<QStringList> encryptCalls(const QList<QStringList> &calls) {
    QList<QStringList> out;
    for (const QStringList &c : calls)
      if (c.contains(QStringLiteral("-eq")))
        out << c;
    return out;
  }

private Q_SLOTS:
  void initTestCase();
  void insertEncryptArgvCarriesNoEncryptTo();
  void reencryptEncryptArgvCarriesNoEncryptTo();
  void everyDecryptionCapsItsOutput();
  void removeWithoutGitReportsWhatItCannotRemove();
  void moveWithoutGitReportsAFailedMove();
  void aFailedForcedMoveWithoutGitKeepsTheDestination();
  void moveAndCopyCleanDotDotBeforeTheyLook();
  void verifyChecksBoundedCopiesWithBatch();
  void verifyRefusesACommitOrTagObject();
};

void tst_imitatepass::initTestCase() { isolateTestSettings(); }

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
 * @brief Every gpg decryption ImitatePass runs carries --max-output: a small
 * compressed entry in a shared store cannot decompress into all of
 * QtPass's memory, whether shown or re-encrypted (decrypt, then the check
 * that the new ciphertext holds the same plaintext).
 */
void tst_imitatepass::everyDecryptionCapsItsOutput() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a recording fake gpg");
#else
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QString logPath = QDir(storeDir.path()).filePath("gpg-argv.log");
  const QString fakeGpg = writeRecordingGpg(storeDir.path(), logPath);
  QVERIFY2(!fakeGpg.isEmpty(), "failed to write the recording fake gpg");

  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), fakeGpg));
  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("/entry0"));
  QVERIFY(showSpy.count() > 0 || showSpy.wait(15000));
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
  pass.reencryptPath(storeDir.path());
  QVERIFY(endSpy.count() > 0 || endSpy.wait(30000));

  const QString limit = QString::number(Pass::kMaxPlaintextBytes);
  int decryptions = 0;
  for (const QStringList &argv : loggedCalls(logPath)) {
    if (!argv.contains(QStringLiteral("-d"))) {
      continue;
    }
    ++decryptions;
    const int at = argv.indexOf(QStringLiteral("--max-output"));
    QVERIFY2(at >= 0 && argv.value(at + 1) == limit,
             qPrintable(QStringLiteral("decryption without --max-output %1: %2")
                            .arg(limit, argv.join(' '))));
  }
  // Show, the re-encryption's decrypt, and its check of the new file.
  QVERIFY2(decryptions >= 3, qPrintable(QString::number(decryptions)));
#endif
}

/**
 * @brief The per-file re-encryption in reencryptPath() is the second place
 * QtPass encrypts; it must carry the same flags. (The decrypt (-d) calls
 * around it carry --max-output; everyDecryptionCapsItsOutput checks those.)
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
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  QSignalSpy statusSpy(&pass, &Pass::statusMsg);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);

  pass.reencryptPath(storeDir.path()); // synchronous (executeBlocking)
  QVERIFY2(endSpy.count() > 0 || endSpy.wait(30000),
           "endReencryptPath must close the run");

  // Positive: with a cooperating gpg every file is re-encrypted, none fails.
  QStringList criticals;
  for (const QList<QVariant> &args : criticalSpy)
    criticals << args.value(1).toString();
  QVERIFY2(criticals.isEmpty(),
           qPrintable(criticals.join(QStringLiteral(" | "))));
  QVERIFY(!statusSpy.isEmpty());
  const QString lastStatus = statusSpy.last().value(0).toString();
  QVERIFY2(lastStatus.contains(
               QStringLiteral("%1 files re-encrypted").arg(fileCount)),
           qPrintable(lastStatus));

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
 * @brief Without Git, Remove() deletes on its own, and a failure was said
 *        nowhere: a folder delete that met something it could not remove
 *        went on, as rm -rf does, and left the rest without a word; a single
 *        entry stayed without one. Both are reported now, as git rm's error
 *        is with Git on.
 */
void tst_imitatepass::removeWithoutGitReportsWhatItCannotRemove() {
#ifdef Q_OS_WIN
  QSKIP("uses directory permissions");
#else
  if (::geteuid() == 0)
    QSKIP("root ignores directory permissions");
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 0));
  const QDir root(storeDir.path());
  QVERIFY(root.mkpath(QStringLiteral("work/archive")));
  QVERIFY(root.mkpath(QStringLiteral("locked")));
  for (const char *name : {"work/db.gpg", "work/vpn.gpg",
                           "work/archive/old-vpn.gpg", "locked/entry.gpg"}) {
    QFile f(root.filePath(QString::fromLatin1(name)));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
  }
  const QStringList readOnly{root.filePath(QStringLiteral("work/archive")),
                             root.filePath(QStringLiteral("locked"))};
  const auto writable = qScopeGuard([&readOnly] {
    for (const QString &dir : readOnly) {
      QFile::setPermissions(dir, QFile::ReadOwner | QFile::WriteOwner |
                                     QFile::ExeOwner);
    }
  });
  for (const QString &dir : readOnly) {
    QVERIFY(QFile::setPermissions(dir, QFile::ReadOwner | QFile::ExeOwner));
  }
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), QStringLiteral("/nonexistent/gpg")));
  QSignalSpy criticalSpy(&pass, &Pass::critical);

  pass.Remove(QStringLiteral("work"), true);
  QVERIFY(!QFileInfo::exists(root.filePath(QStringLiteral("work/db.gpg"))));
  QVERIFY(QFileInfo::exists(
      root.filePath(QStringLiteral("work/archive/old-vpn.gpg"))));
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(criticalSpy.at(0).at(0).toString(), QStringLiteral("Delete failed"));
  QVERIFY2(criticalSpy.at(0).at(1).toString().contains(
               QStringLiteral("Could not remove all of")),
           qPrintable(criticalSpy.at(0).at(1).toString()));

  pass.Remove(QStringLiteral("locked/entry"), false);
  QVERIFY(QFileInfo::exists(root.filePath(QStringLiteral("locked/entry.gpg"))));
  QCOMPARE(criticalSpy.count(), 2);
  QCOMPARE(criticalSpy.at(1).at(0).toString(), QStringLiteral("Delete failed"));
#endif
}

/**
 * @brief Without Git, a move that does not happen says so: renaming an entry
 *        to a name its folder already has did nothing, without a word.
 */
void tst_imitatepass::moveWithoutGitReportsAFailedMove() {
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 2));
  const QDir root(storeDir.path());
  const QString src = root.filePath(QStringLiteral("entry0.gpg"));
  const QString taken = root.filePath(QStringLiteral("entry1.gpg"));
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), QStringLiteral("/nonexistent/gpg")));
  QSignalSpy criticalSpy(&pass, &Pass::critical);

  // As "Rename password" hands it over: the new name without ".gpg".
  pass.Move(src, root.filePath(QStringLiteral("entry1")), false);
  QVERIFY(QFileInfo::exists(src));
  QCOMPARE(contentsOf(taken), QByteArrayLiteral("not really encrypted"));
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(criticalSpy.first().at(0).toString(), QStringLiteral("Move failed"));

  // A forced move still replaces the destination, and says nothing.
  QFile moving(src);
  QVERIFY(moving.open(QIODevice::WriteOnly));
  moving.write("moved bytes");
  moving.close();
  pass.Move(src, taken, true);
  QVERIFY(!QFileInfo::exists(src));
  QCOMPARE(contentsOf(taken), QByteArrayLiteral("moved bytes"));
  QVERIFY(!QFileInfo::exists(taken + QStringLiteral(".move.bak")));
  QCOMPARE(criticalSpy.count(), 1);
}

/**
 * @brief A forced move without Git removed the destination entry and then
 *        renamed: when the rename failed, the destination was gone and the
 *        source not moved, and a forced move of an entry onto its own name
 *        deleted it. The destination is now set aside and put back.
 */
void tst_imitatepass::aFailedForcedMoveWithoutGitKeepsTheDestination() {
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QDir root(storeDir.path());
  const QString own = root.filePath(QStringLiteral("entry0.gpg"));
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), QStringLiteral("/nonexistent/gpg")));
  pass.Move(own, own, true);
  QCOMPARE(contentsOf(own), QByteArrayLiteral("not really encrypted"));
#ifndef Q_OS_WIN
  if (::geteuid() == 0)
    QSKIP("root ignores directory permissions");
  QVERIFY(root.mkpath(QStringLiteral("locked")));
  const QString locked = root.filePath(QStringLiteral("locked"));
  const QString src = root.filePath(QStringLiteral("locked/moving.gpg"));
  {
    QFile f(src);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("moving bytes");
  }
  const auto writable = qScopeGuard([&locked] {
    QFile::setPermissions(locked, QFile::ReadOwner | QFile::WriteOwner |
                                      QFile::ExeOwner);
  });
  QVERIFY(QFile::setPermissions(locked, QFile::ReadOwner | QFile::ExeOwner));
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  pass.Move(src, own, true);
  QCOMPARE(contentsOf(own), QByteArrayLiteral("not really encrypted"));
  QCOMPARE(contentsOf(src), QByteArrayLiteral("moving bytes"));
  QVERIFY(!QFileInfo::exists(own + QStringLiteral(".move.bak")));
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(criticalSpy.first().at(0).toString(), QStringLiteral("Move failed"));
#endif
}

/**
 * @brief Move and Copy clean ".." out of the destination first: rename(2)
 *        follows a link before it applies "..", so "handbook/../team-old"
 *        with handbook linked out of the store, which the store check judged
 *        as <store>/team-old, moved the folder beside the link's target
 *        (ANT-2026-MVI4DGGO).
 */
void tst_imitatepass::moveAndCopyCleanDotDotBeforeTheyLook() {
#ifdef Q_OS_WIN
  QSKIP("uses symlinks");
#else
  QTemporaryDir storeDir;
  QTemporaryDir outsideDir;
  QVERIFY(storeDir.isValid() && outsideDir.isValid());
  QVERIFY(populateStore(storeDir.path(), 1));
  const QDir root(storeDir.path());
  const QDir outside(outsideDir.path());
  QVERIFY(outside.mkpath(QStringLiteral("handbook")));
  QVERIFY(root.mkpath(QStringLiteral("team")));
  {
    QFile f(root.filePath(QStringLiteral("team/db.gpg")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("team");
  }
  QVERIFY(QFile::link(outside.filePath(QStringLiteral("handbook")),
                      root.filePath(QStringLiteral("handbook"))));
  ImitatePass pass;
  pass.init(settingsFor(storeDir.path(), QStringLiteral("/nonexistent/gpg")));

  pass.Move(root.filePath(QStringLiteral("team")),
            root.filePath(QStringLiteral("handbook/../team-old")), false);
  QVERIFY2(!QFileInfo::exists(outside.filePath(QStringLiteral("team-old"))),
           "the folder must not land beside the link's target");
  QCOMPARE(contentsOf(root.filePath(QStringLiteral("team-old/db.gpg"))),
           QByteArrayLiteral("team"));

  pass.Copy(root.filePath(QStringLiteral("entry0.gpg")),
            root.filePath(QStringLiteral("handbook/../entry-copy")), false);
  QVERIFY2(
      !QFileInfo::exists(outside.filePath(QStringLiteral("entry-copy.gpg"))),
      "the copy must not land beside the link's target");
  QCOMPARE(contentsOf(root.filePath(QStringLiteral("entry-copy.gpg"))),
           QByteArrayLiteral("not really encrypted"));
#endif
}

/**
 * @brief verifyGpgIdFile() hands gpg copies of a .gpg-id and its .sig read
 *        here, with --batch: a .sig over 4 KiB, a FIFO or a link under
 *        either name is refused before gpg runs (ANT-2026-XQUAU1HG).
 */
void tst_imitatepass::verifyChecksBoundedCopiesWithBatch() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QDir root(dir.path());
  const QString fpr =
      QStringLiteral("0123456789ABCDEF0123456789ABCDEF01234567");
  const QString log = root.filePath(QStringLiteral("gpg.log"));
  const QString seen = root.filePath(QStringLiteral("seen.sig"));
  const QString script = root.filePath(QStringLiteral("gpg.sh"));
  {
    QFile f(script);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&f);
    out << "#!/bin/sh\n"
        << "printf '%s\\n' \"$*\" >> '" << log << "'\n"
        << "while [ \"$1\" != \"--\" ]; do shift; done\n"
        << "cat \"$2\" > '" << seen << "'\n"
        << "echo '[GNUPG:] VALIDSIG " << fpr << " 2026-11-25 1 0 4 0 1 10 00 "
        << fpr << "'\n"
        << "exit 0\n";
  }
  QVERIFY(QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                            QFile::ExeOwner));
  const QString gpgId = root.filePath(QStringLiteral(".gpg-id"));
  {
    QFile f(gpgId);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("ALICE\n");
  }
  const auto writeSig = [&gpgId](qint64 size) {
    QFile f(gpgId + QStringLiteral(".sig"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      f.write(QByteArray(size, 'S'));
    }
  };
  AppSettings s = settingsFor(dir.path(), script);
  s.passSigningKey = fpr;
  ImitatePass pass;
  pass.init(s);

  writeSig(3 * 1024);
  QVERIFY(pass.verifyGpgIdFile(gpgId));
  QFile logFile(log);
  QVERIFY(logFile.open(QIODevice::ReadOnly));
  const QString args = QString::fromUtf8(logFile.readAll());
  logFile.close();
  QVERIFY2(
      args.startsWith(QStringLiteral("--batch --verify --status-fd=1 -- ")),
      qPrintable(args));
  QVERIFY2(!args.contains(gpgId), "gpg must get copies, not the store's files");
  QFile seenFile(seen);
  QVERIFY(seenFile.open(QIODevice::ReadOnly));
  QCOMPARE(seenFile.readAll(), QByteArray(3 * 1024, 'S'));
  QVERIFY(QFile::remove(log));

  // Room for two signatures by a key built to be slow, not dozens.
  writeSig(8 * 1024);
  QVERIFY(!pass.verifyGpgIdFile(gpgId));
  QVERIFY2(!QFileInfo::exists(log), "gpg must not run on an oversized .sig");

  QVERIFY(QFile::remove(gpgId + QStringLiteral(".sig")));
  QCOMPARE(
      ::mkfifo(QFile::encodeName(gpgId + QStringLiteral(".sig")).constData(),
               0600),
      0);
  QVERIFY(!pass.verifyGpgIdFile(gpgId));
  QVERIFY2(!QFileInfo::exists(log), "gpg must not run on a FIFO");

  QVERIFY(QFile::remove(gpgId + QStringLiteral(".sig")));
  writeSig(64);
  const QString other = root.filePath(QStringLiteral("other-list"));
  QVERIFY(QFile::rename(gpgId, other));
  QVERIFY(QFile::link(other, gpgId));
  QVERIFY(!pass.verifyGpgIdFile(gpgId));
  QVERIFY2(!QFileInfo::exists(log), "gpg must not run on a linked list");
#endif
}

/**
 * @brief verifyGpgIdFile() refuses a git commit or tag object as the signed
 *        list: git signs those the same way, so a signed commit and its
 *        signature could be pushed as .gpg-id and .gpg-id.sig
 *        (ANT-2026-NOWX23I7).
 */
void tst_imitatepass::verifyRefusesACommitOrTagObject() {
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as a fake gpg");
#else
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QDir root(dir.path());
  const QString fpr =
      QStringLiteral("0123456789ABCDEF0123456789ABCDEF01234567");
  const QString log = root.filePath(QStringLiteral("gpg.log"));
  const QString script = root.filePath(QStringLiteral("gpg.sh"));
  {
    // A gpg that vouches for any signature: only the refusal can stop it.
    QFile f(script);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&f);
    out << "#!/bin/sh\n"
        << "printf '%s\\n' \"$*\" >> '" << log << "'\n"
        << "echo '[GNUPG:] VALIDSIG " << fpr << " 2026-11-25 1 0 4 0 1 10 00 "
        << fpr << "'\n"
        << "exit 0\n";
  }
  QVERIFY(QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner |
                                            QFile::ExeOwner));
  const QString gpgId = root.filePath(QStringLiteral(".gpg-id"));
  {
    QFile f(gpgId + QStringLiteral(".sig"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("signature");
  }
  const auto writeList = [&gpgId](const QByteArray &bytes) {
    QFile f(gpgId);
    return f.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           f.write(bytes) == bytes.size();
  };
  AppSettings s = settingsFor(dir.path(), script);
  s.passSigningKey = fpr;
  ImitatePass pass;
  pass.init(s);

  const QByteArray sha1(40, 'a');
  const QByteArray sha256(64, 'b');
  for (const QByteArray &object :
       {QByteArray("tree " + sha1 + "\nparent " + sha1 +
                   "\nauthor A <a@example.org> 0 +0000\n\nAdd x\n"),
        QByteArray("tree " + sha256 + "\nauthor A <a@example.org> 0 +0000\n"),
        QByteArray("object " + sha1 + "\ntype commit\ntag v1\n\nv1\n")}) {
    QVERIFY(writeList(object));
    QVERIFY2(!pass.verifyGpgIdFile(gpgId), object.constData());
    QVERIFY2(!QFileInfo::exists(log), "gpg must not be asked about an object");
  }

  QVERIFY(writeList("ALICE\n"));
  QVERIFY(pass.verifyGpgIdFile(gpgId));
  QVERIFY(QFileInfo::exists(log));
#endif
}

QTEST_MAIN(tst_imitatepass)
#include "tst_imitatepass.moc"
