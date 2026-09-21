// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

#include "../../../src/appsettings.h"
#include "../../../src/deselectabletreeview.h"
#include "../../../src/passworddialog.h"
#include "../../../src/qprogressindicator.h"
#include "../../../src/qpushbuttonasqrcode.h"
#include "../../../src/qpushbuttonshowpassword.h"
#include "../../../src/qpushbuttonwithclipboard.h"
#include "../../../src/qtpass.h"
#include "../../../src/qtpasssettings.h"
#include "../testsettings.h"
#include "passwordconfiguration.h"

class tst_ui : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void contentRemainsSame();
  void keyValueLinesFollowTemplateSettings();
  void emptyPassword();
  void multilineRemainingData();
  void cleanupTestCase();
  void passwordDialogBasic();
  void passwordDialogWithTemplate();
  void passwordDialogShowCheckboxTogglesEchoMode();
  void passwordDialogSetLengthUpdatesSpinBox();
  void passwordDialogSetPasswordCharTemplateUpdatesCombo();
  void passwordDialogUsePwgenDisablesTemplateSelector();
  void passwordDialogSetPassPopulatesField();
  void passwordDialogSetAvailableTemplatesAppliesDefault();
  void passwordDialogCycleTemplateAdvancesToNext();
  void passwordDialogCycleTemplateWrapsToFirst();
  void passwordDialogCycleTemplateNoOpWhenEmpty();
  void qrCodePopupDeletesOnClose();
  void qrCodePopupHasDeleteOnCloseAttribute();
  void createQRCodePopupSetsDeleteOnClose();
  void dialogWithoutDeleteOnCloseDoesNotAutoDelete();

  // QPushButtonWithClipboard tests
  void clipboardButtonDefaultText();
  void clipboardButtonConstructorText();
  void clipboardButtonGetSetText();
  void clipboardButtonSetEmptyText();
  void clipboardButtonSetAndGetRoundtrip();
  void clipboardButtonClickEmitsSignal();
  void clipboardButtonClickSignalCarriesText();
  void clipboardButtonClickAfterSetTextCarriesNewText();

  // QPushButtonAsQRCode tests
  void qrCodeButtonDefaultText();
  void qrCodeButtonConstructorText();
  void qrCodeButtonGetSetText();
  void qrCodeButtonSetEmptyText();
  void qrCodeButtonSetAndGetRoundtrip();
  void qrCodeButtonClickEmitsSignal();
  void qrCodeButtonClickSignalCarriesText();
  void qrCodeButtonClickAfterSetTextCarriesNewText();

  // QPushButtonShowPassword tests
  void showPasswordButtonInitialEchoMode();
  void showPasswordButtonClickTogglesEchoMode();
  void showPasswordButtonDoubleClickRestoresEchoMode();

  // QProgressIndicator tests
  void progressIndicatorDefaultNotAnimated();
  void progressIndicatorDefaultDelay();
  void progressIndicatorDefaultNotDisplayedWhenStopped();
  void progressIndicatorDefaultColor();
  void progressIndicatorStartAnimation();
  void progressIndicatorStopAnimation();
  void progressIndicatorStartStopCycle();
  void progressIndicatorSetAnimationDelay();
  void progressIndicatorSetDisplayedWhenStopped();
  void progressIndicatorSetColor();
  void progressIndicatorSizeHint();
  void progressIndicatorHeightForWidth();
  void progressIndicatorStopWhenNotRunningIsHarmless();
  void progressIndicatorStartTwiceDoesNotDuplicate();
  void progressIndicatorPaintsNothingWhileStoppedByDefault();
  void progressIndicatorPaintsWhenStoppedIfDisplayedWhenStopped();
  void progressIndicatorPaintsCapsulesWhileAnimated();
  void progressIndicatorPaintsInTheChosenColour();
  void progressIndicatorPaintsInThePaletteColourByDefault();
  void progressIndicatorSetAnimationDelayWhileRunningKeepsTicking();

  // QtPass::showTextAsQRCode with a stand-in qrencode
  void showTextAsQRCodeFeedsTextToQrencodeAndShowsItsImage();
  void showTextAsQRCodeReportsQrencodeStderr();
  void showTextAsQRCodeReportsExitCodeWhenStderrIsEmpty();
  void showTextAsQRCodeReportsACrash();

  // DeselectableTreeView tests
  void deselectableTreeViewConstruction();
  void deselectableTreeViewHasEmptyClickedSignal();
  void deselectableTreeViewClickDoesNotBlock();
  void deselectableTreeViewClearsSelectionAfterDoubleClickInterval();
  void deselectableTreeViewDoubleClickKeepsSelection();
  void deselectableTreeViewSecondClickCancelsPendingDeselect();
};

/**
 * @brief tst_ui::contentRemainsSame test that content set with
 * PasswordDialog::setPassword is repeated when calling
 * PasswordDialog::getPassword.
 */
void tst_ui::contentRemainsSame() {
  QScopedPointer<PasswordDialog> d(
      new PasswordDialog(PasswordConfiguration{}, nullptr));
  d->setTemplate("", false);
  QString input = "pw\n";
  d->setPass(input);
  QCOMPARE(d->getPassword(), input);

  d.reset(new PasswordDialog(PasswordConfiguration{}, nullptr));
  input = "pw\nname: value\n";
  d->setPass(input);
  QCOMPARE(d->getPassword(), input);

  d.reset(new PasswordDialog(PasswordConfiguration{}, nullptr));
  d->setTemplate("name", false);
  d->setPass(input);
  QCOMPARE(d->getPassword(), input);

  d.reset(new PasswordDialog(PasswordConfiguration{}, nullptr));
  d->setTemplate("name", true);
  d->setPass(input);
  QCOMPARE(d->getPassword(), input);

  d.reset(new PasswordDialog(PasswordConfiguration{}, nullptr));
  d->setTemplate("", false);
  d->templateAll(true);
  d->setPass(input);
  QCOMPARE(d->getPassword(), input);

  d.reset(new PasswordDialog(PasswordConfiguration{}, nullptr));
  d->setTemplate("", true);
  d->templateAll(true);
  d->setPass(input);
  QCOMPARE(d->getPassword(), input);

  d.reset(new PasswordDialog(PasswordConfiguration{}, nullptr));
  d->setTemplate("name", true);
  d->templateAll(true);
  d->setPass(input);
  QCOMPARE(d->getPassword(), input);
}

/**
 * @brief Which `key: value` lines become fields follows the settings, not a
 * hard-coded "all". Regression for #1766: with templates off, every such
 * line was turned into a label-locked field, so the key could no longer be
 * edited as text.
 */
void tst_ui::keyValueLinesFollowTemplateSettings() {
  const QString input = "pw\nDB: db-value\nlogin: user\nfree text\n";
  const auto lineEditNamed = [](PasswordDialog *d, const char *name) {
    return d->findChild<QLineEdit *>(QLatin1String(name));
  };
  const auto body = [](PasswordDialog *d) {
    return d->findChild<QPlainTextEdit *>(QStringLiteral("plainTextEdit"))
        ->toPlainText();
  };
  // Templated modes write the template's fields first, so the line order can
  // legitimately change; the content must not.
  const auto sameLines = [](const QString &a, const QString &b) {
    QStringList la = a.split('\n'), lb = b.split('\n');
    la.sort();
    lb.sort();
    return la == lb;
  };

  // Templates off: nothing is split out, the whole rest is editable text.
  QScopedPointer<PasswordDialog> d(
      new PasswordDialog(PasswordConfiguration{}, nullptr));
  d->setTemplate("login", false);
  d->templateAll(false);
  d->setPass(input);
  QVERIFY2(lineEditNamed(d.data(), "DB") == nullptr,
           "templates off: DB must not become a field");
  QVERIFY2(lineEditNamed(d.data(), "login") == nullptr,
           "templates off: the template's own field must not appear either");
  QVERIFY(body(d.data()).contains(QStringLiteral("DB: db-value")));
  QVERIFY(body(d.data()).contains(QStringLiteral("login: user")));
  QCOMPARE(d->getPassword(), input);

  // Templates on, all-fields off: only the template's field is a widget.
  d.reset(new PasswordDialog(PasswordConfiguration{}, nullptr));
  d->setTemplate("login", true);
  d->templateAll(false);
  d->setPass(input);
  QVERIFY(lineEditNamed(d.data(), "login") != nullptr);
  QCOMPARE(lineEditNamed(d.data(), "login")->text(), QStringLiteral("user"));
  QVERIFY2(lineEditNamed(d.data(), "DB") == nullptr,
           "all-fields off: a line outside the template stays text");
  QVERIFY(body(d.data()).contains(QStringLiteral("DB: db-value")));
  QVERIFY(sameLines(d->getPassword(), input));

  // Templates on, all-fields on: every key: value line is a widget.
  d.reset(new PasswordDialog(PasswordConfiguration{}, nullptr));
  d->setTemplate("login", true);
  d->templateAll(true);
  d->setPass(input);
  QVERIFY(lineEditNamed(d.data(), "login") != nullptr);
  QVERIFY(lineEditNamed(d.data(), "DB") != nullptr);
  QCOMPARE(lineEditNamed(d.data(), "DB")->text(), QStringLiteral("db-value"));
  QVERIFY(!body(d.data()).contains(QStringLiteral("DB:")));
  QVERIFY(body(d.data()).contains(QStringLiteral("free text")));
  QVERIFY(sameLines(d->getPassword(), input));
}

// PasswordDialog's constructor reaches QtPassSettings::getPass(), which loads
// the real settings and can mkpath() the configured store, so redirect
// QSettings before any test runs.
void tst_ui::initTestCase() { isolateTestSettings(); }

void tst_ui::cleanupTestCase() {}

void tst_ui::emptyPassword() {
  QScopedPointer<PasswordDialog> d(
      new PasswordDialog(PasswordConfiguration{}, nullptr));
  d->setTemplate("", false);
  d->setPass("");
  QString result = d->getPassword();
  QVERIFY(result.isEmpty() || result == "\n");
}

void tst_ui::multilineRemainingData() {
  QScopedPointer<PasswordDialog> d(
      new PasswordDialog(PasswordConfiguration{}, nullptr));
  d->setTemplate("", false);
  QString input = "secret\nline1\nline2\nline3\n";
  d->setPass(input);
  QString result = d->getPassword();
  QStringList lines = result.split("\n");
  QVERIFY(lines.length() >= 4);
  lines.removeFirst();
  QString remaining = lines.join("\n") + (result.endsWith("\n") ? "\n" : "");
  QVERIFY(remaining.contains("line1"));
  QVERIFY(remaining.contains("line2"));
  QVERIFY(remaining.contains("line3"));
}

void tst_ui::passwordDialogBasic() {
  PasswordConfiguration config;
  config.length = 20;
  QScopedPointer<PasswordDialog> d(new PasswordDialog(config, nullptr));
  d->setTemplate("", false);
  d->setPass("testpassword");
  QString result = d->getPassword();
  QVERIFY(result.contains("testpassword"));
}

void tst_ui::passwordDialogWithTemplate() {
  PasswordConfiguration config;
  QScopedPointer<PasswordDialog> d(new PasswordDialog(config, nullptr));
  d->setTemplate("username", false);
  d->setPass("mypassword\nusername: testuser");
  QString result = d->getPassword();
  QVERIFY(result.contains("mypassword"));
}

/**
 * @brief The "Show password" checkbox flips the password line-edit echo
 *        mode between Password (hidden) and Normal (visible).
 */
void tst_ui::passwordDialogShowCheckboxTogglesEchoMode() {
  PasswordConfiguration config;
  QScopedPointer<PasswordDialog> d(new PasswordDialog(config, nullptr));
  auto *line = d->findChild<QLineEdit *>(QStringLiteral("lineEditPassword"));
  auto *checkBox = d->findChild<QCheckBox *>(QStringLiteral("checkBoxShow"));
  QVERIFY2(line != nullptr, "lineEditPassword widget must exist");
  QVERIFY2(checkBox != nullptr, "checkBoxShow widget must exist");

  QCOMPARE(line->echoMode(), QLineEdit::Password);
  checkBox->setChecked(true);
  QCOMPARE(line->echoMode(), QLineEdit::Normal);
  checkBox->setChecked(false);
  QCOMPARE(line->echoMode(), QLineEdit::Password);
}

/**
 * @brief setLength writes through to the password-length spinbox.
 */
void tst_ui::passwordDialogSetLengthUpdatesSpinBox() {
  PasswordConfiguration config;
  QScopedPointer<PasswordDialog> d(new PasswordDialog(config, nullptr));
  auto *spin = d->findChild<QSpinBox *>(QStringLiteral("spinBox_pwdLength"));
  QVERIFY2(spin != nullptr, "spinBox_pwdLength widget must exist");
  d->setLength(32);
  QCOMPARE(spin->value(), 32);
  d->setLength(8);
  QCOMPARE(spin->value(), 8);
}

/**
 * @brief setPasswordCharTemplate selects the matching index in the
 *        character-template combo box.
 */
void tst_ui::passwordDialogSetPasswordCharTemplateUpdatesCombo() {
  PasswordConfiguration config;
  QScopedPointer<PasswordDialog> d(new PasswordDialog(config, nullptr));
  auto *combo =
      d->findChild<QComboBox *>(QStringLiteral("passwordTemplateSwitch"));
  QVERIFY2(combo != nullptr, "passwordTemplateSwitch widget must exist");
  const int n = combo->count();
  QVERIFY2(n > 0, "the combo must come pre-populated with character sets");
  // setPasswordCharTemplate forwards the value through; values past the
  // last index are clamped/ignored by QComboBox, so confine the test to
  // valid indices.
  d->setPasswordCharTemplate(0);
  QCOMPARE(combo->currentIndex(), 0);
  if (n > 1) {
    d->setPasswordCharTemplate(1);
    QCOMPARE(combo->currentIndex(), 1);
  }
}

/**
 * @brief usePwgen(true) disables the in-app template selector + label
 *        because pwgen runs an external generator; usePwgen(false)
 *        restores them.
 */
void tst_ui::passwordDialogUsePwgenDisablesTemplateSelector() {
  PasswordConfiguration config;
  QScopedPointer<PasswordDialog> d(new PasswordDialog(config, nullptr));
  auto *combo =
      d->findChild<QComboBox *>(QStringLiteral("passwordTemplateSwitch"));
  auto *label = d->findChild<QLabel *>(QStringLiteral("label_characterset"));
  QVERIFY2(combo != nullptr, "passwordTemplateSwitch widget must exist");
  QVERIFY2(label != nullptr, "label_characterset widget must exist");

  d->usePwgen(true);
  QVERIFY2(!combo->isEnabled(), "pwgen mode must disable the combo box");
  QVERIFY2(!label->isEnabled(), "pwgen mode must disable the label");

  d->usePwgen(false);
  QVERIFY2(combo->isEnabled(), "non-pwgen restores the combo box");
  QVERIFY2(label->isEnabled(), "non-pwgen restores the label");
}

/**
 * @brief setPass() is a thin wrapper around setPassword() — the password
 *        ends up in the first line returned by getPassword.
 */
void tst_ui::passwordDialogSetPassPopulatesField() {
  PasswordConfiguration config;
  QScopedPointer<PasswordDialog> d(new PasswordDialog(config, nullptr));
  d->setPass(QStringLiteral("hunter2\n"));
  QVERIFY2(d->getPassword().startsWith(QStringLiteral("hunter2")),
           "setPass should populate the password line");
}

/**
 * @brief setAvailableTemplates applies the named default — visible by
 *        the corresponding template field widgets appearing in the form.
 */
void tst_ui::passwordDialogSetAvailableTemplatesAppliesDefault() {
  PasswordConfiguration config;
  QScopedPointer<PasswordDialog> d(new PasswordDialog(config, nullptr));
  QHash<QString, QStringList> templates;
  templates[QStringLiteral("login")] =
      QStringList{QStringLiteral("username"), QStringLiteral("url")};
  templates[QStringLiteral("creditcard")] =
      QStringList{QStringLiteral("number"), QStringLiteral("cvv")};

  d->setAvailableTemplates(templates, QStringLiteral("login"));

  // setTemplate names each line-edit after its field; we should see the
  // "login"-template fields populated.
  QVERIFY2(d->findChild<QLineEdit *>(QStringLiteral("username")) != nullptr,
           "username field must exist after login template");
  QVERIFY2(d->findChild<QLineEdit *>(QStringLiteral("url")) != nullptr,
           "url field must exist after login template");
  // creditcard fields must NOT be present.
  QVERIFY2(d->findChild<QLineEdit *>(QStringLiteral("number")) == nullptr,
           "number field belongs to the unselected creditcard template");
}

/**
 * @brief cycleTemplate() advances to the next alphabetical template.
 *        Sorted order: creditcard, login. Default = login → cycle → creditcard.
 */
void tst_ui::passwordDialogCycleTemplateAdvancesToNext() {
  PasswordConfiguration config;
  QScopedPointer<PasswordDialog> d(new PasswordDialog(config, nullptr));
  QHash<QString, QStringList> templates;
  templates[QStringLiteral("login")] = QStringList{QStringLiteral("username")};
  templates[QStringLiteral("creditcard")] = QStringList{QStringLiteral("cvv")};
  d->setAvailableTemplates(templates, QStringLiteral("creditcard"));
  QVERIFY(d->findChild<QLineEdit *>(QStringLiteral("cvv")) != nullptr);

  d->cycleTemplate();
  QVERIFY2(d->findChild<QLineEdit *>(QStringLiteral("username")) != nullptr,
           "cycling from creditcard should land on login");
  QVERIFY2(d->findChild<QLineEdit *>(QStringLiteral("cvv")) == nullptr,
           "old template fields must be cleared on cycle");
}

/**
 * @brief cycleTemplate() wraps from the last alphabetical template back
 *        to the first.
 */
void tst_ui::passwordDialogCycleTemplateWrapsToFirst() {
  PasswordConfiguration config;
  QScopedPointer<PasswordDialog> d(new PasswordDialog(config, nullptr));
  QHash<QString, QStringList> templates;
  templates[QStringLiteral("login")] = QStringList{QStringLiteral("username")};
  templates[QStringLiteral("creditcard")] = QStringList{QStringLiteral("cvv")};
  d->setAvailableTemplates(templates, QStringLiteral("login"));
  QVERIFY(d->findChild<QLineEdit *>(QStringLiteral("username")) != nullptr);

  d->cycleTemplate();
  QVERIFY2(d->findChild<QLineEdit *>(QStringLiteral("cvv")) != nullptr,
           "cycling from login (last) should wrap to creditcard");
}

/**
 * @brief cycleTemplate() is a no-op when no templates were registered.
 */
void tst_ui::passwordDialogCycleTemplateNoOpWhenEmpty() {
  PasswordConfiguration config;
  QScopedPointer<PasswordDialog> d(new PasswordDialog(config, nullptr));
  // The early-return path in cycleTemplate() must not crash or apply a
  // template (template fields would carry a known objectName like
  // "username" or "cvv"; their absence is the signal that nothing
  // happened).
  d->cycleTemplate();
  d->cycleTemplate();
  QVERIFY2(d->findChild<QLineEdit *>(QStringLiteral("username")) == nullptr,
           "no template was registered, so no template fields should appear");
  QVERIFY2(d->findChild<QLineEdit *>(QStringLiteral("cvv")) == nullptr,
           "no template was registered, so no template fields should appear");
}

/**
 * @brief tst_ui::qrCodePopupDeletesOnClose verifies that a QDialog with
 * Qt::WA_DeleteOnClose is automatically destroyed when closed.  This tests
 * the memory leak fix in showTextAsQRCode where the popup dialog now has
 * this attribute set.
 */
void tst_ui::qrCodePopupDeletesOnClose() {
  QPointer<QDialog> popup(
      new QDialog(nullptr, Qt::Popup | Qt::FramelessWindowHint));
  popup->setAttribute(Qt::WA_DeleteOnClose);
  QVERIFY(!popup.isNull());

  popup->close();
  QCoreApplication::processEvents();
  QTRY_VERIFY(popup.isNull());
}

/**
 * @brief tst_ui::qrCodePopupHasDeleteOnCloseAttribute verifies that setting
 * Qt::WA_DeleteOnClose on a QDialog causes testAttribute() to return true,
 * matching the behaviour added in showTextAsQRCode (memory-leak fix).
 */
void tst_ui::qrCodePopupHasDeleteOnCloseAttribute() {
  QDialog *popup = new QDialog(nullptr, Qt::Popup | Qt::FramelessWindowHint);
  popup->setAttribute(Qt::WA_DeleteOnClose);
  QVERIFY(popup->testAttribute(Qt::WA_DeleteOnClose));
  delete popup;
}

/**
 * @brief tst_ui::createQRCodePopupSetsDeleteOnClose verifies that
 * QtPass::createQRCodePopup creates a popup with Qt::WA_DeleteOnClose set.
 * This provides codecov coverage for the memory leak fix in showTextAsQRCode.
 */
void tst_ui::createQRCodePopupSetsDeleteOnClose() {
  QPixmap image;
  QDialog *popup = QtPass::createQRCodePopup(image);
  QVERIFY(popup->testAttribute(Qt::WA_DeleteOnClose));
  delete popup;
}

/**
 * @brief tst_ui::dialogWithoutDeleteOnCloseDoesNotAutoDelete is a regression
 * contrast test.  A QDialog that does NOT have Qt::WA_DeleteOnClose set must
 * remain alive after close(), demonstrating that the attribute set in
 * showTextAsQRCode is the actual cause of the auto-deletion behaviour.
 */
void tst_ui::dialogWithoutDeleteOnCloseDoesNotAutoDelete() {
  QPointer<QDialog> popup(
      new QDialog(nullptr, Qt::Popup | Qt::FramelessWindowHint));
  // Intentionally NOT setting WA_DeleteOnClose.
  QVERIFY(!popup->testAttribute(Qt::WA_DeleteOnClose));

  popup->close();
  QCoreApplication::processEvents();

  QVERIFY2(
      !popup.isNull(),
      "QDialog without WA_DeleteOnClose must NOT be deleted after close()");
  delete popup;
}

// ---- QPushButtonWithClipboard tests ----

void tst_ui::clipboardButtonDefaultText() {
  QPushButtonWithClipboard btn;
  QCOMPARE(btn.getTextToCopy(), QString(""));
}

void tst_ui::clipboardButtonConstructorText() {
  QPushButtonWithClipboard btn("hello");
  QCOMPARE(btn.getTextToCopy(), QString("hello"));
}

void tst_ui::clipboardButtonGetSetText() {
  QPushButtonWithClipboard btn("initial");
  btn.setTextToCopy("updated");
  QCOMPARE(btn.getTextToCopy(), QString("updated"));
}

void tst_ui::clipboardButtonSetEmptyText() {
  QPushButtonWithClipboard btn("nonempty");
  btn.setTextToCopy("");
  QCOMPARE(btn.getTextToCopy(), QString(""));
}

void tst_ui::clipboardButtonSetAndGetRoundtrip() {
  QPushButtonWithClipboard btn;
  QString text = "password123!@#";
  btn.setTextToCopy(text);
  QCOMPARE(btn.getTextToCopy(), text);
}

void tst_ui::clipboardButtonClickEmitsSignal() {
  QPushButtonWithClipboard btn("test");
  QSignalSpy spy(&btn, &QPushButtonWithClipboard::clicked);
  btn.click();
  QCOMPARE(spy.count(), 1);
}

void tst_ui::clipboardButtonClickSignalCarriesText() {
  QPushButtonWithClipboard btn("mytext");
  QSignalSpy spy(&btn, &QPushButtonWithClipboard::clicked);
  btn.click();
  QCOMPARE(spy.count(), 1);
  QList<QVariant> args = spy.takeFirst();
  QCOMPARE(args.at(0).toString(), QString("mytext"));
}

void tst_ui::clipboardButtonClickAfterSetTextCarriesNewText() {
  QPushButtonWithClipboard btn("original");
  btn.setTextToCopy("changed");
  QSignalSpy spy(&btn, &QPushButtonWithClipboard::clicked);
  btn.click();
  QCOMPARE(spy.count(), 1);
  QList<QVariant> args = spy.takeFirst();
  QCOMPARE(args.at(0).toString(), QString("changed"));
}

// ---- QPushButtonAsQRCode tests ----

void tst_ui::qrCodeButtonDefaultText() {
  QPushButtonAsQRCode btn;
  QCOMPARE(btn.getTextToCopy(), QString(""));
}

void tst_ui::qrCodeButtonConstructorText() {
  QPushButtonAsQRCode btn("qrdata");
  QCOMPARE(btn.getTextToCopy(), QString("qrdata"));
}

void tst_ui::qrCodeButtonGetSetText() {
  QPushButtonAsQRCode btn("first");
  btn.setTextToCopy("second");
  QCOMPARE(btn.getTextToCopy(), QString("second"));
}

void tst_ui::qrCodeButtonSetEmptyText() {
  QPushButtonAsQRCode btn("nonempty");
  btn.setTextToCopy("");
  QCOMPARE(btn.getTextToCopy(), QString(""));
}

void tst_ui::qrCodeButtonSetAndGetRoundtrip() {
  QPushButtonAsQRCode btn;
  QString text = "otpauth://totp/Example?secret=JBSWY3DPEHPK3PXP";
  btn.setTextToCopy(text);
  QCOMPARE(btn.getTextToCopy(), text);
}

void tst_ui::qrCodeButtonClickEmitsSignal() {
  QPushButtonAsQRCode btn("somedata");
  QSignalSpy spy(&btn, &QPushButtonAsQRCode::clicked);
  btn.click();
  QCOMPARE(spy.count(), 1);
}

void tst_ui::qrCodeButtonClickSignalCarriesText() {
  QPushButtonAsQRCode btn("payload");
  QSignalSpy spy(&btn, &QPushButtonAsQRCode::clicked);
  btn.click();
  QCOMPARE(spy.count(), 1);
  QList<QVariant> args = spy.takeFirst();
  QCOMPARE(args.at(0).toString(), QString("payload"));
}

void tst_ui::qrCodeButtonClickAfterSetTextCarriesNewText() {
  QPushButtonAsQRCode btn("old");
  btn.setTextToCopy("new");
  QSignalSpy spy(&btn, &QPushButtonAsQRCode::clicked);
  btn.click();
  QCOMPARE(spy.count(), 1);
  QList<QVariant> args = spy.takeFirst();
  QCOMPARE(args.at(0).toString(), QString("new"));
}

// ---- QPushButtonShowPassword tests ----

void tst_ui::showPasswordButtonInitialEchoMode() {
  QLineEdit line;
  line.setEchoMode(QLineEdit::Password);
  QPushButtonShowPassword btn(&line);
  // Initial state: echo mode should remain Password until button is clicked
  QCOMPARE(line.echoMode(), QLineEdit::Password);
}

void tst_ui::showPasswordButtonClickTogglesEchoMode() {
  QLineEdit line;
  line.setEchoMode(QLineEdit::Password);
  QPushButtonShowPassword btn(&line);
  // Click once: Password -> Normal
  btn.click();
  QCOMPARE(line.echoMode(), QLineEdit::Normal);
}

void tst_ui::showPasswordButtonDoubleClickRestoresEchoMode() {
  QLineEdit line;
  line.setEchoMode(QLineEdit::Password);
  QPushButtonShowPassword btn(&line);
  // Click once: Password -> Normal
  btn.click();
  QCOMPARE(line.echoMode(), QLineEdit::Normal);
  // Click again: Normal -> Password
  btn.click();
  QCOMPARE(line.echoMode(), QLineEdit::Password);
}

// ---- QProgressIndicator tests ----

void tst_ui::progressIndicatorDefaultNotAnimated() {
  QProgressIndicator indicator;
  QVERIFY(!indicator.isAnimated());
}

void tst_ui::progressIndicatorDefaultDelay() {
  QProgressIndicator indicator;
  QCOMPARE(indicator.animationDelay(), 40);
}

void tst_ui::progressIndicatorDefaultNotDisplayedWhenStopped() {
  QProgressIndicator indicator;
  QVERIFY(!indicator.isDisplayedWhenStopped());
}

void tst_ui::progressIndicatorDefaultColor() {
  QProgressIndicator indicator;
  QVERIFY(!indicator.color().isValid());
}

void tst_ui::progressIndicatorStartAnimation() {
  QProgressIndicator indicator;
  QVERIFY(!indicator.isAnimated());
  indicator.startAnimation();
  QVERIFY(indicator.isAnimated());
  indicator.stopAnimation();
}

void tst_ui::progressIndicatorStopAnimation() {
  QProgressIndicator indicator;
  indicator.startAnimation();
  QVERIFY(indicator.isAnimated());
  indicator.stopAnimation();
  QVERIFY(!indicator.isAnimated());
}

void tst_ui::progressIndicatorStartStopCycle() {
  QProgressIndicator indicator;
  indicator.startAnimation();
  QVERIFY(indicator.isAnimated());
  indicator.stopAnimation();
  QVERIFY(!indicator.isAnimated());
  indicator.startAnimation();
  QVERIFY(indicator.isAnimated());
  indicator.stopAnimation();
  QVERIFY(!indicator.isAnimated());
}

void tst_ui::progressIndicatorSetAnimationDelay() {
  QProgressIndicator indicator;
  indicator.setAnimationDelay(100);
  QCOMPARE(indicator.animationDelay(), 100);
}

void tst_ui::progressIndicatorSetDisplayedWhenStopped() {
  QProgressIndicator indicator;
  QVERIFY(!indicator.isDisplayedWhenStopped());
  indicator.setDisplayedWhenStopped(true);
  QVERIFY(indicator.isDisplayedWhenStopped());
  indicator.setDisplayedWhenStopped(false);
  QVERIFY(!indicator.isDisplayedWhenStopped());
}

void tst_ui::progressIndicatorSetColor() {
  QProgressIndicator indicator;
  QColor red(Qt::red);
  indicator.setColor(red);
  QCOMPARE(indicator.color(), red);
  indicator.setColor(QColor());
  QVERIFY(!indicator.color().isValid());
}

void tst_ui::progressIndicatorSizeHint() {
  QProgressIndicator indicator;
  QSize hint = indicator.sizeHint();
  QCOMPARE(hint, QSize(20, 20));
}

void tst_ui::progressIndicatorHeightForWidth() {
  QProgressIndicator indicator;
  QCOMPARE(indicator.heightForWidth(30), 30);
  QCOMPARE(indicator.heightForWidth(50), 50);
  QCOMPARE(indicator.heightForWidth(0), 0);
}

void tst_ui::progressIndicatorStopWhenNotRunningIsHarmless() {
  QProgressIndicator indicator;
  QVERIFY(!indicator.isAnimated());
  // Stopping when already stopped should not crash or change state
  indicator.stopAnimation();
  QVERIFY(!indicator.isAnimated());
}

void tst_ui::progressIndicatorStartTwiceDoesNotDuplicate() {
  QProgressIndicator indicator;
  indicator.startAnimation();
  QVERIFY(indicator.isAnimated());
  // Starting again when already running should remain animated without crash
  indicator.startAnimation();
  QVERIFY(indicator.isAnimated());
  indicator.stopAnimation();
  QVERIFY(!indicator.isAnimated());
}

namespace {

/**
 * @brief Paint the indicator at the given square size into a transparent
 * image, so what paintEvent() draws can be inspected pixel by pixel. render()
 * works on hidden widgets and the flags leave the window background out.
 */
auto renderIndicator(QProgressIndicator &indicator, int size) -> QImage {
  indicator.resize(size, size);
  QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  indicator.render(&image, QPoint(), QRegion(), QWidget::RenderFlags());
  return image;
}

/**
 * @brief Whether any pixel of the image has been painted at all.
 */
auto hasOpaquePixel(const QImage &image) -> bool {
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (qAlpha(image.pixel(x, y)) > 0) {
        return true;
      }
    }
  }
  return false;
}

/**
 * @brief Whether some fully opaque pixel has the given RGB colour: the
 * first capsule is drawn with alpha 1.0, so the chosen colour must appear
 * unblended somewhere.
 */
auto hasOpaquePixelOfColour(const QImage &image, const QColor &colour) -> bool {
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const QRgb px = image.pixel(x, y);
      if (qAlpha(px) == 255 && QColor(px).rgb() == colour.rgb()) {
        return true;
      }
    }
  }
  return false;
}

/**
 * @brief Write an executable /bin/sh script standing in for qrencode into
 * dir and point the (isolated) settings at it.
 * @return The script's path, empty when it could not be written.
 */
auto installFakeQrencode(const QTemporaryDir &dir, const QByteArray &body)
    -> QString {
  const QString path = QDir(dir.path()).filePath(QStringLiteral("qrencode"));
  QFile script(path);
  if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return QString();
  }
  script.write("#!/bin/sh\n");
  script.write(body);
  script.close();
  if (!script.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                             QFile::ExeOwner)) {
    return QString();
  }
  AppSettings s = QtPassSettings::load();
  s.qrencodeExecutable = path;
  QtPassSettings::save(s);
  return path;
}

} // namespace

/**
 * @brief tst_ui::progressIndicatorPaintsNothingWhileStoppedByDefault pins the
 * early return in paintEvent(): a stopped indicator that is not displayed
 * when stopped leaves the widget untouched, so it disappears from the toolbar
 * between operations instead of showing a frozen spinner.
 */
void tst_ui::progressIndicatorPaintsNothingWhileStoppedByDefault() {
  QProgressIndicator indicator;
  QVERIFY(!indicator.isAnimated());
  QVERIFY(!indicator.isDisplayedWhenStopped());
  const QImage image = renderIndicator(indicator, 40);
  QVERIFY2(!hasOpaquePixel(image),
           "a stopped indicator must not paint anything by default");
}

/**
 * @brief tst_ui::progressIndicatorPaintsWhenStoppedIfDisplayedWhenStopped
 * pins that setDisplayedWhenStopped(true) makes the spinner visible even while
 * no animation is running.
 */
void tst_ui::progressIndicatorPaintsWhenStoppedIfDisplayedWhenStopped() {
  QProgressIndicator indicator;
  indicator.setDisplayedWhenStopped(true);
  QVERIFY(!indicator.isAnimated());
  const QImage image = renderIndicator(indicator, 40);
  QVERIFY2(hasOpaquePixel(image),
           "displayedWhenStopped must paint the spinner while stopped");
}

/**
 * @brief tst_ui::progressIndicatorPaintsCapsulesWhileAnimated pins the
 * geometry of the twelve capsules: something is drawn near the rim, nothing
 * in the hollow centre, for both the small (<= 32 px) and large capsule width
 * branch.
 */
void tst_ui::progressIndicatorPaintsCapsulesWhileAnimated() {
  QProgressIndicator indicator;
  indicator.startAnimation();
  QVERIFY(indicator.isAnimated());

  for (int size : {20, 64}) {
    const QImage image = renderIndicator(indicator, size);
    QVERIFY2(hasOpaquePixel(image),
             qPrintable(
                 QStringLiteral("size %1: spinner must be painted").arg(size)));
    // The capsules start at innerRadius = 0.38 * outerRadius from the centre,
    // so the very middle stays clear.
    const QRgb centre = image.pixel(size / 2, size / 2);
    QVERIFY2(
        qAlpha(centre) == 0,
        qPrintable(
            QStringLiteral("size %1: the centre must stay empty").arg(size)));
  }
  indicator.stopAnimation();
}

/**
 * @brief tst_ui::progressIndicatorPaintsInTheChosenColour pins that a valid
 * colour set through setColor() is what the capsules are drawn in.
 */
void tst_ui::progressIndicatorPaintsInTheChosenColour() {
  QProgressIndicator indicator;
  indicator.setDisplayedWhenStopped(true);
  const QColor paletteColour(0x12, 0x34, 0x56);
  QPalette palette = indicator.palette();
  palette.setColor(QPalette::WindowText, paletteColour);
  indicator.setPalette(palette);
  indicator.setColor(Qt::red);
  const QImage image = renderIndicator(indicator, 64);
  QVERIFY2(hasOpaquePixelOfColour(image, QColor(Qt::red)),
           "the leading capsule must be drawn fully opaque in the set colour");
  QVERIFY2(!hasOpaquePixelOfColour(image, paletteColour),
           "the palette text colour must not be used once a colour is set");
}

/**
 * @brief tst_ui::progressIndicatorPaintsInThePaletteColourByDefault pins the
 * fallback: with no colour set the spinner follows the palette's window text
 * colour, so it stays legible in both light and dark themes.
 */
void tst_ui::progressIndicatorPaintsInThePaletteColourByDefault() {
  QProgressIndicator indicator;
  indicator.setDisplayedWhenStopped(true);
  QPalette palette = indicator.palette();
  palette.setColor(QPalette::WindowText, QColor(0x12, 0x34, 0x56));
  indicator.setPalette(palette);
  QVERIFY(!indicator.color().isValid());
  const QImage image = renderIndicator(indicator, 64);
  QVERIFY2(hasOpaquePixelOfColour(image, QColor(0x12, 0x34, 0x56)),
           "without a colour the capsules follow palette().windowText()");
}

/**
 * @brief tst_ui::progressIndicatorSetAnimationDelayWhileRunningKeepsTicking
 * pins that changing the delay of a running spinner restarts its timer
 * rather than silently stopping the animation: the rendering keeps changing
 * afterwards and isAnimated() stays true.
 */
void tst_ui::progressIndicatorSetAnimationDelayWhileRunningKeepsTicking() {
  QProgressIndicator indicator;
  indicator.startAnimation();
  QVERIFY(indicator.isAnimated());
  indicator.setAnimationDelay(10);
  QCOMPARE(indicator.animationDelay(), 10);
  QVERIFY2(indicator.isAnimated(),
           "changing the delay must not stop a running animation");

  const QImage before = renderIndicator(indicator, 64);
  QVERIFY(hasOpaquePixel(before));
  // Each tick rotates the capsules by 30 degrees, so the picture changes.
  QTRY_VERIFY2(renderIndicator(indicator, 64) != before,
               "the timer must keep firing after setAnimationDelay()");
  indicator.stopAnimation();
  QVERIFY(!indicator.isAnimated());
}

// ---- QtPass::showTextAsQRCode tests ----

/**
 * @brief tst_ui::showTextAsQRCodeFeedsTextToQrencodeAndShowsItsImage pins the
 * success path: the text goes to qrencode's stdin, its PNG on stdout ends up
 * as the popup label's pixmap, the popup is shown modally and no status
 * message is emitted.
 */
void tst_ui::showTextAsQRCodeFeedsTextToQrencodeAndShowsItsImage() {
#ifdef Q_OS_WIN
  QSKIP("the stand-in qrencode is a shell script");
#else
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString pngPath = QDir(dir.path()).filePath(QStringLiteral("qr.png"));
  const QString stdinPath =
      QDir(dir.path()).filePath(QStringLiteral("stdin.txt"));
  QImage qr(23, 23, QImage::Format_RGB32);
  qr.fill(Qt::black);
  qr.setPixelColor(3, 3, Qt::white);
  QVERIFY(qr.save(pngPath, "PNG"));

  const QString exe =
      installFakeQrencode(dir, "cat > \"" + stdinPath.toUtf8() + "\"\ncat \"" +
                                   pngPath.toUtf8() + "\"\nexit 0\n");
  QVERIFY(!exe.isEmpty());

  QtPass qtpass;
  QSignalSpy status(&qtpass, &QtPass::statusMessage);

  // The popup runs a nested event loop; poke it closed from a timer once it
  // is up, remembering what it showed.
  QImage shown;
  bool popupSeen = false;
  QTimer poker;
  poker.setInterval(20);
  QObject::connect(&poker, &QTimer::timeout, [&]() {
    for (QWidget *w : QApplication::topLevelWidgets()) {
      auto *dialog = qobject_cast<QDialog *>(w);
      if (dialog == nullptr || dialog->windowType() != Qt::Popup ||
          !dialog->isVisible()) {
        continue;
      }
      auto *label = dialog->findChild<QLabel *>();
      if (label != nullptr) {
        shown = label->pixmap().toImage();
      }
      popupSeen = true;
      dialog->close();
      poker.stop();
    }
  });
  poker.start();
  // Never hang the suite if the popup is not found.
  QTimer::singleShot(10000, &poker, [&]() {
    for (QWidget *w : QApplication::topLevelWidgets()) {
      if (auto *dialog = qobject_cast<QDialog *>(w)) {
        dialog->close();
      }
    }
  });

  qtpass.showTextAsQRCode(QStringLiteral("hunter2\nline two"));
  poker.stop();

  QVERIFY2(popupSeen, "a visible popup dialog must have been exec()ed");
  // The bytes qrencode wrote to stdout are what the label shows, decoded.
  QCOMPARE(shown.size(), QSize(23, 23));
  QCOMPARE(shown.pixelColor(3, 3), QColor(Qt::white));
  QCOMPARE(shown.pixelColor(0, 0), QColor(Qt::black));
  QVERIFY2(status.isEmpty(), "no status message on success");

  QFile captured(stdinPath);
  QVERIFY(captured.open(QIODevice::ReadOnly));
  QCOMPARE(QString::fromUtf8(captured.readAll()),
           QStringLiteral("hunter2\nline two"));
#endif
}

/**
 * @brief tst_ui::showTextAsQRCodeReportsQrencodeStderr pins that a failing
 * qrencode's own error text (non-zero exit, something on stderr) is what the
 * status bar gets, verbatim.
 */
void tst_ui::showTextAsQRCodeReportsQrencodeStderr() {
#ifdef Q_OS_WIN
  QSKIP("the stand-in qrencode is a shell script");
#else
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QVERIFY(!installFakeQrencode(dir, "cat > /dev/null\n"
                                    "echo 'too much data' >&2\n"
                                    "exit 1\n")
               .isEmpty());

  QtPass qtpass;
  QSignalSpy status(&qtpass, &QtPass::statusMessage);
  // Would block in QDialog::exec() if the error path were not taken.
  qtpass.showTextAsQRCode(QStringLiteral("hunter2"));

  QCOMPARE(status.count(), 1);
  QCOMPARE(status.first().at(0).toString().trimmed(),
           QStringLiteral("too much data"));
  QCOMPARE(status.first().at(1).toInt(), 2000);
#endif
}

/**
 * @brief tst_ui::showTextAsQRCodeReportsExitCodeWhenStderrIsEmpty pins the
 * fallback message for a silent failure: the exit code is named instead of
 * showing an empty status.
 */
void tst_ui::showTextAsQRCodeReportsExitCodeWhenStderrIsEmpty() {
#ifdef Q_OS_WIN
  QSKIP("the stand-in qrencode is a shell script");
#else
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QVERIFY(!installFakeQrencode(dir, "cat > /dev/null\nexit 3\n").isEmpty());

  QtPass qtpass;
  QSignalSpy status(&qtpass, &QtPass::statusMessage);
  qtpass.showTextAsQRCode(QStringLiteral("hunter2"));

  QCOMPARE(status.count(), 1);
  QCOMPARE(status.first().at(0).toString(),
           QStringLiteral("qrencode exited with code 3"));
  QCOMPARE(status.first().at(1).toInt(), 2000);
#endif
}

/**
 * @brief tst_ui::showTextAsQRCodeReportsACrash pins that a qrencode killed by
 * a signal is reported as a crash and that its (meaningless) exit code is not
 * consulted.
 */
void tst_ui::showTextAsQRCodeReportsACrash() {
#ifdef Q_OS_WIN
  QSKIP("the stand-in qrencode is a shell script");
#else
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QVERIFY(
      !installFakeQrencode(dir, "cat > /dev/null\nkill -SEGV $$\n").isEmpty());

  QtPass qtpass;
  QSignalSpy status(&qtpass, &QtPass::statusMessage);
  qtpass.showTextAsQRCode(QStringLiteral("hunter2"));

  QCOMPARE(status.count(), 1);
  QVERIFY2(status.first().at(0).toString() ==
               QStringLiteral("qrencode crashed"),
           qPrintable(QStringLiteral("status must report the crash, not an "
                                     "exit code: ") +
                      status.first().at(0).toString()));
  QCOMPARE(status.first().at(1).toInt(), 2000);
#endif
}

// ---- DeselectableTreeView tests ----

void tst_ui::deselectableTreeViewConstruction() {
  // Verify the view can be constructed and destroyed without issues
  QScopedPointer<DeselectableTreeView> view(new DeselectableTreeView(nullptr));
  QVERIFY(view != nullptr);
}

void tst_ui::deselectableTreeViewHasEmptyClickedSignal() {
  // Verify emptyClicked signal is connectable via QSignalSpy
  QScopedPointer<DeselectableTreeView> view(new DeselectableTreeView(nullptr));
  QSignalSpy spy(view.data(), &DeselectableTreeView::emptyClicked);
  QVERIFY(spy.isValid());
  // No click occurred yet, so count should be 0
  QCOMPARE(spy.count(), 0);
}

/// A release must return immediately. The old implementation spun
/// QCoreApplication::processEvents for 200 ms inside mouseReleaseEvent, which
/// delayed every click — and therefore every decrypt — by that much.
void tst_ui::deselectableTreeViewClickDoesNotBlock() {
  DeselectableTreeView view(nullptr);
  QStandardItemModel model(2, 1);
  model.setItem(0, 0, new QStandardItem(QStringLiteral("one")));
  model.setItem(1, 0, new QStandardItem(QStringLiteral("two")));
  view.setModel(&model);
  view.resize(200, 100);

  const QModelIndex first = model.index(0, 0);
  view.selectionModel()->select(first, QItemSelectionModel::Select);
  const QPoint pos = view.visualRect(first).center();

  QElapsedTimer timer;
  timer.start();
  QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, pos);
  QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier, pos);
  const qint64 elapsed = timer.elapsed();

  QVERIFY2(elapsed < 100,
           qPrintable(QStringLiteral("a click took %1 ms").arg(elapsed)));
}

/// Clicking an already-selected item clears the selection once the platform's
/// double-click interval has passed without a second click.
void tst_ui::deselectableTreeViewClearsSelectionAfterDoubleClickInterval() {
  DeselectableTreeView view(nullptr);
  QStandardItemModel model(1, 1);
  model.setItem(0, 0, new QStandardItem(QStringLiteral("one")));
  view.setModel(&model);
  view.resize(200, 100);

  const QModelIndex first = model.index(0, 0);
  view.selectionModel()->select(first, QItemSelectionModel::Select);
  QVERIFY(view.selectionModel()->isSelected(first));

  QSignalSpy spy(&view, &DeselectableTreeView::emptyClicked);
  const QPoint pos = view.visualRect(first).center();
  QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, pos);
  QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier, pos);

  QVERIFY2(spy.wait(5000), "emptyClicked should fire after the interval");
  QVERIFY(!view.selectionModel()->isSelected(first));
}

/// A double-click opens the editor, so it must cancel the pending deselect.
void tst_ui::deselectableTreeViewDoubleClickKeepsSelection() {
  DeselectableTreeView view(nullptr);
  QStandardItemModel model(1, 1);
  model.setItem(0, 0, new QStandardItem(QStringLiteral("one")));
  view.setModel(&model);
  view.resize(200, 100);

  const QModelIndex first = model.index(0, 0);
  view.selectionModel()->select(first, QItemSelectionModel::Select);

  QSignalSpy spy(&view, &DeselectableTreeView::emptyClicked);
  const QPoint pos = view.visualRect(first).center();
  QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, pos);
  QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier, pos);
  QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, pos);

  // Wait out the interval: nothing may be cleared.
  QTest::qWait(QGuiApplication::styleHints()->mouseDoubleClickInterval() + 200);
  QCOMPARE(spy.count(), 0);
  QVERIFY(view.selectionModel()->isSelected(first));
}

/// Clicking a selected item and then another item before the interval passes
/// must not clear the new selection: the pending deselect belongs to the
/// first click and has to be cancelled by the second press.
void tst_ui::deselectableTreeViewSecondClickCancelsPendingDeselect() {
  DeselectableTreeView view(nullptr);
  QStandardItemModel model(2, 1);
  model.setItem(0, 0, new QStandardItem(QStringLiteral("one")));
  model.setItem(1, 0, new QStandardItem(QStringLiteral("two")));
  view.setModel(&model);
  view.resize(200, 200);

  const QModelIndex first = model.index(0, 0);
  const QModelIndex second = model.index(1, 0);
  view.selectionModel()->select(first, QItemSelectionModel::Select);

  QSignalSpy spy(&view, &DeselectableTreeView::emptyClicked);
  const QPoint firstPos = view.visualRect(first).center();
  QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, firstPos);
  QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier,
                      firstPos);

  // Second click on a different row, before the pending deselect fires.
  const QPoint secondPos = view.visualRect(second).center();
  QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, secondPos);
  QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier,
                      secondPos);

  QTest::qWait(QGuiApplication::styleHints()->mouseDoubleClickInterval() + 200);
  QCOMPARE(spy.count(), 0);
  QVERIFY2(view.selectionModel()->isSelected(second),
           "the second row must stay selected");
}

QTEST_MAIN(tst_ui)
#include "tst_ui.moc"
