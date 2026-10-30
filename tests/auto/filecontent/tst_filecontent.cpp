// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QRegularExpression>
#include <QtTest>

#include "../../../src/appsettings.h"
#include "../../../src/filecontent.h"

class tst_filecontent : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void parsePlainPassword();
  void parseTakesABoundedNumberOfFields();
  void parseFindsOtpPastTheFieldCap();
  void parseShowsABoundedNumberOfLines();
  void boundedForDisplayCutsAtTheLineLimit();
  void parseKeepsEmptyLinesAsSplitDid();
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
  void parseOtpauthNotAFieldEvenWhenTheTemplateNamesIt();
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
  void isSecretFieldNameMatchesWholeWords_data();
  void isSecretFieldNameMatchesWholeWords();
  void isSecretFieldNameFollowsTheList();
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

/**
 * @brief Every named field becomes a row of widgets. A compressed entry in
 * a shared store can expand into millions of "a: b" lines within the
 * plaintext limit: past kMaxNamedFields they stay text.
 */
void tst_filecontent::parseTakesABoundedNumberOfFields() {
  QString content = QStringLiteral("pw");
  const int lines = FileContent::kMaxNamedFields + 5;
  for (int i = 0; i < lines; ++i) {
    content += QStringLiteral("\nf%1: v").arg(i);
  }
  const FileContent fc = FileContent::parse(content, {}, true);
  QCOMPARE(fc.getPassword(), QStringLiteral("pw"));
  QCOMPARE(fc.getNamedValues().size(), FileContent::kMaxNamedFields);
  QCOMPARE(fc.getRemainingData(),
           QStringLiteral("f%1: v\nf%2: v\nf%3: v\nf%4: v\nf%5: v")
               .arg(lines - 5)
               .arg(lines - 4)
               .arg(lines - 3)
               .arg(lines - 2)
               .arg(lines - 1));
}

/**
 * @brief Lines past kMaxNamedFields stay text but are still looked at for
 * an OTP configuration, as fields would be.
 */
void tst_filecontent::parseFindsOtpPastTheFieldCap() {
  QString content = QStringLiteral("pw");
  for (int i = 0; i < FileContent::kMaxNamedFields; ++i) {
    content += QStringLiteral("\nf%1: v").arg(i);
  }
  const QString uri =
      QStringLiteral("otpauth://totp/x?secret=JBSWY3DPEHPK3PXP");
  content += QStringLiteral("\notp: ") + uri;
  const FileContent fc = FileContent::parse(content, {}, true);
  QCOMPARE(fc.getNamedValues().size(), FileContent::kMaxNamedFields);
  QCOMPARE(fc.getOtpUri(), uri);
  QVERIFY2(!fc.getRemainingDataForDisplay().contains(QStringLiteral("JBSWY")),
           "the secret stays out of the display");
}

/**
 * @brief The text browser makes a block per line: past kMaxDisplayLines the
 * display text ends in one line saying how many more there are, while the
 * remaining text keeps them all.
 */
void tst_filecontent::parseShowsABoundedNumberOfLines() {
  const int lines = FileContent::kMaxDisplayLines + 7;
  QString content = QStringLiteral("pw");
  for (int i = 0; i < lines; ++i) {
    content += QStringLiteral("\nline %1").arg(i);
  }
  const FileContent fc = FileContent::parse(content, {}, false);
  QCOMPARE(fc.getRemainingLineCount(), qsizetype{lines});
  QCOMPARE(fc.getRemainingData().count(QLatin1Char('\n')), lines - 1);
  const QString shown = fc.getRemainingDataForDisplay();
  QCOMPARE(shown.count(QLatin1Char('\n')), FileContent::kMaxDisplayLines);
  QVERIFY2(
      shown.section(QLatin1Char('\n'), -1).contains(QStringLiteral("7 more")),
      qPrintable(shown.right(60)));
}

/**
 * @brief An entry shown as it is (displayAsIs) is cut at the same line limit
 * as the remaining text; one that fits is left alone.
 */
void tst_filecontent::boundedForDisplayCutsAtTheLineLimit() {
  const int limit = FileContent::kMaxDisplayLines;
  QString fits;
  for (int i = 0; i < limit; ++i) {
    fits += (i > 0 ? QStringLiteral("\n") : QString()) +
            QStringLiteral("l%1").arg(i);
  }
  QCOMPARE(FileContent::boundedForDisplay(fits), fits);
  QCOMPARE(FileContent::boundedForDisplay(QString()), QString());

  const QString tooLong = fits + QStringLiteral("\nx\ny\nz");
  const QString shown = FileContent::boundedForDisplay(tooLong);
  QVERIFY(shown.startsWith(fits + QLatin1Char('\n')));
  QCOMPARE(shown.count(QLatin1Char('\n')), limit);
  QVERIFY2(
      shown.section(QLatin1Char('\n'), -1).contains(QStringLiteral("3 more")),
      qPrintable(shown.right(60)));
}

/**
 * @brief Read line by line now rather than split into a list: empty lines,
 * a trailing newline and an empty entry come out as split() and join() made
 * them.
 */
void tst_filecontent::parseKeepsEmptyLinesAsSplitDid() {
  const FileContent empty = FileContent::parse(QString(), {}, false);
  QCOMPARE(empty.getPassword(), QString());
  QCOMPARE(empty.getRemainingData(), QString());
  const FileContent fc =
      FileContent::parse(QStringLiteral("pw\n\nnote\n\n"), {}, false);
  QCOMPARE(fc.getPassword(), QStringLiteral("pw"));
  QCOMPARE(fc.getRemainingData(), QStringLiteral("\nnote\n\n"));
  QCOMPARE(fc.getRemainingDataForDisplay(), QStringLiteral("\nnote\n\n"));
}

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

/**
 * @brief A template listing "otpauth" must not split an otpauth:// line into
 * a field: without its scheme the value no longer reads as a shared secret
 * and was shown, seed and all.
 */
void tst_filecontent::parseOtpauthNotAFieldEvenWhenTheTemplateNamesIt() {
  const QString uri =
      QStringLiteral("otpauth://totp/Example:me?secret=JBSWY3DPEHPK3PXP");
  const FileContent fc = FileContent::parse(
      QStringLiteral("hunter2\n") + uri + QStringLiteral("\nlogin: me"),
      {QStringLiteral("otpauth"), QStringLiteral("login")}, false);
  for (const NamedValue &nv : fc.getNamedValues()) {
    QVERIFY2(!nv.value.contains(QLatin1String("secret=")),
             qPrintable(nv.name + QStringLiteral(" carries the seed")));
  }
  QVERIFY(!fc.getRemainingDataForDisplay().contains(QLatin1String("secret=")));
  QCOMPARE(fc.getOtpUri(), uri);
  // Still there to edit and save back.
  QVERIFY(fc.getRemainingData().contains(uri));
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

void tst_filecontent::isSecretFieldNameMatchesWholeWords_data() {
  QTest::addColumn<QString>("name");
  QTest::addColumn<bool>("secret");
  QTest::newRow("PIN, any case") << QStringLiteral("PIN") << true;
  QTest::newRow("a word in a longer name") << QStringLiteral("API key") << true;
  QTest::newRow("another word around it")
      << QStringLiteral("Recovery codes") << true;
  QTest::newRow("its own entry") << QStringLiteral("pincode") << true;
  QTest::newRow("not part of a word")
      << QStringLiteral("keyboard layout") << false;
  QTest::newRow("not a word ending") << QStringLiteral("spinning") << false;
  QTest::newRow("no secret") << QStringLiteral("username") << false;
  QTest::newRow("after punctuation")
      << QStringLiteral("security-answer") << true;
}

/**
 * @brief A field counts as secret when one of the names occurs in it as
 *        whole words, case aside: "key" in "API key", not in "keyboard".
 */
void tst_filecontent::isSecretFieldNameMatchesWholeWords() {
  QFETCH(QString, name);
  QFETCH(bool, secret);
  QCOMPARE(FileContent::isSecretFieldName(
               name, AppSettings::defaultSecretFieldNames()),
           secret);
}

/**
 * @brief The pattern is built once per list, and a changed list is matched
 *        at once: another list, an empty one (nothing is secret), and a
 *        word with pattern characters, which are taken as they are.
 */
void tst_filecontent::isSecretFieldNameFollowsTheList() {
  const QStringList first{QStringLiteral("iban")};
  const QStringList second{QStringLiteral("pin")};
  QVERIFY(FileContent::isSecretFieldName(QStringLiteral("IBAN"), first));
  QVERIFY(!FileContent::isSecretFieldName(QStringLiteral("PIN"), first));
  QVERIFY(FileContent::isSecretFieldName(QStringLiteral("PIN"), second));
  QVERIFY(!FileContent::isSecretFieldName(QStringLiteral("IBAN"), second));
  QVERIFY(!FileContent::isSecretFieldName(QStringLiteral("PIN"), {}));
  QVERIFY(!FileContent::isSecretFieldName(QStringLiteral("PIN"),
                                          {QStringLiteral("  ")}));
  const QStringList special{QStringLiteral("a.b")};
  QVERIFY(FileContent::isSecretFieldName(QStringLiteral("my a.b"), special));
  QVERIFY(!FileContent::isSecretFieldName(QStringLiteral("axb"), special));

  // More words than fit one pattern (PCRE2's size limit): still matched,
  // word by word, not every secret field shown.
  QStringList many;
  for (int i = 0; i < 40000; ++i) {
    many << QStringLiteral("word%1").arg(i);
  }
  QVERIFY2(!QRegularExpression(
                QStringLiteral("(?:%1)").arg(many.join(QLatin1Char('|'))))
                .isValid(),
           "the list must be too large for one pattern");
  QVERIFY(FileContent::isSecretFieldName(QStringLiteral("my word39999"), many));
  QVERIFY(!FileContent::isSecretFieldName(QStringLiteral("keyboard"), many));
}

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
