// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qtpasssettings.h"
#include "pass.h"
#include "passbackendfactory.h"
#include "settingsserializer.h"

#include "util.h"

#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <utility>

bool QtPassSettings::initialized = false;

QtPassSettings *QtPassSettings::m_instance = nullptr;
// Portable mode: a qtpass.ini next to the executable wins over the platform
// settings store.
auto QtPassSettings::getInstance() -> QtPassSettings * {
  if (!QtPassSettings::initialized) {
    QString portable_ini = QCoreApplication::applicationDirPath() +
                           QDir::separator() + "qtpass.ini";
    if (QFile(portable_ini).exists()) {
      m_instance = new QtPassSettings(portable_ini, QSettings::IniFormat);
    } else {
      m_instance = new QtPassSettings("IJHack", "QtPass");
    }

    initialized = true;
  }

  return m_instance;
}

auto QtPassSettings::load() -> AppSettings {
  AppSettings s = SettingsSerializer::load(*getInstance());
  if (!s.passStore.isEmpty()) {
    s.passStore = QDir::cleanPath(QDir(s.passStore).absolutePath());
    if (!s.passStore.endsWith('/'))
      s.passStore += '/';
  }
  return s;
}

void QtPassSettings::save(const AppSettings &settings) {
  SettingsSerializer::save(*getInstance(), settings);
  // The "use pass" mode may have changed; rebuild the backend on next use.
  PassBackendFactory::invalidate();
}

auto QtPassSettings::getPasswordConfiguration() -> PasswordConfiguration {
  return SettingsSerializer::loadPasswordConfiguration(*getInstance());
}

auto QtPassSettings::getProfiles() -> Profiles {
  getInstance()->beginGroup(SettingsConstants::profile);
  Profiles profiles;

  // migration from version <= v1.3.2: profiles datastructure
  QStringList childKeys = getInstance()->childKeys();
  for (const auto &key : std::as_const(childKeys)) {
    Profile profile;
    profile.path = getInstance()->value(key).toString();
    profiles.insert(key, profile);
  }
  // /migration from version <= v1.3.2

  QStringList childGroups = getInstance()->childGroups();
  for (const auto &group : std::as_const(childGroups)) {
    Profile profile;
    profile.path = getInstance()->value(group + "/path").toString();
    profile.signingKey = getInstance()->value(group + "/signingKey").toString();
    profile.useGit = Profile::flagFromString(
        getInstance()->value(group + "/useGit").toString());
    profile.autoPush = Profile::flagFromString(
        getInstance()->value(group + "/autoPush").toString());
    profile.autoPull = Profile::flagFromString(
        getInstance()->value(group + "/autoPull").toString());
    profiles.insert(group, profile);
  }

  getInstance()->endGroup();

  return profiles;
}

void QtPassSettings::setProfiles(const Profiles &profiles) {
  // "profile" is both the active-profile scalar (getProfile()) and the
  // profile/<name>/ group; remove() wipes both, which would switch the active
  // store on every save, so preserve the active name across the rewrite.
  const QString activeProfile =
      getInstance()->value(SettingsConstants::profile).toString();
  getInstance()->remove(SettingsConstants::profile);
  if (!activeProfile.isEmpty()) {
    getInstance()->setValue(SettingsConstants::profile, activeProfile);
  }
  getInstance()->beginGroup(SettingsConstants::profile);

  for (auto i = profiles.cbegin(); i != profiles.cend(); ++i) {
    getInstance()->setValue(i.key() + "/path", i.value().path);
    getInstance()->setValue(i.key() + "/signingKey", i.value().signingKey);
    getInstance()->setValue(i.key() + "/useGit",
                            Profile::flagToString(i.value().useGit));
    getInstance()->setValue(i.key() + "/autoPush",
                            Profile::flagToString(i.value().autoPush));
    getInstance()->setValue(i.key() + "/autoPull",
                            Profile::flagToString(i.value().autoPull));
  }

  getInstance()->endGroup();
}

auto QtPassSettings::getPass() -> Pass * {
  return PassBackendFactory::getPass();
}

auto QtPassSettings::getVersion(const QString &defaultValue) -> QString {
  return getInstance()
      ->value(SettingsConstants::version, defaultValue)
      .toString();
}
void QtPassSettings::setVersion(const QString &version) {
  getInstance()->setValue(SettingsConstants::version, version);
}

auto QtPassSettings::getGeometry(const QByteArray &defaultValue) -> QByteArray {
  return getInstance()
      ->value(SettingsConstants::geometry, defaultValue)
      .toByteArray();
}
void QtPassSettings::setGeometry(const QByteArray &geometry) {
  getInstance()->setValue(SettingsConstants::geometry, geometry);
}

auto QtPassSettings::getSavestate(const QByteArray &defaultValue)
    -> QByteArray {
  return getInstance()
      ->value(SettingsConstants::savestate, defaultValue)
      .toByteArray();
}
void QtPassSettings::setSavestate(const QByteArray &saveState) {
  getInstance()->setValue(SettingsConstants::savestate, saveState);
}

auto QtPassSettings::getDialogGeometry(const QString &key,
                                       const QByteArray &defaultValue)
    -> QByteArray {
  return getInstance()
      ->value(SettingsConstants::dialogGeometry + "/" + key, defaultValue)
      .toByteArray();
}
void QtPassSettings::setDialogGeometry(const QString &key,
                                       const QByteArray &geometry) {
  getInstance()->setValue(SettingsConstants::dialogGeometry + "/" + key,
                          geometry);
}

void QtPassSettings::setUsePass(const bool &usePass) {
  getInstance()->setValue(SettingsConstants::usePass, usePass);
  // Backend selection changed: force re-selection on next getPass().
  PassBackendFactory::invalidate();
}

auto QtPassSettings::getAutoclearSeconds(const int &defaultValue) -> int {
  return getInstance()
      ->value(SettingsConstants::autoclearSeconds, defaultValue)
      .toInt();
}
// Creates the directory if missing; the result always ends in a separator.
auto QtPassSettings::getPassStore(const QString &defaultValue) -> QString {
  QString returnValue = getInstance()
                            ->value(SettingsConstants::passStore, defaultValue)
                            .toString();

  returnValue = QDir(returnValue).absolutePath();

  // ensure directory exists if never used pass or misconfigured.
  // otherwise process->setWorkingDirectory(passStore); will fail on execution.
  if (!QDir(returnValue).exists()) {
    if (!QDir().mkdir(returnValue)) {
      qWarning() << "Failed to create password store directory:" << returnValue;
    }
  }

  if (!returnValue.endsWith("/") && !returnValue.endsWith(QDir::separator())) {
    returnValue += QDir::separator();
  }

  return returnValue;
}
void QtPassSettings::setPassStore(const QString &passStore) {
  getInstance()->setValue(SettingsConstants::passStore, passStore);
}
void QtPassSettings::initExecutables() {
  AppSettings s = QtPassSettings::load();
  if (s.passExecutable.isEmpty())
    s.passExecutable = Util::findBinaryInPath("pass");
  if (s.gitExecutable.isEmpty())
    s.gitExecutable = Util::findBinaryInPath("git");
  if (s.gpgExecutable.isEmpty()) {
    s.gpgExecutable = Util::findBinaryInPath("gpg2");
    if (s.gpgExecutable.isEmpty())
      s.gpgExecutable = Util::findBinaryInPath("gpg");
  }
  if (s.pwgenExecutable.isEmpty())
    s.pwgenExecutable = Util::findBinaryInPath("pwgen");
  QtPassSettings::save(s);
}
auto QtPassSettings::getPassExecutable(const QString &defaultValue) -> QString {
  return getInstance()
      ->value(SettingsConstants::passExecutable, defaultValue)
      .toString();
}

auto QtPassSettings::getProfile(const QString &defaultValue) -> QString {
  return getInstance()
      ->value(SettingsConstants::profile, defaultValue)
      .toString();
}
// With a true default and nothing stored, a store with a .git dir means Git.
auto QtPassSettings::isUseGit(const bool &defaultValue) -> bool {
  bool storedValue =
      getInstance()->value(SettingsConstants::useGit, defaultValue).toBool();
  if (storedValue == defaultValue && defaultValue) {
    QString passStore = getPassStore();
    if (QFileInfo(passStore).isDir() &&
        QFileInfo(passStore + QDir::separator() + ".git").isDir()) {
      return true;
    }
  }
  return storedValue;
}
auto QtPassSettings::isUseGrepSearch(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::useGrepSearch, defaultValue)
      .toBool();
}

auto QtPassSettings::isUseOtp(const bool &defaultValue) -> bool {
  return getInstance()->value(SettingsConstants::useOtp, defaultValue).toBool();
}

void QtPassSettings::setQrencodeExecutable(const QString &qrencodeExecutable) {
  getInstance()->setValue(SettingsConstants::qrencodeExecutable,
                          qrencodeExecutable);
}

auto QtPassSettings::isHideOnClose(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::hideOnClose, defaultValue)
      .toBool();
}
auto QtPassSettings::isAlwaysOnTop(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::alwaysOnTop, defaultValue)
      .toBool();
}

auto QtPassSettings::isAutoPush(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::autoPush, defaultValue)
      .toBool();
}

auto QtPassSettings::isShowProcessOutput(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::showProcessOutput, defaultValue)
      .toBool();
}
auto QtPassSettings::getRealPass() -> RealPass * {
  return PassBackendFactory::getRealPass();
}
auto QtPassSettings::getImitatePass() -> ImitatePass * {
  return PassBackendFactory::getImitatePass();
}
