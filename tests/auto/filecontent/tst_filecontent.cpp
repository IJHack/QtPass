// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "../../../src/filecontent.h"

class tst_filecontent : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void parsePlainPassword();
  void parsePasswordWithNamedFields();
  void parseWithTemplateFields();
  void parseWithAllFields();
  void getRemainingData();
  void getRemainingDataForDisplay();
  void namedValuesTakeValue();
  void namedValuesTakeValueNotFound();
  void parseEmptyContent();
  void parsePasswordOnly();
  void parseMultipleNamedFields();
};

void tst_filecontent::initTestCase() {}

void tst_filecontent::parsePlainPassword() {
  QString content = "my_secret_password";
  FileContent fc = FileContent::parse(content, QStringList(), false);
  QVERIFY2(fc.getPassword() == "my_secret_password", "Password should match");
  QVERIFY(fc.getNamedValues().isEmpty());
}

void tst_filecontent::parsePasswordWithNamedFields() {
  QString content = "secret123\nusername: john\npassword: doe";
  QStringList templateFields;
  FileContent fc = FileContent::parse(content, templateFields, false);
  QVERIFY2(fc.getPassword() == "secret123", "Password should be secret123");
  QVERIFY(fc.getNamedValues().isEmpty());
}

void tst_filecontent::parseWithTemplateFields() {
  QString content =
      "mypassword\nusername: john@example.com\nurl: https://example.com";
  QStringList templateFields = {"username", "url"};
  FileContent fc = FileContent::parse(content, templateFields, false);
  QVERIFY2(fc.getPassword() == "mypassword", "Password should be mypassword");

  NamedValues nv = fc.getNamedValues();
  QCOMPARE(nv.size(), 2);
  QCOMPARE(nv[0].name, QString("username"));
  QCOMPARE(nv[0].value, QString("john@example.com"));
  QCOMPARE(nv[1].name, QString("url"));
  QCOMPARE(nv[1].value, QString("https://example.com"));
}

void tst_filecontent::parseWithAllFields() {
  QString content =
      "pass123\nusername: admin\nnotes: some notes\ncustom: value";
  QStringList templateFields;
  FileContent fc = FileContent::parse(content, templateFields, true);

  QVERIFY2(fc.getPassword() == "pass123", "Password should be pass123");

  NamedValues nv = fc.getNamedValues();
  QCOMPARE(nv.size(), 3);
}

void tst_filecontent::getRemainingData() {
  QString content = "secret\nfield1: value1\nfield2: value2\nextra: data";
  QStringList templateFields = {"field1"};
  FileContent fc = FileContent::parse(content, templateFields, false);

  QString remaining = fc.getRemainingData();
  QVERIFY(remaining.contains("field2"));
  QVERIFY(remaining.contains("extra"));
}

void tst_filecontent::getRemainingDataForDisplay() {
  QString content =
      "secret\nnotes: some notes\notpauth://totp/Secret: SKI123456";
  QStringList templateFields;
  FileContent fc = FileContent::parse(content, templateFields, false);

  QString display = fc.getRemainingDataForDisplay();
  QVERIFY(display.contains("notes"));
  QVERIFY(!display.contains("otpauth"));
}

void tst_filecontent::namedValuesTakeValue() {
  NamedValues nv = {{"username", "john"}, {"password", "secret"}};

  QString val = nv.takeValue("username");
  QVERIFY2(val == "john", "Should return 'john'");

  val = nv.takeValue("username");
  QVERIFY2(val.isEmpty(), "Should return empty after taking");
}

void tst_filecontent::namedValuesTakeValueNotFound() {
  NamedValues nv = {{"username", "john"}};

  QString val = nv.takeValue("nonexistent");
  QVERIFY2(val.isEmpty(), "Should return empty for nonexistent key");
}

void tst_filecontent::parseEmptyContent() {
  FileContent fc = FileContent::parse("", QStringList(), false);
  QVERIFY(fc.getPassword().isEmpty());
}

void tst_filecontent::parsePasswordOnly() {
  FileContent fc = FileContent::parse("single_line", QStringList(), false);
  QCOMPARE(fc.getPassword(), QString("single_line"));
}

void tst_filecontent::parseMultipleNamedFields() {
  QString content = "pass\nuser: u1\nuser: u2\nuser: u3";
  QStringList templateFields = {"user"};
  FileContent fc = FileContent::parse(content, templateFields, false);
  QCOMPARE(fc.getPassword(), QString("pass"));
  NamedValues nv = fc.getNamedValues();
  QVERIFY(nv.size() >= 1);
}

QTEST_MAIN(tst_filecontent)
#include "tst_filecontent.moc"