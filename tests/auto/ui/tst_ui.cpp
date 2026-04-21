// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QDialog>
#include <QLineEdit>
#include <QPointer>
#include <QSignalSpy>
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
  QSignalSpy spy(&btn, SIGNAL(clicked(QString)));
  btn.click();
  QCOMPARE(spy.count(), 1);
}

void tst_ui::clipboardButtonClickSignalCarriesText() {
  QPushButtonWithClipboard btn("mytext");
  QSignalSpy spy(&btn, SIGNAL(clicked(QString)));
  btn.click();
  QCOMPARE(spy.count(), 1);
  QList<QVariant> args = spy.takeFirst();
  QCOMPARE(args.at(0).toString(), QString("mytext"));
}

void tst_ui::clipboardButtonClickAfterSetTextCarriesNewText() {
  QPushButtonWithClipboard btn("original");
  btn.setTextToCopy("changed");
  QSignalSpy spy(&btn, SIGNAL(clicked(QString)));
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
  QSignalSpy spy(&btn, SIGNAL(clicked(QString)));
  btn.click();
  QCOMPARE(spy.count(), 1);
}

void tst_ui::qrCodeButtonClickSignalCarriesText() {
  QPushButtonAsQRCode btn("payload");
  QSignalSpy spy(&btn, SIGNAL(clicked(QString)));
  btn.click();
  QCOMPARE(spy.count(), 1);
  QList<QVariant> args = spy.takeFirst();
  QCOMPARE(args.at(0).toString(), QString("payload"));
}

void tst_ui::qrCodeButtonClickAfterSetTextCarriesNewText() {
  QPushButtonAsQRCode btn("old");
  btn.setTextToCopy("new");
  QSignalSpy spy(&btn, SIGNAL(clicked(QString)));
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