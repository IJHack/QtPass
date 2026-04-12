// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <algorithm>

#include "../../../src/gpgkeystate.h"
#include "../../../src/gpgkeystate_p.h"
#include "../../../src/userinfo.h"

class tst_gpgkeystate : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void parseMultiKeyPublic();
  void parseMultiKeyPublic_data();
  void parseSecretKeys();
  void parseSecretKeys_data();
  void parseSingleKey();
  void parseSingleKey_data();
  void parseKeyRollover();
  void parseKeyRollover_data();
  void classifyRecordTypes();
  void classifyRecordEmpty();
  void handlePubSecEmptyFields();
  void handlePubSecShortList();
  void handleFprEdgeCases();
};

void tst_gpgkeystate::parseMultiKeyPublic() {
  QFETCH(QString, input);
  QFETCH(int, expectedCount);

  const bool includeSecretKeys = false;
  QList<UserInfo> result = parseGpgColonOutput(input, includeSecretKeys);

  QVERIFY2(result.size() == expectedCount,
           qPrintable(QString("Expected %1 keys, got %2")
                          .arg(expectedCount)
                          .arg(result.size())));

  for (const UserInfo &user : result) {
    QVERIFY2(!user.have_secret,
             "Public keys should not have secret capability");
  }

  if (expectedCount > 0) {
    QVERIFY2(!result.at(0).key_id.isEmpty(), "First key should have key_id");
    QVERIFY2(!result.at(0).name.isEmpty(), "First key should have name");
  }
  if (expectedCount > 1) {
    QVERIFY2(!result.at(1).key_id.isEmpty(), "Second key should have key_id");
    QVERIFY2(!result.at(1).name.isEmpty(), "Second key should have name");
  }
}

void tst_gpgkeystate::parseMultiKeyPublic_data() {
  QTest::addColumn<QString>("input");
  QTest::addColumn<int>("expectedCount");

  QString input = R"(tru::1:1775005973:0:3:1:5
pub:u:4096:1:31850CF72D9CDDE9:1774947438:::u:::escarESCA::::::23::0:
fpr:::::::::13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9:
uid:u::::1774947438::CBF23008234AA5F88824CE76140F482FAE34923E::Anne Jan Brouwer <henk@annejan.com>::::::::::0:
sub:u:4096:1:6DF67C6BAD8383CB:1774947438::::::esa::::::23:
pub:f:4096:1:693A0AF3FA364E76:1775005968:::f:::escarESCA::::::23::0:
fpr:::::::::4EF2550F79F4E9E68B09F71D693A0AF3FA364E76:
uid:f::::1775005968::8AA011711F27F6E08DF71653718C299A13B323A0::Harrie de Bot <harrie@annejan.com>::::::::::0:)";

  QTest::newRow("two-public-keys") << input << 2;
}

void tst_gpgkeystate::parseSecretKeys() {
  QFETCH(QString, input);
  QFETCH(int, expectedCount);
  QFETCH(bool, expectHaveSecret);

  const bool includeSecretKeys = true;
  QList<UserInfo> result = parseGpgColonOutput(input, includeSecretKeys);

  QVERIFY2(result.size() == expectedCount,
           qPrintable(QString("Expected %1 keys, got %2")
                          .arg(expectedCount)
                          .arg(result.size())));

  if (expectedCount > 0) {
    QVERIFY(!result.isEmpty());
    for (int i = 0; i < result.size(); ++i) {
      const UserInfo &user = result.at(i);
      QVERIFY2(
          user.have_secret == expectHaveSecret,
          qPrintable(QString("Key at index %1 has have_secret=%2, expected %3")
                         .arg(i)
                         .arg(user.have_secret)
                         .arg(expectHaveSecret)));
    }
  }
}

void tst_gpgkeystate::parseSecretKeys_data() {
  QTest::addColumn<QString>("input");
  QTest::addColumn<int>("expectedCount");
  QTest::addColumn<bool>("expectHaveSecret");

  QString input =
      R"(sec:u:4096:1:31850CF72D9CDDE9:1774947438:::u:::escarESCA:::+:::23::0:
fpr:::::::::13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9:
uid:u::::1774947438::CBF23008234AA5F88824CE76140F482FAE34923E::Anne Jan Brouwer <henk@annejan.com>::::::::::0:
ssb:u:4096:1:6DF67C6BAD8383CB:1774947438::::::esa:::+:::23:)";

  QTest::newRow("single-secret-key") << input << 1 << true;
}

void tst_gpgkeystate::parseSingleKey() {
  QFETCH(QString, input);
  QFETCH(int, expectedCount);
  QFETCH(QString, expectedKeyId);

  const bool includeSecretKeys = false;
  QList<UserInfo> result = parseGpgColonOutput(input, includeSecretKeys);
  QVERIFY2(result.size() == expectedCount,
           qPrintable(QString("Expected %1 keys, got %2")
                          .arg(expectedCount)
                          .arg(result.size())));

  if (!result.isEmpty()) {
    QVERIFY2(!result.first().key_id.isEmpty(),
             "Parsed key should have a key_id");
    if (!expectedKeyId.isEmpty()) {
      QVERIFY2(result.first().key_id == expectedKeyId,
               qPrintable(QString("Expected key_id %1, got %2")
                              .arg(expectedKeyId)
                              .arg(result.first().key_id)));
    }
  }
}

void tst_gpgkeystate::parseSingleKey_data() {
  QTest::addColumn<QString>("input");
  QTest::addColumn<int>("expectedCount");
  QTest::addColumn<QString>("expectedKeyId");

  QTest::newRow("pub-only")
      << "pub:u:4096:1:ABC123:1774947438:::u::::::23::0:" << 1 << "ABC123";
  QTest::newRow("pub-with-fpr") << "pub:u:4096:1:ABC123:1774947438:::u::::::23:"
                                   ":0:\nfpr:::::::::FINGERPRINT123456789:"
                                << 1 << "ABC123";
}

void tst_gpgkeystate::parseKeyRollover() {
  QFETCH(QString, input);
  QFETCH(int, expectedCount);

  const bool includeSecretKeys = false;
  QList<UserInfo> result = parseGpgColonOutput(input, includeSecretKeys);
  QVERIFY2(result.size() == expectedCount,
           qPrintable(QString("Expected %1 keys, got %2")
                          .arg(expectedCount)
                          .arg(result.size())));

  auto containsKeyId = [&](const QString &keyId) {
    return std::any_of(
        result.cbegin(), result.cend(),
        [&](const UserInfo &user) { return user.key_id == keyId; });
  };

  QVERIFY2(containsKeyId("AAA111"), "Expected AAA111 key to be parsed");
  QVERIFY2(containsKeyId("BBB222"), "Expected BBB222 key to be parsed");
  QVERIFY2(containsKeyId("CCC333"), "Expected CCC333 key to be parsed");
}

void tst_gpgkeystate::parseKeyRollover_data() {
  QTest::addColumn<QString>("input");
  QTest::addColumn<int>("expectedCount");

  QString input = R"(pub:u:4096:1:AAA111:1774947438:::u::::
fpr:::::::::AAA111FINGERPRINT:
uid:u::::1774947438::NAME1::user1@test.com:::::::0:
pub:u:4096:1:BBB222:1774947438:::u::::
fpr:::::::::BBB222FINGERPRINT:
uid:u::::1774947438::NAME2::user2@test.com:::::::0:
pub:u:4096:1:CCC333:1774947438:::u::::
fpr:::::::::CCC333FINGERPRINT:
uid:u::::1774947438::NAME3::user3@test.com:::::::0:)";

  QTest::newRow("three-keys-rollover") << input << 3;
}

void tst_gpgkeystate::classifyRecordTypes() {
  QVERIFY2(classifyRecord("pub") == GpgRecordType::Pub,
           "Should classify pub record");
  QVERIFY2(classifyRecord("sec") == GpgRecordType::Sec,
           "Should classify sec record");
  QVERIFY2(classifyRecord("uid") == GpgRecordType::Uid,
           "Should classify uid record");
  QVERIFY2(classifyRecord("fpr") == GpgRecordType::Fpr,
           "Should classify fpr record");
  QVERIFY2(classifyRecord("sub") == GpgRecordType::Sub,
           "Should classify sub record");
  QVERIFY2(classifyRecord("ssb") == GpgRecordType::Ssb,
           "Should classify ssb record");
  QVERIFY2(classifyRecord("grp") == GpgRecordType::Grp,
           "Should classify grp record");
  QVERIFY2(classifyRecord("unknown") == GpgRecordType::Unknown,
           "Should classify unknown record types as Unknown");
}

void tst_gpgkeystate::classifyRecordEmpty() {
  QVERIFY2(classifyRecord("") == GpgRecordType::Unknown,
           "Should classify empty as Unknown");
  QVERIFY2(classifyRecord("PUB") == GpgRecordType::Unknown,
           "Should be case-sensitive");
  QVERIFY2(classifyRecord("pubx") == GpgRecordType::Unknown,
           "Should not match partial");
}

void tst_gpgkeystate::handlePubSecEmptyFields() {
  UserInfo user;
  QStringList props;
  props << "pub"
        << "" // validity empty
        << "4096"
        << "1"
        << "keyId00001"
        << "" // created empty
        << "" // expiry empty
        << ""
        << ""
        << "Test User"; // name

  handlePubSecRecord(props, false, user);

  QVERIFY2(user.key_id == "keyId00001", "Should parse key_id");
  QVERIFY2(user.name == "Test User", "Should parse name");
  QVERIFY2(user.validity == '-', "Empty validity should be dash");
  QVERIFY2(!user.created.isValid(), "Empty created should be invalid");
  QVERIFY2(!user.expiry.isValid(), "Empty expiry should be invalid");
  QVERIFY2(!user.have_secret, "Should not have secret");
}

void tst_gpgkeystate::handlePubSecShortList() {
  UserInfo user;
  QStringList shortProps;
  shortProps.append("pub");
  shortProps.append("");
  shortProps.append("4096");
  shortProps.append("1");
  shortProps.append(""); // 5: created
  handlePubSecRecord(shortProps, true, user);
  QVERIFY2(user.key_id.isEmpty(), "Short list should be ignored");
  QVERIFY2(user.name.isEmpty(), "Short list ignored: name empty");
  QVERIFY2(user.validity == '-', "Short list ignored: default validity");
  QVERIFY2(!user.created.isValid(), "Short list ignored: created invalid");
  QVERIFY2(!user.expiry.isValid(), "Short list ignored: expiry invalid");
  QVERIFY2(!user.have_secret, "Short list ignored: no secret");
}

void tst_gpgkeystate::handleFprEdgeCases() {
  UserInfo user;
  user.key_id = "id123";

  QStringList emptyProps;
  for (int i = 0; i < 10; ++i)
    emptyProps.append("");
  emptyProps[9] = "";
  handleFprRecord(emptyProps, user);
  QVERIFY2(user.key_id == "id123", "Empty fpr should not change key_id");

  QStringList nonMatchingProps;
  for (int i = 0; i < 10; ++i)
    nonMatchingProps.append("");
  nonMatchingProps[9] = "otherFingerprint";
  handleFprRecord(nonMatchingProps, user);
  QVERIFY2(user.key_id == "id123", "Non-matching fpr should not change key_id");

  user.key_id = "id456";
  QStringList matchingProps;
  for (int i = 0; i < 10; ++i)
    matchingProps.append("");
  matchingProps[9] = "full fingerprint id456";
  handleFprRecord(matchingProps, user);
  QVERIFY2(user.key_id == "full fingerprint id456",
           "fpr ending with key_id should update to full fingerprint");
}

QTEST_MAIN(tst_gpgkeystate)
#include "tst_gpgkeystate.moc"
