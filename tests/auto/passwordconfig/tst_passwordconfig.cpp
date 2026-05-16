// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QString>
#include <QtTest>

#include "../../../src/passwordconfiguration.h"
#include "../../../src/qtpasssettings.h"

class tst_passwordconfig : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void passwordConfigurationDefaults();
  void passwordConfigurationSetters();
  void passwordConfigurationCharacterSets();
  void passwordConfigurationLength();
  void passwordConfigurationCustomChars();
  void charsetCountIsCorrect();
  void alphanumericContainsNoSpecialChars();
  void alphabeticalContainsNoDigits();
  void customCharsDefaultMatchAllChars();
};

void tst_passwordconfig::initTestCase() {
  // Reset any leftover test settings to ensure clean state
  QtPassSettings::setPasswordChars(QString());
  QtPassSettings::setPasswordCharsSelection(PasswordConfiguration::ALLCHARS);
}

void tst_passwordconfig::passwordConfigurationDefaults() {
  PasswordConfiguration config;
  QCOMPARE(config.length, 16);
  QCOMPARE(config.selected, PasswordConfiguration::ALLCHARS);
  QVERIFY(!config.Characters[PasswordConfiguration::ALLCHARS].isEmpty());
  QVERIFY(!config.Characters[PasswordConfiguration::ALPHABETICAL].isEmpty());
  QVERIFY(!config.Characters[PasswordConfiguration::ALPHANUMERIC].isEmpty());
}

void tst_passwordconfig::passwordConfigurationSetters() {
  // Reset any previous test settings to ensure clean state
  QtPassSettings::setPasswordChars(QString());
  QtPassSettings::setPasswordCharsSelection(PasswordConfiguration::ALLCHARS);

  QtPassSettings::setPasswordLength(24);
  QtPassSettings::setPasswordCharsSelection(
      PasswordConfiguration::ALPHANUMERIC);
  QtPassSettings::setPasswordCharsSelection(3);

  PasswordConfiguration config = QtPassSettings::getPasswordConfiguration();
  QCOMPARE(config.length, 24);
  QCOMPARE(config.selected, 3);

  // Reset after test
  QtPassSettings::setPasswordCharsSelection(PasswordConfiguration::ALLCHARS);
  QtPassSettings::setPasswordChars(QString());
}

void tst_passwordconfig::passwordConfigurationCharacterSets() {
  PasswordConfiguration config;
  QVERIFY(config.Characters[PasswordConfiguration::ALLCHARS].length() >
          config.Characters[PasswordConfiguration::ALPHABETICAL].length());
  QVERIFY(config.Characters[PasswordConfiguration::ALPHANUMERIC].length() >
          config.Characters[PasswordConfiguration::ALPHABETICAL].length());
}

void tst_passwordconfig::passwordConfigurationLength() {
  PasswordConfiguration config;
  config.length = 0;
  QCOMPARE(config.length, 0);
  config.length = 100;
  QCOMPARE(config.length, 100);
}

void tst_passwordconfig::passwordConfigurationCustomChars() {
  PasswordConfiguration config;
  QString custom = "abc123";
  config.Characters[PasswordConfiguration::CUSTOM] = custom;
  QCOMPARE(config.Characters[PasswordConfiguration::CUSTOM], custom);
}

void tst_passwordconfig::charsetCountIsCorrect() {
  QCOMPARE(static_cast<int>(PasswordConfiguration::CHARSETS_COUNT), 4);
}

void tst_passwordconfig::alphanumericContainsNoSpecialChars() {
  PasswordConfiguration config;
  const QString &chars = config.Characters[PasswordConfiguration::ALPHANUMERIC];
  for (QChar c : chars) {
    QVERIFY2(c.isLetterOrNumber(),
             qPrintable(QString("ALPHANUMERIC contains non-alphanumeric char: "
                                "'%1'")
                            .arg(c)));
  }
}

void tst_passwordconfig::alphabeticalContainsNoDigits() {
  PasswordConfiguration config;
  const QString &chars = config.Characters[PasswordConfiguration::ALPHABETICAL];
  for (QChar c : chars) {
    QVERIFY2(c.isLetter(),
             qPrintable(QString("ALPHABETICAL contains non-letter char: "
                                "'%1'")
                            .arg(c)));
  }
}

void tst_passwordconfig::customCharsDefaultMatchAllChars() {
  PasswordConfiguration config;
  QVERIFY2(config.Characters[PasswordConfiguration::CUSTOM] ==
               config.Characters[PasswordConfiguration::ALLCHARS],
           "CUSTOM character set must default to ALLCHARS");
}

QTEST_MAIN(tst_passwordconfig)
#include "tst_passwordconfig.moc"
