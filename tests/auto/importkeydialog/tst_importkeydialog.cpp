// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include <functional>

#include "../../../src/importkeydialog.h"
#include "../testsettings.h"

class tst_importkeydialog : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
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
  void pasteButtonKeepsInputForEmptyClipboard();
  void importButtonIgnoresEmptyInput();
  void importSuccessAcceptsWithKeyIdAndFeedsInputToGpg();
  void importFailureShowsStderrAndStaysOpen();
  void importUnparseableOutputShowsParseErrorAndStaysOpen();
  void importFallsBackToStderrForKeyId();
  void fileButtonCancelLeavesInputUntouched();
  void fileButtonLoadsArmoredFileIntoInput();
  void fileButtonRejectsNonArmoredFile();
  void fileButtonWarnsWhenFileCannotBeOpened();

private:
  /**
   * @brief Outcome of driving the modal dialogs a slot opens.
   */
  struct ModalRun {
    int fileDialogs = 0;
    int messageBoxes = 0;
    QString messageText;
    QStringList selectedFiles;
  };

  /// A stand-in gpg at m_dir/@p name: "#!/bin/sh" followed by @p body.
  /// Empty when it could not be written.
  auto writeGpg(const QString &name, const QString &body) -> QString;
  /// Run @p trigger while a timer drives every modal it opens: a QFileDialog
  /// gets @p filePath selected and accepted (or is cancelled when the path
  /// is empty), a QMessageBox has its text captured and is accepted.
  auto driveModals(const std::function<void()> &trigger,
                   const QString &filePath = QString()) -> ModalRun;
  /// Write @p bytes to m_dir/@p name; empty when that failed.
  auto writeFile(const QString &name, const QByteArray &bytes) -> QString;

  QTemporaryDir m_dir;
};

/**
 * @brief Isolate settings, force Qt's own dialogs and claim the scratch dir.
 *
 * driveModals() finds the dialogs a slot opens through
 * QApplication::activeModalWidget(). A native (platform-theme) file dialog
 * or message box never enters that modal-widget stack, so with a desktop
 * theme such as gtk3 the poker would wait forever; the widget-based dialogs
 * behave the same on every platform.
 */
void tst_importkeydialog::initTestCase() {
  isolateTestSettings();
  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  QVERIFY2(m_dir.isValid(), qPrintable(m_dir.errorString()));
}

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
  ImportKeyDialog dialog(QString{});
  QVERIFY(dialog.importedKeyId().isEmpty());
}

void tst_importkeydialog::importButtonStartsDisabled() {
  ImportKeyDialog dialog(QString{});
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
  ImportKeyDialog dialog(QString{});
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
  ImportKeyDialog dialog(QString{});
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
  // RAII restore so the user's clipboard is reset on every exit path
  // (early QSKIP, normal completion, or QVERIFY/QCOMPARE failure).
  const auto restoreClipboard = qScopeGuard([clipboard, originalClipboard]() {
    clipboard->setText(originalClipboard);
  });
  const QString probe = QStringLiteral("__qtpass_clipboard_probe__");
  clipboard->setText(probe);
  if (clipboard->text() != probe) {
    QSKIP("Clipboard is not functional on this platform");
  }

  ImportKeyDialog dialog(QString{});
  auto *edit =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("inputTextEdit"));
  QVERIFY(edit != nullptr);
  auto *pasteButton =
      dialog.findChild<QPushButton *>(QStringLiteral("pasteButton"));
  QVERIFY(pasteButton != nullptr);

  const QString payload =
      QStringLiteral("-----BEGIN PGP PUBLIC KEY BLOCK-----\nABC\n"
                     "-----END PGP PUBLIC KEY BLOCK-----\n");
  clipboard->setText(payload);

  pasteButton->click();

  QCOMPARE(edit->toPlainText(), payload);
}

// ---------------------------------------------------------------------------
// Import and file-button behaviour, driven through fake gpg and the modal
// dialogs the slots open
// ---------------------------------------------------------------------------

auto tst_importkeydialog::writeGpg(const QString &name, const QString &body)
    -> QString {
  const QString gpg = QDir(m_dir.path()).filePath(name);
  QFile script(gpg);
  if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return {};
  QTextStream out(&script);
  out << "#!/bin/sh\n" << body;
  out.flush();
  script.close();
  if (!script.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                             QFile::ExeOwner))
    return {};
  return gpg;
}

auto tst_importkeydialog::writeFile(const QString &name,
                                    const QByteArray &bytes) -> QString {
  const QString path = QDir(m_dir.path()).filePath(name);
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return {};
  if (file.write(bytes) != bytes.size())
    return {};
  file.close();
  return path;
}

auto tst_importkeydialog::driveModals(const std::function<void()> &trigger,
                                      const QString &filePath) -> ModalRun {
  ModalRun run;
  QTimer poker;
  poker.setInterval(20);
  QObject::connect(&poker, &QTimer::timeout, [&]() {
    QWidget *modal = QApplication::activeModalWidget();
    if (modal == nullptr || modal->property("tst_driven").toBool()) {
      return;
    }
    if (auto *fileDialog = qobject_cast<QFileDialog *>(modal)) {
      modal->setProperty("tst_driven", true);
      ++run.fileDialogs;
      if (filePath.isEmpty()) {
        fileDialog->reject();
      } else {
        // selectFile() leaves the name edit alone while it has focus (a
        // user is typing); drop focus first so the selection lands.
        if (QWidget *focused = fileDialog->focusWidget()) {
          focused->clearFocus();
        }
        fileDialog->selectFile(filePath);
        run.selectedFiles = fileDialog->selectedFiles();
        // QFileDialog redeclares accept() protected; the QDialog view of it
        // is public and still dispatches virtually to the QFileDialog logic.
        static_cast<QDialog *>(fileDialog)->accept();
      }
      return;
    }
    if (auto *box = qobject_cast<QMessageBox *>(modal)) {
      modal->setProperty("tst_driven", true);
      ++run.messageBoxes;
      run.messageText = box->text();
      box->accept();
    }
  });
  poker.start();
  // The trigger runs the slot, and thus the nested modal loops,
  // synchronously.
  trigger();
  poker.stop();
  return run;
}

/**
 * @brief Pins the empty-clipboard guard of the paste button: whatever the
 *        user already typed or loaded must survive a paste from an empty
 *        clipboard instead of being wiped.
 */
void tst_importkeydialog::pasteButtonKeepsInputForEmptyClipboard() {
  QClipboard *clipboard = QApplication::clipboard();
  const QString originalClipboard = clipboard->text();
  const auto restoreClipboard = qScopeGuard([clipboard, originalClipboard]() {
    clipboard->setText(originalClipboard);
  });
  clipboard->clear();
  if (!clipboard->text().isEmpty()) {
    QSKIP("Clipboard cannot be cleared on this platform");
  }

  ImportKeyDialog dialog(QString{});
  auto *edit =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("inputTextEdit"));
  auto *pasteButton =
      dialog.findChild<QPushButton *>(QStringLiteral("pasteButton"));
  QVERIFY2(edit != nullptr, "inputTextEdit widget must exist");
  QVERIFY2(pasteButton != nullptr, "pasteButton widget must exist");
  const QString typed = QStringLiteral("-----BEGIN PGP PUBLIC KEY BLOCK-----");
  edit->setPlainText(typed);

  pasteButton->click();

  QCOMPARE(edit->toPlainText(), typed);
}

/**
 * @brief Pins that the import slot is a no-op on empty input: no gpg is
 *        spawned (the fake would leave a marker file), no message box opens
 *        and the dialog stays open with an empty key id. The button itself
 *        is disabled then, so the slot is invoked directly.
 */
void tst_importkeydialog::importButtonIgnoresEmptyInput() {
#ifdef Q_OS_WIN
  QSKIP("Fake gpg is a shell script");
#endif
  const QString gpg = writeGpg(QStringLiteral("gpg-never"),
                               QStringLiteral("touch \"$0.ran\"\nexit 0\n"));
  QVERIFY2(!gpg.isEmpty(), "could not write fake gpg");
  ImportKeyDialog dialog(gpg);
  auto *edit =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("inputTextEdit"));
  QVERIFY2(edit != nullptr, "inputTextEdit widget must exist");
  edit->setPlainText(QStringLiteral("  \n\t "));
  QSignalSpy accepted(&dialog, &QDialog::accepted);

  const ModalRun run = driveModals([&]() {
    QVERIFY2(QMetaObject::invokeMethod(&dialog, "on_importButton_clicked",
                                       Qt::DirectConnection),
             "on_importButton_clicked slot must be callable through the "
             "meta-object");
  });

  QCOMPARE(run.messageBoxes, 0);
  QVERIFY2(!QFile::exists(gpg + QStringLiteral(".ran")),
           "gpg must not run for empty input");
  QCOMPARE(accepted.count(), 0);
  QVERIFY2(dialog.importedKeyId().isEmpty(), "no key id without an import");
}

/**
 * @brief Pins the happy path: the armored text is fed to gpg on stdin with
 *        --import and --status-fd 1, the fingerprint from IMPORT_OK is
 *        stored, the success box names it and warns about verifying the
 *        owner, and the dialog accepts itself.
 */
void tst_importkeydialog::importSuccessAcceptsWithKeyIdAndFeedsInputToGpg() {
#ifdef Q_OS_WIN
  QSKIP("Fake gpg is a shell script");
#endif
  const QString fingerprint =
      QStringLiteral("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
  const QString gpg = writeGpg(
      QStringLiteral("gpg-ok"),
      QStringLiteral("cat > \"$0.stdin\"\nprintf '%s\\n' \"$*\" > \"$0.args\"\n"
                     "printf '[GNUPG:] IMPORTED DEADBEEFCAFE0123 "
                     "user@example.com\\n'\n"
                     "printf '[GNUPG:] IMPORT_OK 1 %1\\n'\nexit 0\n")
          .arg(fingerprint));
  QVERIFY2(!gpg.isEmpty(), "could not write fake gpg");
  ImportKeyDialog dialog(gpg);
  auto *edit =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("inputTextEdit"));
  auto *importButton =
      dialog.findChild<QPushButton *>(QStringLiteral("importButton"));
  QVERIFY2(edit != nullptr, "inputTextEdit widget must exist");
  QVERIFY2(importButton != nullptr, "importButton widget must exist");
  const QString armored =
      QStringLiteral("-----BEGIN PGP PUBLIC KEY BLOCK-----\nABC\n"
                     "-----END PGP PUBLIC KEY BLOCK-----");
  edit->setPlainText(armored + QStringLiteral("\n\n"));
  QVERIFY2(importButton->isEnabled(), "armored input must enable Import");
  QSignalSpy accepted(&dialog, &QDialog::accepted);

  const ModalRun run = driveModals([&]() { importButton->click(); });

  QCOMPARE(run.messageBoxes, 1);
  QVERIFY2(run.messageText.contains(fingerprint),
           qPrintable(QStringLiteral("success box must name the key: %1")
                          .arg(run.messageText)));
  QVERIFY2(run.messageText.contains(QStringLiteral("fingerprint")),
           "success box must tell the user to check the fingerprint");
  QCOMPARE(dialog.importedKeyId(), fingerprint);
  QCOMPARE(accepted.count(), 1);
  QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));

  QFile stdinFile(gpg + QStringLiteral(".stdin"));
  QVERIFY2(stdinFile.open(QIODevice::ReadOnly), "gpg must have read stdin");
  // The slot trims the input before handing it to gpg: the surrounding
  // blank lines typed above must not reach stdin.
  QCOMPARE(QString::fromUtf8(stdinFile.readAll()), armored);
  QFile argsFile(gpg + QStringLiteral(".args"));
  QVERIFY2(argsFile.open(QIODevice::ReadOnly), "gpg must have been invoked");
  const QString args = QString::fromUtf8(argsFile.readAll());
  QVERIFY2(args.contains(QStringLiteral("--import")), qPrintable(args));
  QVERIFY2(args.contains(QStringLiteral("--status-fd 1")), qPrintable(args));
  QVERIFY2(args.contains(QStringLiteral("--batch")), qPrintable(args));
}

/**
 * @brief Pins the failure path: a non-zero gpg exit shows gpg's own stderr
 *        in a warning box, stores no key id and keeps the dialog open so the
 *        user can fix the input.
 */
void tst_importkeydialog::importFailureShowsStderrAndStaysOpen() {
#ifdef Q_OS_WIN
  QSKIP("Fake gpg is a shell script");
#endif
  const QString gpg = writeGpg(
      QStringLiteral("gpg-fail"),
      QStringLiteral("echo 'gpg: no valid OpenPGP data found.' >&2\nexit 2\n"));
  QVERIFY2(!gpg.isEmpty(), "could not write fake gpg");
  ImportKeyDialog dialog(gpg);
  auto *edit =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("inputTextEdit"));
  auto *importButton =
      dialog.findChild<QPushButton *>(QStringLiteral("importButton"));
  QVERIFY2(edit != nullptr, "inputTextEdit widget must exist");
  QVERIFY2(importButton != nullptr, "importButton widget must exist");
  edit->setPlainText(QStringLiteral("not a key"));
  QSignalSpy accepted(&dialog, &QDialog::accepted);

  const ModalRun run = driveModals([&]() { importButton->click(); });

  QCOMPARE(run.messageBoxes, 1);
  QVERIFY2(run.messageText.startsWith(QStringLiteral("GPG import failed")),
           qPrintable(run.messageText));
  QVERIFY2(run.messageText.contains(
               QStringLiteral("gpg: no valid OpenPGP data found.")),
           qPrintable(run.messageText));
  QVERIFY2(dialog.importedKeyId().isEmpty(), "failed import stores no id");
  QCOMPARE(accepted.count(), 0);
  QCOMPARE(dialog.result(), static_cast<int>(QDialog::Rejected));
}

/**
 * @brief Pins the parse-failure path: gpg exiting 0 without any recognisable
 *        key line yields a dedicated warning, no key id and no accept, rather
 *        than reporting success for an import we cannot identify.
 */
void tst_importkeydialog::importUnparseableOutputShowsParseErrorAndStaysOpen() {
#ifdef Q_OS_WIN
  QSKIP("Fake gpg is a shell script");
#endif
  const QString gpg = writeGpg(
      QStringLiteral("gpg-noid"),
      QStringLiteral("echo 'gpg: Total number processed: 0'\n"
                     "echo '[GNUPG:] IMPORT_RES 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 "
                     "0'\nexit 0\n"));
  QVERIFY2(!gpg.isEmpty(), "could not write fake gpg");
  ImportKeyDialog dialog(gpg);
  auto *edit =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("inputTextEdit"));
  auto *importButton =
      dialog.findChild<QPushButton *>(QStringLiteral("importButton"));
  QVERIFY2(edit != nullptr, "inputTextEdit widget must exist");
  QVERIFY2(importButton != nullptr, "importButton widget must exist");
  edit->setPlainText(QStringLiteral("-----BEGIN PGP PUBLIC KEY BLOCK-----"));
  QSignalSpy accepted(&dialog, &QDialog::accepted);

  const ModalRun run = driveModals([&]() { importButton->click(); });

  QCOMPARE(run.messageBoxes, 1);
  QCOMPARE(run.messageText,
           QStringLiteral("Could not parse imported key id from GPG output."));
  QVERIFY2(dialog.importedKeyId().isEmpty(), "unparsed import stores no id");
  QCOMPARE(accepted.count(), 0);
}

/**
 * @brief Pins the stderr fallback: when stdout carries no key line (older
 *        gpg without --status-fd honoured, or a wrapper that swallows it) the
 *        human-readable line on stderr still identifies the imported key.
 */
void tst_importkeydialog::importFallsBackToStderrForKeyId() {
#ifdef Q_OS_WIN
  QSKIP("Fake gpg is a shell script");
#endif
  const QString gpg = writeGpg(
      QStringLiteral("gpg-stderr"),
      QStringLiteral("echo 'gpg: key DEADBEEFCAFE0123: public key \"u\" "
                     "imported' >&2\nexit 0\n"));
  QVERIFY2(!gpg.isEmpty(), "could not write fake gpg");
  ImportKeyDialog dialog(gpg);
  auto *edit =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("inputTextEdit"));
  auto *importButton =
      dialog.findChild<QPushButton *>(QStringLiteral("importButton"));
  QVERIFY2(edit != nullptr, "inputTextEdit widget must exist");
  QVERIFY2(importButton != nullptr, "importButton widget must exist");
  edit->setPlainText(QStringLiteral("-----BEGIN PGP PUBLIC KEY BLOCK-----"));
  QSignalSpy accepted(&dialog, &QDialog::accepted);

  const ModalRun run = driveModals([&]() { importButton->click(); });

  QCOMPARE(run.messageBoxes, 1);
  QVERIFY2(run.messageText.contains(QStringLiteral("DEADBEEFCAFE0123")),
           qPrintable(run.messageText));
  QCOMPARE(dialog.importedKeyId(), QStringLiteral("DEADBEEFCAFE0123"));
  QCOMPARE(accepted.count(), 1);
}

/**
 * @brief Pins that cancelling the file picker is harmless: the input keeps
 *        what the user already had and no message box appears.
 */
void tst_importkeydialog::fileButtonCancelLeavesInputUntouched() {
  ImportKeyDialog dialog(QString{});
  auto *edit =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("inputTextEdit"));
  auto *fileButton =
      dialog.findChild<QPushButton *>(QStringLiteral("fileButton"));
  QVERIFY2(edit != nullptr, "inputTextEdit widget must exist");
  QVERIFY2(fileButton != nullptr, "fileButton widget must exist");
  const QString typed = QStringLiteral("keep me");
  edit->setPlainText(typed);

  const ModalRun run = driveModals([&]() { fileButton->click(); });

  QCOMPARE(run.fileDialogs, 1);
  QCOMPARE(run.messageBoxes, 0);
  QCOMPARE(edit->toPlainText(), typed);
}

/**
 * @brief Pins the file happy path: picking an ASCII-armored .asc loads its
 *        exact contents into the input, which in turn enables Import.
 */
void tst_importkeydialog::fileButtonLoadsArmoredFileIntoInput() {
  const QByteArray armored =
      QByteArrayLiteral("\n-----BEGIN PGP PUBLIC KEY BLOCK-----\nABC\n"
                        "-----END PGP PUBLIC KEY BLOCK-----\n");
  const QString path = writeFile(QStringLiteral("key.asc"), armored);
  QVERIFY2(!path.isEmpty(), "could not write key file");
  ImportKeyDialog dialog(QString{});
  auto *edit =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("inputTextEdit"));
  auto *fileButton =
      dialog.findChild<QPushButton *>(QStringLiteral("fileButton"));
  auto *importButton =
      dialog.findChild<QPushButton *>(QStringLiteral("importButton"));
  QVERIFY2(edit != nullptr, "inputTextEdit widget must exist");
  QVERIFY2(fileButton != nullptr, "fileButton widget must exist");
  QVERIFY2(importButton != nullptr, "importButton widget must exist");
  QVERIFY2(!importButton->isEnabled(), "Import starts disabled");

  const ModalRun run = driveModals([&]() { fileButton->click(); }, path);

  QCOMPARE(run.fileDialogs, 1);
  QCOMPARE(run.selectedFiles, QStringList{path});
  QCOMPARE(run.messageBoxes, 0);
  QCOMPARE(edit->toPlainText(), QString::fromUtf8(armored));
  QVERIFY2(importButton->isEnabled(), "loaded key must enable Import");
}

/**
 * @brief Pins the armor check: a binary keyring is refused with a warning
 *        that names the (HTML-escaped) path and points at --armor, and the
 *        input stays empty so bytes are never mangled through UTF-8.
 */
void tst_importkeydialog::fileButtonRejectsNonArmoredFile() {
  const QByteArray binary = QByteArrayLiteral("\x99\x01\x0d\x04\x00\xff<b>");
  // An ampersand: HTML-special, yet legal in a file name everywhere (a
  // "<" or ">" is not on Windows).
  const QString path = writeFile(QStringLiteral("key&b.gpg"), binary);
  QVERIFY2(!path.isEmpty(), "could not write key file");
  ImportKeyDialog dialog(QString{});
  auto *edit =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("inputTextEdit"));
  auto *fileButton =
      dialog.findChild<QPushButton *>(QStringLiteral("fileButton"));
  QVERIFY2(edit != nullptr, "inputTextEdit widget must exist");
  QVERIFY2(fileButton != nullptr, "fileButton widget must exist");

  const ModalRun run = driveModals([&]() { fileButton->click(); }, path);

  QCOMPARE(run.fileDialogs, 1);
  QCOMPARE(run.selectedFiles, QStringList{path});
  QCOMPARE(run.messageBoxes, 1);
  QVERIFY2(run.messageText.contains(path.toHtmlEscaped()),
           qPrintable(run.messageText));
  QVERIFY2(!run.messageText.contains(QStringLiteral("key&b.gpg")),
           "raw path must not reach the rich-text body");
  QVERIFY2(run.messageText.contains(QStringLiteral("--armor --export")),
           qPrintable(run.messageText));
  QVERIFY2(edit->toPlainText().isEmpty(), "binary content must not be shown");
}

/**
 * @brief Pins the open-failure warning: an unreadable file yields a
 *        "Could not open file" box naming the path and leaves the input
 *        untouched.
 */
void tst_importkeydialog::fileButtonWarnsWhenFileCannotBeOpened() {
#ifdef Q_OS_WIN
  QSKIP("Permission bits are POSIX");
#endif
  const QString path = writeFile(QStringLiteral("locked.asc"),
                                 QByteArrayLiteral("-----BEGIN PGP"));
  QVERIFY2(!path.isEmpty(), "could not write key file");
  QVERIFY2(QFile::setPermissions(path, QFile::Permissions()),
           "could not strip the file permissions");
  const auto restore = qScopeGuard([path]() {
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
  });
  {
    QFile probe(path);
    if (probe.open(QIODevice::ReadOnly)) {
      QSKIP("File stays readable (running as root?)");
    }
  }
  ImportKeyDialog dialog(QString{});
  auto *edit =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("inputTextEdit"));
  auto *fileButton =
      dialog.findChild<QPushButton *>(QStringLiteral("fileButton"));
  QVERIFY2(edit != nullptr, "inputTextEdit widget must exist");
  QVERIFY2(fileButton != nullptr, "fileButton widget must exist");
  const QString typed = QStringLiteral("keep me");
  edit->setPlainText(typed);

  const ModalRun run = driveModals([&]() { fileButton->click(); }, path);

  QCOMPARE(run.fileDialogs, 1);
  QCOMPARE(run.selectedFiles, QStringList{path});
  QCOMPARE(run.messageBoxes, 1);
  QVERIFY2(run.messageText.startsWith(QStringLiteral("Could not open file")),
           qPrintable(run.messageText));
  QVERIFY2(run.messageText.contains(path), qPrintable(run.messageText));
  QCOMPARE(edit->toPlainText(), typed);
}

QTEST_MAIN(tst_importkeydialog)
#include "tst_importkeydialog.moc"
