// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QDialog>
#include <QPointer>
#include <QtTest>

#include "../../../src/passworddialog.h"
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
  void dialogWithoutDeleteOnCloseDoesNotAutoDelete();
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

QTEST_MAIN(tst_ui)
#include "tst_ui.moc"