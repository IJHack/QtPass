// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QScopeGuard>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>
#ifndef Q_OS_WIN
#include <unistd.h>
#endif

#include "../../../src/gpgidgeneration.h"
#include "../../../src/pass.h"
#include "../testsettings.h"

class tst_gpgidgeneration : public QObject {
  Q_OBJECT

  static void write(const QString &path, const QByteArray &bytes) {
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QCOMPARE(f.write(bytes), bytes.size());
  }
  static auto gen(const std::optional<GpgIdGeneration::Header> &h) -> qint64 {
    return h ? h->generation : -1;
  }

private slots:
  void initTestCase() { isolateTestSettings(); }
  void parseIsStrict();
  void headerRoundTripsAndIsIgnoredByTheRecipientParser();
  void folderOfIsStoreRelativeAndCanonical();
  void keyIsTheCanonicalPathHashed();
  void acceptRefusesARollbackAndRemembersTheHighest();
  void acceptTellsTwoListsOfOneGenerationApart();
  void recordWrittenDigestsTheBytesJustWritten();
  void aRecordHeldByAnotherProcessFailsClosed();
  void acceptRefusesAListWrittenForAnotherFolder();
  void reserveNextIsOneAboveRecordAndVerifiedDiskAndRecorded();
  void reserveNextNeverExceedsWhatParseAccepts();
  void reservationIsWrittenThrough();
  void unreadableRecordFailsClosed();
  void storeLineIsStrictAndRoundTrips();
  void acceptRefusesAListOfAnotherStore();
  void aNewListWithoutAStoreLineIsRefusedOnceTheStoreHasAnId();
  void storeIdForKeepsTakesOverOrMakesAnId();
  void malformedStorePinsFailClosed();
};

/**
 * @brief No header is generation 0 (a list from pass or from before), one
 *        generation line with one folder line is that header, anything else
 *        under our name is not a list to trust.
 */
void tst_gpgidgeneration::parseIsStrict() {
  QString err;
  QCOMPARE(gen(GpgIdGeneration::parse("ALICE\nBOB\n")), 0);
  QCOMPARE(gen(GpgIdGeneration::parse("")), 0);
  QCOMPARE(gen(GpgIdGeneration::parse("# just a comment\nALICE\n")), 0);
  QVERIFY(!GpgIdGeneration::parse("ALICE\n")->folder.has_value());
  const auto ok = GpgIdGeneration::parse(
      "# QtPass-GpgId-Generation: 17\n# QtPass-GpgId-Folder: .\nALICE\n");
  QCOMPARE(gen(ok), 17);
  QCOMPARE(ok->folder, std::optional<QString>(QStringLiteral(".")));
  const auto crlf = GpgIdGeneration::parse(
      "ALICE\n# QtPass-GpgId-Folder: work/mail\r\n# QtPass-GpgId-Generation: "
      "3\r\n");
  QCOMPARE(gen(crlf), 3);
  QCOMPARE(crlf->folder, std::optional<QString>(QStringLiteral("work/mail")));
  QCOMPARE(gen(GpgIdGeneration::parse("# QtPass-GpgId-Generation: "
                                      "999999999999999999\n# "
                                      "QtPass-GpgId-Folder: .\n")),
           999999999999999999LL);
  const char *bad[] = {
      "# QtPass-GpgId-Generation: banana\n# QtPass-GpgId-Folder: .\n",
      "# QtPass-GpgId-Generation: -1\n# QtPass-GpgId-Folder: .\n",
      "# QtPass-GpgId-Generation: 17 \n# QtPass-GpgId-Folder: .\n",
      "# QtPass-GpgId-Generation: 1999999999999999999\n# QtPass-GpgId-Folder: "
      ".\n",
      "# QtPass-GpgId-Generation:17\n# QtPass-GpgId-Folder: .\n",
      "# QtPass-GpgId-Generation: \n# QtPass-GpgId-Folder: .\n",
      "# QtPass-GpgId-Generation: 1\n# QtPass-GpgId-Generation: 2\n# "
      "QtPass-GpgId-Folder: .\n",
      "# QtPass-GpgId-Generation: 5\n# QtPass-GpgId-Folder: .\n# "
      "QtPass-GpgId-Folder: .\n",
      "# QtPass-GpgId-Generation\n",
      "# QtPass-GpgId-Generation: 0x10\n# QtPass-GpgId-Folder: .\n",
      "# QtPass-GpgId-Generation: 7\n",
      "# QtPass-GpgId-Folder: \n# QtPass-GpgId-Generation: 7\n",
      "# QtPass-GpgId-Something: 7\n",
      // A folder that would not survive being written back (found by
      // fuzz_gpgid): "\r\r\n" leaves a \r in it, a tab or DEL is no name.
      "# QtPass-GpgId-Generation: 7\n# QtPass-GpgId-Folder: 7w]\r\r\n",
      "# QtPass-GpgId-Generation: 7\n# QtPass-GpgId-Folder: a\tb\n",
      "# QtPass-GpgId-Generation: 7\n# QtPass-GpgId-Folder: a\x7f\n",
  };
  for (const char *b : bad) {
    err.clear();
    QVERIFY2(!GpgIdGeneration::parse(b, &err).has_value(), b);
    QVERIFY2(!err.isEmpty(), b);
  }
  const QByteArray nul("# QtPass-GpgId-Generation: 7\n# QtPass-GpgId-Folder: "
                       "a\0b\n",
                       56);
  err.clear();
  QVERIFY(!GpgIdGeneration::parse(nul, &err).has_value());
  QVERIFY(!err.isEmpty());
  // A folder line alone (no generation) is odd but not a lie about
  // freshness; it parses as generation 0 with a folder.
  const auto folderOnly = GpgIdGeneration::parse("# QtPass-GpgId-Folder: x\n");
  QCOMPARE(gen(folderOnly), 0);
}

/**
 * @brief The header QtPass writes parses back, and is comments to the
 *        recipient parser, as it is to pass 1.7.4 and later.
 */
void tst_gpgidgeneration::headerRoundTripsAndIsIgnoredByTheRecipientParser() {
  const QByteArray list = GpgIdGeneration::withHeader(
      42, QStringLiteral("work/mail"), "ALICE\nBOB\n");
  QCOMPARE(list,
           QByteArrayLiteral("# QtPass-GpgId-Generation: 42\n# "
                             "QtPass-GpgId-Folder: work/mail\nALICE\nBOB\n"));
  const auto header = GpgIdGeneration::parse(list);
  QCOMPARE(gen(header), 42);
  QCOMPARE(header->folder, std::optional<QString>(QStringLiteral("work/mail")));
  QCOMPARE(Pass::parseRecipients(list, QStringLiteral("test")),
           (QStringList{QStringLiteral("ALICE"), QStringLiteral("BOB")}));
}

/// The folder a list is bound to: `.` at the root, `/`-separated below, the
/// same through a link or a `..`, nothing outside the store or through a
/// name with a line break.
void tst_gpgidgeneration::folderOfIsStoreRelativeAndCanonical() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QDir root(dir.path());
  QVERIFY(root.mkpath(QStringLiteral("store/work/mail")));
  QVERIFY(root.mkpath(QStringLiteral("elsewhere")));
  const QString store = root.filePath(QStringLiteral("store"));
  QCOMPARE(GpgIdGeneration::folderOf(store + "/.gpg-id", store),
           std::optional<QString>(QStringLiteral(".")));
  QCOMPARE(GpgIdGeneration::folderOf(store + "/.gpg-id", store + "/"),
           std::optional<QString>(QStringLiteral(".")));
  QCOMPARE(GpgIdGeneration::folderOf(store + "/work/mail/.gpg-id", store),
           std::optional<QString>(QStringLiteral("work/mail")));
  QCOMPARE(
      GpgIdGeneration::folderOf(store + "/work/../work/mail/.gpg-id", store),
      std::optional<QString>(QStringLiteral("work/mail")));
  QVERIFY(!GpgIdGeneration::folderOf(
               root.filePath(QStringLiteral("elsewhere/.gpg-id")), store)
               .has_value());
#ifndef Q_OS_WIN
  QVERIFY(QFile::link(store, root.filePath(QStringLiteral("link"))));
  QCOMPARE(GpgIdGeneration::folderOf(
               root.filePath(QStringLiteral("link/work/mail/.gpg-id")), store),
           std::optional<QString>(QStringLiteral("work/mail")));
  // A line break would end the folder line; what follows it would be read
  // as a recipient of the signed list.
  const QString broken = QStringLiteral("store/x\nEVE");
  QVERIFY(root.mkpath(broken));
  QVERIFY(!GpgIdGeneration::folderOf(
               root.filePath(broken + QStringLiteral("/.gpg-id")), store)
               .has_value());
  // Nor any other control character: parse() would refuse the list.
  for (const QString &name :
       {QStringLiteral("store/x\rEVE"), QStringLiteral("store/x\tEVE"),
        QStringLiteral("store/x") + QChar(0x7f) + QStringLiteral("EVE")}) {
    QVERIFY(root.mkpath(name));
    QVERIFY2(!GpgIdGeneration::folderOf(
                  root.filePath(name + QStringLiteral("/.gpg-id")), store)
                  .has_value(),
             qPrintable(name));
  }
#endif
}

/**
 * @brief Spelling does not matter: the key is a hash of the canonical path,
 *        so a trailing separator, a `..` or a link on the way name the same
 *        state, and a file that does not exist yet still has a key.
 */
void tst_gpgidgeneration::keyIsTheCanonicalPathHashed() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QDir root(dir.path());
  QVERIFY(root.mkpath(QStringLiteral("store/sub")));
  const QString file = root.filePath(QStringLiteral("store/.gpg-id"));
  write(file, "ALICE\n");
  const QString key = GpgIdGeneration::key(file);
  QCOMPARE(key.size(), 64);
  QCOMPARE(GpgIdGeneration::key(
               root.filePath(QStringLiteral("store/sub/../.gpg-id"))),
           key);
  QCOMPARE(
      GpgIdGeneration::key(root.filePath(QStringLiteral("store//.gpg-id"))),
      key);
#ifndef Q_OS_WIN
  QVERIFY(QFile::link(root.filePath(QStringLiteral("store")),
                      root.filePath(QStringLiteral("link"))));
  QCOMPARE(GpgIdGeneration::key(root.filePath(QStringLiteral("link/.gpg-id"))),
           key);
#endif
  QVERIFY(GpgIdGeneration::key(
              root.filePath(QStringLiteral("store/sub/.gpg-id"))) != key);
  const QString fresh =
      GpgIdGeneration::key(root.filePath(QStringLiteral("store/new/.gpg-id")));
  QCOMPARE(fresh.size(), 64);
  QVERIFY(fresh != key);
}

/**
 * @brief First sight is accepted (nothing to compare against), a higher
 *        generation is accepted and remembered, an equal one is accepted, a
 *        lower one is refused with a reason that names both numbers and
 *        warns about the preselected recipients, an equal one with other
 *        bytes is a conflict while the same bytes pass again, a headerless
 *        one is refused as unbound with its own explanation while one with
 *        only a folder line is an ordinary rollback, a malformed one is
 *        refused, and a reservation climbs above the refused one.
 */
void tst_gpgidgeneration::acceptRefusesARollbackAndRemembersTheHighest() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString store = dir.path();
  const QString file = QDir(store).filePath(QStringLiteral(".gpg-id"));
  const auto list = [](qint64 g, const char *who) {
    return GpgIdGeneration::withHeader(g, QStringLiteral("."), who);
  };
  write(file, list(17, "ALICE\n"));
  QString why;
  QCOMPARE(GpgIdGeneration::remembered(file), std::optional<qint64>(0));
  QVERIFY(GpgIdGeneration::accept(file, list(17, "ALICE\n"), store, &why) ==
          GpgIdGeneration::Verdict::Accepted);
  QCOMPARE(GpgIdGeneration::remembered(file), std::optional<qint64>(17));
  QVERIFY(GpgIdGeneration::accept(file, list(18, "ALICE\nBOB\n"), store) ==
          GpgIdGeneration::Verdict::Accepted);
  QCOMPARE(GpgIdGeneration::remembered(file), std::optional<qint64>(18));
  why.clear();
  QVERIFY2(GpgIdGeneration::accept(file, list(18, "ALICE\nCAROL\n"), store,
                                   &why) == GpgIdGeneration::Verdict::Conflict,
           "two devices both making 18 from 17 is git's conflict; the one "
           "that accepted the first 18 does not take the other for it");
  QVERIFY2(why.contains(QStringLiteral("generation 18")) &&
               why.contains(QStringLiteral("same generation")) &&
               why.contains(QStringLiteral("Git")) &&
               why.contains(QStringLiteral("Users")) &&
               why.contains(GpgIdGeneration::recordFile()),
           qPrintable(why));
  QVERIFY2(GpgIdGeneration::accept(file, list(18, "ALICE\nBOB\n"), store) ==
               GpgIdGeneration::Verdict::Accepted,
           "the same 18 again is the same list");
  why.clear();
  QCOMPARE(GpgIdGeneration::accept(file, list(17, "ALICE\nBOB\n"), store, &why),
           GpgIdGeneration::Verdict::Rollback);
  QVERIFY2(why.contains(QStringLiteral("generation 17")) &&
               why.contains(QStringLiteral("generation 18")) &&
               why.contains(QStringLiteral("Users")) &&
               why.contains(QStringLiteral("preselected")) &&
               why.contains(GpgIdGeneration::recordFile()),
           qPrintable(why));
  why.clear();
  QVERIFY2(GpgIdGeneration::accept(file, "ALICE\nBOB\n", store, &why) ==
               GpgIdGeneration::Verdict::Unbound,
           "a list from before generations existed, or from pass, is below, "
           "and with no folder line it is nobody's rollback to recover");
  QVERIFY2(why.contains(QStringLiteral("no generation line")) &&
               why.contains(QStringLiteral("pass")) &&
               why.contains(QStringLiteral("another folder")) &&
               why.contains(QStringLiteral("afresh")) &&
               !why.contains(QStringLiteral("preselected")),
           qPrintable(why));
  why.clear();
  QVERIFY2(GpgIdGeneration::accept(file, "# QtPass-GpgId-Folder: .\nALICE\n",
                                   store,
                                   &why) == GpgIdGeneration::Verdict::Rollback,
           "a folder line alone binds the list here: an ordinary rollback");
  QVERIFY(GpgIdGeneration::accept(
              file,
              "# QtPass-GpgId-Generation: x\n# QtPass-GpgId-Folder: .\nALICE\n",
              store, &why) != GpgIdGeneration::Verdict::Accepted);
  QCOMPARE(GpgIdGeneration::remembered(file), std::optional<qint64>(18));
  // Saving again is the way through: one above the highest known, and the
  // reservation itself moves the record.
  QCOMPARE(GpgIdGeneration::reserveNext(file, 17), std::optional<qint64>(19));
  QCOMPARE(GpgIdGeneration::remembered(file), std::optional<qint64>(19));
}

/**
 * @brief Equal generation, different bytes: two devices both wrote it, or
 *        one authentic list of that generation was swapped for another. The
 *        record keeps a digest of the exact bytes with the generation; a
 *        record without a digest (a write not read back yet) takes the
 *        next bytes seen as the ones; headerless lists, generation 0, are
 *        not pinned, since pass writes a new one for every change.
 */
void tst_gpgidgeneration::acceptTellsTwoListsOfOneGenerationApart() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString store = dir.path();
  const QString file = QDir(store).filePath(QStringLiteral(".gpg-id"));
  const auto list = [](qint64 g, const char *who) {
    return GpgIdGeneration::withHeader(g, QStringLiteral("."), who);
  };
  QString why;
  QCOMPARE(GpgIdGeneration::accept(file, list(1, "ALICE\n"), store),
           GpgIdGeneration::Verdict::Accepted);
  QCOMPARE(GpgIdGeneration::accept(file, list(2, "BOB\n"), store),
           GpgIdGeneration::Verdict::Accepted);
  QCOMPARE(GpgIdGeneration::accept(file, list(2, "BOB\n"), store),
           GpgIdGeneration::Verdict::Accepted);
  QCOMPARE(GpgIdGeneration::accept(file, list(2, "CAROL\n"), store, &why),
           GpgIdGeneration::Verdict::Conflict);
  QVERIFY2(why.contains(QStringLiteral("generation 2")) &&
               why.contains(QStringLiteral("generation 3")) &&
               why.contains(QStringLiteral("checking the recipients")),
           qPrintable(why));
  QCOMPARE(GpgIdGeneration::remembered(file), std::optional<qint64>(2));
  QVERIFY2(GpgIdGeneration::accept(file, list(3, "CAROL\n"), store) ==
               GpgIdGeneration::Verdict::Accepted,
           "a higher generation is a new list, whatever the bytes");
  QCOMPARE(GpgIdGeneration::accept(file, list(3, "DAVE\n"), store),
           GpgIdGeneration::Verdict::Conflict);

  // A record that knows the generation but not the bytes (a reservation
  // whose write was never recorded): the next bytes seen are taken as the
  // list, then pinned. Edited on disk, as the record is read afresh.
  {
    QFile record(GpgIdGeneration::recordFile());
    QVERIFY(record.open(QIODevice::ReadOnly));
    QJsonObject root = QJsonDocument::fromJson(record.readAll()).object();
    record.close();
    QJsonObject lists = root.value(QLatin1String("lists")).toObject();
    QJsonObject item = lists.value(GpgIdGeneration::key(file)).toObject();
    QCOMPARE(item.value(QLatin1String("generation")).toString(),
             QStringLiteral("3"));
    QVERIFY(item.contains(QLatin1String("sha256")));
    item.remove(QLatin1String("sha256"));
    lists.insert(GpgIdGeneration::key(file), item);
    root.insert(QLatin1String("lists"), lists);
    QVERIFY(record.open(QIODevice::WriteOnly | QIODevice::Truncate));
    record.write(QJsonDocument(root).toJson());
  }
  QCOMPARE(GpgIdGeneration::accept(file, list(3, "ERIN\n"), store),
           GpgIdGeneration::Verdict::Accepted);
  QCOMPARE(GpgIdGeneration::accept(file, list(3, "CAROL\n"), store),
           GpgIdGeneration::Verdict::Conflict);

  // Headerless lists are generation 0, where nothing is pinned: pass writes
  // one for every change (also through QtPass's pass backend), so other
  // bytes at generation 0 are the normal course of a store kept with pass.
  QTemporaryDir other;
  QVERIFY(other.isValid());
  const QString plain = QDir(other.path()).filePath(QStringLiteral(".gpg-id"));
  QCOMPARE(GpgIdGeneration::accept(plain, "ALICE\n", other.path()),
           GpgIdGeneration::Verdict::Accepted);
  QCOMPARE(GpgIdGeneration::accept(plain, "ALICE\nBOB\n", other.path()),
           GpgIdGeneration::Verdict::Accepted);
  QCOMPARE(GpgIdGeneration::remembered(plain), std::optional<qint64>(0));
}

/**
 * @brief The writer records the bytes it wrote right after writing them, so
 *        its own list is recognised and another one of that generation is
 *        not; bytes of a generation other than the reserved one are not
 *        recorded (something else moved the record in between).
 */
void tst_gpgidgeneration::recordWrittenDigestsTheBytesJustWritten() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString store = dir.path();
  const QString file = QDir(store).filePath(QStringLiteral(".gpg-id"));
  const auto list = [](qint64 g, const char *who) {
    return GpgIdGeneration::withHeader(g, QStringLiteral("."), who);
  };
  QCOMPARE(GpgIdGeneration::reserveNext(file, std::nullopt),
           std::optional<qint64>(1));
  QString why;
  QVERIFY2(!GpgIdGeneration::recordWritten(file, list(2, "ALICE\n"), &why),
           "not the reserved generation");
  QVERIFY2(why.contains(QStringLiteral("generation 1")) &&
               why.contains(QStringLiteral("generation 2")),
           qPrintable(why));
  QVERIFY(GpgIdGeneration::recordWritten(file, list(1, "ALICE\n")));
  QCOMPARE(GpgIdGeneration::accept(file, list(1, "ALICE\n"), store),
           GpgIdGeneration::Verdict::Accepted);
  QCOMPARE(GpgIdGeneration::accept(file, list(1, "BOB\n"), store),
           GpgIdGeneration::Verdict::Conflict);
  // Reserving the next number drops the digest: the old bytes must not
  // pass for the new generation while it is being written.
  QCOMPARE(GpgIdGeneration::reserveNext(file, 1), std::optional<qint64>(2));
  QCOMPARE(GpgIdGeneration::accept(file, list(2, "BOB\n"), store),
           GpgIdGeneration::Verdict::Accepted);
}

/**
 * @brief The record is one per user, not per process: while another QtPass
 *        holds the lock file next to it, nothing is read, accepted or
 *        reserved here (unknown state is not "never seen").
 */
void tst_gpgidgeneration::aRecordHeldByAnotherProcessFailsClosed() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString store = dir.path();
  const QString file = QDir(store).filePath(QStringLiteral(".gpg-id"));
  const auto list = [](qint64 g, const char *who) {
    return GpgIdGeneration::withHeader(g, QStringLiteral("."), who);
  };
  QCOMPARE(GpgIdGeneration::accept(file, list(1, "ALICE\n"), store),
           GpgIdGeneration::Verdict::Accepted);
  QLockFile other(GpgIdGeneration::recordFile() + QStringLiteral(".lock"));
  QVERIFY(other.tryLock(0));
  QString why;
  QCOMPARE(GpgIdGeneration::accept(file, list(1, "ALICE\n"), store, &why),
           GpgIdGeneration::Verdict::RecordUnavailable);
  QVERIFY2(why.contains(QStringLiteral("locked")) &&
               why.contains(GpgIdGeneration::recordFile()),
           qPrintable(why));
  other.unlock();
  QCOMPARE(GpgIdGeneration::accept(file, list(1, "ALICE\n"), store),
           GpgIdGeneration::Verdict::Accepted);
}

/**
 * @brief A signed pair is only valid where it was written for: copied into
 *        a folder that never had a list (a key this device has not seen, so
 *        first sight would accept it) it is refused by its folder line.
 */
void tst_gpgidgeneration::acceptRefusesAListWrittenForAnotherFolder() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString store = dir.path();
  QVERIFY(QDir(store).mkpath(QStringLiteral("team")));
  const QByteArray rootList =
      GpgIdGeneration::withHeader(1, QStringLiteral("."), "ALICE\nBOB\n");
  const QString planted = QDir(store).filePath(QStringLiteral("team/.gpg-id"));
  write(planted, rootList);
  QString why;
  QCOMPARE(GpgIdGeneration::accept(planted, rootList, store, &why),
           GpgIdGeneration::Verdict::WrongFolder);
  QVERIFY2(why.contains(QStringLiteral("\".\"")) &&
               why.contains(QStringLiteral("\"team\"")),
           qPrintable(why));
  QCOMPARE(GpgIdGeneration::remembered(planted), std::optional<qint64>(0));
  // Written for team, it is accepted in team.
  const QByteArray teamList =
      GpgIdGeneration::withHeader(1, QStringLiteral("team"), "ALICE\n");
  QVERIFY(GpgIdGeneration::accept(planted, teamList, store) ==
          GpgIdGeneration::Verdict::Accepted);
  // And a list outside the store as named is nowhere.
  QTemporaryDir outside;
  QVERIFY(outside.isValid());
  const QString foreign =
      QDir(outside.path()).filePath(QStringLiteral(".gpg-id"));
  write(foreign, rootList);
  QVERIFY(GpgIdGeneration::accept(foreign, rootList, store, &why) !=
          GpgIdGeneration::Verdict::Accepted);
}

/// A reservation is one above the record and the verified list on disk, and
/// is recorded at once: two writers cannot get the same number.
void tst_gpgidgeneration::
    reserveNextIsOneAboveRecordAndVerifiedDiskAndRecorded() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString file = QDir(dir.path()).filePath(QStringLiteral(".gpg-id"));
  QCOMPARE(GpgIdGeneration::reserveNext(file, std::nullopt),
           std::optional<qint64>(1));
  QCOMPARE(GpgIdGeneration::reserveNext(file, std::nullopt),
           std::optional<qint64>(2));
  QCOMPARE(GpgIdGeneration::reserveNext(file, 7), std::optional<qint64>(8));
  QCOMPARE(GpgIdGeneration::reserveNext(file, 3), std::optional<qint64>(9));
  QCOMPARE(GpgIdGeneration::remembered(file), std::optional<qint64>(9));
}

/**
 * @brief The counter cannot leave the grammar: a planted 18-nines line (not
 *        verified, so not taken) does not move it, a verified list at the
 *        ceiling refuses the save instead of writing a 19-digit header the
 *        parser would reject for good.
 */
void tst_gpgidgeneration::reserveNextNeverExceedsWhatParseAccepts() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString file = QDir(dir.path()).filePath(QStringLiteral(".gpg-id"));
  QCOMPARE(GpgIdGeneration::reserveNext(file, std::nullopt),
           std::optional<qint64>(1));
  QString why;
  QCOMPARE(
      GpgIdGeneration::reserveNext(file, GpgIdGeneration::kMaxGeneration - 1),
      std::optional<qint64>(GpgIdGeneration::kMaxGeneration));
  QVERIFY(GpgIdGeneration::parse(
              GpgIdGeneration::withHeader(GpgIdGeneration::kMaxGeneration,
                                          QStringLiteral("."), "ALICE\n"))
              .has_value());
  QVERIFY(!GpgIdGeneration::reserveNext(file, std::nullopt, &why).has_value());
  QVERIFY2(why.contains(QStringLiteral("highest")), qPrintable(why));
  QCOMPARE(GpgIdGeneration::remembered(file),
           std::optional<qint64>(GpgIdGeneration::kMaxGeneration));
  // A rollback or an unbound list against a record at the ceiling does not
  // promise a save that reserveNext() would refuse: the record goes first.
  why.clear();
  QCOMPARE(GpgIdGeneration::accept(
               file,
               GpgIdGeneration::withHeader(5, QStringLiteral("."), "ALICE\n"),
               dir.path(), &why),
           GpgIdGeneration::Verdict::Rollback);
  QVERIFY2(why.contains(QStringLiteral("highest")) &&
               why.contains(GpgIdGeneration::recordFile()) &&
               !why.contains(QStringLiteral("1000000000000000000")),
           qPrintable(why));
  why.clear();
  QCOMPARE(GpgIdGeneration::accept(file, "ALICE\n", dir.path(), &why),
           GpgIdGeneration::Verdict::Unbound);
  QVERIFY2(why.contains(QStringLiteral("highest")) &&
               !why.contains(QStringLiteral("1000000000000000000")),
           qPrintable(why));
}

/// The record is on disk when a reservation returns, in a JSON file of its
/// own next to where the settings would be, never in QtPass.conf and never
/// in a registry (a Windows QSettings "file" is one), with the generation
/// as a string and no digest until the bytes are known.
void tst_gpgidgeneration::reservationIsWrittenThrough() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString file = QDir(dir.path()).filePath(QStringLiteral(".gpg-id"));
  QCOMPARE(GpgIdGeneration::reserveNext(file, std::nullopt),
           std::optional<qint64>(1));
  const QString recordFile = GpgIdGeneration::recordFile();
  QVERIFY2(recordFile.endsWith(QStringLiteral(".json")) &&
               !recordFile.contains(QStringLiteral("QtPass.conf")) &&
               !recordFile.contains(QStringLiteral("HKEY")),
           qPrintable(recordFile));
  const QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                           QStringLiteral("IJHack"), QStringLiteral("QtPass"));
  QCOMPARE(QFileInfo(recordFile).absolutePath(),
           QFileInfo(settings.fileName()).absolutePath());
  QFile record(recordFile);
  QVERIFY(record.open(QIODevice::ReadOnly));
  const QJsonObject root = QJsonDocument::fromJson(record.readAll()).object();
  QCOMPARE(root.value(QLatin1String("format")).toInt(), 1);
  const QJsonObject item = root.value(QLatin1String("lists"))
                               .toObject()
                               .value(GpgIdGeneration::key(file))
                               .toObject();
  QCOMPARE(item.value(QLatin1String("generation")).toString(),
           QStringLiteral("1"));
  QVERIFY(!item.contains(QLatin1String("sha256")));
}

/**
 * @brief No freshness state, no acceptance and no reservation. A record
 *        that cannot be written (its folder read-only) yields no
 *        reservation, and a signed list above anything known is still
 *        refused because accepting it means recording it. A record that
 *        cannot be parsed (garbage where the JSON was, or JSON that is not
 *        a record) reads as nothing, not as 0, and keeps refusing.
 */
void tst_gpgidgeneration::unreadableRecordFailsClosed() {
#ifdef Q_OS_WIN
  QSKIP("permission bits do not stop writes on Windows");
#else
  if (::geteuid() == 0) {
    QSKIP("root writes anywhere");
  }
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString store = dir.path();
  const QString file = QDir(store).filePath(QStringLiteral(".gpg-id"));
  const auto list = [](qint64 g) {
    return GpgIdGeneration::withHeader(g, QStringLiteral("."), "ALICE\n");
  };
  QCOMPARE(GpgIdGeneration::reserveNext(file, std::nullopt),
           std::optional<qint64>(1));
  const QString recordFile = GpgIdGeneration::recordFile();
  QVERIFY(QFile::exists(recordFile));
  const QString recordDir = QFileInfo(recordFile).absolutePath();
  const QFile::Permissions was = QFile::permissions(recordDir);
  QVERIFY(QFile::setPermissions(recordDir, QFile::ReadOwner | QFile::ExeOwner));
  const auto restore =
      qScopeGuard([&] { QFile::setPermissions(recordDir, was); });
  // The garbage written below is not left for the tests after this one.
  const auto forget = qScopeGuard([&] { QFile::remove(recordFile); });
  QString why;
  QVERIFY2(!GpgIdGeneration::reserveNext(file, std::nullopt, &why).has_value(),
           "no record, no generation to write");
  QVERIFY(!why.isEmpty());
  why.clear();
  QVERIFY2(GpgIdGeneration::accept(file, list(5), store, &why) !=
               GpgIdGeneration::Verdict::Accepted,
           "accepting means recording; when that fails, nothing is accepted");
  QVERIFY(!why.isEmpty());
  why.clear();
  QVERIFY2(GpgIdGeneration::accept(file, list(1), store, &why) !=
               GpgIdGeneration::Verdict::Accepted,
           "while the record cannot be written, nothing is established");
  QVERIFY(QFile::setPermissions(recordDir, was));
  QVERIFY2(GpgIdGeneration::remembered(file) == std::optional<qint64>(1),
           "the refused 5 was not left pending in Qt's cache to be flushed "
           "later");
  QVERIFY(GpgIdGeneration::accept(file, list(1), store) ==
          GpgIdGeneration::Verdict::Accepted);

  // Garbage where the record was is "unknown", not "0".
  {
    QFile f(recordFile);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("{unterminated\n\x00\x01garbage\n=\n");
  }
  why.clear();
  QVERIFY(!GpgIdGeneration::remembered(file, &why).has_value());
  QVERIFY2(why.contains(recordFile) && why.contains(QStringLiteral("repaired")),
           qPrintable(why));
  // So is a well-formed file that is not a record.
  {
    QFile f(recordFile);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("{\"format\": 1, \"lists\": {\"abc\": {\"generation\": 7}}}\n");
  }
  QVERIFY2(!GpgIdGeneration::remembered(file).has_value(),
           "a generation that is not a string of digits is not a record");
  {
    QFile f(recordFile);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("{unterminated\n");
  }
  why.clear();
  QVERIFY(GpgIdGeneration::accept(file, list(5), store, &why) !=
          GpgIdGeneration::Verdict::Accepted);
  QVERIFY(!why.isEmpty());
  QVERIFY(!GpgIdGeneration::reserveNext(file, std::nullopt).has_value());
#endif
}

/**
 * @brief A store line is 32 lowercase hex digits, once, and only with a
 *        generation line; the header QtPass writes with one parses back.
 */
void tst_gpgidgeneration::storeLineIsStrictAndRoundTrips() {
  const QString id = QStringLiteral("0123456789abcdef0123456789abcdef");
  const QByteArray list =
      GpgIdGeneration::withHeader(4, QStringLiteral("team"), "ALICE\n", id);
  QVERIFY(list.contains("\n# QtPass-GpgId-Store: " + id.toLatin1() + "\n"));
  const auto parsed = GpgIdGeneration::parse(list);
  QVERIFY(parsed.has_value());
  QCOMPARE(parsed->generation, 4);
  QCOMPARE(parsed->store, std::optional<QString>(id));
  QCOMPARE(Pass::parseRecipients(list, QStringLiteral("test")),
           QStringList{QStringLiteral("ALICE")});
  QVERIFY2(!GpgIdGeneration::withHeader(4, QStringLiteral("."), "A\n")
                .contains("Store"),
           "no id, no line");
  const char *bad[] = {
      // upper case, short, long, not hex, empty, twice, without generation
      "# QtPass-GpgId-Generation: 1\n# QtPass-GpgId-Folder: .\n"
      "# QtPass-GpgId-Store: 0123456789ABCDEF0123456789ABCDEF\n",
      "# QtPass-GpgId-Generation: 1\n# QtPass-GpgId-Folder: .\n"
      "# QtPass-GpgId-Store: 0123\n",
      "# QtPass-GpgId-Generation: 1\n# QtPass-GpgId-Folder: .\n"
      "# QtPass-GpgId-Store: 0123456789abcdef0123456789abcdef0\n",
      "# QtPass-GpgId-Generation: 1\n# QtPass-GpgId-Folder: .\n"
      "# QtPass-GpgId-Store: 0123456789abcdef0123456789abcdeg\n",
      "# QtPass-GpgId-Generation: 1\n# QtPass-GpgId-Folder: .\n"
      "# QtPass-GpgId-Store: \n",
      "# QtPass-GpgId-Generation: 1\n# QtPass-GpgId-Folder: .\n"
      "# QtPass-GpgId-Store: 0123456789abcdef0123456789abcdef\n"
      "# QtPass-GpgId-Store: 0123456789abcdef0123456789abcdef\n",
      "# QtPass-GpgId-Store: 0123456789abcdef0123456789abcdef\n",
  };
  for (const char *b : bad) {
    QString err;
    QVERIFY2(!GpgIdGeneration::parse(b, &err).has_value(), b);
    QVERIFY2(!err.isEmpty(), b);
  }
}

/**
 * @brief #2050: a list of another store, signed with the same key and for
 *        the same folder, copied in, is refused once this device knows the
 *        store, whatever its generation: higher would have passed before,
 *        lower must not reach the Users dialog as this folder's rollback.
 *        The first store line seen is the one the store is known by.
 */
void tst_gpgidgeneration::acceptRefusesAListOfAnotherStore() {
  QTemporaryDir a;
  QTemporaryDir b;
  QVERIFY(a.isValid() && b.isValid());
  const QString idA = QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const QString idB = QStringLiteral("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  const QString fileB = QDir(b.path()).filePath(QStringLiteral(".gpg-id"));
  const QByteArray ownB =
      GpgIdGeneration::withHeader(5, QStringLiteral("."), "BOB\n", idB);
  write(fileB, ownB);
  QCOMPARE(GpgIdGeneration::accept(fileB, ownB, b.path()),
           GpgIdGeneration::Verdict::Accepted);
  QCOMPARE(GpgIdGeneration::storeIdFor(b.path(), std::nullopt),
           std::optional<QString>(idB));

  QString why;
  const QByteArray fromA =
      GpgIdGeneration::withHeader(9, QStringLiteral("."), "MALLORY\n", idA);
  write(fileB, fromA);
  QCOMPARE(GpgIdGeneration::accept(fileB, fromA, b.path(), &why),
           GpgIdGeneration::Verdict::WrongStore);
  QVERIFY2(why.contains(idA) && why.contains(idB), qPrintable(why));
  QCOMPARE(GpgIdGeneration::remembered(fileB), std::optional<qint64>(5));
  const QByteArray olderFromA =
      GpgIdGeneration::withHeader(2, QStringLiteral("."), "MALLORY\n", idA);
  QCOMPARE(GpgIdGeneration::accept(fileB, olderFromA, b.path()),
           GpgIdGeneration::Verdict::WrongStore);
  // B's own next list still passes.
  const QByteArray nextB =
      GpgIdGeneration::withHeader(6, QStringLiteral("."), "BOB\n", idB);
  QCOMPARE(GpgIdGeneration::accept(fileB, nextB, b.path()),
           GpgIdGeneration::Verdict::Accepted);
}

/**
 * @brief Once the store has an id, a new list without a store line is
 *        refused (it may be another store's, from before store lines); a
 *        list this device accepted before stays accepted, and a headerless
 *        list (pass) is as before. Before the store has an id, a list
 *        without one is accepted as before.
 */
void tst_gpgidgeneration::
    aNewListWithoutAStoreLineIsRefusedOnceTheStoreHasAnId() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString store = dir.path();
  QVERIFY(QDir(store).mkpath(QStringLiteral("team")));
  QVERIFY(QDir(store).mkpath(QStringLiteral("plain")));
  const QString root = QDir(store).filePath(QStringLiteral(".gpg-id"));
  const QString team = QDir(store).filePath(QStringLiteral("team/.gpg-id"));
  const QString plain = QDir(store).filePath(QStringLiteral("plain/.gpg-id"));
  const QString id = QStringLiteral("0123456789abcdef0123456789abcdef");

  const QByteArray oldRoot =
      GpgIdGeneration::withHeader(3, QStringLiteral("."), "ALICE\n");
  QCOMPARE(GpgIdGeneration::accept(root, oldRoot, store),
           GpgIdGeneration::Verdict::Accepted);
  const QByteArray teamList =
      GpgIdGeneration::withHeader(1, QStringLiteral("team"), "ALICE\n", id);
  QCOMPARE(GpgIdGeneration::accept(team, teamList, store),
           GpgIdGeneration::Verdict::Accepted);

  QCOMPARE(GpgIdGeneration::accept(root, oldRoot, store),
           GpgIdGeneration::Verdict::Accepted);
  QString why;
  const QByteArray newRoot =
      GpgIdGeneration::withHeader(4, QStringLiteral("."), "MALLORY\n");
  QCOMPARE(GpgIdGeneration::accept(root, newRoot, store, &why),
           GpgIdGeneration::Verdict::WrongStore);
  QVERIFY2(why.contains(QStringLiteral("no store line")) && why.contains(id),
           qPrintable(why));
  QCOMPARE(GpgIdGeneration::accept(plain, "ALICE\n", store),
           GpgIdGeneration::Verdict::Accepted);
}

/**
 * @brief The id to write: the one the store is known by; else the verified
 *        one on disk, taken over; else a new random one; each remembered.
 *        Two stores get two ids.
 */
void tst_gpgidgeneration::storeIdForKeepsTakesOverOrMakesAnId() {
  QTemporaryDir joined;
  QTemporaryDir fresh;
  QTemporaryDir other;
  QVERIFY(joined.isValid() && fresh.isValid() && other.isValid());
  const QString onDisk = QStringLiteral("0123456789abcdef0123456789abcdef");
  QCOMPARE(GpgIdGeneration::storeIdFor(joined.path(), onDisk),
           std::optional<QString>(onDisk));
  QCOMPARE(
      GpgIdGeneration::storeIdFor(
          joined.path(), QStringLiteral("ffffffffffffffffffffffffffffffff")),
      std::optional<QString>(onDisk));

  const std::optional<QString> made =
      GpgIdGeneration::storeIdFor(fresh.path(), QStringLiteral("not an id"));
  QVERIFY(made.has_value());
  QVERIFY2(GpgIdGeneration::parse(GpgIdGeneration::withHeader(
                                      1, QStringLiteral("."), "A\n", *made))
               .has_value(),
           qPrintable(*made));
  QCOMPARE(GpgIdGeneration::storeIdFor(fresh.path(), std::nullopt), made);
  const std::optional<QString> second =
      GpgIdGeneration::storeIdFor(other.path(), std::nullopt);
  QVERIFY(second.has_value() && *second != *made);
}

/**
 * @brief A record whose store ids are not what QtPass writes is unreadable:
 *        nothing is accepted against it, as for its lists.
 */
void tst_gpgidgeneration::malformedStorePinsFailClosed() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QVERIFY(GpgIdGeneration::storeIdFor(dir.path(), std::nullopt).has_value());
  const QString recordFile = GpgIdGeneration::recordFile();
  QFile in(recordFile);
  QVERIFY(in.open(QIODevice::ReadOnly));
  const QByteArray before = in.readAll();
  in.close();
  const auto restore = qScopeGuard([&] { write(recordFile, before); });
  QJsonObject root = QJsonDocument::fromJson(before).object();
  QVERIFY(root.contains(QStringLiteral("stores")));
  QJsonObject stores = root.value(QStringLiteral("stores")).toObject();
  QJsonObject bad;
  bad.insert(QStringLiteral("id"), QStringLiteral("NOT-AN-ID"));
  stores.insert(QStringLiteral("x"), bad);
  root.insert(QStringLiteral("stores"), stores);
  write(recordFile, QJsonDocument(root).toJson());
  QString why;
  const QString file = QDir(dir.path()).filePath(QStringLiteral(".gpg-id"));
  QCOMPARE(GpgIdGeneration::accept(
               file, GpgIdGeneration::withHeader(1, QStringLiteral("."), "A\n"),
               dir.path(), &why),
           GpgIdGeneration::Verdict::RecordUnavailable);
  QVERIFY(!GpgIdGeneration::storeIdFor(dir.path(), std::nullopt).has_value());
}

QTEST_MAIN(tst_gpgidgeneration)
#include "tst_gpgidgeneration.moc"
