// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QtTest>

#include "../../../src/keygendialog.h"
#include "../../../src/pass.h"
#include "../../../src/qprogressindicator.h"
#include "../../../src/qtpasssettings.h"
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
  void emptyPassphrasesDisableButtonBoxUntilWaived();
  void noPassphraseCheckboxClearsAndDisablesTheFields();
  void freshDialogHasOkDisabled();
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
  void rejectSavesGeometry();
  void acceptedDialogRunsGenerationOnThePassBackend();
  void generationSuccessAcceptsTheDialog();
  void generationFailureReenablesTheForm();
  void generationFailureKeepsThePassphraseGate();
  void acceptingWithoutAPassphraseChoiceStartsNothing();
  void cancelWhileGeneratingDetachesFromTheBackend();
  void unrelatedProcessErrorsDoNotTouchTheDialog();
};

/**
 * @brief The default GPG batch template gets loaded into the editor on
 *        construction, regardless of whether ed25519 is supported.
 */
void tst_keygendialog::constructionLoadsNonEmptyTemplate() {
  KeygenDialog dialog(QString(), nullptr);
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
  KeygenDialog dialog(QString(), nullptr);
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
  KeygenDialog dialog(QString(), nullptr);
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
  KeygenDialog dialog(QString(), nullptr);
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
  KeygenDialog dialog(QString(), nullptr);
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
  KeygenDialog dialog(QString(), nullptr);
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

/**
 * @brief Two empty fields are not a decision: the key would be stored
 *        unprotected without anyone saying so. OK stays off until a
 *        passphrase is typed or "no passphrase" is ticked on purpose.
 */
void tst_keygendialog::emptyPassphrasesDisableButtonBoxUntilWaived() {
  KeygenDialog dialog(QString(), nullptr);
  auto *pp1 = dialog.findChild<QLineEdit *>(QStringLiteral("passphrase1"));
  auto *pp2 = dialog.findChild<QLineEdit *>(QStringLiteral("passphrase2"));
  auto *waive = dialog.findChild<QCheckBox *>(QStringLiteral("noPassphrase"));
  auto *buttonBox =
      dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
  QVERIFY2(pp1 != nullptr, "passphrase1 widget must exist");
  QVERIFY2(pp2 != nullptr, "passphrase2 widget must exist");
  QVERIFY2(waive != nullptr, "noPassphrase checkbox must exist");
  QVERIFY2(buttonBox != nullptr, "buttonBox widget must exist");
  QVERIFY(!waive->isChecked());

  // Set to non-empty first to ensure signals fire when cleared.
  pp1->setText(QStringLiteral("testkey123"));
  pp2->setText(QStringLiteral("testkey123"));
  pp1->setText(QString());
  pp2->setText(QString());
  QVERIFY2(!buttonBox->isEnabled(),
           "two empty passphrases must not enable OK by themselves");
  waive->setChecked(true);
  QVERIFY2(buttonBox->isEnabled(), "waiving the passphrase enables OK");
  waive->setChecked(false);
  QVERIFY2(!buttonBox->isEnabled(),
           "taking the waiver back asks for a passphrase again");
}

/**
 * @brief Ticking "no passphrase" empties and disables the fields, so a
 *        passphrase typed earlier cannot be sent by accident, and unticking
 *        hands them back.
 */
void tst_keygendialog::noPassphraseCheckboxClearsAndDisablesTheFields() {
  KeygenDialog dialog(QString(), nullptr);
  auto *pp1 = dialog.findChild<QLineEdit *>(QStringLiteral("passphrase1"));
  auto *pp2 = dialog.findChild<QLineEdit *>(QStringLiteral("passphrase2"));
  auto *waive = dialog.findChild<QCheckBox *>(QStringLiteral("noPassphrase"));
  auto *buttonBox =
      dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
  QVERIFY(pp1 && pp2 && waive && buttonBox);
  pp1->setText(QStringLiteral("typed"));
  pp2->setText(QStringLiteral("typed"));
  waive->setChecked(true);
  QVERIFY(pp1->text().isEmpty() && pp2->text().isEmpty());
  QVERIFY(!pp1->isEnabled() && !pp2->isEnabled());
  QVERIFY(buttonBox->isEnabled());
  waive->setChecked(false);
  QVERIFY(pp1->isEnabled() && pp2->isEnabled());
  QVERIFY(!buttonBox->isEnabled());
  pp1->setText(QStringLiteral("again"));
  pp2->setText(QStringLiteral("again"));
  QVERIFY(buttonBox->isEnabled());
}

/// A freshly opened dialog cannot be accepted as it is.
void tst_keygendialog::freshDialogHasOkDisabled() {
  KeygenDialog dialog(QString(), nullptr);
  auto *buttonBox =
      dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
  QVERIFY(buttonBox != nullptr);
  QVERIFY(!buttonBox->isEnabled());
}

void tst_keygendialog::secondPassphraseChangeTriggersStateUpdate() {
  KeygenDialog dialog(QString(), nullptr);
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
  KeygenDialog dialog(QString(), nullptr);
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
  KeygenDialog dialog(QString(), nullptr);
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
  KeygenDialog dialog(QString(), nullptr);
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
  KeygenDialog dialog(QString(), nullptr);
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

/**
 * @brief Leaving through Cancel/Escape runs QDialog::done(), not
 *        closeEvent(); the geometry must be recorded on that path too.
 */
void tst_keygendialog::rejectSavesGeometry() {
  const QString key = QStringLiteral("keygenDialog");
  QtPassSettings::setDialogGeometry(key, QByteArray());
  KeygenDialog dialog(QString(), nullptr);
  dialog.show();
  QVERIFY(QTest::qWaitForWindowExposed(&dialog));
  dialog.reject();
  QVERIFY2(!QtPassSettings::getDialogGeometry(key, QByteArray()).isEmpty(),
           "reject() must save the dialog geometry");
}

namespace {
/**
 * Pass::GenerateGPGKeys() is not virtual, so it runs for real: with no gpg
 * configured it reports "No GPG executable configured" through
 * processErrorExit() on a queued call. Success is simulated by emitting
 * finishedGenerateGPGKeys() directly; failure by letting that queued error
 * arrive.
 */
class FakePass : public Pass {
public:
  FakePass() { init(AppSettings()); }
  void GitInit() override {}
  void GitPull() override {}
  void GitPull_b() override {}
  void GitPush() override {}
  void Show(QString) override {}
  void Insert(QString, QString, bool) override {}
  void Remove(QString, bool) override {}
  void Move(const QString, const QString, const bool) override {}
  void Copy(const QString, const QString, const bool) override {}
  void Init(QString, const QList<UserInfo> &) override {}
  void Grep(QString, bool) override {}
};

/// Name, e-mail and a passphrase choice (waived): what done(Accepted) needs
/// to get as far as the backend.
void fillValidIdentity(KeygenDialog &d, bool waivePassphrase = true) {
  d.findChild<QLineEdit *>(QStringLiteral("name"))
      ->setText(QStringLiteral("Alice Example"));
  d.findChild<QLineEdit *>(QStringLiteral("email"))
      ->setText(QStringLiteral("alice@example.org"));
  d.findChild<QCheckBox *>(QStringLiteral("noPassphrase"))
      ->setChecked(waivePassphrase);
}
} // namespace

/**
 * @brief OK hands the batch to Pass directly (no ConfigDialog/MainWindow/
 *        QtPass relay) and locks the form while waiting.
 */
void tst_keygendialog::acceptedDialogRunsGenerationOnThePassBackend() {
  FakePass pass;
  KeygenDialog d(QString(), &pass);
  fillValidIdentity(d);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  QMetaObject::invokeMethod(&d, "done", Qt::DirectConnection,
                            Q_ARG(int, QDialog::Accepted));
  QVERIFY2(d.isVisible(), "the dialog waits for the backend");
  QVERIFY2(!d.findChild<QLineEdit *>(QStringLiteral("name"))->isEnabled(),
           "the form is locked while generating");
}

/**
 * @brief The old relay closed the dialog through close() -> reject(), so a
 *        successful keygen made checkSecretKeys()'s exec() return Rejected
 *        and the wizard bailed out. Success now accepts.
 */
void tst_keygendialog::generationSuccessAcceptsTheDialog() {
  FakePass pass;
  KeygenDialog d(QString(), &pass);
  fillValidIdentity(d);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  QMetaObject::invokeMethod(&d, "done", Qt::DirectConnection,
                            Q_ARG(int, QDialog::Accepted));
  emit pass.finishedGenerateGPGKeys(QString(), QString());
  QCOMPARE(d.result(), static_cast<int>(QDialog::Accepted));
  QVERIFY(!d.isVisible());
  // The queued "no gpg" error from GenerateGPGKeys() arrives now; the
  // dialog must have let go of the backend and stay accepted.
  QTest::qWait(50);
  QCOMPARE(d.result(), static_cast<int>(QDialog::Accepted));
}

void tst_keygendialog::generationFailureReenablesTheForm() {
  FakePass pass;
  KeygenDialog d(QString(), &pass);
  fillValidIdentity(d);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  QMetaObject::invokeMethod(&d, "done", Qt::DirectConnection,
                            Q_ARG(int, QDialog::Accepted));
  auto *name = d.findChild<QLineEdit *>(QStringLiteral("name"));
  QTRY_VERIFY_WITH_TIMEOUT(name->isEnabled(), 3000);
  QVERIFY2(d.isVisible(), "a failed generation keeps the dialog open");
  const QString label = d.findChild<QLabel *>(QStringLiteral("label"))->text();
  QVERIFY2(label.contains(QStringLiteral("No GPG executable")),
           qPrintable("the backend's reason must be shown: " + label));

  // A retry must show the spinner again, not the stopped one from before.
  auto *spinner = d.findChild<QProgressIndicator *>();
  QVERIFY(spinner != nullptr);
  QVERIFY(!spinner->isVisible());
  QMetaObject::invokeMethod(&d, "done", Qt::DirectConnection,
                            Q_ARG(int, QDialog::Accepted));
  QVERIFY2(spinner->isVisible() && spinner->isAnimated(),
           "the retry must restart the progress indicator");
}

/**
 * @brief done(Accepted) without a passphrase choice starts no generation and
 *        keeps the dialog open, however it was reached; with the waiver
 *        ticked it goes ahead.
 */
void tst_keygendialog::acceptingWithoutAPassphraseChoiceStartsNothing() {
  FakePass pass;
  KeygenDialog d(QString(), &pass);
  fillValidIdentity(d, false);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  auto *spinner = d.findChild<QProgressIndicator *>();
  QMetaObject::invokeMethod(&d, "done", Qt::DirectConnection,
                            Q_ARG(int, QDialog::Accepted));
  QVERIFY2(d.isVisible(), "the dialog stays open");
  QVERIFY2(spinner == nullptr || !spinner->isVisible(),
           "no generation may have started");
  QVERIFY(d.findChild<QLineEdit *>(QStringLiteral("name"))->isEnabled());
  auto *waive = d.findChild<QCheckBox *>(QStringLiteral("noPassphrase"));
  QVERIFY(waive != nullptr);
  waive->setChecked(true);
  QMetaObject::invokeMethod(&d, "done", Qt::DirectConnection,
                            Q_ARG(int, QDialog::Accepted));
  spinner = d.findChild<QProgressIndicator *>();
  QVERIFY2(spinner != nullptr && spinner->isVisible(),
           "with the waiver, generation is attempted");
}

/**
 * @brief After a failed attempt OK follows the passphrase rule again rather
 *        than coming back enabled: with the fields empty and the waiver
 *        unticked it stays off, with a passphrase typed it is on.
 */
void tst_keygendialog::generationFailureKeepsThePassphraseGate() {
  FakePass pass;
  KeygenDialog d(QString(), &pass);
  fillValidIdentity(d, false);
  auto *pp1 = d.findChild<QLineEdit *>(QStringLiteral("passphrase1"));
  auto *pp2 = d.findChild<QLineEdit *>(QStringLiteral("passphrase2"));
  auto *buttonBox =
      d.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
  QVERIFY(pp1 && pp2 && buttonBox);
  QVERIFY(!buttonBox->isEnabled());
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  // Drive done() directly, as a stray Enter or a script could.
  QMetaObject::invokeMethod(&d, "done", Qt::DirectConnection,
                            Q_ARG(int, QDialog::Accepted));
  auto *name = d.findChild<QLineEdit *>(QStringLiteral("name"));
  QTRY_VERIFY_WITH_TIMEOUT(name->isEnabled(), 3000);
  QVERIFY2(!buttonBox->isEnabled(),
           "a failed attempt must not hand out OK without a passphrase choice");
  pp1->setText(QStringLiteral("chosen"));
  pp2->setText(QStringLiteral("chosen"));
  QVERIFY(buttonBox->isEnabled());
  QMetaObject::invokeMethod(&d, "done", Qt::DirectConnection,
                            Q_ARG(int, QDialog::Accepted));
  QTRY_VERIFY_WITH_TIMEOUT(name->isEnabled(), 3000);
  QVERIFY2(buttonBox->isEnabled(),
           "with a passphrase standing, OK comes back after a failure");
}

void tst_keygendialog::cancelWhileGeneratingDetachesFromTheBackend() {
  FakePass pass;
  KeygenDialog d(QString(), &pass);
  fillValidIdentity(d);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  QMetaObject::invokeMethod(&d, "done", Qt::DirectConnection,
                            Q_ARG(int, QDialog::Accepted));
  auto *name = d.findChild<QLineEdit *>(QStringLiteral("name"));
  QVERIFY(!name->isEnabled());
  d.reject();
  QCOMPARE(d.result(), static_cast<int>(QDialog::Rejected));

  // The dialog is still alive; late backend signals must change nothing:
  // not the result, not the locked form.
  emit pass.finishedGenerateGPGKeys(QString(), QString());
  QCOMPARE(d.result(), static_cast<int>(QDialog::Rejected));
  emit pass.generateGPGKeysFailed(QStringLiteral("late"));
  QCOMPARE(d.result(), static_cast<int>(QDialog::Rejected));
  QVERIFY2(!name->isEnabled(), "a late failure must not restore the form");
  QTest::qWait(50); // the queued "no gpg" error arrives too
  QCOMPARE(d.result(), static_cast<int>(QDialog::Rejected));
}

/**
 * @brief Only the keygen's own failure signal reaches the dialog; another
 *        command failing on the same backend meanwhile must not take it
 *        down while gpg is still generating.
 */
void tst_keygendialog::unrelatedProcessErrorsDoNotTouchTheDialog() {
  FakePass pass;
  KeygenDialog d(QString(), &pass);
  fillValidIdentity(d);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  QMetaObject::invokeMethod(&d, "done", Qt::DirectConnection,
                            Q_ARG(int, QDialog::Accepted));
  auto *name = d.findChild<QLineEdit *>(QStringLiteral("name"));
  emit pass.processErrorExit(1, QStringLiteral("git push failed"));
  QVERIFY2(!name->isEnabled(), "an unrelated error must not unlock the form");
  QVERIFY(d.isVisible());
  emit pass.finishedGenerateGPGKeys(QString(), QString());
  QCOMPARE(d.result(), static_cast<int>(QDialog::Accepted));
}

QTEST_MAIN(tst_keygendialog)
#include "tst_keygendialog.moc"
