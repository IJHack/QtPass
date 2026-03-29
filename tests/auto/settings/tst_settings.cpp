// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include <QDir>
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
  void boolRoundTrip_data();
  void boolRoundTrip();
  void setAndGetClipBoardType();
  void setAndGetAutoclearSeconds();
  void setAndGetPasswordLength();
  void autoDetectGit();
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
  QHash<QString, QHash<QString, QString>> emptyProfiles;
  QtPassSettings::setProfiles(emptyProfiles);

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

void tst_settings::boolRoundTrip_data() {
  QTest::addColumn<QString>("setting");
  QTest::addColumn<bool>("testValue");

  auto addBool = [](const char *name) {
    QString n(name);
    QTest::newRow(n + "_true") << n << true;
    QTest::newRow(n + "_false") << n << false;
  };

  addBool("usePass");
  addBool("useGit");
  addBool("useOtp");
  addBool("useTrayIcon");
  addBool("usePwgen");
  addBool("hidePassword");
  addBool("hideContent");
  addBool("useSelection");
  addBool("useAutoclear");
  addBool("useMonospace");
  addBool("noLineWrapping");
  addBool("addGPGId");
  addBool("avoidCapitals");
  addBool("avoidNumbers");
  addBool("lessRandom");
  addBool("useSymbols");
  addBool("displayAsIs");
  addBool("hideOnClose");
  addBool("startMinimized");
  addBool("alwaysOnTop");
  addBool("autoPull");
  addBool("autoPush");
  addBool("useTemplate");
  addBool("templateAllFields");
  addBool("useWebDav");
  addBool("useQrencode");
  addBool("useAutoclearPanel");
  addBool("maximized");
}

void tst_settings::boolRoundTrip() {
  QFETCH(QString, setting);
  QFETCH(bool, testValue);

  if (setting == "usePass") {
    QtPassSettings::setUsePass(testValue);
    QCOMPARE(QtPassSettings::isUsePass(), testValue);
  } else if (setting == "useGit") {
    QtPassSettings::setUseGit(testValue);
    QCOMPARE(QtPassSettings::isUseGit(), testValue);
  } else if (setting == "useOtp") {
    QtPassSettings::setUseOtp(testValue);
    QCOMPARE(QtPassSettings::isUseOtp(), testValue);
  } else if (setting == "useTrayIcon") {
    QtPassSettings::setUseTrayIcon(testValue);
    QCOMPARE(QtPassSettings::isUseTrayIcon(), testValue);
  } else if (setting == "usePwgen") {
    QtPassSettings::setUsePwgen(testValue);
    QCOMPARE(QtPassSettings::isUsePwgen(), testValue);
  } else if (setting == "hidePassword") {
    QtPassSettings::setHidePassword(testValue);
    QCOMPARE(QtPassSettings::isHidePassword(), testValue);
  } else if (setting == "hideContent") {
    QtPassSettings::setHideContent(testValue);
    QCOMPARE(QtPassSettings::isHideContent(), testValue);
  } else if (setting == "useSelection") {
    QtPassSettings::setUseSelection(testValue);
    QCOMPARE(QtPassSettings::isUseSelection(), testValue);
  } else if (setting == "useAutoclear") {
    QtPassSettings::setUseAutoclear(testValue);
    QCOMPARE(QtPassSettings::isUseAutoclear(), testValue);
  } else if (setting == "useMonospace") {
    QtPassSettings::setUseMonospace(testValue);
    QCOMPARE(QtPassSettings::isUseMonospace(), testValue);
  } else if (setting == "noLineWrapping") {
    QtPassSettings::setNoLineWrapping(testValue);
    QCOMPARE(QtPassSettings::isNoLineWrapping(), testValue);
  } else if (setting == "addGPGId") {
    QtPassSettings::setAddGPGId(testValue);
    QCOMPARE(QtPassSettings::isAddGPGId(), testValue);
  } else if (setting == "avoidCapitals") {
    QtPassSettings::setAvoidCapitals(testValue);
    QCOMPARE(QtPassSettings::isAvoidCapitals(), testValue);
  } else if (setting == "avoidNumbers") {
    QtPassSettings::setAvoidNumbers(testValue);
    QCOMPARE(QtPassSettings::isAvoidNumbers(), testValue);
  } else if (setting == "lessRandom") {
    QtPassSettings::setLessRandom(testValue);
    QCOMPARE(QtPassSettings::isLessRandom(), testValue);
  } else if (setting == "useSymbols") {
    QtPassSettings::setUseSymbols(testValue);
    QCOMPARE(QtPassSettings::isUseSymbols(), testValue);
  } else if (setting == "displayAsIs") {
    QtPassSettings::setDisplayAsIs(testValue);
    QCOMPARE(QtPassSettings::isDisplayAsIs(), testValue);
  } else if (setting == "hideOnClose") {
    QtPassSettings::setHideOnClose(testValue);
    QCOMPARE(QtPassSettings::isHideOnClose(), testValue);
  } else if (setting == "startMinimized") {
    QtPassSettings::setStartMinimized(testValue);
    QCOMPARE(QtPassSettings::isStartMinimized(), testValue);
  } else if (setting == "alwaysOnTop") {
    QtPassSettings::setAlwaysOnTop(testValue);
    QCOMPARE(QtPassSettings::isAlwaysOnTop(), testValue);
  } else if (setting == "autoPull") {
    QtPassSettings::setAutoPull(testValue);
    QCOMPARE(QtPassSettings::isAutoPull(), testValue);
  } else if (setting == "autoPush") {
    QtPassSettings::setAutoPush(testValue);
    QCOMPARE(QtPassSettings::isAutoPush(), testValue);
  } else if (setting == "useTemplate") {
    QtPassSettings::setUseTemplate(testValue);
    QCOMPARE(QtPassSettings::isUseTemplate(), testValue);
  } else if (setting == "templateAllFields") {
    QtPassSettings::setTemplateAllFields(testValue);
    QCOMPARE(QtPassSettings::isTemplateAllFields(), testValue);
  } else if (setting == "useWebDav") {
    QtPassSettings::setUseWebDav(testValue);
    QCOMPARE(QtPassSettings::isUseWebDav(), testValue);
  } else if (setting == "useQrencode") {
    QtPassSettings::setUseQrencode(testValue);
    QCOMPARE(QtPassSettings::isUseQrencode(), testValue);
  } else if (setting == "useAutoclearPanel") {
    QtPassSettings::setUseAutoclearPanel(testValue);
    QCOMPARE(QtPassSettings::isUseAutoclearPanel(), testValue);
  } else if (setting == "maximized") {
    QtPassSettings::setMaximized(testValue);
    QCOMPARE(QtPassSettings::isMaximized(), testValue);
  } else {
    QFAIL(qPrintable(QString("Unknown setting: %1").arg(setting)));
  }
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

void tst_settings::autoDetectGit() {
  QTemporaryDir tempDir;
  QtPassSettings::setPassStore(tempDir.path());

  QDir gitDir(tempDir.path());
  QVERIFY(gitDir.mkdir(".git"));
  QtPassSettings::getInstance()->sync();

  QtPassSettings::getInstance()->remove("useGit");
  QtPassSettings::getInstance()->sync();
  QVERIFY2(QtPassSettings::isUseGit(true),
           "Should auto-detect .git and return true when default is true");

  QtPassSettings::getInstance()->remove("useGit");
  QtPassSettings::getInstance()->sync();
  QVERIFY2(!QtPassSettings::isUseGit(false),
           "Should respect false default and not auto-detect when .git exists");

  QVERIFY(gitDir.rmdir(".git"));
  QtPassSettings::getInstance()->sync();

  QtPassSettings::getInstance()->remove("useGit");
  QtPassSettings::getInstance()->sync();
  QVERIFY2(QtPassSettings::isUseGit(true),
           "Should return true default when .git not present");

  QtPassSettings::getInstance()->remove("useGit");
  QtPassSettings::getInstance()->sync();
  QVERIFY2(!QtPassSettings::isUseGit(false),
           "Should return false default when .git not present");
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
  profile1["path"] = "/path/to/store1";
  profiles["profile1"] = std::move(profile1);

  QHash<QString, QString> profile2;
  profile2["path"] = "/path/to/store2";
  profiles["profile2"] = std::move(profile2);

  QtPassSettings::setProfiles(profiles);
  QHash<QString, QHash<QString, QString>> readProfiles =
      QtPassSettings::getProfiles();
  QVERIFY(readProfiles.size() >= 1);
  QVERIFY(readProfiles.contains("profile1"));
  QVERIFY(readProfiles.contains("profile2"));
  QVERIFY(readProfiles.contains("profile2"));
  QVERIFY(readProfiles["profile1"].contains("path"));
  QVERIFY(readProfiles["profile2"].contains("path"));
}

void tst_settings::setAndGetProfileDefault() {
  const QString expectedProfile = QStringLiteral("defaultProfile");
  QtPassSettings::setProfile(expectedProfile);
  QCOMPARE(QtPassSettings::getProfile(), expectedProfile);
}

QTEST_MAIN(tst_settings)
#include "tst_settings.moc"
