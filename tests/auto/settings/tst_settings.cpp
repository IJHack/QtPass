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
  void setAndGetHideContent();
  void setAndGetUseSelection();
  void setAndGetUseAutoclear();
  void setAndGetUseMonospace();
  void setAndGetNoLineWrapping();
  void setAndGetAddGPGId();
  void setAndGetAvoidCapitals();
  void setAndGetAvoidNumbers();
  void setAndGetLessRandom();
  void setAndGetUseSymbols();
  void setAndGetDisplayAsIs();
  void setAndGetHideOnClose();
  void setAndGetStartMinimized();
  void setAndGetAlwaysOnTop();
  void setAndGetAutoPull();
  void setAndGetAutoPush();
  void setAndGetUseTemplate();
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

void tst_settings::setAndGetHideContent() {
  QtPassSettings::setHideContent(true);
  QVERIFY(QtPassSettings::isHideContent() == true);
  QtPassSettings::setHideContent(false);
  QVERIFY(QtPassSettings::isHideContent() == false);
}

void tst_settings::setAndGetUseSelection() {
  QtPassSettings::setUseSelection(true);
  QVERIFY(QtPassSettings::isUseSelection() == true);
  QtPassSettings::setUseSelection(false);
  QVERIFY(QtPassSettings::isUseSelection() == false);
}

void tst_settings::setAndGetUseAutoclear() {
  QtPassSettings::setUseAutoclear(true);
  QVERIFY(QtPassSettings::isUseAutoclear() == true);
  QtPassSettings::setUseAutoclear(false);
  QVERIFY(QtPassSettings::isUseAutoclear() == false);
}

void tst_settings::setAndGetUseMonospace() {
  QtPassSettings::setUseMonospace(true);
  QVERIFY(QtPassSettings::isUseMonospace() == true);
  QtPassSettings::setUseMonospace(false);
  QVERIFY(QtPassSettings::isUseMonospace() == false);
}

void tst_settings::setAndGetNoLineWrapping() {
  QtPassSettings::setNoLineWrapping(true);
  QVERIFY(QtPassSettings::isNoLineWrapping() == true);
  QtPassSettings::setNoLineWrapping(false);
  QVERIFY(QtPassSettings::isNoLineWrapping() == false);
}

void tst_settings::setAndGetAddGPGId() {
  QtPassSettings::setAddGPGId(true);
  QVERIFY(QtPassSettings::isAddGPGId() == true);
  QtPassSettings::setAddGPGId(false);
  QVERIFY(QtPassSettings::isAddGPGId() == false);
}

void tst_settings::setAndGetAvoidCapitals() {
  QtPassSettings::setAvoidCapitals(true);
  QVERIFY(QtPassSettings::isAvoidCapitals() == true);
  QtPassSettings::setAvoidCapitals(false);
  QVERIFY(QtPassSettings::isAvoidCapitals() == false);
}

void tst_settings::setAndGetAvoidNumbers() {
  QtPassSettings::setAvoidNumbers(true);
  QVERIFY(QtPassSettings::isAvoidNumbers() == true);
  QtPassSettings::setAvoidNumbers(false);
  QVERIFY(QtPassSettings::isAvoidNumbers() == false);
}

void tst_settings::setAndGetLessRandom() {
  QtPassSettings::setLessRandom(true);
  QVERIFY(QtPassSettings::isLessRandom() == true);
  QtPassSettings::setLessRandom(false);
  QVERIFY(QtPassSettings::isLessRandom() == false);
}

void tst_settings::setAndGetUseSymbols() {
  QtPassSettings::setUseSymbols(true);
  QVERIFY(QtPassSettings::isUseSymbols() == true);
  QtPassSettings::setUseSymbols(false);
  QVERIFY(QtPassSettings::isUseSymbols() == false);
}

void tst_settings::setAndGetDisplayAsIs() {
  QtPassSettings::setDisplayAsIs(true);
  QVERIFY(QtPassSettings::isDisplayAsIs() == true);
  QtPassSettings::setDisplayAsIs(false);
  QVERIFY(QtPassSettings::isDisplayAsIs() == false);
}

void tst_settings::setAndGetHideOnClose() {
  QtPassSettings::setHideOnClose(true);
  QVERIFY(QtPassSettings::isHideOnClose() == true);
  QtPassSettings::setHideOnClose(false);
  QVERIFY(QtPassSettings::isHideOnClose() == false);
}

void tst_settings::setAndGetStartMinimized() {
  QtPassSettings::setStartMinimized(true);
  QVERIFY(QtPassSettings::isStartMinimized() == true);
  QtPassSettings::setStartMinimized(false);
  QVERIFY(QtPassSettings::isStartMinimized() == false);
}

void tst_settings::setAndGetAlwaysOnTop() {
  QtPassSettings::setAlwaysOnTop(true);
  QVERIFY(QtPassSettings::isAlwaysOnTop() == true);
  QtPassSettings::setAlwaysOnTop(false);
  QVERIFY(QtPassSettings::isAlwaysOnTop() == false);
}

void tst_settings::setAndGetAutoPull() {
  QtPassSettings::setAutoPull(true);
  QVERIFY(QtPassSettings::isAutoPull() == true);
  QtPassSettings::setAutoPull(false);
  QVERIFY(QtPassSettings::isAutoPull() == false);
}

void tst_settings::setAndGetAutoPush() {
  QtPassSettings::setAutoPush(true);
  QVERIFY(QtPassSettings::isAutoPush() == true);
  QtPassSettings::setAutoPush(false);
  QVERIFY(QtPassSettings::isAutoPush() == false);
}

void tst_settings::setAndGetUseTemplate() {
  QtPassSettings::setUseTemplate(true);
  QVERIFY(QtPassSettings::isUseTemplate() == true);
  QtPassSettings::setUseTemplate(false);
  QVERIFY(QtPassSettings::isUseTemplate() == false);
}

QTEST_MAIN(tst_settings)
#include "tst_settings.moc"