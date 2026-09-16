// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QtTest>

#include "../../../src/keygendialog.h"
#include "../testsettings.h"

/**
 * @class tst_keygendialog
 * @brief Widget-level tests for KeygenDialog.
 *
 * KeygenDialog is the modal that fronts GPG key-pair generation. These tests
 * cover construction and the input-driven UI state transitions; they don't
 * drive a full key generation (that needs a real gpg + several seconds of
 * entropy gathering).
 *
 * The dialog is normally parented to a ConfigDialog; tests pass nullptr to
 * sidestep ConfigDialog construction entirely. The protected `done()` slot
 * isn't exercised here for the same reason (it dispatches into
 * dialog->genKey(), which dereferences the ConfigDialog parent).
 */
class tst_keygendialog : public QObject {
  Q_OBJECT

private Q_SLOTS:
  // KeygenDialog reads QtPassSettings; redirect QSettings so the developer's
  // real configuration is neither read nor written.
  void initTestCase() { isolateTestSettings(); }
  void constructionLoadsNonEmptyTemplate();
  void expertCheckboxTogglesTemplateEditor();
  void nameTextUpdatesNameRealLine();
  void emailTextUpdatesNameEmailLine();
  void matchingPassphrasesEnableButtonBox();
  void mismatchedPassphrasesDisableButtonBox();
  void emptyPassphrasesEnableButtonBox();
  void secondPassphraseChangeTriggersStateUpdate();
  void clearingFirstPassphraseDisablesButtonBox();
  void nameAndEmailBothUpdateTemplate();
  void passphraseNeverWrittenToTemplate();
  void templateEditorHiddenUnlessExpert();
  void applyPassphraseReplacesNoProtection();
  void applyPassphraseEmptyKeepsNoProtection();
  void applyPassphraseReplacesExistingPassphraseLine();
  void applyPassphraseEmptyRestoresNoProtection();
  void applyPassphraseInsertsBeforeCommitWhenAbsent();
  void applyPassphraseCopiesSpecialCharactersVerbatim();
  void applyPassphraseCollapsesDuplicateProtectionLines();
  void applyPassphraseMatchesNoProtectionLikeGpg();
  void applyPassphraseMatchesPassphraseKeywordLikeGpg();
  void applyPassphraseMatchesCommitLikeGpg();
};

/**
 * @brief The default GPG batch template gets loaded into the editor on
 *        construction, regardless of whether ed25519 is supported.
 */
void tst_keygendialog::constructionLoadsNonEmptyTemplate() {
  KeygenDialog dialog(nullptr);
  auto *editor =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("plainTextEdit"));
  QVERIFY2(editor != nullptr, "plainTextEdit widget must exist");
  const QString tpl = editor->toPlainText();
  QVERIFY2(!tpl.isEmpty(), "default key template must be loaded");
  // Both the ed25519 and RSA fallback templates contain Name-Real and
  // Name-Email placeholders we rely on in the other tests.
  QVERIFY2(tpl.contains(QStringLiteral("Name-Real:")),
           "template must contain a Name-Real: line");
  QVERIFY2(tpl.contains(QStringLiteral("Name-Email:")),
           "template must contain a Name-Email: line");
}

/**
 * @brief Toggling the "Expert" checkbox enables/disables direct editing of
 *        the GPG batch template.
 */
void tst_keygendialog::expertCheckboxTogglesTemplateEditor() {
  KeygenDialog dialog(nullptr);
  auto *checkBox = dialog.findChild<QCheckBox *>(QStringLiteral("checkBox"));
  auto *editor =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("plainTextEdit"));
  QVERIFY2(checkBox != nullptr, "checkBox widget must exist");
  QVERIFY2(editor != nullptr, "plainTextEdit widget must exist");

  // Default state: checkbox unchecked, editor read-only / disabled.
  checkBox->setChecked(false);
  QVERIFY2(editor->isReadOnly(), "editor should start read-only");
  QVERIFY2(!editor->isEnabled(), "editor should start disabled");

  checkBox->setChecked(true);
  QVERIFY2(!editor->isReadOnly(), "expert mode should drop read-only");
  QVERIFY2(editor->isEnabled(), "expert mode should enable editor");

  checkBox->setChecked(false);
  QVERIFY2(editor->isReadOnly(), "unchecking expert restores read-only");
  QVERIFY2(!editor->isEnabled(), "unchecking expert disables editor");
}

/**
 * @brief Typing in the Name field replaces the Name-Real: line in the
 *        template.
 */
void tst_keygendialog::nameTextUpdatesNameRealLine() {
  KeygenDialog dialog(nullptr);
  auto *nameEdit = dialog.findChild<QLineEdit *>(QStringLiteral("name"));
  auto *editor =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("plainTextEdit"));
  QVERIFY2(nameEdit != nullptr, "name widget must exist");
  QVERIFY2(editor != nullptr, "plainTextEdit widget must exist");

  // The slot fires on textChanged(); setText() is enough to trigger it.
  nameEdit->setText(QStringLiteral("QtPass Tester"));
  QVERIFY2(editor->toPlainText().contains(
               QStringLiteral("Name-Real: QtPass Tester")),
           "template should reflect typed name");
}

/**
 * @brief Typing in the Email field replaces the Name-Email: line.
 */
void tst_keygendialog::emailTextUpdatesNameEmailLine() {
  KeygenDialog dialog(nullptr);
  auto *emailEdit = dialog.findChild<QLineEdit *>(QStringLiteral("email"));
  auto *editor =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("plainTextEdit"));
  QVERIFY2(emailEdit != nullptr, "email widget must exist");
  QVERIFY2(editor != nullptr, "plainTextEdit widget must exist");

  emailEdit->setText(QStringLiteral("tester@qtpass.example"));
  QVERIFY2(editor->toPlainText().contains(
               QStringLiteral("Name-Email: tester@qtpass.example")),
           "template should reflect typed email");
}

/**
 * @brief Matching passphrases in both passphrase fields enable the
 *        DialogButtonBox so OK can be clicked.
 */
void tst_keygendialog::matchingPassphrasesEnableButtonBox() {
  KeygenDialog dialog(nullptr);
  auto *pp1 = dialog.findChild<QLineEdit *>(QStringLiteral("passphrase1"));
  auto *pp2 = dialog.findChild<QLineEdit *>(QStringLiteral("passphrase2"));
  auto *buttonBox =
      dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
  QVERIFY2(pp1 != nullptr, "passphrase1 widget must exist");
  QVERIFY2(pp2 != nullptr, "passphrase2 widget must exist");
  QVERIFY2(buttonBox != nullptr, "buttonBox widget must exist");

  pp1->setText(QStringLiteral("testkey123"));
  pp2->setText(QStringLiteral("testkey123"));
  QVERIFY2(buttonBox->isEnabled(), "matching passphrases enable OK");
}

/**
 * @brief Mismatched passphrases disable the DialogButtonBox.
 */
void tst_keygendialog::mismatchedPassphrasesDisableButtonBox() {
  KeygenDialog dialog(nullptr);
  auto *pp1 = dialog.findChild<QLineEdit *>(QStringLiteral("passphrase1"));
  auto *pp2 = dialog.findChild<QLineEdit *>(QStringLiteral("passphrase2"));
  auto *buttonBox =
      dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
  QVERIFY2(pp1 != nullptr, "passphrase1 widget must exist");
  QVERIFY2(pp2 != nullptr, "passphrase2 widget must exist");
  QVERIFY2(buttonBox != nullptr, "buttonBox widget must exist");

  pp1->setText(QStringLiteral("testkey123"));
  pp2->setText(QStringLiteral("testkey456"));
  QVERIFY2(!buttonBox->isEnabled(), "mismatched passphrases disable OK");
}

void tst_keygendialog::emptyPassphrasesEnableButtonBox() {
  KeygenDialog dialog(nullptr);
  auto *pp1 = dialog.findChild<QLineEdit *>(QStringLiteral("passphrase1"));
  auto *pp2 = dialog.findChild<QLineEdit *>(QStringLiteral("passphrase2"));
  auto *buttonBox =
      dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
  QVERIFY2(pp1 != nullptr, "passphrase1 widget must exist");
  QVERIFY2(pp2 != nullptr, "passphrase2 widget must exist");
  QVERIFY2(buttonBox != nullptr, "buttonBox widget must exist");

  // Set to non-empty first to ensure signals fire when cleared.
  pp1->setText(QStringLiteral("testkey123"));
  pp2->setText(QStringLiteral("testkey123"));
  pp1->setText(QString());
  pp2->setText(QString());
  QVERIFY2(
      buttonBox->isEnabled(),
      "both empty passphrases should enable buttonBox (no-protection mode)");
}

void tst_keygendialog::secondPassphraseChangeTriggersStateUpdate() {
  KeygenDialog dialog(nullptr);
  auto *pp1 = dialog.findChild<QLineEdit *>(QStringLiteral("passphrase1"));
  auto *pp2 = dialog.findChild<QLineEdit *>(QStringLiteral("passphrase2"));
  auto *buttonBox =
      dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
  QVERIFY2(pp1 != nullptr, "passphrase1 widget must exist");
  QVERIFY2(pp2 != nullptr, "passphrase2 widget must exist");
  QVERIFY2(buttonBox != nullptr, "buttonBox widget must exist");

  pp1->setText(QStringLiteral("testkey123"));
  pp2->setText(QStringLiteral("testkey123"));
  QVERIFY2(buttonBox->isEnabled(),
           "matching passphrases should enable buttonBox");
  pp2->setText(QStringLiteral("testkey456"));
  QVERIFY2(!buttonBox->isEnabled(),
           "changing pp2 to a mismatch should disable buttonBox");
}

void tst_keygendialog::clearingFirstPassphraseDisablesButtonBox() {
  KeygenDialog dialog(nullptr);
  auto *pp1 = dialog.findChild<QLineEdit *>(QStringLiteral("passphrase1"));
  auto *pp2 = dialog.findChild<QLineEdit *>(QStringLiteral("passphrase2"));
  auto *buttonBox =
      dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
  QVERIFY2(pp1 != nullptr, "passphrase1 widget must exist");
  QVERIFY2(pp2 != nullptr, "passphrase2 widget must exist");
  QVERIFY2(buttonBox != nullptr, "buttonBox widget must exist");

  pp1->setText(QStringLiteral("testkey123"));
  pp2->setText(QStringLiteral("testkey123"));
  QVERIFY2(buttonBox->isEnabled(),
           "matching passphrases should enable buttonBox");
  pp1->setText(QString());
  QVERIFY2(!buttonBox->isEnabled(),
           "clearing pp1 while pp2 is non-empty must disable buttonBox");
}

void tst_keygendialog::nameAndEmailBothUpdateTemplate() {
  KeygenDialog dialog(nullptr);
  auto *nameEdit = dialog.findChild<QLineEdit *>(QStringLiteral("name"));
  auto *emailEdit = dialog.findChild<QLineEdit *>(QStringLiteral("email"));
  auto *editor =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("plainTextEdit"));
  QVERIFY2(nameEdit != nullptr, "name widget must exist");
  QVERIFY2(emailEdit != nullptr, "email widget must exist");
  QVERIFY2(editor != nullptr, "plainTextEdit widget must exist");

  nameEdit->setText(QStringLiteral("Test User"));
  emailEdit->setText(QStringLiteral("user@test.example"));
  const QString tpl = editor->toPlainText();
  QVERIFY2(tpl.contains(QStringLiteral("Name-Real: Test User")),
           "template must contain Name-Real: Test User");
  QVERIFY2(tpl.contains(QStringLiteral("Name-Email: user@test.example")),
           "template must contain Name-Email: user@test.example");
}

/**
 * @brief The masked passphrase fields must never leak into the always
 *        readable template box; the template keeps its %no-protection
 *        placeholder until done() splices the passphrase in.
 */
void tst_keygendialog::passphraseNeverWrittenToTemplate() {
  KeygenDialog dialog(nullptr);
  auto *pp1 = dialog.findChild<QLineEdit *>(QStringLiteral("passphrase1"));
  auto *pp2 = dialog.findChild<QLineEdit *>(QStringLiteral("passphrase2"));
  auto *editor =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("plainTextEdit"));
  QVERIFY2(pp1 != nullptr, "passphrase1 widget must exist");
  QVERIFY2(pp2 != nullptr, "passphrase2 widget must exist");
  QVERIFY2(editor != nullptr, "plainTextEdit widget must exist");

  const QString before = editor->toPlainText();
  QVERIFY2(before.contains(QStringLiteral("%no-protection")),
           "default template must start out unprotected");

  pp1->setText(QStringLiteral("testkey123"));
  pp2->setText(QStringLiteral("testkey123"));
  const QString after = editor->toPlainText();
  QVERIFY2(!after.contains(QStringLiteral("testkey123")),
           "passphrase must not be echoed into the template box");
  QVERIFY2(!after.contains(QStringLiteral("Passphrase:")),
           "template box must not gain a Passphrase: line");
  QCOMPARE(after, before);
}

/**
 * @brief The batch template box is hidden until Expert is checked so a
 *        non-expert never sees a misleading %no-protection placeholder.
 */
void tst_keygendialog::templateEditorHiddenUnlessExpert() {
  KeygenDialog dialog(nullptr);
  auto *checkBox = dialog.findChild<QCheckBox *>(QStringLiteral("checkBox"));
  auto *editor =
      dialog.findChild<QPlainTextEdit *>(QStringLiteral("plainTextEdit"));
  QVERIFY2(checkBox != nullptr, "checkBox widget must exist");
  QVERIFY2(editor != nullptr, "plainTextEdit widget must exist");

  QVERIFY2(!checkBox->isChecked(), "Expert should start unchecked");
  QVERIFY2(editor->isHidden(), "template box should start hidden");

  checkBox->setChecked(true);
  QVERIFY2(!editor->isHidden(), "expert mode should reveal the template box");

  checkBox->setChecked(false);
  QVERIFY2(editor->isHidden(), "unchecking expert hides the template box");
}

void tst_keygendialog::applyPassphraseReplacesNoProtection() {
  const QString batch = QStringLiteral("%echo Generating a default key\n"
                                       "Key-Type: RSA\n"
                                       "Name-Real: QtPass Tester\n"
                                       "%no-protection\n"
                                       "%commit\n"
                                       "%echo done");
  const QString result =
      KeygenDialog::applyPassphrase(batch, QStringLiteral("testkey123"));
  QCOMPARE(result, QStringLiteral("%echo Generating a default key\n"
                                  "Key-Type: RSA\n"
                                  "Name-Real: QtPass Tester\n"
                                  "Passphrase: testkey123\n"
                                  "%commit\n"
                                  "%echo done"));
}

void tst_keygendialog::applyPassphraseEmptyKeepsNoProtection() {
  const QString batch = QStringLiteral("Key-Type: RSA\n"
                                       "%no-protection\n"
                                       "%commit");
  QCOMPARE(KeygenDialog::applyPassphrase(batch, QString()), batch);
}

void tst_keygendialog::applyPassphraseReplacesExistingPassphraseLine() {
  // An expert may already have typed a Passphrase: line; the masked fields
  // win and no second line is added.
  const QString batch = QStringLiteral("Key-Type: RSA\n"
                                       "Passphrase: old-value\n"
                                       "%commit");
  QCOMPARE(KeygenDialog::applyPassphrase(batch, QStringLiteral("newkey456")),
           QStringLiteral("Key-Type: RSA\n"
                          "Passphrase: newkey456\n"
                          "%commit"));
}

void tst_keygendialog::applyPassphraseEmptyRestoresNoProtection() {
  const QString batch = QStringLiteral("Key-Type: RSA\n"
                                       "Passphrase: old-value\n"
                                       "%commit");
  QCOMPARE(KeygenDialog::applyPassphrase(batch, QString()),
           QStringLiteral("Key-Type: RSA\n"
                          "%no-protection\n"
                          "%commit"));
}

void tst_keygendialog::applyPassphraseInsertsBeforeCommitWhenAbsent() {
  const QString batch = QStringLiteral("Key-Type: RSA\n"
                                       "%commit\n"
                                       "%echo done");
  QCOMPARE(KeygenDialog::applyPassphrase(batch, QStringLiteral("testkey123")),
           QStringLiteral("Key-Type: RSA\n"
                          "Passphrase: testkey123\n"
                          "%commit\n"
                          "%echo done"));

  // No %commit at all: append, so the parameter is still part of the batch.
  QCOMPARE(KeygenDialog::applyPassphrase(QStringLiteral("Key-Type: RSA"),
                                         QStringLiteral("testkey123")),
           QStringLiteral("Key-Type: RSA\nPassphrase: testkey123"));

  // Empty passphrase and nothing to replace: leave the expert's batch alone.
  QCOMPARE(KeygenDialog::applyPassphrase(batch, QString()), batch);
}

/**
 * @brief The passphrase is spliced in as a plain string: backslashes,
 *        digits, "$" and "%" must all come through untouched.
 */
void tst_keygendialog::applyPassphraseCopiesSpecialCharactersVerbatim() {
  const QString passphrase = QStringLiteral("a\\1b\\9c\\0 $1 %s");
  const QString result = KeygenDialog::applyPassphrase(
      QStringLiteral("%no-protection\n%commit"), passphrase);
  QCOMPARE(result, QStringLiteral("Passphrase: ") + passphrase +
                       QStringLiteral("\n%commit"));
}

void tst_keygendialog::applyPassphraseCollapsesDuplicateProtectionLines() {
  const QString batch = QStringLiteral("Key-Type: RSA\n"
                                       "%no-protection\n"
                                       "Passphrase: stale\n"
                                       "%commit");
  QCOMPARE(KeygenDialog::applyPassphrase(batch, QStringLiteral("testkey123")),
           QStringLiteral("Key-Type: RSA\n"
                          "Passphrase: testkey123\n"
                          "%commit"));
}

/**
 * @brief gpg (g10/keygen.c read_parameter_file) compares control statements
 *        case-insensitively and cuts the keyword at the first whitespace, so
 *        "%No-Protection" and "%no-protection  # comment" both switch
 *        protection off. Leaving either next to a Passphrase: line yields an
 *        unprotected key; every such spelling must be replaced.
 */
void tst_keygendialog::applyPassphraseMatchesNoProtectionLikeGpg() {
  const QString expected = QStringLiteral("Key-Type: RSA\n"
                                          "Passphrase: testkey123\n"
                                          "%commit");
  QCOMPARE(KeygenDialog::applyPassphrase(QStringLiteral("Key-Type: RSA\n"
                                                        "%No-Protection\n"
                                                        "%commit"),
                                         QStringLiteral("testkey123")),
           expected);
  QCOMPARE(KeygenDialog::applyPassphrase(
               QStringLiteral("Key-Type: RSA\n"
                              "%no-protection   # expert comment\n"
                              "%commit"),
               QStringLiteral("testkey123")),
           expected);
  QCOMPARE(KeygenDialog::applyPassphrase(QStringLiteral("Key-Type: RSA\n"
                                                        "\t%NO-PROTECTION\t\n"
                                                        "%commit"),
                                         QStringLiteral("testkey123")),
           expected);

  // A different control statement that merely shares the prefix is not
  // protection and must be left alone.
  const QString unrelated = QStringLiteral("Key-Type: RSA\n"
                                           "%no-protection-something-else\n"
                                           "%commit");
  QCOMPARE(
      KeygenDialog::applyPassphrase(unrelated, QStringLiteral("testkey123")),
      QStringLiteral("Key-Type: RSA\n"
                     "%no-protection-something-else\n"
                     "Passphrase: testkey123\n"
                     "%commit"));
}

/**
 * @brief Parameter names are case-insensitive for gpg too; a lowercase
 *        "passphrase:" typed in expert mode must be collapsed, otherwise gpg
 *        sees two Passphrase parameters and aborts with "duplicate keyword".
 */
void tst_keygendialog::applyPassphraseMatchesPassphraseKeywordLikeGpg() {
  QCOMPARE(
      KeygenDialog::applyPassphrase(QStringLiteral("Key-Type: RSA\n"
                                                   "passphrase: old-value\n"
                                                   "%commit"),
                                    QStringLiteral("newkey456")),
      QStringLiteral("Key-Type: RSA\n"
                     "Passphrase: newkey456\n"
                     "%commit"));
  QCOMPARE(
      KeygenDialog::applyPassphrase(QStringLiteral("Key-Type: RSA\n"
                                                   "  PASSPHRASE: old-value\n"
                                                   "%commit"),
                                    QString()),
      QStringLiteral("Key-Type: RSA\n"
                     "%no-protection\n"
                     "%commit"));
}

/**
 * @brief "%commit " (trailing whitespace, other case, leading indent) is a
 *        commit for gpg; the Passphrase: line has to land before it, not
 *        after, or the key is generated without it.
 */
void tst_keygendialog::applyPassphraseMatchesCommitLikeGpg() {
  QCOMPARE(KeygenDialog::applyPassphrase(QStringLiteral("Key-Type: RSA\n"
                                                        "%commit \n"
                                                        "%echo done"),
                                         QStringLiteral("testkey123")),
           QStringLiteral("Key-Type: RSA\n"
                          "Passphrase: testkey123\n"
                          "%commit \n"
                          "%echo done"));
  QCOMPARE(KeygenDialog::applyPassphrase(QStringLiteral("Key-Type: RSA\n"
                                                        "  %Commit\n"
                                                        "%echo done"),
                                         QStringLiteral("testkey123")),
           QStringLiteral("Key-Type: RSA\n"
                          "Passphrase: testkey123\n"
                          "  %Commit\n"
                          "%echo done"));
}

QTEST_MAIN(tst_keygendialog)
#include "tst_keygendialog.moc"
