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
  QVERIFY2(buttonBox->isEnabled(), "matching passphrases should enable buttonBox");
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

QTEST_MAIN(tst_keygendialog)
#include "tst_keygendialog.moc"
