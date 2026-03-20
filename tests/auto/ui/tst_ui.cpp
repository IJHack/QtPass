#include "../../../src/passworddialog.h"
#include "passwordconfiguration.h"
#include <QCoreApplication>
#include <QtTest>

class tst_ui : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void contentRemainsSame();
  void emptyPassword();
  void multilineRemainingData();
  void cleanupTestCase();
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

QTEST_MAIN(tst_ui)
#include "tst_ui.moc"
