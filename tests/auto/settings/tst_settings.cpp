// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>
#include <limits>

#include "../../../src/appsettings.h"
#include "../../../src/passwordconfiguration.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/settingsconstants.h"
#include "../../../src/settingsserializer.h"
#include "../testsettings.h"

class tst_settings : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void cleanupTestCase();
  void getPasswordConfigurationDefault();
  void setAndGetPasswordConfiguration();
  void getProfilesEmpty();
  void setAndGetProfiles();
  void setProfilesPreservesActiveProfile();
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
  void setAndGetDialogGeometry();
  void setAndGetPasswordCharsSelection();
  void setAndGetPasswordChars();
  void setAndGetMultipleProfiles();
  void profileGitOptions();
  void setAndGetProfileDefault();
  void serializerLoadDefaults();
  void serializerRoundTrip();
  void serializerKeyCompatibility();
  void serializerDropsObsoleteWebDavKeys();
  void facadeLoadReflectsSave();
  void serializerPasswordCharsSelection_data();
  void serializerPasswordCharsSelection();
  void facadePasswordCharsSelectionOutOfRange();

private:
};

void tst_settings::initTestCase() {
  // Every suite run starts from an empty, private settings directory, so
  // getPasswordConfigurationDefault() sees real defaults and nothing here can
  // touch the user's live config.
  isolateTestSettings();
}

void tst_settings::cleanupTestCase() {
  // Clean up test profiles that may have been created during tests
  // This ensures cleanup happens even if individual tests abort on QVERIFY
  QtPassSettings::getInstance()->beginGroup("profile");
  QtPassSettings::getInstance()->remove("test-git-profile");
  QtPassSettings::getInstance()->endGroup();
}

void tst_settings::getPasswordConfigurationDefault() {
  PasswordConfiguration config = QtPassSettings::getPasswordConfiguration();
  QCOMPARE(config.length, 16);
  QCOMPARE(config.selected, PasswordConfiguration::ALLCHARS);
}

void tst_settings::setAndGetPasswordConfiguration() {
  const PasswordConfiguration saved =
      QtPassSettings::load().passwordConfiguration;

  PasswordConfiguration config;
  config.length = 20;
  config.selected = PasswordConfiguration::ALPHABETICAL;
  config.Characters[PasswordConfiguration::CUSTOM] = "abc";

  AppSettings toSave = QtPassSettings::load();
  toSave.passwordConfiguration = config;
  QtPassSettings::save(toSave);

  PasswordConfiguration readConfig = QtPassSettings::getPasswordConfiguration();
  QCOMPARE(readConfig.length, 20);
  QVERIFY2(readConfig.selected == PasswordConfiguration::ALPHABETICAL,
           "Password config should be ALPHABETICAL");

  // Restore to avoid polluting subsequent test runs
  toSave = QtPassSettings::load();
  toSave.passwordConfiguration = saved;
  QtPassSettings::save(toSave);
}

void tst_settings::getProfilesEmpty() {
  QtPassSettings::setProfiles(Profiles());
  QVERIFY(QtPassSettings::getProfiles().isEmpty());
}

void tst_settings::setAndGetProfiles() {
  Profiles profiles;
  Profile profile1;
  profile1.path = "/test/path";
  profile1.signingKey = "ABC123";
  profiles.insert("profile1", profile1);

  QtPassSettings::setProfiles(profiles);

  const Profiles readProfiles = QtPassSettings::getProfiles();
  QVERIFY(!readProfiles.isEmpty());
  QVERIFY(readProfiles.contains("profile1"));
  QCOMPARE(readProfiles["profile1"].path, QString("/test/path"));
  QCOMPARE(readProfiles["profile1"].signingKey, QString("ABC123"));
}

void tst_settings::setProfilesPreservesActiveProfile() {
  // The active-profile name (getProfile) is stored under the same "profile"
  // key that setProfiles() clears before rewriting the profile group.
  // Regression: setProfiles() used to wipe the scalar, silently switching the
  // active password store on every config-dialog OK.
  AppSettings s = QtPassSettings::load();
  s.activeProfile = QStringLiteral("work");
  QtPassSettings::save(s);
  QCOMPARE(QtPassSettings::getProfile(), QStringLiteral("work"));

  Profiles profiles;
  Profile profile;
  profile.path = ("/store/work");
  profiles.insert("work", profile);
  QtPassSettings::setProfiles(profiles);

  QCOMPARE(QtPassSettings::getProfile(), QStringLiteral("work"));
  QVERIFY(QtPassSettings::getProfiles().contains("work"));
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
  const bool plausiblePath = store.isEmpty() || QDir::isAbsolutePath(store) ||
                             store.contains('/') || store.contains('\\');
  QVERIFY2(plausiblePath, "Pass store should be empty or a plausible path");
}

void tst_settings::setAndGetPassStore() {
  QtPassSettings::setPassStore("/tmp/test-store");
  QString store = QtPassSettings::getPassStore();
  QVERIFY(store.contains("test-store"));
}

namespace {
struct BoolSetting {
  const char *name;
  bool AppSettings::*field;
};

const BoolSetting boolSettings[] = {
    {"usePass", &AppSettings::usePass},
    {"useGit", &AppSettings::useGit},
    {"useOtp", &AppSettings::useOtp},
    {"useTrayIcon", &AppSettings::useTrayIcon},
    {"usePwgen", &AppSettings::usePwgen},
    {"hidePassword", &AppSettings::hidePassword},
    {"hideContent", &AppSettings::hideContent},
    {"useSelection", &AppSettings::useSelection},
    {"useAutoclear", &AppSettings::useAutoclear},
    {"useMonospace", &AppSettings::useMonospace},
    {"noLineWrapping", &AppSettings::noLineWrapping},
    {"addGPGId", &AppSettings::addGPGId},
    {"avoidCapitals", &AppSettings::avoidCapitals},
    {"avoidNumbers", &AppSettings::avoidNumbers},
    {"lessRandom", &AppSettings::lessRandom},
    {"useSymbols", &AppSettings::useSymbols},
    {"displayAsIs", &AppSettings::displayAsIs},
    {"hideOnClose", &AppSettings::hideOnClose},
    {"startMinimized", &AppSettings::startMinimized},
    {"alwaysOnTop", &AppSettings::alwaysOnTop},
    {"autoPull", &AppSettings::autoPull},
    {"autoPush", &AppSettings::autoPush},
    {"useTemplate", &AppSettings::useTemplate},
    {"templateAllFields", &AppSettings::templateAllFields},
    {"useQrencode", &AppSettings::useQrencode},
    {"useAutoclearPanel", &AppSettings::useAutoclearPanel},
    {"useGrepSearch", &AppSettings::useGrepSearch},
    {"showProcessOutput", &AppSettings::showProcessOutput},
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
      AppSettings toSave = QtPassSettings::load();
      toSave.*s.field = testValue;
      QtPassSettings::save(toSave);
      const AppSettings loaded = QtPassSettings::load();
      const bool actual = loaded.*s.field;
      QVERIFY2(actual == testValue,
               qPrintable(QString("%1 should be %2, got %3")
                              .arg(setting)
                              .arg(testValue ? "true" : "false")
                              .arg(actual ? "true" : "false")));
      return;
    }
  }
  QFAIL(qPrintable(QString("Unknown setting: %1").arg(setting)));
}

void tst_settings::setAndGetClipBoardType() {
  AppSettings toSave = QtPassSettings::load();
  toSave.clipBoardType = static_cast<Enums::clipBoardType>(1);
  QtPassSettings::save(toSave);
  QCOMPARE(QtPassSettings::load().clipBoardType,
           static_cast<Enums::clipBoardType>(1));
}

void tst_settings::setAndGetPasswordLength() {
  AppSettings toSave = QtPassSettings::load();
  const int savedLength = toSave.passwordConfiguration.length;
  toSave.passwordConfiguration.length = 24;
  QtPassSettings::save(toSave);
  PasswordConfiguration config = QtPassSettings::getPasswordConfiguration();
  QCOMPARE(config.length, 24);
  // Restore to avoid polluting subsequent test runs
  toSave = QtPassSettings::load();
  toSave.passwordConfiguration.length = savedLength;
  QtPassSettings::save(toSave);
}

namespace {
struct IntSetting {
  const char *name;
  int AppSettings::*field;
};

const IntSetting intSettings[] = {
    {"autoclearSeconds", &AppSettings::autoclearSeconds},
    {"autoclearPanelSeconds", &AppSettings::autoclearPanelSeconds},
};

struct StringSetting {
  const char *name;
  QString AppSettings::*field;
};

const StringSetting stringSettings[] = {
    {"passSigningKey", &AppSettings::passSigningKey},
    {"passExecutable", &AppSettings::passExecutable},
    {"gitExecutable", &AppSettings::gitExecutable},
    {"gpgExecutable", &AppSettings::gpgExecutable},
    {"pwgenExecutable", &AppSettings::pwgenExecutable},
    {"qrencodeExecutable", &AppSettings::qrencodeExecutable},
    {"profile", &AppSettings::activeProfile},
    {"passTemplate", &AppSettings::passTemplate},
    {"sshAuthSockOverride", &AppSettings::sshAuthSockOverride},
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
      AppSettings toSave = QtPassSettings::load();
      toSave.*s.field = testValue;
      QtPassSettings::save(toSave);
      QCOMPARE(QtPassSettings::load().*s.field, testValue);
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
  addString("profile", "work");
  addString("profile", "personal");
  addString("passTemplate", "username: {username}\npassword: {password}");
  addString("passTemplate", "user: {username}\npass: {password}");
  addString("sshAuthSockOverride", "/run/user/1000/gnupg/S.gpg-agent.ssh");
  addString("sshAuthSockOverride", "");
}

void tst_settings::stringRoundTrip() {
  QFETCH(QString, setting);
  QFETCH(QString, testValue);

  for (const auto &s : stringSettings) {
    if (setting == s.name) {
      AppSettings toSave = QtPassSettings::load();
      toSave.*s.field = testValue;
      QtPassSettings::save(toSave);
      QCOMPARE(QtPassSettings::load().*s.field, testValue);
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

void tst_settings::setAndGetDialogGeometry() {
  const QString key = "testDialog";
  QByteArray geometry("test_dialog_geometry");
  QtPassSettings::setDialogGeometry(key, geometry);
  QByteArray read = QtPassSettings::getDialogGeometry(key, QByteArray());
  QVERIFY2(read == geometry, "Dialog geometry should match");
}

void tst_settings::setAndGetPasswordCharsSelection() {
  AppSettings toSave = QtPassSettings::load();
  const PasswordConfiguration::characterSet savedSelected =
      toSave.passwordConfiguration.selected;
  toSave.passwordConfiguration.selected = PasswordConfiguration::ALPHABETICAL;
  QtPassSettings::save(toSave);
  PasswordConfiguration config = QtPassSettings::getPasswordConfiguration();
  QCOMPARE(config.selected, PasswordConfiguration::ALPHABETICAL);
  // Restore to avoid polluting subsequent tests
  toSave = QtPassSettings::load();
  toSave.passwordConfiguration.selected = savedSelected;
  QtPassSettings::save(toSave);
}

void tst_settings::setAndGetPasswordChars() {
  AppSettings toSave = QtPassSettings::load();
  toSave.passwordConfiguration.Characters[PasswordConfiguration::CUSTOM] =
      "abc123";
  QtPassSettings::save(toSave);
  PasswordConfiguration config = QtPassSettings::getPasswordConfiguration();
  QVERIFY2(config.Characters[PasswordConfiguration::CUSTOM].contains("abc"),
           "PasswordChars should contain 'abc'");
  // Reset to avoid affecting subsequent tests and live QtPass
  toSave = QtPassSettings::load();
  toSave.passwordConfiguration.Characters[PasswordConfiguration::CUSTOM] =
      QString();
  QtPassSettings::save(toSave);
}

void tst_settings::setAndGetMultipleProfiles() {
  Profiles profiles;
  Profile profile1;
  profile1.path = "/path/to/store1";
  profiles["profile1"] = profile1;

  Profile profile2;
  profile2.path = "/path/to/store2";
  profiles["profile2"] = profile2;

  QtPassSettings::setProfiles(profiles);
  const Profiles readProfiles = QtPassSettings::getProfiles();
  QVERIFY2(readProfiles.size() == 2, "Should have exactly 2 profiles");
  QCOMPARE(readProfiles["profile1"].path, QString("/path/to/store1"));
  QCOMPARE(readProfiles["profile2"].path, QString("/path/to/store2"));
  // Git flags stay unset (follow the global setting) unless written (#112)
  for (const QString &profileName : {"profile1", "profile2"}) {
    QVERIFY2(!readProfiles[profileName].useGit.has_value(),
             "useGit must be unset for a fresh profile");
    QVERIFY(!readProfiles[profileName].autoPush.has_value());
    QVERIFY(!readProfiles[profileName].autoPull.has_value());
  }
}

/**
 * @brief The per-profile Git flags round-trip as a tri-state: unset, true and
 *        false are each preserved, on the same on-disk keys as before.
 */
void tst_settings::profileGitOptions() {
  Profiles profiles;
  Profile work;
  work.path = "/work";
  work.useGit = true;
  work.autoPush = false;
  // autoPull deliberately left unset
  profiles["work"] = work;
  QtPassSettings::setProfiles(profiles);

  const Profiles read = QtPassSettings::getProfiles();
  QCOMPARE(read["work"].useGit, std::optional<bool>(true));
  QCOMPARE(read["work"].autoPush, std::optional<bool>(false));
  QVERIFY(!read["work"].autoPull.has_value());
  QCOMPARE(read["work"], work);

  // The on-disk representation is unchanged: "" / "true" / "false".
  QCOMPARE(Profile::flagToString(std::nullopt), QString());
  QCOMPARE(Profile::flagToString(true), QStringLiteral("true"));
  QCOMPARE(Profile::flagFromString(QStringLiteral("false")),
           std::optional<bool>(false));
  QVERIFY(!Profile::flagFromString(QStringLiteral("maybe")).has_value());
}

void tst_settings::setAndGetProfileDefault() {
  const QString expectedProfile = QStringLiteral("defaultProfile");
  {
    AppSettings s = QtPassSettings::load();
    s.activeProfile = expectedProfile;
    QtPassSettings::save(s);
  }
  QCOMPARE(QtPassSettings::load().activeProfile, expectedProfile);
}

void tst_settings::serializerLoadDefaults() {
  // An empty store must yield the documented defaults.
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QSettings qs(dir.filePath("empty.ini"), QSettings::IniFormat);

  const AppSettings s = SettingsSerializer::load(qs);

  QCOMPARE(s.usePass, false);
  QCOMPARE(s.useGit, false);
  QCOMPARE(s.showProcessOutput, false);
  QCOMPARE(s.useGrepSearch, false);
  QCOMPARE(s.clipBoardType, Enums::CLIPBOARD_NEVER);
  // addGPGId defaults to true (every isAddGPGId() call site passes true).
  QCOMPARE(s.addGPGId, true);
  QCOMPARE(s.autoclearSeconds, 0);
  QCOMPARE(s.passStore, QString());
  // PasswordConfiguration default length is 16, not 0.
  QCOMPARE(s.passwordConfiguration.length, 16);
  QCOMPARE(s.passwordConfiguration.selected, PasswordConfiguration::ALLCHARS);
}

void tst_settings::serializerRoundTrip() {
  // save() then load() must reproduce every field.
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QSettings qs(dir.filePath("roundtrip.ini"), QSettings::IniFormat);

  AppSettings out;
  out.version = QStringLiteral("9.9.9");
  out.usePass = true;
  out.passStore = QStringLiteral("/tmp/store");
  out.passSigningKey = QStringLiteral("DEADBEEF");
  out.passExecutable = QStringLiteral("/usr/bin/pass");
  out.gitExecutable = QStringLiteral("/usr/bin/git");
  out.gpgExecutable = QStringLiteral("/usr/bin/gpg2");
  out.pwgenExecutable = QStringLiteral("/usr/bin/pwgen");
  out.qrencodeExecutable = QStringLiteral("/usr/bin/qrencode");
  out.sshAuthSockOverride = QStringLiteral("/run/agent.sock");
  out.clipBoardType = Enums::CLIPBOARD_ALWAYS;
  out.useSelection = true;
  out.useAutoclear = true;
  out.autoclearSeconds = 42;
  out.useAutoclearPanel = true;
  out.autoclearPanelSeconds = 7;
  out.hidePassword = true;
  out.hideContent = true;
  out.useMonospace = true;
  out.displayAsIs = true;
  out.noLineWrapping = true;
  out.addGPGId = true;
  out.useGit = true;
  out.useGrepSearch = true;
  out.useOtp = true;
  out.useQrencode = true;
  out.usePwgen = true;
  out.autoPull = true;
  out.autoPush = true;
  out.showProcessOutput = true;
  out.passTemplate = QStringLiteral("login\nurl");
  out.useTemplate = true;
  out.templateAllFields = true;
  out.passwordConfiguration.length = 24;
  out.passwordConfiguration.selected = PasswordConfiguration::ALPHANUMERIC;
  out.passwordConfiguration.Characters[PasswordConfiguration::CUSTOM] =
      QStringLiteral("abc123");
  out.avoidCapitals = true;
  out.avoidNumbers = true;
  out.lessRandom = true;
  out.useSymbols = true;
  out.useTrayIcon = true;
  out.hideOnClose = true;
  out.startMinimized = true;
  out.alwaysOnTop = true;
  out.activeProfile = QStringLiteral("work");

  SettingsSerializer::save(qs, out);
  const AppSettings in = SettingsSerializer::load(qs);

  QCOMPARE(in.version, out.version);
  QCOMPARE(in.usePass, out.usePass);
  QCOMPARE(in.passStore, out.passStore);
  QCOMPARE(in.passSigningKey, out.passSigningKey);
  QCOMPARE(in.passExecutable, out.passExecutable);
  QCOMPARE(in.gitExecutable, out.gitExecutable);
  QCOMPARE(in.gpgExecutable, out.gpgExecutable);
  QCOMPARE(in.pwgenExecutable, out.pwgenExecutable);
  QCOMPARE(in.qrencodeExecutable, out.qrencodeExecutable);
  QCOMPARE(in.sshAuthSockOverride, out.sshAuthSockOverride);
  QCOMPARE(in.clipBoardType, out.clipBoardType);
  QCOMPARE(in.useSelection, out.useSelection);
  QCOMPARE(in.useAutoclear, out.useAutoclear);
  QCOMPARE(in.autoclearSeconds, out.autoclearSeconds);
  QCOMPARE(in.useAutoclearPanel, out.useAutoclearPanel);
  QCOMPARE(in.autoclearPanelSeconds, out.autoclearPanelSeconds);
  QCOMPARE(in.hidePassword, out.hidePassword);
  QCOMPARE(in.hideContent, out.hideContent);
  QCOMPARE(in.useMonospace, out.useMonospace);
  QCOMPARE(in.displayAsIs, out.displayAsIs);
  QCOMPARE(in.noLineWrapping, out.noLineWrapping);
  QCOMPARE(in.addGPGId, out.addGPGId);
  QCOMPARE(in.useGit, out.useGit);
  QCOMPARE(in.useGrepSearch, out.useGrepSearch);
  QCOMPARE(in.useOtp, out.useOtp);
  QCOMPARE(in.useQrencode, out.useQrencode);
  QCOMPARE(in.usePwgen, out.usePwgen);
  QCOMPARE(in.autoPull, out.autoPull);
  QCOMPARE(in.autoPush, out.autoPush);
  QCOMPARE(in.showProcessOutput, out.showProcessOutput);
  QCOMPARE(in.passTemplate, out.passTemplate);
  QCOMPARE(in.useTemplate, out.useTemplate);
  QCOMPARE(in.templateAllFields, out.templateAllFields);
  QCOMPARE(in.passwordConfiguration.length, out.passwordConfiguration.length);
  QCOMPARE(in.passwordConfiguration.selected,
           out.passwordConfiguration.selected);
  QCOMPARE(in.passwordConfiguration.Characters[PasswordConfiguration::CUSTOM],
           out.passwordConfiguration.Characters[PasswordConfiguration::CUSTOM]);
  QCOMPARE(in.avoidCapitals, out.avoidCapitals);
  QCOMPARE(in.avoidNumbers, out.avoidNumbers);
  QCOMPARE(in.lessRandom, out.lessRandom);
  QCOMPARE(in.useSymbols, out.useSymbols);
  QCOMPARE(in.useTrayIcon, out.useTrayIcon);
  QCOMPARE(in.hideOnClose, out.hideOnClose);
  QCOMPARE(in.startMinimized, out.startMinimized);
  QCOMPARE(in.alwaysOnTop, out.alwaysOnTop);
  QCOMPARE(in.activeProfile, out.activeProfile);
}

void tst_settings::serializerKeyCompatibility() {
  // The serializer must write the same QSettings keys the legacy getters read,
  // so existing config files keep working after migration.
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QSettings qs(dir.filePath("keys.ini"), QSettings::IniFormat);

  AppSettings out;
  out.usePass = true;
  out.passStore = QStringLiteral("/tmp/store");
  out.autoclearSeconds = 13;
  SettingsSerializer::save(qs, out);

  QCOMPARE(qs.value(SettingsConstants::usePass).toBool(), true);
  QCOMPARE(qs.value(SettingsConstants::passStore).toString(),
           QStringLiteral("/tmp/store"));
  QCOMPARE(qs.value(SettingsConstants::autoclearSeconds).toInt(), 13);
}

void tst_settings::serializerDropsObsoleteWebDavKeys() {
  // The WebDAV mount was removed in 2.0. Saving over a configuration written
  // by an older version must clear its four keys, so no plaintext
  // webDavPassword is left behind in the ini file.
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString path = dir.filePath("webdav.ini");
  {
    QSettings qs(path, QSettings::IniFormat);
    qs.setValue("useWebDav", true);
    qs.setValue("webDavUrl", QStringLiteral("https://dav.example/"));
    qs.setValue("webDavUser", QStringLiteral("alice"));
    qs.setValue("webDavPassword", QStringLiteral("s3cr3t"));
    qs.setValue(SettingsConstants::passStore, QStringLiteral("/tmp/store"));
    qs.sync();
  }

  QSettings qs(path, QSettings::IniFormat);
  AppSettings out = SettingsSerializer::load(qs);
  QCOMPARE(out.passStore, QStringLiteral("/tmp/store"));
  SettingsSerializer::save(qs, out);
  qs.sync();

  for (const QString &key :
       {QStringLiteral("useWebDav"), QStringLiteral("webDavUrl"),
        QStringLiteral("webDavUser"), QStringLiteral("webDavPassword")}) {
    QVERIFY2(!qs.contains(key), qPrintable(key + " should have been removed"));
  }
  QFile ini(path);
  QVERIFY(ini.open(QIODevice::ReadOnly | QIODevice::Text));
  QVERIFY2(!QString::fromUtf8(ini.readAll()).contains("s3cr3t"),
           "the stored WebDAV password must not survive in the ini file");
  QCOMPARE(qs.value(SettingsConstants::passStore).toString(),
           QStringLiteral("/tmp/store"));
}

void tst_settings::facadeLoadReflectsSave() {
  // QtPassSettings::save then ::load must round-trip through the singleton.
  AppSettings out = QtPassSettings::load();
  out.useMonospace = !out.useMonospace;
  out.autoclearSeconds = 77;
  out.passSigningKey = QStringLiteral("FACADEKEY");
  QtPassSettings::save(out);

  const AppSettings in = QtPassSettings::load();
  QCOMPARE(in.useMonospace, out.useMonospace);
  QCOMPARE(in.autoclearSeconds, 77);
  QCOMPARE(in.passSigningKey, QStringLiteral("FACADEKEY"));
}

void tst_settings::serializerPasswordCharsSelection_data() {
  QTest::addColumn<int>("stored");
  QTest::addColumn<int>("expected");
  QTest::addColumn<bool>("warns");

  // Legitimate values must survive the load untouched.
  QTest::newRow("allchars")
      << 0 << static_cast<int>(PasswordConfiguration::ALLCHARS) << false;
  QTest::newRow("alphabetical")
      << 1 << static_cast<int>(PasswordConfiguration::ALPHABETICAL) << false;
  QTest::newRow("alphanumeric")
      << 2 << static_cast<int>(PasswordConfiguration::ALPHANUMERIC) << false;
  QTest::newRow("custom") << 3
                          << static_cast<int>(PasswordConfiguration::CUSTOM)
                          << false;
  // Anything outside the enum would index past Characters[CHARSETS_COUNT];
  // it must be rejected and replaced by the default, never forwarded.
  QTest::newRow("count-sentinel")
      << static_cast<int>(PasswordConfiguration::CHARSETS_COUNT)
      << static_cast<int>(PasswordConfiguration::ALLCHARS) << true;
  QTest::newRow("too-large")
      << 7 << static_cast<int>(PasswordConfiguration::ALLCHARS) << true;
  QTest::newRow("negative")
      << -1 << static_cast<int>(PasswordConfiguration::ALLCHARS) << true;
  QTest::newRow("int-min") << std::numeric_limits<int>::min()
                           << static_cast<int>(PasswordConfiguration::ALLCHARS)
                           << true;
}

void tst_settings::serializerPasswordCharsSelection() {
  QFETCH(int, stored);
  QFETCH(int, expected);
  QFETCH(bool, warns);

  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QSettings qs(dir.filePath("selection.ini"), QSettings::IniFormat);
  qs.setValue(SettingsConstants::passwordCharsSelection, stored);
  qs.setValue(SettingsConstants::passwordChars, QStringLiteral("xyz"));

  if (warns) {
    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression(QStringLiteral(
                             "Ignoring out-of-range passwordCharsSelection")));
  }
  const AppSettings s = SettingsSerializer::load(qs);
  QCOMPARE(static_cast<int>(s.passwordConfiguration.selected), expected);
  // The clamped selection must be a safe index into Characters.
  QVERIFY(s.passwordConfiguration.selected >= 0);
  QVERIFY(s.passwordConfiguration.selected <
          PasswordConfiguration::CHARSETS_COUNT);
  QVERIFY(!s.passwordConfiguration.Characters[s.passwordConfiguration.selected]
               .isEmpty());
  // Rejecting the selection must not drop the neighbouring keys.
  QCOMPARE(s.passwordConfiguration.Characters[PasswordConfiguration::CUSTOM],
           QStringLiteral("xyz"));
}

void tst_settings::facadePasswordCharsSelectionOutOfRange() {
  // A hand-edited ini reaching the singleton must yield a valid selection from
  // QtPassSettings::getPasswordConfiguration(), the path the dialogs use.
  const int savedRaw = QtPassSettings::getInstance()
                           ->value(SettingsConstants::passwordCharsSelection, 0)
                           .toInt();
  QtPassSettings::getInstance()->setValue(
      SettingsConstants::passwordCharsSelection, 7);

  QTest::ignoreMessage(QtWarningMsg,
                       QRegularExpression(QStringLiteral(
                           "Ignoring out-of-range passwordCharsSelection")));
  const PasswordConfiguration config =
      QtPassSettings::getPasswordConfiguration();
  QCOMPARE(config.selected, PasswordConfiguration::ALLCHARS);

  QTest::ignoreMessage(QtWarningMsg,
                       QRegularExpression(QStringLiteral(
                           "Ignoring out-of-range passwordCharsSelection")));
  QCOMPARE(QtPassSettings::load().passwordConfiguration.selected,
           PasswordConfiguration::ALLCHARS);

  QtPassSettings::getInstance()->setValue(
      SettingsConstants::passwordCharsSelection, savedRaw);
}

QTEST_MAIN(tst_settings)
#include "tst_settings.moc"
