// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QPushButton>
#include <QtTest>

#include "../../../src/importkeydialog.h"

class tst_importkeydialog : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void parseGpgImportOutput_data();
  void parseGpgImportOutput();
  void parseGpgImportOutputPrefersStatusOverHumanLine();
  void parseGpgImportOutputPrefersImportOkOverImported();
  void initialKeyIdEmpty();
  void importButtonStartsDisabled();
};

void tst_importkeydialog::parseGpgImportOutput_data() {
  QTest::addColumn<QString>("output");
  QTest::addColumn<QString>("expected");

  QTest::newRow("empty") << QString() << QString();

  QTest::newRow("status_import_ok_fingerprint")
      << QStringLiteral("[GNUPG:] IMPORT_OK 1 "
                        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
                        "[GNUPG:] IMPORT_RES 1 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0\n")
      << QStringLiteral("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");

  QTest::newRow("status_imported_keyid")
      << QStringLiteral("[GNUPG:] IMPORTED DEADBEEFCAFE0123 user@example.com\n")
      << QStringLiteral("DEADBEEFCAFE0123");

  QTest::newRow("english_human_line")
      << QStringLiteral(
             "gpg: key DEADBEEFCAFE0123: public key \"user\" imported\n")
      << QStringLiteral("DEADBEEFCAFE0123");

  QTest::newRow("english_human_line_terse")
      << QStringLiteral("gpg: key DEADBEEFCAFE0123: imported\n")
      << QStringLiteral("DEADBEEFCAFE0123");

  // Non-English locale: only the [GNUPG:] status line should match;
  // the human-readable line uses translated tokens we can't parse.
  QTest::newRow("non_english_locale_with_status")
      << QStringLiteral("gpg: clave DEADBEEFCAFE0123: importada\n"
                        "[GNUPG:] IMPORT_OK 1 "
                        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n")
      << QStringLiteral("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");

  QTest::newRow("non_english_locale_no_status_returns_empty")
      << QStringLiteral("gpg: clave DEADBEEFCAFE0123: importada\n")
      << QString();

  QTest::newRow("no_match")
      << QStringLiteral("gpg: nothing happened\n") << QString();

  QTest::newRow("multiple_keys_returns_first_status_match")
      << QStringLiteral("[GNUPG:] IMPORT_OK 1 "
                        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
                        "[GNUPG:] IMPORT_OK 1 "
                        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB\n")
      << QStringLiteral("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");

  QTest::newRow("import_ok_with_zero_reason")
      << QStringLiteral("[GNUPG:] IMPORT_OK 0 "
                        "DEADBEEFCAFE0123DEADBEEFCAFE0123DEADBEEF\n")
      << QStringLiteral("DEADBEEFCAFE0123DEADBEEFCAFE0123DEADBEEF");
}

void tst_importkeydialog::parseGpgImportOutput() {
  QFETCH(QString, output);
  QFETCH(QString, expected);
  QCOMPARE(ImportKeyDialog::parseGpgImportOutput(output), expected);
}

void tst_importkeydialog::parseGpgImportOutputPrefersStatusOverHumanLine() {
  // Both lines present; status line is locale-independent so it wins.
  const QString output = QStringLiteral(
      "gpg: key 1111111111111111: imported\n"
      "[GNUPG:] IMPORT_OK 1 AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n");
  QCOMPARE(ImportKeyDialog::parseGpgImportOutput(output),
           QStringLiteral("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));
}

void tst_importkeydialog::parseGpgImportOutputPrefersImportOkOverImported() {
  // IMPORT_OK provides a fingerprint, IMPORTED provides a key id; prefer
  // the more specific fingerprint.
  const QString output = QStringLiteral(
      "[GNUPG:] IMPORTED DEADBEEFCAFE0123 user@example.com\n"
      "[GNUPG:] IMPORT_OK 1 AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n");
  QCOMPARE(ImportKeyDialog::parseGpgImportOutput(output),
           QStringLiteral("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));
}

void tst_importkeydialog::initialKeyIdEmpty() {
  ImportKeyDialog dialog;
  QVERIFY(dialog.importedKeyId().isEmpty());
}

void tst_importkeydialog::importButtonStartsDisabled() {
  ImportKeyDialog dialog;
  auto *button =
      dialog.findChild<QPushButton *>(QStringLiteral("importButton"));
  QVERIFY(button != nullptr);
  QVERIFY(!button->isEnabled());
}

QTEST_MAIN(tst_importkeydialog)
#include "tst_importkeydialog.moc"
