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
  void parseAllFieldsModeIncludesExtraFields();
  void parseAllFieldsModeExcludesUrlLines();
  void parseCrlfNamedValuesAreTrimmed();
  void parseMultipleOtpauthLinesAllHidden();
  void parseOtpauthCaseInsensitiveHidden();
  void namedValuesDefaultConstructorIsEmpty();
  void namedValueEqualityOperator();
  void namedValueInequalityByName();
  void namedValueInequalityByValue();
  void namedValuesTakeValueRemovesOnlyFirst();
  void parseUnicodePassword();
  void parseUnicodeFieldValue();
  void parseFieldWithSpacesAroundColon();
  void getRemainingDataEmptyWhenAllTemplate();
  void isOtpFieldNameMatchesOtpAndTotp_data();
  void isOtpFieldNameMatchesOtpAndTotp();
  void getOtpUriFromTemplateField();
  void getOtpUriFromAllFieldsMode();
  void getOtpUriFromUnpromotedOtpLine();
  void getOtpUriFromBareOtpauthLine();
  void getOtpUriFromBareBase32Secret();
  void getOtpUriPrefersFieldOverBareLine();
  void getOtpUriEmptyWhenAbsent();
  void otpFieldHiddenFromDisplay();
  void otpFieldRoundTripsInNamedValues();
  void getOtpUriFromUriAsOnlyLine();
  void passwordForDisplayBlanksOtpUri();
  void getOtpUriFromDifferentlyNamedField();
  void isOtpUriValueMatchesScheme_data();
  void isOtpUriValueMatchesScheme();
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
  QVERIFY2(fc.getRemainingData() == "second_line",
           "remaining should be exactly second_line");
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
  if (nv.size() == 1) {
    QVERIFY2(nv[0].name == "username", "field name should be username");
    QVERIFY2(nv[0].value == "test", "field value should be test");
  }
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

void tst_filecontent::parseAllFieldsModeIncludesExtraFields() {
  QString content = "secret\nnote: some note\nkey: value";
  FileContent fc = FileContent::parse(content, QStringList(), true);
  NamedValues nv = fc.getNamedValues();
  QVERIFY2(nv.size() == 2,
           "allFields=true should include all name:value pairs");
  QVERIFY2(nv[0].name == "note" && nv[0].value == "some note",
           "first field should be note");
  QVERIFY2(nv[1].name == "key" && nv[1].value == "value",
           "second field should be key");
  QVERIFY2(fc.getRemainingData().isEmpty(),
           "no remaining data when all fields are named");
}

void tst_filecontent::parseAllFieldsModeExcludesUrlLines() {
  QString content = "secret\nhttp://example.com\nkey: value";
  FileContent fc = FileContent::parse(content, QStringList(), true);
  NamedValues nv = fc.getNamedValues();
  QVERIFY2(nv.size() == 1, "URL-like lines should not become named values");
  QVERIFY2(nv[0].name == "key" && nv[0].value == "value",
           "non-url field should be parsed");
  QVERIFY2(fc.getRemainingData().contains("http://example.com"),
           "URL line should stay in remaining data");
}

void tst_filecontent::parseCrlfNamedValuesAreTrimmed() {
  QString content = "secret\r\nusername: john\r\n";
  FileContent fc = FileContent::parse(content, {"username"}, false);
  NamedValues nv = fc.getNamedValues();
  QVERIFY2(nv.size() == 1, "CRLF file should yield one named value");
  QVERIFY2(nv[0].value == "john",
           "value.trimmed() must strip trailing \\r from CRLF lines");
}

void tst_filecontent::parseMultipleOtpauthLinesAllHidden() {
  QString content =
      "secret\notpauth://totp/A?secret=x\notpauth://totp/B?secret=y";
  FileContent fc = FileContent::parse(content, QStringList(), false);
  QVERIFY2(fc.getRemainingData().contains("otpauth://totp/A"),
           "otpauth lines must appear in getRemainingData");
  QVERIFY2(fc.getRemainingData().contains("otpauth://totp/B"),
           "both otpauth lines must appear in getRemainingData");
  QVERIFY2(fc.getRemainingDataForDisplay().isEmpty(),
           "getRemainingDataForDisplay must hide all otpauth lines");
}

void tst_filecontent::parseOtpauthCaseInsensitiveHidden() {
  QString content = "secret\nOTPAUTH://totp/Example?secret=key";
  FileContent fc = FileContent::parse(content, QStringList(), false);
  QVERIFY2(fc.getRemainingData().contains("OTPAUTH://"),
           "uppercase OTPAUTH should be in remaining data");
  QVERIFY2(fc.getRemainingDataForDisplay().isEmpty(),
           "uppercase OTPAUTH must be hidden from display");
}

void tst_filecontent::namedValuesDefaultConstructorIsEmpty() {
  NamedValues nv;
  QVERIFY2(nv.isEmpty(), "default-constructed NamedValues must be empty");
}

void tst_filecontent::namedValueEqualityOperator() {
  NamedValue a{"user", "john"};
  NamedValue b{"user", "john"};
  QVERIFY2(a == b, "identical NamedValues must compare equal");
}

void tst_filecontent::namedValueInequalityByName() {
  NamedValue a{"user", "john"};
  NamedValue b{"login", "john"};
  QVERIFY2(!(a == b), "NamedValues with different names must not be equal");
}

void tst_filecontent::namedValueInequalityByValue() {
  NamedValue a{"user", "john"};
  NamedValue b{"user", "jane"};
  QVERIFY2(!(a == b), "NamedValues with different values must not be equal");
}

void tst_filecontent::namedValuesTakeValueRemovesOnlyFirst() {
  NamedValues nv = {{"key", "first"}, {"key", "second"}};
  QString taken = nv.takeValue("key");
  QVERIFY2(taken == "first", "takeValue must return the first match");
  QVERIFY2(nv.size() == 1, "takeValue must remove only the first match");
  QVERIFY2(nv[0].value == "second", "second entry must remain after takeValue");
}

void tst_filecontent::parseUnicodePassword() {
  QString content = "pässwörд\nusername: alice";
  FileContent fc = FileContent::parse(content, {"username"}, false);
  QVERIFY2(fc.getPassword() == "pässwörд",
           "unicode password must be preserved verbatim");
}

void tst_filecontent::parseUnicodeFieldValue() {
  QString content = "pass\nkommentar: héllo wörld";
  FileContent fc = FileContent::parse(content, {"kommentar"}, false);
  NamedValues nv = fc.getNamedValues();
  QVERIFY2(nv.size() == 1, "unicode field must be parsed");
  QVERIFY2(nv[0].value == "héllo wörld",
           "unicode field value must be preserved");
}

void tst_filecontent::parseFieldWithSpacesAroundColon() {
  // allFields=true: name and value are trimmed on append, so
  // "user : alice" produces name="user", value="alice"
  QString content = "pass\nuser : alice";
  FileContent fc = FileContent::parse(content, QStringList(), true);
  NamedValues nv = fc.getNamedValues();
  QVERIFY2(nv.size() == 1,
           "field with spaces around colon should be parsed in allFields mode");
  QVERIFY2(nv[0].name == "user", "name must be trimmed");
  QVERIFY2(nv[0].value == "alice", "value must be trimmed");
}

void tst_filecontent::getRemainingDataEmptyWhenAllTemplate() {
  QString content = "secret\nuser: alice\nurl: example.com";
  QStringList templateFields = {"user", "url"};
  FileContent fc = FileContent::parse(content, templateFields, false);
  QVERIFY2(fc.getRemainingData().isEmpty(),
           "remaining data must be empty when all fields match the template");
}

/// The canonical OTP field value QtPass writes.
static const QString kOtpUri = QStringLiteral(
    "otpauth://totp/Example:alice?secret=JBSWY3DPEHPK3PXP&issuer=Example&"
    "digits=6&period=30");

void tst_filecontent::isOtpFieldNameMatchesOtpAndTotp_data() {
  QTest::addColumn<QString>("name");
  QTest::addColumn<bool>("expected");

  QTest::newRow("OTP") << QStringLiteral("OTP") << true;
  QTest::newRow("otp") << QStringLiteral("otp") << true;
  QTest::newRow("Otp") << QStringLiteral("Otp") << true;
  QTest::newRow("TOTP") << QStringLiteral("TOTP") << true;
  QTest::newRow("padded TOTP") << QStringLiteral("  TOTP  ") << true;
  // Only those two exact names, so an unrelated field is not made invisible.
  QTest::newRow("otp_backup") << QStringLiteral("otp_backup") << false;
  QTest::newRow("notes") << QStringLiteral("notes") << false;
  QTest::newRow("empty") << QString() << false;
}

void tst_filecontent::isOtpFieldNameMatchesOtpAndTotp() {
  QFETCH(QString, name);
  QFETCH(bool, expected);
  QCOMPARE(FileContent::isOtpFieldName(name), expected);
}

void tst_filecontent::getOtpUriFromTemplateField() {
  const QString content = "secret\nlogin: alice\nOTP: " + kOtpUri;
  FileContent fc = FileContent::parse(content, {"login", "OTP"}, false);
  QCOMPARE(fc.getOtpUri(), kOtpUri);
}

void tst_filecontent::getOtpUriFromAllFieldsMode() {
  const QString content = "secret\nOTP: " + kOtpUri;
  FileContent fc = FileContent::parse(content, QStringList(), true);
  QCOMPARE(fc.getOtpUri(), kOtpUri);
}

/**
 * @brief With allFields off and a template that does not list OTP, the line
 * never becomes a named value; the lookup must still find it.
 */
void tst_filecontent::getOtpUriFromUnpromotedOtpLine() {
  const QString content = "secret\nOTP: " + kOtpUri;
  FileContent fc = FileContent::parse(content, {"login"}, false);
  QVERIFY2(fc.getNamedValues().isEmpty(),
           "OTP is not in the template, so it must not be a named value");
  QVERIFY2(fc.getRemainingData().contains("otpauth://"),
           "the line must round-trip through remaining data");
  QCOMPARE(fc.getOtpUri(), kOtpUri);
}

void tst_filecontent::getOtpUriFromBareOtpauthLine() {
  const QString content = "secret\n" + kOtpUri + "\nlogin: alice";
  FileContent fc = FileContent::parse(content, {"login"}, false);
  QCOMPARE(fc.getOtpUri(), kOtpUri);
  QVERIFY2(fc.getRemainingData().contains("otpauth://"),
           "pass-otp style lines must round-trip through remaining data");
}

void tst_filecontent::getOtpUriFromBareBase32Secret() {
  const QString content = "secret\nOTP: JBSWY3DPEHPK3PXP";
  FileContent fc = FileContent::parse(content, {"OTP"}, false);
  QCOMPARE(fc.getOtpUri(), QStringLiteral("JBSWY3DPEHPK3PXP"));
}

void tst_filecontent::getOtpUriPrefersFieldOverBareLine() {
  const QString bare =
      QStringLiteral("otpauth://totp/Bare?secret=GEZDGNBVGY3TQOJQ");
  const QString content = "secret\n" + bare + "\nOTP: " + kOtpUri;
  FileContent fc = FileContent::parse(content, {"OTP"}, false);
  QCOMPARE(fc.getOtpUri(), kOtpUri);
}

void tst_filecontent::getOtpUriEmptyWhenAbsent() {
  FileContent fc =
      FileContent::parse("secret\nlogin: alice\nurl: https://example.com",
                         {"login", "url"}, false);
  QVERIFY(fc.getOtpUri().isEmpty());
}

/**
 * @brief An `OTP:` line carries the shared secret, so it must never reach the
 * display, whether or not it was promoted to a named value.
 */
void tst_filecontent::otpFieldHiddenFromDisplay() {
  const QString content = "secret\nOTP: " + kOtpUri + "\nnotes: hello";
  FileContent fc = FileContent::parse(content, {"login"}, false);
  const QString display = fc.getRemainingDataForDisplay();
  QVERIFY2(!display.contains("otpauth"),
           "the otpauth URI must not be displayed");
  QVERIFY2(!display.contains("JBSWY3DPEHPK3PXP"),
           "the shared secret must not be displayed");
  QVERIFY2(display.contains("notes: hello"),
           "unrelated lines must still be displayed");

  // Also true for a bare base32 secret, which contains no "otpauth" marker.
  FileContent bare =
      FileContent::parse("secret\nTOTP: JBSWY3DPEHPK3PXP", {"login"}, false);
  QVERIFY2(!bare.getRemainingDataForDisplay().contains("JBSWY3DPEHPK3PXP"),
           "a bare secret in a TOTP field must not be displayed");
}

/**
 * @brief The value must stay in the data model: PasswordDialog reads the field
 * from getNamedValues() and drops empty fields when saving, so stripping it
 * would delete the secret on the next edit.
 */
void tst_filecontent::otpFieldRoundTripsInNamedValues() {
  const QString content = "secret\nOTP: " + kOtpUri;
  FileContent fc = FileContent::parse(content, {"OTP"}, false);
  NamedValues values = fc.getNamedValues();
  QCOMPARE(values.size(), 1);
  QCOMPARE(values.at(0).name, QStringLiteral("OTP"));
  QCOMPARE(values.at(0).value, kOtpUri);
}

/**
 * @brief `pass otp insert` writes the URI as the entry's only line, so it lands
 * in the password position and the line-by-line scan never sees it.
 */
void tst_filecontent::getOtpUriFromUriAsOnlyLine() {
  FileContent fc = FileContent::parse(kOtpUri, QStringList(), false);
  QCOMPARE(fc.getOtpUri(), kOtpUri);

  // Also when the URI is line 1 and other fields follow.
  FileContent withFields =
      FileContent::parse(kOtpUri + "\nlogin: alice", {"login"}, false);
  QCOMPARE(withFields.getOtpUri(), kOtpUri);
}

/**
 * @brief getPassword() must still return it so the edit dialog round-trips the
 * file, but nothing on the display path may see it.
 */
void tst_filecontent::passwordForDisplayBlanksOtpUri() {
  FileContent fc = FileContent::parse(kOtpUri, QStringList(), false);
  QCOMPARE(fc.getPassword(), kOtpUri);
  QVERIFY2(fc.getPasswordForDisplay().isEmpty(),
           "a password line that is an otpauth URI must not be displayed");

  // An ordinary password is untouched.
  FileContent normal =
      FileContent::parse("hunter2\nlogin: alice", {"login"}, false);
  QCOMPARE(normal.getPasswordForDisplay(), QStringLiteral("hunter2"));
}

/// A URI is a secret whatever the field is called.
void tst_filecontent::getOtpUriFromDifferentlyNamedField() {
  const QString content = "secret\n2fa: " + kOtpUri;
  FileContent fc = FileContent::parse(content, QStringList(), true);
  QCOMPARE(fc.getOtpUri(), kOtpUri);
  QVERIFY2(!fc.getRemainingDataForDisplay().contains("otpauth"),
           "the URI must be hidden from the text browser too");
}

void tst_filecontent::isOtpUriValueMatchesScheme_data() {
  QTest::addColumn<QString>("value");
  QTest::addColumn<bool>("expected");

  QTest::newRow("uri") << QStringLiteral("otpauth://totp/x?secret=y") << true;
  QTest::newRow("uppercase") << QStringLiteral("OTPAUTH://TOTP/x") << true;
  QTest::newRow("leading space")
      << QStringLiteral("  otpauth://totp/x") << true;
  QTest::newRow("bare secret") << QStringLiteral("JBSWY3DPEHPK3PXP") << false;
  QTest::newRow("url") << QStringLiteral("https://example.com") << false;
  QTest::newRow("empty") << QString() << false;
}

void tst_filecontent::isOtpUriValueMatchesScheme() {
  QFETCH(QString, value);
  QFETCH(bool, expected);
  QCOMPARE(FileContent::isOtpUriValue(value), expected);
}

QTEST_MAIN(tst_filecontent)
#include "tst_filecontent.moc"
