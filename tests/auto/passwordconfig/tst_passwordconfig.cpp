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

QTEST_MAIN(tst_passwordconfig)
#include "tst_passwordconfig.moc"