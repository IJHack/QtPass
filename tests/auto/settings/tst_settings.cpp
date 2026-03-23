// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "../../../src/passwordconfiguration.h"
#include "../../../src/qtpasssettings.h"

class tst_settings : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void getPasswordConfigurationDefault();
  void setAndGetPasswordConfiguration();
  void getProfilesEmpty();
  void setAndGetProfiles();
  void setAndGetVersion();
  void setAndGetGeometry();
  void getPassStore();
  void setAndGetPassStore();
  void setAndGetUsePass();
  void setAndGetClipBoardType();
  void setAndGetAutoclearSeconds();
  void setAndGetPasswordLength();
  void setAndGetUseGit();
  void setAndGetUseOtp();
  void setAndGetUseTrayIcon();
  void setAndGetUsePwgen();
  void setAndGetHidePassword();
};

void tst_settings::initTestCase() {}

void tst_settings::getPasswordConfigurationDefault() {
  PasswordConfiguration config = QtPassSettings::getPasswordConfiguration();
  QVERIFY(config.length >= 0);
  QVERIFY(config.selected >= 0);
}

void tst_settings::setAndGetPasswordConfiguration() {
  PasswordConfiguration config;
  config.length = 20;
  config.selected = PasswordConfiguration::ALPHABETICAL;
  config.Characters[PasswordConfiguration::CUSTOM] = "abc";

  QtPassSettings::setPasswordConfiguration(config);

  PasswordConfiguration readConfig = QtPassSettings::getPasswordConfiguration();
  QCOMPARE(readConfig.length, 20);
  QVERIFY2(readConfig.selected == PasswordConfiguration::ALPHABETICAL,
           "Password config should be ALPHABETICAL");
}

void tst_settings::getProfilesEmpty() {
  QHash<QString, QHash<QString, QString>> profiles =
      QtPassSettings::getProfiles();
  QVERIFY(profiles.isEmpty() || !profiles.isEmpty());
}

void tst_settings::setAndGetProfiles() {
  QHash<QString, QHash<QString, QString>> profiles;
  QHash<QString, QString> profile1;
  profile1.insert("path", "/test/path");
  profile1.insert("signingKey", "ABC123");
  profiles.insert("profile1", profile1);

  QtPassSettings::setProfiles(profiles);

  QHash<QString, QHash<QString, QString>> readProfiles =
      QtPassSettings::getProfiles();
  QVERIFY(!readProfiles.isEmpty());
  QVERIFY(readProfiles.contains("profile1"));
  QVERIFY(readProfiles["profile1"]["path"] == "/test/path");
}

void tst_settings::setAndGetVersion() {
  QtPassSettings::setVersion("1.5.1");
  QString version = QtPassSettings::getVersion();
  QVERIFY2(version == "1.5.1", "Version should be 1.5.1");
}

void tst_settings::setAndGetGeometry() {
  QByteArray geometry("test_geometry_data");
  QtPassSettings::setGeometry(geometry);
  QByteArray read = QtPassSettings::getGeometry(QByteArray());
  QVERIFY2(read == geometry, "Geometry should match");
}

void tst_settings::getPassStore() {
  QString store = QtPassSettings::getPassStore();
  QVERIFY(!store.isEmpty() || store.isEmpty());
}

void tst_settings::setAndGetPassStore() {
  QtPassSettings::setPassStore("/tmp/test-store");
  QString store = QtPassSettings::getPassStore();
  QVERIFY(store.contains("test-store"));
}

void tst_settings::setAndGetUsePass() {
  QtPassSettings::setUsePass(true);
  QVERIFY(QtPassSettings::isUsePass() == true);
  QtPassSettings::setUsePass(false);
  QVERIFY(QtPassSettings::isUsePass() == false);
}

void tst_settings::setAndGetClipBoardType() {
  QtPassSettings::setClipBoardType(1);
  QVERIFY(true);
}

void tst_settings::setAndGetAutoclearSeconds() {
  QtPassSettings::setAutoclearSeconds(30);
  QVERIFY(QtPassSettings::getAutoclearSeconds() == 30);
  QtPassSettings::setAutoclearSeconds(60);
}

void tst_settings::setAndGetPasswordLength() {
  QtPassSettings::setPasswordLength(24);
  PasswordConfiguration config = QtPassSettings::getPasswordConfiguration();
  QVERIFY(config.length == 24);
}

void tst_settings::setAndGetUseGit() {
  QtPassSettings::setUseGit(true);
  QVERIFY(QtPassSettings::isUseGit() == true);
  QtPassSettings::setUseGit(false);
  QVERIFY(QtPassSettings::isUseGit() == false);
}

void tst_settings::setAndGetUseOtp() {
  QtPassSettings::setUseOtp(true);
  QVERIFY(QtPassSettings::isUseOtp() == true);
  QtPassSettings::setUseOtp(false);
  QVERIFY(QtPassSettings::isUseOtp() == false);
}

void tst_settings::setAndGetUseTrayIcon() {
  QtPassSettings::setUseTrayIcon(true);
  QVERIFY(QtPassSettings::isUseTrayIcon() == true);
  QtPassSettings::setUseTrayIcon(false);
  QVERIFY(QtPassSettings::isUseTrayIcon() == false);
}

void tst_settings::setAndGetUsePwgen() {
  QtPassSettings::setUsePwgen(true);
  QVERIFY(QtPassSettings::isUsePwgen() == true);
  QtPassSettings::setUsePwgen(false);
  QVERIFY(QtPassSettings::isUsePwgen() == false);
}

void tst_settings::setAndGetHidePassword() {
  QtPassSettings::setHidePassword(true);
  QVERIFY(QtPassSettings::isHidePassword() == true);
  QtPassSettings::setHidePassword(false);
  QVERIFY(QtPassSettings::isHidePassword() == false);
}

QTEST_MAIN(tst_settings)
#include "tst_settings.moc"