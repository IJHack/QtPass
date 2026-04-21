// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "../../../src/filecontent.h"

class tst_filecontent : public QObject {
  Q_OBJECT

private Q_SLOTS:
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
  void parseMatchingTemplateFields();
  void parseOtpauthHiddenLine();
  void parseColonInValue();
  void parseTemplateFieldsCaseSensitive();
  void parseMultiplePasswordLines();
  void parseWhitespaceOnlyContent();
  void parseOnlyNamedFields();
  void parseTemplateFieldsWithEmptyValues();
};

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
  QVERIFY(nv.size() == 2);
  QVERIFY(nv[0].name == "username");
  QVERIFY(nv[0].value == "john@example.com");
  QVERIFY(nv[1].name == "url");
  QVERIFY(nv[1].value == "https://example.com");
}

void tst_filecontent::parseWithAllFields() {
  QString content =
      "pass123\nusername: admin\nnotes: some notes\ncustom: value";
  QStringList templateFields;
  FileContent fc = FileContent::parse(content, templateFields, true);

  QVERIFY2(fc.getPassword() == "pass123", "Password should be pass123");

  NamedValues nv = fc.getNamedValues();
  QVERIFY(nv.size() == 3);
  QVERIFY(nv[0].name == "username");
  QVERIFY(nv[0].value == "admin");
  QVERIFY(nv[1].name == "notes");
  QVERIFY(nv[1].value == "some notes");
  QVERIFY(nv[2].name == "custom");
  QVERIFY(nv[2].value == "value");
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
  QVERIFY(fc.getPassword() == "single_line");
}

void tst_filecontent::parseMultipleNamedFields() {
  QString content = "pass\nuser: u1\nuser: u2\nuser: u3";
  QStringList templateFields = {"user"};
  FileContent fc = FileContent::parse(content, templateFields, false);
  QVERIFY(fc.getPassword() == "pass");
  NamedValues nv = fc.getNamedValues();
  QVERIFY2(nv.size() == 3, "Expected exactly three parsed user fields");
  QVERIFY2(nv[0].name == "user" && nv[0].value == "u1",
           "First user field should be parsed as user: u1");
  QVERIFY2(nv[1].name == "user" && nv[1].value == "u2",
           "Second user field should be parsed as user: u2");
  QVERIFY2(nv[2].name == "user" && nv[2].value == "u3",
           "Third user field should be parsed as user: u3");
}

void tst_filecontent::parseOtpauthHiddenLine() {
  QString content = "secret\notpauth://totp/Example:alice@email?secret=key";
  FileContent fc = FileContent::parse(content, QStringList(), false);
  QString expected = "otpauth://totp/Example:alice@email?secret=key";
  QVERIFY2(fc.getRemainingData() == expected,
           "otpauth line should be preserved in remaining data");
  QVERIFY(fc.getRemainingDataForDisplay().isEmpty());
}

void tst_filecontent::parseMatchingTemplateFields() {
  QString content = "secret123\nusername: john\npassword: doe";
  QStringList templateFields = {"username", "password"};
  FileContent fc = FileContent::parse(content, templateFields, false);
  QVERIFY2(fc.getPassword() == "secret123", "Password should be secret123");
  NamedValues nv = fc.getNamedValues();
  QVERIFY(nv.size() == 2);
  QVERIFY(nv[0].name == "username");
  QVERIFY(nv[0].value == "john");
  QVERIFY(nv[1].name == "password");
  QVERIFY(nv[1].value == "doe");
}

void tst_filecontent::parseColonInValue() {
  QString content = "pass\nurl: https://example.com:8080/path";
  QStringList templateFields = {"url"};
  FileContent fc = FileContent::parse(content, templateFields, false);
  NamedValues nv = fc.getNamedValues();
  QVERIFY(nv.size() == 1);
  QString urlValue = nv.takeValue("url");
  QVERIFY2(urlValue == "https://example.com:8080/path",
           "url value should match full URL with port");
}

void tst_filecontent::parseTemplateFieldsCaseSensitive() {
  QString content = "pass\nUser: value";
  QStringList templateFields = {"user"};
  FileContent fc = FileContent::parse(content, templateFields, false);
  NamedValues nv = fc.getNamedValues();
  QVERIFY(nv.isEmpty());
  QVERIFY2(fc.getRemainingData().contains("User: value"),
           "unmatched case should be in remaining data");
}

void tst_filecontent::parseMultiplePasswordLines() {
  QString content = "first_password\nsecond_line";
  FileContent fc = FileContent::parse(content, QStringList(), false);
  QVERIFY2(fc.getPassword() == "first_password",
           "first line should be password");
  QVERIFY2(fc.getRemainingData().contains("second_line"),
           "remaining should contain second line");
}

void tst_filecontent::parseWhitespaceOnlyContent() {
  QString content = "   \n  \n  ";
  FileContent fc = FileContent::parse(content, QStringList(), false);
  QVERIFY2(fc.getPassword().trimmed().isEmpty(),
           "FileContent::parse preserves whitespace; trimmed() is empty");
}

void tst_filecontent::parseOnlyNamedFields() {
  QString content = "url: https://example.com\nusername: test";
  QStringList templateFields = {"url", "username"};
  FileContent fc = FileContent::parse(content, templateFields, false);
  QVERIFY2(fc.getPassword() == "url: https://example.com",
           "first line becomes password even if it looks like named field");
  NamedValues nv = fc.getNamedValues();
  QVERIFY2(
      nv.size() == 1,
      qPrintable(
          QString("expected 1 named value (username), got %1").arg(nv.size())));
}

void tst_filecontent::parseTemplateFieldsWithEmptyValues() {
  QString content = "secret\nurl: \nusername: ";
  QStringList templateFields = {"url", "username"};
  FileContent fc = FileContent::parse(content, templateFields, false);
  QVERIFY2(fc.getPassword() == "secret", "password should be 'secret'");
  NamedValues nv = fc.getNamedValues();
  QVERIFY2(nv.size() == 2, "should have 2 named values");
  if (nv.size() >= 1) {
    QVERIFY2(nv[0].name == "url", "first field should be url");
    QVERIFY2(nv[0].value.isEmpty(), "url value should be empty");
  }
  if (nv.size() >= 2) {
    QVERIFY2(nv[1].name == "username", "second field should be username");
    QVERIFY2(nv[1].value.isEmpty(), "username value should be empty");
  }
}

QTEST_MAIN(tst_filecontent)
#include "tst_filecontent.moc"