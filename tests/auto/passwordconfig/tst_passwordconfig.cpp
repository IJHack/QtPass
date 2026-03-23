// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
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
};

void tst_passwordconfig::initTestCase() {}

void tst_passwordconfig::passwordConfigurationDefaults() {
  PasswordConfiguration config;
  QVERIFY(config.length == 16);
  QVERIFY(config.selected == PasswordConfiguration::ALLCHARS);
  QVERIFY(!config.Characters[PasswordConfiguration::ALLCHARS].isEmpty());
  QVERIFY(!config.Characters[PasswordConfiguration::ALPHABETICAL].isEmpty());
  QVERIFY(!config.Characters[PasswordConfiguration::ALPHANUMERIC].isEmpty());
}

void tst_passwordconfig::passwordConfigurationSetters() {
  QtPassSettings::setPasswordLength(24);
  QtPassSettings::setPasswordCharsselection(
      PasswordConfiguration::ALPHANUMERIC);
  QtPassSettings::setPasswordCharsselection(3);

  PasswordConfiguration config = QtPassSettings::getPasswordConfiguration();
  QVERIFY(config.length == 24);
  QVERIFY(config.selected == 3);
}

void tst_passwordconfig::passwordConfigurationCharacterSets() {
  PasswordConfiguration config;
  QVERIFY(config.Characters[PasswordConfiguration::ALLCHARS].length() >
          config.Characters[PasswordConfiguration::ALPHABETICAL].length());
  QVERIFY(config.Characters[PasswordConfiguration::ALPHANUMERIC].length() >
          config.Characters[PasswordConfiguration::ALPHABETICAL].length());
}

QTEST_MAIN(tst_passwordconfig)
#include "tst_passwordconfig.moc"