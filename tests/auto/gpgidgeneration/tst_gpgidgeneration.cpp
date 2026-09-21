// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

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

private slots:
  void initTestCase() { isolateTestSettings(); }
  void parseIsStrict();
  void headerRoundTripsAndIsIgnoredByTheRecipientParser();
  void keyIsTheCanonicalPathHashed();
  void acceptRefusesARollbackAndRemembersTheHighest();
  void nextIsOneAboveDiskAndMemory();
  void rememberIsWrittenThrough();
};

/**
 * @brief Missing is generation 0 (a list from pass or from before), exactly
 *        one well-formed line is that generation, anything else under the
 *        same name is not a list to trust.
 */
void tst_gpgidgeneration::parseIsStrict() {
  QString err;
  QCOMPARE(GpgIdGeneration::parse("ALICE\nBOB\n"), std::optional<qint64>(0));
  QCOMPARE(GpgIdGeneration::parse(""), std::optional<qint64>(0));
  QCOMPARE(GpgIdGeneration::parse("# just a comment\nALICE\n"),
           std::optional<qint64>(0));
  QCOMPARE(GpgIdGeneration::parse("# QtPass-GpgId-Generation: 17\nALICE\n"),
           std::optional<qint64>(17));
  QCOMPARE(GpgIdGeneration::parse("ALICE\n# QtPass-GpgId-Generation: 3\r\n"),
           std::optional<qint64>(3));
  QCOMPARE(GpgIdGeneration::parse("# QtPass-GpgId-Generation: "
                                  "999999999999999999\n"),
           std::optional<qint64>(999999999999999999LL));
  const char *bad[] = {
      "# QtPass-GpgId-Generation: banana\n",
      "# QtPass-GpgId-Generation: -1\n",
      "# QtPass-GpgId-Generation: 17 \n",
      "# QtPass-GpgId-Generation: 1999999999999999999\n",
      "# QtPass-GpgId-Generation:17\n",
      "# QtPass-GpgId-Generation: \n",
      "# QtPass-GpgId-Generation: 1\n# QtPass-GpgId-Generation: 2\n",
      "# QtPass-GpgId-Generation: 5\n# QtPass-GpgId-Generation: 5\n",
      "# QtPass-GpgId-Generation\n",
      "# QtPass-GpgId-Generation: 0x10\n",
  };
  for (const char *b : bad) {
    err.clear();
    QVERIFY2(!GpgIdGeneration::parse(b, &err).has_value(), b);
    QVERIFY2(!err.isEmpty(), b);
  }
}

/**
 * @brief The header QtPass writes parses back to its number and is a
 *        comment to the recipient parser, as it is to pass.
 */
void tst_gpgidgeneration::headerRoundTripsAndIsIgnoredByTheRecipientParser() {
  const QByteArray list = GpgIdGeneration::withHeader(42, "ALICE\nBOB\n");
  QCOMPARE(list,
           QByteArrayLiteral("# QtPass-GpgId-Generation: 42\nALICE\nBOB\n"));
  QCOMPARE(GpgIdGeneration::parse(list), std::optional<qint64>(42));
  QCOMPARE(Pass::parseRecipients(list, QStringLiteral("test")),
           (QStringList{QStringLiteral("ALICE"), QStringLiteral("BOB")}));
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
 *        lower one is refused with a reason that names both numbers, a
 *        malformed one is refused, and a save (next) climbs above the
 *        refused one so the way back is to save again.
 */
void tst_gpgidgeneration::acceptRefusesARollbackAndRemembersTheHighest() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString file = QDir(dir.path()).filePath(QStringLiteral(".gpg-id"));
  write(file, GpgIdGeneration::withHeader(17, "ALICE\n"));
  QString why;
  QCOMPARE(GpgIdGeneration::remembered(file), 0);
  QVERIFY(GpgIdGeneration::accept(
      file, GpgIdGeneration::withHeader(17, "ALICE\n"), &why));
  QCOMPARE(GpgIdGeneration::remembered(file), 17);
  QVERIFY(GpgIdGeneration::accept(
      file, GpgIdGeneration::withHeader(18, "ALICE\nBOB\n")));
  QCOMPARE(GpgIdGeneration::remembered(file), 18);
  QVERIFY2(
      GpgIdGeneration::accept(
          file, GpgIdGeneration::withHeader(18, "ALICE\nCAROL\n")),
      "two devices both making 18 from 17 is git's conflict, not a rollback");
  QVERIFY(!GpgIdGeneration::accept(
      file, GpgIdGeneration::withHeader(17, "ALICE\nBOB\n"), &why));
  QVERIFY2(why.contains(QStringLiteral("17")) &&
               why.contains(QStringLiteral("18")) &&
               why.contains(QStringLiteral("Users")),
           qPrintable(why));
  QVERIFY2(!GpgIdGeneration::accept(file, "ALICE\nBOB\n", &why),
           "a list from before generations existed is a rollback too");
  QVERIFY(!GpgIdGeneration::accept(
      file, "# QtPass-GpgId-Generation: x\nALICE\n", &why));
  QCOMPARE(GpgIdGeneration::remembered(file), 18);
  // Saving again is the way through: one above the highest known.
  write(file, GpgIdGeneration::withHeader(17, "ALICE\nBOB\n"));
  QCOMPARE(GpgIdGeneration::next(file), 19);
}

/// next() is one above both the file on disk and what is remembered.
void tst_gpgidgeneration::nextIsOneAboveDiskAndMemory() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString file = QDir(dir.path()).filePath(QStringLiteral(".gpg-id"));
  QCOMPARE(GpgIdGeneration::next(file), 1);
  write(file, "ALICE\n");
  QCOMPARE(GpgIdGeneration::next(file), 1);
  write(file, GpgIdGeneration::withHeader(5, "ALICE\n"));
  QCOMPARE(GpgIdGeneration::next(file), 6);
  QVERIFY(GpgIdGeneration::remember(file, 9));
  QCOMPARE(GpgIdGeneration::next(file), 10);
  write(file, "# QtPass-GpgId-Generation: broken\nALICE\n");
  QCOMPARE(GpgIdGeneration::next(file), 10);
}

/// The record is on disk when remember() returns, in its own settings file.
void tst_gpgidgeneration::rememberIsWrittenThrough() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString file = QDir(dir.path()).filePath(QStringLiteral(".gpg-id"));
  QVERIFY(GpgIdGeneration::remember(file, 23));
  QSettings other(QSettings::defaultFormat(), QSettings::UserScope,
                  QStringLiteral("IJHack"),
                  QStringLiteral("QtPass-gpgid-generations"));
  other.sync();
  QCOMPARE(other.value(GpgIdGeneration::key(file)).toLongLong(), 23);
  QVERIFY2(!other.fileName().contains(QStringLiteral("QtPass.conf")),
           qPrintable(other.fileName()));
}

QTEST_MAIN(tst_gpgidgeneration)
#include "tst_gpgidgeneration.moc"
