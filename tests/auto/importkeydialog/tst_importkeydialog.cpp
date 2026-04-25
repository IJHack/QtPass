// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QClipboard>
#include <QPlainTextEdit>
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
  void parseGpgImportOutputMixedCaseFingerprint();
  void parseGpgImportOutputMultiDigitReasonCode();
  void parseGpgImportOutputImportedNoEmail();
  void parseGpgImportOutputNoTrailingNewline();
  void parseGpgImportOutputFallbackRequires16Or40Chars_data();
  void parseGpgImportOutputFallbackRequires16Or40Chars();
  void parseGpgImportOutputLowercaseOnlyFingerprint();
  void parseGpgImportOutputWhitespaceOnly();
  void parseGpgImportOutputImportResWithoutImportOk();
  void importButtonEnabledAfterTextInput();
  void importButtonDisabledForWhitespaceOnlyInput();
  void pasteButtonSetsTextFromClipboard();
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

  // Regression: a 40-character fingerprint on the English fallback line
  // must also be extracted (the fallback accepts 16 or 40 hex chars).
  QTest::newRow("english_human_line_40char_fingerprint")
      << QStringLiteral(
             "gpg: key AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA: imported\n")
      << QStringLiteral("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");

  // The fallback pattern requires exactly 16 or 40 hex chars; 8-char short
  // key ids must NOT match (they would be ambiguous / false positives).
  QTest::newRow("english_human_line_8char_no_match")
      << QStringLiteral("gpg: key DEADBEEF: imported\n") << QString();

  // IMPORT_OK followed only by IMPORT_RES (no IMPORTED line) still works.
  QTest::newRow("import_ok_then_import_res_only")
      << QStringLiteral(
             "[GNUPG:] IMPORT_OK 1 AABBCCDDEEFF00112233445566778899AABBCCDD\n"
             "[GNUPG:] IMPORT_RES 1 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0\n")
      << QStringLiteral("AABBCCDDEEFF00112233445566778899AABBCCDD");
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

// ---------------------------------------------------------------------------
// Additional tests
// ---------------------------------------------------------------------------

void tst_importkeydialog::parseGpgImportOutputMixedCaseFingerprint() {
  // The IMPORT_OK regex uses [0-9A-Fa-f] so mixed case must be captured
  // exactly as-is (the caller is responsible for normalisation).
  const QString output = QStringLiteral(
      "[GNUPG:] IMPORT_OK 1 aAbBcCdDeEfF00112233445566778899aAbBcCdD\n");
  QCOMPARE(ImportKeyDialog::parseGpgImportOutput(output),
           QStringLiteral("aAbBcCdDeEfF00112233445566778899aAbBcCdD"));
}

void tst_importkeydialog::parseGpgImportOutputMultiDigitReasonCode() {
  // Reason codes are bitmasks; they can be multi-digit (e.g., 17 = new key +
  // signatures). The \d+ in the regex must accept more than one digit.
  const QString output = QStringLiteral(
      "[GNUPG:] IMPORT_OK 17 CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC\n");
  QCOMPARE(ImportKeyDialog::parseGpgImportOutput(output),
           QStringLiteral("CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"));
}

void tst_importkeydialog::parseGpgImportOutputImportedNoEmail() {
  // IMPORTED lines do not always carry a user-id after the key id.
  const QString output = QStringLiteral("[GNUPG:] IMPORTED DEADBEEFCAFE0123\n");
  QCOMPARE(ImportKeyDialog::parseGpgImportOutput(output),
           QStringLiteral("DEADBEEFCAFE0123"));
}

void tst_importkeydialog::parseGpgImportOutputNoTrailingNewline() {
  // Output from some gpg builds may lack a trailing newline; the split
  // must still find the one line present.
  const QString output = QStringLiteral(
      "[GNUPG:] IMPORT_OK 1 EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE");
  QCOMPARE(ImportKeyDialog::parseGpgImportOutput(output),
           QStringLiteral("EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE"));
}

void tst_importkeydialog::
    parseGpgImportOutputFallbackRequires16Or40Chars_data() {
  QTest::addColumn<QString>("output");
  QTest::addColumn<QString>("expected");

  // Exactly 16 hex chars: matches
  QTest::newRow("16_chars")
      << QStringLiteral("gpg: key DEADBEEFCAFE0123: imported\n")
      << QStringLiteral("DEADBEEFCAFE0123");

  // Exactly 40 hex chars: matches
  QTest::newRow("40_chars")
      << QStringLiteral(
             "gpg: key AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA: imported\n")
      << QStringLiteral("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");

  // 8 hex chars: does NOT match (too short)
  QTest::newRow("8_chars_no_match")
      << QStringLiteral("gpg: key DEADBEEF: imported\n") << QString();

  // 32 hex chars: does NOT match (not 16 or 40)
  QTest::newRow("32_chars_no_match")
      << QStringLiteral("gpg: key DEADBEEFCAFE0123DEADBEEFCAFE0123: imported\n")
      << QString();
}

void tst_importkeydialog::parseGpgImportOutputFallbackRequires16Or40Chars() {
  QFETCH(QString, output);
  QFETCH(QString, expected);
  QCOMPARE(ImportKeyDialog::parseGpgImportOutput(output), expected);
}

void tst_importkeydialog::parseGpgImportOutputLowercaseOnlyFingerprint() {
  // Fingerprints are sometimes reported fully lowercase by gpg builds.
  const QString output = QStringLiteral(
      "[GNUPG:] IMPORT_OK 1 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n");
  QCOMPARE(ImportKeyDialog::parseGpgImportOutput(output),
           QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
}

void tst_importkeydialog::parseGpgImportOutputWhitespaceOnly() {
  // A string of only whitespace / blank lines must return empty.
  QVERIFY(ImportKeyDialog::parseGpgImportOutput(QStringLiteral("   \n\n  \t\n"))
              .isEmpty());
}

void tst_importkeydialog::parseGpgImportOutputImportResWithoutImportOk() {
  // IMPORT_RES without a preceding IMPORT_OK/IMPORTED should return empty
  // rather than attempting to extract anything from the summary line.
  const QString output =
      QStringLiteral("[GNUPG:] IMPORT_RES 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0\n");
  QVERIFY(ImportKeyDialog::parseGpgImportOutput(output).isEmpty());
}

void tst_importkeydialog::importButtonEnabledAfterTextInput() {
  // Simulating on_inputTextEdit_textChanged via the text edit widget.
  ImportKeyDialog dialog;
  auto *button =
      dialog.findChild<QPushButton *>(QStringLiteral("importButton"));
  auto *edit =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("inputTextEdit"));
  QVERIFY(button != nullptr);
  QVERIFY(edit != nullptr);

  QVERIFY(!button->isEnabled()); // precondition

  // Setting text on the QPlainTextEdit emits textChanged which triggers
  // on_inputTextEdit_textChanged() and should enable the import button.
  edit->setPlainText(QStringLiteral("-----BEGIN PGP PUBLIC KEY BLOCK-----"));
  QVERIFY(button->isEnabled());
}

void tst_importkeydialog::importButtonDisabledForWhitespaceOnlyInput() {
  // Whitespace-only content trims to empty; the button must stay disabled.
  ImportKeyDialog dialog;
  auto *button =
      dialog.findChild<QPushButton *>(QStringLiteral("importButton"));
  auto *edit =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("inputTextEdit"));
  QVERIFY(button != nullptr);
  QVERIFY(edit != nullptr);

  edit->setPlainText(QStringLiteral("   \n\t\n   "));
  QVERIFY(!button->isEnabled());
}

void tst_importkeydialog::pasteButtonSetsTextFromClipboard() {
  // Verify clipboard round-trip capability before proceeding.
  QClipboard *clipboard = QApplication::clipboard();
  const QString originalClipboard = clipboard->text();
  const QString probe = QStringLiteral("__qtpass_clipboard_probe__");
  clipboard->setText(probe);
  if (clipboard->text() != probe) {
    clipboard->setText(originalClipboard);
    QSKIP("Clipboard is not functional on this platform");
  }
  clipboard->setText(originalClipboard);

  ImportKeyDialog dialog;
  auto *edit =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("inputTextEdit"));
  QVERIFY(edit != nullptr);

  const QString payload =
      QStringLiteral("-----BEGIN PGP PUBLIC KEY BLOCK-----\nABC\n"
                     "-----END PGP PUBLIC KEY BLOCK-----\n");
  clipboard->setText(payload);

  QMetaObject::invokeMethod(&dialog, "on_pasteButton_clicked",
                            Qt::DirectConnection);

  QCOMPARE(edit->toPlainText(), payload);

  // Restore clipboard
  clipboard->setText(originalClipboard);
}

QTEST_MAIN(tst_importkeydialog)
#include "tst_importkeydialog.moc"
