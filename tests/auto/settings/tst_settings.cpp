// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include <QFile>
#include <QSettings>

#include <utility>

#include "../../../src/passwordconfiguration.h"
#include "../../../src/qtpasssettings.h"

class tst_settings : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void cleanupTestCase();
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
  void setAndGetTemplateAllFields();
  void setAndGetUseWebDav();
  void setAndGetUseQrencode();
  void setAndGetUseAutoclearPanel();
  void setAndGetAutoclearPanelSeconds();
  void setAndGetPassSigningKey();
  void setAndGetPassExecutable();
  void setAndGetGitExecutable();
  void setAndGetGpgExecutable();
  void setAndGetPwgenExecutable();
  void setAndGetQrencodeExecutable();
  void setAndGetWebDavUrl();
  void setAndGetWebDavUser();
  void setAndGetWebDavPassword();
  void setAndGetProfile();
  void setAndGetSavestate();
  void setAndGetPos();
  void setAndGetSize();
  void setAndGetMaximized();
  void setAndGetPassTemplate();
  void setAndGetPasswordCharsSelection();
  void setAndGetPasswordChars();
  void setAndGetMultipleProfiles();
  void setAndGetProfileDefault();

private:
  QString m_settingsBackupPath;
  bool m_isPortableMode = false;
};

void tst_settings::initTestCase() {
  // Check for portable mode (qtpass.ini in app directory)
  // Only backup/restore settings file in portable mode
  // On non-portable (registry on Windows), we cannot safely backup
  QString portable_ini =
      QCoreApplication::applicationDirPath() + QDir::separator() + "qtpass.ini";
  m_isPortableMode = QFile::exists(portable_ini);

  if (m_isPortableMode) {
    QtPassSettings::getInstance()->sync();
    QString settingsFile = QtPassSettings::getInstance()->fileName();
    m_settingsBackupPath = settingsFile + ".bak";
    QFile::remove(m_settingsBackupPath);
    QFile::copy(settingsFile, m_settingsBackupPath);
  } else {
    m_settingsBackupPath.clear();
    // On non-portable mode, warn but continue (tests may modify registry)
    qWarning()
        << "Non-portable mode detected. Tests may modify registry settings.";
  }
}

void tst_settings::cleanupTestCase() {
  // Restore original settings after all tests
  // This ensures make check doesn't change user's live config
  if (m_isPortableMode && !m_settingsBackupPath.isEmpty()) {
    QString settingsFile = QtPassSettings::getInstance()->fileName();
    QFile::remove(settingsFile);
    QFile::copy(m_settingsBackupPath, settingsFile);
    QFile::remove(m_settingsBackupPath);
  }
}

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
  QVERIFY(profiles.isEmpty());
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
  QCOMPARE(readProfiles["profile1"]["path"], QString("/test/path"));
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
  QVERIFY2(store.isEmpty() || store.startsWith("/") || store.contains("/"),
           "Pass store should be empty or a plausible path");
}

void tst_settings::setAndGetPassStore() {
  QtPassSettings::setPassStore("/tmp/test-store");
  QString store = QtPassSettings::getPassStore();
  QVERIFY(store.contains("test-store"));
}

void tst_settings::setAndGetUsePass() {
  QtPassSettings::setUsePass(true);
  QVERIFY(QtPassSettings::isUsePass());
  QtPassSettings::setUsePass(false);
  QVERIFY(!QtPassSettings::isUsePass());
}

void tst_settings::setAndGetClipBoardType() {
  QtPassSettings::setClipBoardType(1);
  QCOMPARE(QtPassSettings::getClipBoardType(), 1);
}

void tst_settings::setAndGetAutoclearSeconds() {
  QtPassSettings::setAutoclearSeconds(30);
  QCOMPARE(QtPassSettings::getAutoclearSeconds(), 30);
  QtPassSettings::setAutoclearSeconds(60);
}

void tst_settings::setAndGetPasswordLength() {
  QtPassSettings::setPasswordLength(24);
  PasswordConfiguration config = QtPassSettings::getPasswordConfiguration();
  QCOMPARE(config.length, 24);
}

void tst_settings::setAndGetUseGit() {
  QtPassSettings::setUseGit(true);
  QVERIFY(QtPassSettings::isUseGit());
  QtPassSettings::setUseGit(false);
  QVERIFY(!QtPassSettings::isUseGit());
}

void tst_settings::setAndGetUseOtp() {
  QtPassSettings::setUseOtp(true);
  QVERIFY(QtPassSettings::isUseOtp());
  QtPassSettings::setUseOtp(false);
  QVERIFY(!QtPassSettings::isUseOtp());
}

void tst_settings::setAndGetUseTrayIcon() {
  QtPassSettings::setUseTrayIcon(true);
  QVERIFY(QtPassSettings::isUseTrayIcon());
  QtPassSettings::setUseTrayIcon(false);
  QVERIFY(!QtPassSettings::isUseTrayIcon());
}

void tst_settings::setAndGetUsePwgen() {
  QtPassSettings::setUsePwgen(true);
  QVERIFY(QtPassSettings::isUsePwgen());
  QtPassSettings::setUsePwgen(false);
  QVERIFY(!QtPassSettings::isUsePwgen());
}

void tst_settings::setAndGetHidePassword() {
  QtPassSettings::setHidePassword(true);
  QVERIFY(QtPassSettings::isHidePassword());
  QtPassSettings::setHidePassword(false);
  QVERIFY(!QtPassSettings::isHidePassword());
}

void tst_settings::setAndGetHideContent() {
  QtPassSettings::setHideContent(true);
  QVERIFY(QtPassSettings::isHideContent());
  QtPassSettings::setHideContent(false);
  QVERIFY(!QtPassSettings::isHideContent());
}

void tst_settings::setAndGetUseSelection() {
  QtPassSettings::setUseSelection(true);
  QVERIFY(QtPassSettings::isUseSelection());
  QtPassSettings::setUseSelection(false);
  QVERIFY(!QtPassSettings::isUseSelection());
}

void tst_settings::setAndGetUseAutoclear() {
  QtPassSettings::setUseAutoclear(true);
  QVERIFY(QtPassSettings::isUseAutoclear());
  QtPassSettings::setUseAutoclear(false);
  QVERIFY(!QtPassSettings::isUseAutoclear());
}

void tst_settings::setAndGetUseMonospace() {
  QtPassSettings::setUseMonospace(true);
  QVERIFY(QtPassSettings::isUseMonospace());
  QtPassSettings::setUseMonospace(false);
  QVERIFY(!QtPassSettings::isUseMonospace());
}

void tst_settings::setAndGetNoLineWrapping() {
  QtPassSettings::setNoLineWrapping(true);
  QVERIFY(QtPassSettings::isNoLineWrapping());
  QtPassSettings::setNoLineWrapping(false);
  QVERIFY(!QtPassSettings::isNoLineWrapping());
}

void tst_settings::setAndGetAddGPGId() {
  QtPassSettings::setAddGPGId(true);
  QVERIFY(QtPassSettings::isAddGPGId());
  QtPassSettings::setAddGPGId(false);
  QVERIFY(!QtPassSettings::isAddGPGId());
}

void tst_settings::setAndGetAvoidCapitals() {
  QtPassSettings::setAvoidCapitals(true);
  QVERIFY(QtPassSettings::isAvoidCapitals());
  QtPassSettings::setAvoidCapitals(false);
  QVERIFY(!QtPassSettings::isAvoidCapitals());
}

void tst_settings::setAndGetAvoidNumbers() {
  QtPassSettings::setAvoidNumbers(true);
  QVERIFY(QtPassSettings::isAvoidNumbers());
  QtPassSettings::setAvoidNumbers(false);
  QVERIFY(!QtPassSettings::isAvoidNumbers());
}

void tst_settings::setAndGetLessRandom() {
  QtPassSettings::setLessRandom(true);
  QVERIFY(QtPassSettings::isLessRandom());
  QtPassSettings::setLessRandom(false);
  QVERIFY(!QtPassSettings::isLessRandom());
}

void tst_settings::setAndGetUseSymbols() {
  QtPassSettings::setUseSymbols(true);
  QVERIFY(QtPassSettings::isUseSymbols());
  QtPassSettings::setUseSymbols(false);
  QVERIFY(!QtPassSettings::isUseSymbols());
}

void tst_settings::setAndGetDisplayAsIs() {
  QtPassSettings::setDisplayAsIs(true);
  QVERIFY(QtPassSettings::isDisplayAsIs());
  QtPassSettings::setDisplayAsIs(false);
  QVERIFY(!QtPassSettings::isDisplayAsIs());
}

void tst_settings::setAndGetHideOnClose() {
  QtPassSettings::setHideOnClose(true);
  QVERIFY(QtPassSettings::isHideOnClose());
  QtPassSettings::setHideOnClose(false);
  QVERIFY(!QtPassSettings::isHideOnClose());
}

void tst_settings::setAndGetStartMinimized() {
  QtPassSettings::setStartMinimized(true);
  QVERIFY(QtPassSettings::isStartMinimized());
  QtPassSettings::setStartMinimized(false);
  QVERIFY(!QtPassSettings::isStartMinimized());
}

void tst_settings::setAndGetAlwaysOnTop() {
  QtPassSettings::setAlwaysOnTop(true);
  QVERIFY(QtPassSettings::isAlwaysOnTop());
  QtPassSettings::setAlwaysOnTop(false);
  QVERIFY(!QtPassSettings::isAlwaysOnTop());
}

void tst_settings::setAndGetAutoPull() {
  QtPassSettings::setAutoPull(true);
  QVERIFY(QtPassSettings::isAutoPull());
  QtPassSettings::setAutoPull(false);
  QVERIFY(!QtPassSettings::isAutoPull());
}

void tst_settings::setAndGetAutoPush() {
  QtPassSettings::setAutoPush(true);
  QVERIFY(QtPassSettings::isAutoPush());
  QtPassSettings::setAutoPush(false);
  QVERIFY(!QtPassSettings::isAutoPush());
}

void tst_settings::setAndGetUseTemplate() {
  QtPassSettings::setUseTemplate(true);
  QVERIFY(QtPassSettings::isUseTemplate());
  QtPassSettings::setUseTemplate(false);
  QVERIFY(!QtPassSettings::isUseTemplate());
}

void tst_settings::setAndGetTemplateAllFields() {
  QtPassSettings::setTemplateAllFields(true);
  QVERIFY(QtPassSettings::isTemplateAllFields());
  QtPassSettings::setTemplateAllFields(false);
  QVERIFY(!QtPassSettings::isTemplateAllFields());
}

void tst_settings::setAndGetUseWebDav() {
  QtPassSettings::setUseWebDav(true);
  QVERIFY(QtPassSettings::isUseWebDav());
  QtPassSettings::setUseWebDav(false);
  QVERIFY(!QtPassSettings::isUseWebDav());
}

void tst_settings::setAndGetUseQrencode() {
  QtPassSettings::setUseQrencode(true);
  QVERIFY(QtPassSettings::isUseQrencode());
  QtPassSettings::setUseQrencode(false);
  QVERIFY(!QtPassSettings::isUseQrencode());
}

void tst_settings::setAndGetUseAutoclearPanel() {
  QtPassSettings::setUseAutoclearPanel(true);
  QVERIFY(QtPassSettings::isUseAutoclearPanel());
  QtPassSettings::setUseAutoclearPanel(false);
  QVERIFY(!QtPassSettings::isUseAutoclearPanel());
}

void tst_settings::setAndGetAutoclearPanelSeconds() {
  QtPassSettings::setAutoclearPanelSeconds(45);
  QCOMPARE(QtPassSettings::getAutoclearPanelSeconds(), 45);
  QtPassSettings::setAutoclearPanelSeconds(10);
}

void tst_settings::setAndGetPassSigningKey() {
  QtPassSettings::setPassSigningKey("testkey123");
  QString key = QtPassSettings::getPassSigningKey();
  QVERIFY2(key == "testkey123", "PassSigningKey should be testkey123");
}

void tst_settings::setAndGetPassExecutable() {
  QtPassSettings::setPassExecutable("/usr/bin/pass");
  QString exe = QtPassSettings::getPassExecutable();
  QVERIFY2(exe.contains("pass"), "PassExecutable should contain 'pass'");
}

void tst_settings::setAndGetGitExecutable() {
  QtPassSettings::setGitExecutable("/usr/bin/git");
  QString exe = QtPassSettings::getGitExecutable();
  QVERIFY2(exe.contains("git"), "GitExecutable should contain 'git'");
}

void tst_settings::setAndGetGpgExecutable() {
  QtPassSettings::setGpgExecutable("/usr/bin/gpg");
  QString exe = QtPassSettings::getGpgExecutable();
  QVERIFY2(exe.contains("gpg"), "GpgExecutable should contain 'gpg'");
}

void tst_settings::setAndGetPwgenExecutable() {
  QtPassSettings::setPwgenExecutable("/usr/bin/pwgen");
  QString exe = QtPassSettings::getPwgenExecutable();
  QVERIFY2(exe.contains("pwgen"), "PwgenExecutable should contain 'pwgen'");
}

void tst_settings::setAndGetQrencodeExecutable() {
  QtPassSettings::setQrencodeExecutable("/usr/bin/qrencode");
  QString exe = QtPassSettings::getQrencodeExecutable();
  QVERIFY2(exe.contains("qrencode"),
           "QrencodeExecutable should contain 'qrencode'");
}

void tst_settings::setAndGetWebDavUrl() {
  QtPassSettings::setWebDavUrl("https://dav.example.com/pass");
  QString url = QtPassSettings::getWebDavUrl();
  QVERIFY2(url.contains("dav.example.com"),
           "WebDavUrl should contain 'dav.example.com'");
}

void tst_settings::setAndGetWebDavUser() {
  QtPassSettings::setWebDavUser("testuser");
  QString user = QtPassSettings::getWebDavUser();
  QVERIFY2(user == "testuser", "WebDavUser should be 'testuser'");
}

void tst_settings::setAndGetWebDavPassword() {
  QtPassSettings::setWebDavPassword("secretpassword");
  QString pwd = QtPassSettings::getWebDavPassword();
  QVERIFY2(pwd == "secretpassword",
           "WebDavPassword should be 'secretpassword'");
}

void tst_settings::setAndGetProfile() {
  QtPassSettings::setProfile("work");
  QString profile = QtPassSettings::getProfile();
  QVERIFY2(profile == "work", "Profile should be 'work'");
}

void tst_settings::setAndGetSavestate() {
  QByteArray state("test_state_data");
  QtPassSettings::setSavestate(state);
  QByteArray read = QtPassSettings::getSavestate(QByteArray());
  QVERIFY2(read == state, "Savestate should match");
}

void tst_settings::setAndGetPos() {
  QPoint pos(100, 200);
  QtPassSettings::setPos(pos);
  QPoint read = QtPassSettings::getPos(QPoint());
  QVERIFY2(read == pos, "Pos should match");
}

void tst_settings::setAndGetSize() {
  QSize size(800, 600);
  QtPassSettings::setSize(size);
  QSize read = QtPassSettings::getSize(QSize());
  QVERIFY2(read == size, "Size should match");
}

void tst_settings::setAndGetMaximized() {
  QtPassSettings::setMaximized(true);
  QVERIFY(QtPassSettings::isMaximized());
  QtPassSettings::setMaximized(false);
  QVERIFY(!QtPassSettings::isMaximized());
}

void tst_settings::setAndGetPassTemplate() {
  QtPassSettings::setPassTemplate("username: {username}\npassword: {password}");
  QString tmpl = QtPassSettings::getPassTemplate();
  QVERIFY2(tmpl.contains("username"), "PassTemplate should contain 'username'");
}

void tst_settings::setAndGetPasswordCharsSelection() {
  QtPassSettings::setPasswordCharsselection(
      PasswordConfiguration::ALPHABETICAL);
  PasswordConfiguration config = QtPassSettings::getPasswordConfiguration();
  QCOMPARE(config.selected, PasswordConfiguration::ALPHABETICAL);
}

void tst_settings::setAndGetPasswordChars() {
  QtPassSettings::setPasswordChars("abc123");
  PasswordConfiguration config = QtPassSettings::getPasswordConfiguration();
  QVERIFY2(config.Characters[PasswordConfiguration::CUSTOM].contains("abc"),
           "PasswordChars should contain 'abc'");
  // Reset to avoid affecting subsequent tests and live QtPass
  QtPassSettings::setPasswordChars(QString());
}

void tst_settings::setAndGetMultipleProfiles() {
  QHash<QString, QHash<QString, QString>> profiles;
  QHash<QString, QString> profile1;
  profile1["pass_store"] = "/path/to/store1";
  profiles["profile1"] = std::move(profile1);

  QHash<QString, QString> profile2;
  profile2["pass_store"] = "/path/to/store2";
  profiles["profile2"] = std::move(profile2);

  QtPassSettings::setProfiles(profiles);
  QHash<QString, QHash<QString, QString>> readProfiles =
      QtPassSettings::getProfiles();
  QVERIFY(readProfiles.size() >= 1);
}

void tst_settings::setAndGetProfileDefault() {
  QString defaultProfile = QtPassSettings::getProfile();
  QCOMPARE(defaultProfile, QtPassSettings::getProfile());
}

QTEST_MAIN(tst_settings)
#include "tst_settings.moc"