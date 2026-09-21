// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
  void acceptRefusesAListWrittenForAnotherFolder();
  void reserveNextIsOneAboveRecordAndVerifiedDiskAndRecorded();
  void reserveNextNeverExceedsWhatParseAccepts();
  void reservationIsWrittenThrough();
  void unreadableRecordFailsClosed();
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
  };
  for (const char *b : bad) {
    err.clear();
    QVERIFY2(!GpgIdGeneration::parse(b, &err).has_value(), b);
    QVERIFY2(!err.isEmpty(), b);
  }
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
/// same through a link or a `..`, nothing outside the store.
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
 *        warns about the preselected recipients, a headerless one is
 *        refused as unbound with its own explanation while one with only a
 *        folder line is an ordinary rollback, a malformed one is refused,
 *        and a reservation climbs above the refused one.
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
  QVERIFY2(GpgIdGeneration::accept(file, list(18, "ALICE\nCAROL\n"), store) ==
               GpgIdGeneration::Verdict::Accepted,
           "two devices both making 18 from 17 is git's conflict, not a "
           "rollback");
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

/// The record is on disk when a reservation returns, in a file of its own.
void tst_gpgidgeneration::reservationIsWrittenThrough() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString file = QDir(dir.path()).filePath(QStringLiteral(".gpg-id"));
  QCOMPARE(GpgIdGeneration::reserveNext(file, std::nullopt),
           std::optional<qint64>(1));
  QSettings other(QSettings::defaultFormat(), QSettings::UserScope,
                  QStringLiteral("IJHack"),
                  QStringLiteral("QtPass-gpgid-generations"));
  other.sync();
  QCOMPARE(other.value(GpgIdGeneration::key(file)).toLongLong(), 1);
  QCOMPARE(other.fileName(), GpgIdGeneration::recordFile());
  QVERIFY2(!other.fileName().contains(QStringLiteral("QtPass.conf")),
           qPrintable(other.fileName()));
}

/**
 * @brief No freshness state, no acceptance and no reservation. A record
 *        that cannot be written (its folder read-only) yields no
 *        reservation, and a signed list above anything known is still
 *        refused because accepting it means recording it. A record that
 *        cannot be parsed (garbage where the ini was) reads as nothing, not
 *        as 0, and keeps refusing.
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

  // Garbage where the record was: QSettings reports a format error, and
  // that is "unknown", not "0".
  {
    QFile f(recordFile);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("[unterminated\n\x00\x01garbage\n=\n");
  }
  // QSettings notices a changed file by size and time; make sure of both.
  QTest::qWait(1100);
  why.clear();
  QVERIFY(!GpgIdGeneration::remembered(file, &why).has_value());
  QVERIFY2(why.contains(recordFile), qPrintable(why));
  // And it stays that way for the process: Qt's cache would otherwise carry
  // on with an empty record, which is "never seen", not "unknown".
  why.clear();
  QVERIFY(GpgIdGeneration::accept(file, list(5), store, &why) !=
          GpgIdGeneration::Verdict::Accepted);
  QVERIFY(!why.isEmpty());
  QVERIFY(!GpgIdGeneration::reserveNext(file, std::nullopt).has_value());
#endif
}

QTEST_MAIN(tst_gpgidgeneration)
#include "tst_gpgidgeneration.moc"
