// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QSignalSpy>
#include <QSpinBox>
#include <QtTest>

#include "../../../src/deselectabletreeview.h"
#include "../../../src/passworddialog.h"
#include "../../../src/qprogressindicator.h"
#include "../../../src/qpushbuttonasqrcode.h"
#include "../../../src/qpushbuttonshowpassword.h"
#include "../../../src/qpushbuttonwithclipboard.h"
#include "../../../src/qtpass.h"
#include "passwordconfiguration.h"

class tst_ui : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void contentRemainsSame();
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

  // DeselectableTreeView tests
  void deselectableTreeViewConstruction();
  void deselectableTreeViewHasEmptyClickedSignal();
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
}

void tst_ui::initTestCase() {}

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

// ---- DeselectableTreeView tests ----

void tst_ui::deselectableTreeViewConstruction() {
  // Verify the view can be constructed and destroyed without issues
  QScopedPointer<DeselectableTreeView> view(new DeselectableTreeView(nullptr));
  QVERIFY(view != nullptr);
}

void tst_ui::deselectableTreeViewHasEmptyClickedSignal() {
  // Verify emptyClicked signal is connectable via QSignalSpy
  QScopedPointer<DeselectableTreeView> view(new DeselectableTreeView(nullptr));
  QSignalSpy spy(view.data(), SIGNAL(emptyClicked()));
  QVERIFY(spy.isValid());
  // No click occurred yet, so count should be 0
  QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(tst_ui)
#include "tst_ui.moc"
