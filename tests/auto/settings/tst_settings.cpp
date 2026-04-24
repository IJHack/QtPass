// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
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
  void intRoundTrip_data();
  void intRoundTrip();
  void stringRoundTrip_data();
  void stringRoundTrip();
  void setAndGetClipBoardType();
  void setAndGetPasswordLength();
  void autoDetectGit();
  void setAndGetSavestate();
  void setAndGetPos();
  void setAndGetSize();
  void setAndGetDialogGeometry();
  void setAndGetDialogPos();
  void setAndGetDialogSize();
  void setAndGetDialogMaximized();
  void setAndGetPasswordCharsSelection();
  void setAndGetPasswordChars();
  void setAndGetMultipleProfiles();
  void profileGitOptions();
  void setAndGetProfileDefault();

private:
  QString m_settingsBackupPath;
  bool m_isPortableMode = false;
};

void tst_settings::initTestCase() {
  // Check for portable mode (qtpass.ini in app directory)
  // Only backup/restore settings file in portable mode
  // On non-portable (registry on Windows), we cannot safely backup
  QString portableIni =
      QCoreApplication::applicationDirPath() + QDir::separator() + "qtpass.ini";
  m_isPortableMode = QFile::exists(portableIni);

  if (m_isPortableMode) {
    QtPassSettings::getInstance()->sync();
    QString settingsFile = QtPassSettings::getInstance()->fileName();
    m_settingsBackupPath = settingsFile + ".bak";
    QFile::remove(m_settingsBackupPath);
    QVERIFY(QFile::copy(settingsFile, m_settingsBackupPath));
  } else {
    m_settingsBackupPath.clear();
    // No qtpass.ini next to the binary, so QSettings writes to per-user storage
    // (e.g. the Windows registry). Automatic backup/restore is only safe in
    // portable mode, so persistent user settings may be modified by this run.
    qWarning() << tr("Non-portable mode detected: tests may modify persistent "
                     "user settings (e.g. Windows registry). For an isolated "
                     "run, drop a qtpass.ini next to the test binary.");
  }
}

void tst_settings::cleanupTestCase() {
  // Clean up test profiles that may have been created during tests
  // This ensures cleanup happens even if individual tests abort on QVERIFY
  QtPassSettings::getInstance()->beginGroup("profile");
  QtPassSettings::getInstance()->remove("test-git-profile");
  QtPassSettings::getInstance()->endGroup();

  // Restore original settings after all tests
  // This ensures make check doesn't change user's live config
  if (m_isPortableMode && !m_settingsBackupPath.isEmpty()) {
    QString settingsFile = QtPassSettings::getInstance()->fileName();
    QtPassSettings::getInstance()->sync();
    QVERIFY2(QFile::remove(settingsFile) || !QFile::exists(settingsFile),
             "Failed to remove current settings file before restore");
    QVERIFY2(QFile::copy(m_settingsBackupPath, settingsFile),
             "Failed to restore settings file from backup");
    QVERIFY2(QFile::remove(m_settingsBackupPath),
             "Failed to remove temporary settings backup file");
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

namespace {
struct BoolSetting {
  const char *name;
  void (*setter)(const bool &);
  bool (*getter)(const bool &);
};

const BoolSetting boolSettings[] = {
    {"usePass", QtPassSettings::setUsePass, QtPassSettings::isUsePass},
    {"useGit", QtPassSettings::setUseGit, QtPassSettings::isUseGit},
    {"useOtp", QtPassSettings::setUseOtp, QtPassSettings::isUseOtp},
    {"useTrayIcon", QtPassSettings::setUseTrayIcon,
     QtPassSettings::isUseTrayIcon},
    {"usePwgen", QtPassSettings::setUsePwgen, QtPassSettings::isUsePwgen},
    {"hidePassword", QtPassSettings::setHidePassword,
     QtPassSettings::isHidePassword},
    {"hideContent", QtPassSettings::setHideContent,
     QtPassSettings::isHideContent},
    {"useSelection", QtPassSettings::setUseSelection,
     QtPassSettings::isUseSelection},
    {"useAutoclear", QtPassSettings::setUseAutoclear,
     QtPassSettings::isUseAutoclear},
    {"useMonospace", QtPassSettings::setUseMonospace,
     QtPassSettings::isUseMonospace},
    {"noLineWrapping", QtPassSettings::setNoLineWrapping,
     QtPassSettings::isNoLineWrapping},
    {"addGPGId", QtPassSettings::setAddGPGId, QtPassSettings::isAddGPGId},
    {"avoidCapitals", QtPassSettings::setAvoidCapitals,
     QtPassSettings::isAvoidCapitals},
    {"avoidNumbers", QtPassSettings::setAvoidNumbers,
     QtPassSettings::isAvoidNumbers},
    {"lessRandom", QtPassSettings::setLessRandom, QtPassSettings::isLessRandom},
    {"useSymbols", QtPassSettings::setUseSymbols, QtPassSettings::isUseSymbols},
    {"displayAsIs", QtPassSettings::setDisplayAsIs,
     QtPassSettings::isDisplayAsIs},
    {"hideOnClose", QtPassSettings::setHideOnClose,
     QtPassSettings::isHideOnClose},
    {"startMinimized", QtPassSettings::setStartMinimized,
     QtPassSettings::isStartMinimized},
    {"alwaysOnTop", QtPassSettings::setAlwaysOnTop,
     QtPassSettings::isAlwaysOnTop},
    {"autoPull", QtPassSettings::setAutoPull, QtPassSettings::isAutoPull},
    {"autoPush", QtPassSettings::setAutoPush, QtPassSettings::isAutoPush},
    {"useTemplate", QtPassSettings::setUseTemplate,
     QtPassSettings::isUseTemplate},
    {"templateAllFields", QtPassSettings::setTemplateAllFields,
     QtPassSettings::isTemplateAllFields},
    {"useWebDav", QtPassSettings::setUseWebDav, QtPassSettings::isUseWebDav},
    {"useQrencode", QtPassSettings::setUseQrencode,
     QtPassSettings::isUseQrencode},
    {"useAutoclearPanel", QtPassSettings::setUseAutoclearPanel,
     QtPassSettings::isUseAutoclearPanel},
    {"maximized", QtPassSettings::setMaximized, QtPassSettings::isMaximized},
};
} // namespace

void tst_settings::boolRoundTrip_data() {
  QTest::addColumn<QString>("setting");
  QTest::addColumn<bool>("testValue");

  for (const auto &s : boolSettings) {
    QByteArray name(s.name);
    QTest::newRow(name + "_true") << s.name << true;
    QTest::newRow(name + "_false") << s.name << false;
  }
}

void tst_settings::boolRoundTrip() {
  QFETCH(QString, setting);
  QFETCH(bool, testValue);

  for (const auto &s : boolSettings) {
    if (setting == s.name) {
      s.setter(testValue);
      // Pass !testValue as the getter's default so a missing/unstored value
      // would surface as a mismatch instead of silently equalling testValue.
      QVERIFY2(s.getter(!testValue) == testValue,
               qPrintable(QString("%1 should be %2, got %3")
                              .arg(setting)
                              .arg(testValue ? "true" : "false")
                              .arg(s.getter(!testValue) ? "true" : "false")));
      return;
    }
  }
  QFAIL(qPrintable(QString("Unknown setting: %1").arg(setting)));
}

void tst_settings::setAndGetClipBoardType() {
  QtPassSettings::setClipBoardType(1);
  QCOMPARE(QtPassSettings::getClipBoardType(), 1);
}

void tst_settings::setAndGetPasswordLength() {
  QtPassSettings::setPasswordLength(24);
  PasswordConfiguration config = QtPassSettings::getPasswordConfiguration();
  QCOMPARE(config.length, 24);
}

namespace {
struct IntSetting {
  const char *name;
  void (*setter)(const int &);
  int (*getter)(const int &);
};

const IntSetting intSettings[] = {
    {"autoclearSeconds", QtPassSettings::setAutoclearSeconds,
     QtPassSettings::getAutoclearSeconds},
    {"autoclearPanelSeconds", QtPassSettings::setAutoclearPanelSeconds,
     QtPassSettings::getAutoclearPanelSeconds},
};

struct StringSetting {
  const char *name;
  void (*setter)(const QString &);
  QString (*getter)(const QString &);
};

const StringSetting stringSettings[] = {
    {"passSigningKey", QtPassSettings::setPassSigningKey,
     QtPassSettings::getPassSigningKey},
    {"passExecutable", QtPassSettings::setPassExecutable,
     QtPassSettings::getPassExecutable},
    {"gitExecutable", QtPassSettings::setGitExecutable,
     QtPassSettings::getGitExecutable},
    {"gpgExecutable", QtPassSettings::setGpgExecutable,
     QtPassSettings::getGpgExecutable},
    {"pwgenExecutable", QtPassSettings::setPwgenExecutable,
     QtPassSettings::getPwgenExecutable},
    {"qrencodeExecutable", QtPassSettings::setQrencodeExecutable,
     QtPassSettings::getQrencodeExecutable},
    {"webDavUrl", QtPassSettings::setWebDavUrl, QtPassSettings::getWebDavUrl},
    {"webDavUser", QtPassSettings::setWebDavUser,
     QtPassSettings::getWebDavUser},
    {"webDavPassword", QtPassSettings::setWebDavPassword,
     QtPassSettings::getWebDavPassword},
    {"profile", QtPassSettings::setProfile, QtPassSettings::getProfile},
    {"passTemplate", QtPassSettings::setPassTemplate,
     QtPassSettings::getPassTemplate},
};
} // namespace

void tst_settings::intRoundTrip_data() {
  QTest::addColumn<QString>("setting");
  QTest::addColumn<int>("testValue");

  for (const auto &s : intSettings) {
    QByteArray name(s.name);
    QTest::newRow(name + "_30") << s.name << 30;
    QTest::newRow(name + "_60") << s.name << 60;
  }
}

void tst_settings::intRoundTrip() {
  QFETCH(QString, setting);
  QFETCH(int, testValue);

  for (const auto &s : intSettings) {
    if (setting == s.name) {
      s.setter(testValue);
      QCOMPARE(s.getter(-1), testValue);
      return;
    }
  }
  QFAIL(qPrintable(QString("Unknown setting: %1").arg(setting)));
}

void tst_settings::stringRoundTrip_data() {
  QTest::addColumn<QString>("setting");
  QTest::addColumn<QString>("testValue");

  auto addString = [](const char *name, const QString &value) {
    QTest::newRow((QByteArray(name) + "_" + value.toUtf8()).constData())
        << name << value;
  };

  addString("passSigningKey", "testkey123");
  addString("passSigningKey", "anotherkey456");
  addString("passExecutable", "/usr/bin/pass");
  addString("passExecutable", "/usr/local/bin/pass");
  addString("gitExecutable", "/usr/bin/git");
  addString("gitExecutable", "/usr/local/bin/git");
  addString("gpgExecutable", "/usr/bin/gpg");
  addString("gpgExecutable", "/usr/local/bin/gpg");
  addString("pwgenExecutable", "/usr/bin/pwgen");
  addString("pwgenExecutable", "/usr/local/bin/pwgen");
  addString("qrencodeExecutable", "/usr/bin/qrencode");
  addString("qrencodeExecutable", "/usr/local/bin/qrencode");
  addString("webDavUrl", "https://dav.example.com/pass");
  addString("webDavUrl", "https://dav2.example.com/pass");
  addString("webDavUser", "testuser");
  addString("webDavUser", "admin");
  addString("webDavPassword", "secretpassword");
  addString("webDavPassword", "anothersecret");
  addString("profile", "work");
  addString("profile", "personal");
  addString("passTemplate", "username: {username}\npassword: {password}");
  addString("passTemplate", "user: {username}\npass: {password}");
}

void tst_settings::stringRoundTrip() {
  QFETCH(QString, setting);
  QFETCH(QString, testValue);

  for (const auto &s : stringSettings) {
    if (setting == s.name) {
      s.setter(testValue);
      QCOMPARE(s.getter(QString()), testValue);
      return;
    }
  }
  QFAIL(qPrintable(QString("Unknown setting: %1").arg(setting)));
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
           "Should return false when default is false, even if .git exists");

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

void tst_settings::setAndGetDialogGeometry() {
  const QString key = "testDialog";
  QByteArray geometry("test_dialog_geometry");
  QtPassSettings::setDialogGeometry(key, geometry);
  QByteArray read = QtPassSettings::getDialogGeometry(key, QByteArray());
  QVERIFY2(read == geometry, "Dialog geometry should match");
}

void tst_settings::setAndGetDialogPos() {
  const QString key = "testDialog";
  QPoint pos(100, 200);
  QtPassSettings::setDialogPos(key, pos);
  QPoint read = QtPassSettings::getDialogPos(key, QPoint());
  QVERIFY2(read == pos, "Dialog pos should match");
}

void tst_settings::setAndGetDialogSize() {
  const QString key = "testDialog";
  QSize size(640, 480);
  QtPassSettings::setDialogSize(key, size);
  QSize read = QtPassSettings::getDialogSize(key, QSize());
  QVERIFY2(read == size, "Dialog size should match");
}

void tst_settings::setAndGetDialogMaximized() {
  const QString key = "testDialog";
  QtPassSettings::setDialogMaximized(key, true);
  bool read = QtPassSettings::isDialogMaximized(key, false);
  QVERIFY2(read == true, "Dialog maximized should be true");
  QtPassSettings::setDialogMaximized(key, false);
  read = QtPassSettings::isDialogMaximized(key, true);
  QVERIFY2(read == false, "Dialog maximized should be false");
}

void tst_settings::setAndGetPasswordCharsSelection() {
  QtPassSettings::setPasswordCharsSelection(
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
  QVERIFY(!readProfiles.isEmpty());
  QVERIFY2(readProfiles.size() == 2, "Should have exactly 2 profiles");
  QVERIFY(readProfiles.contains("profile1"));
  QVERIFY(readProfiles.contains("profile2"));
  QVERIFY(readProfiles["profile1"].contains("path"));
  QVERIFY(readProfiles["profile2"].contains("path"));
  // Verify new git options are in profile (issue #112)
  QVERIFY(readProfiles["profile1"].contains("useGit"));
  QVERIFY(readProfiles["profile1"].contains("autoPush"));
  QVERIFY(readProfiles["profile1"].contains("autoPull"));
  QVERIFY2(!readProfiles["profile1"].value("useGit").toInt(),
           "useGit should default to false");
  QVERIFY2(!readProfiles["profile1"].value("autoPush").toInt(),
           "autoPush should default to false");
  QVERIFY2(!readProfiles["profile1"].value("autoPull").toInt(),
           "autoPull should default to false");
}

void tst_settings::profileGitOptions() {
  const QString profileName = "test-git-profile";

  // Initially should return defaults
  QVERIFY(!QtPassSettings::getProfileAutoPush(profileName, false));
  QVERIFY(!QtPassSettings::getProfileAutoPull(profileName, false));
  QVERIFY(!QtPassSettings::getProfileUseGit(profileName, false));

  // Set values
  QtPassSettings::setProfileUseGit(profileName, true);
  QtPassSettings::setProfileAutoPush(profileName, true);
  QtPassSettings::setProfileAutoPull(profileName, true);

  // Verify values persisted
  QVERIFY(QtPassSettings::getProfileUseGit(profileName, false));
  QVERIFY(QtPassSettings::getProfileAutoPush(profileName, false));
  QVERIFY(QtPassSettings::getProfileAutoPull(profileName, false));

  // Reset to false
  QtPassSettings::setProfileUseGit(profileName, false);
  QtPassSettings::setProfileAutoPush(profileName, false);
  QtPassSettings::setProfileAutoPull(profileName, false);

  QVERIFY(!QtPassSettings::getProfileUseGit(profileName, true));
  QVERIFY(!QtPassSettings::getProfileAutoPush(profileName, true));
  QVERIFY(!QtPassSettings::getProfileAutoPull(profileName, true));

  // Cleanup moved to cleanupTestCase() to ensure it runs even on test failure
}

void tst_settings::setAndGetProfileDefault() {
  const QString expectedProfile = QStringLiteral("defaultProfile");
  QtPassSettings::setProfile(expectedProfile);
  QCOMPARE(QtPassSettings::getProfile(), expectedProfile);
}

QTEST_MAIN(tst_settings)
#include "tst_settings.moc"