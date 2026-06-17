// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @class QtPassSettings
 * @brief Singleton settings manager implementation.
 *
 * Implementation of QtPassSettings singleton. Handles persistence using
 * QSettings with support for portable mode (qtpass.ini next to executable).
 *
 * @see qtpasssettings.h
 */

#include "qtpasssettings.h"
#include "pass.h"
#include "passbackendfactory.h"
#include "settingsserializer.h"

#include "util.h"

#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <utility>

bool QtPassSettings::initialized = false;

QtPassSettings *QtPassSettings::m_instance = nullptr;
/**
 * @brief Returns the singleton instance of QtPassSettings, creating it on first
 * access.
 * @example
 * QtPassSettings *settings = QtPassSettings::getInstance();
 * std::cout << settings << std::endl; // Expected output sample: a valid
 * QtPassSettings pointer
 *
 * @return QtPassSettings* - Pointer to the shared QtPassSettings instance.
 */
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
  // A settings save may have changed the "use pass" mode; drop the cached
  // backend so the correct one is rebuilt on next use (matches the previous
  // setUsePass() side effect).
  PassBackendFactory::invalidate();
}

/**
 * @brief Retrieves the current password configuration from application
 * settings.
 * @example
 * PasswordConfiguration result = QtPassSettings::getPasswordConfiguration();
 * std::cout << result.length << std::endl; // Expected output sample
 *
 * @return PasswordConfiguration - The password configuration populated from
 * stored settings, including length, selected character set, and custom
 * characters.
 */
auto QtPassSettings::getPasswordConfiguration() -> PasswordConfiguration {
  PasswordConfiguration config;

  config.length =
      getInstance()->value(SettingsConstants::passwordLength, 16).toInt();
  if (config.length <= 0) {
    config.length = 16;
  }
  config.selected = static_cast<PasswordConfiguration::characterSet>(
      getInstance()
          ->value(SettingsConstants::passwordCharsSelection, 0)
          .toInt());
  config.Characters[PasswordConfiguration::CUSTOM] =
      getInstance()
          ->value(SettingsConstants::passwordChars, QString())
          .toString();

  return config;
}

void QtPassSettings::setPasswordConfiguration(
    const PasswordConfiguration &config) {
  getInstance()->setValue(SettingsConstants::passwordLength, config.length);
  getInstance()->setValue(SettingsConstants::passwordCharsSelection,
                          config.selected);
  getInstance()->setValue(SettingsConstants::passwordChars,
                          config.Characters[PasswordConfiguration::CUSTOM]);
}

/**
 * @brief Retrieves the stored profiles configuration as a nested hash map.
 * @details Reads profile data from the settings group, including legacy profile
 *          formats from versions <= v1.3.2, and returns each profile name
 * mapped to its properties such as path and signing key.
 * @example
 * QHash<QString, QHash<QString, QString>> profiles =
 * QtPassSettings::getProfiles(); std::cout << profiles.size() << std::endl; //
 * Expected output: number of profiles
 *
 * @return QHash<QString, QHash<QString, QString>> - A hash map of profile names
 *         mapped to their key/value properties.
 */
auto QtPassSettings::getProfiles() -> QHash<QString, QHash<QString, QString>> {
  getInstance()->beginGroup(SettingsConstants::profile);
  QHash<QString, QHash<QString, QString>> profiles;

  // migration from version <= v1.3.2: profiles datastructure
  QStringList childKeys = getInstance()->childKeys();
  if (!childKeys.empty()) {
    for (const auto &key : std::as_const(childKeys)) {
      QHash<QString, QString> profile;
      profile.insert("path", getInstance()->value(key).toString());
      profile.insert("signingKey", "");
      profile.insert("useGit", "");
      profile.insert("autoPush", "");
      profile.insert("autoPull", "");
      profiles.insert(key, profile);
    }
  }
  // /migration from version <= v1.3.2

  QStringList childGroups = getInstance()->childGroups();
  for (const auto &group : std::as_const(childGroups)) {
    QHash<QString, QString> profile;
    profile.insert("path", getInstance()->value(group + "/path").toString());
    profile.insert("signingKey",
                   getInstance()->value(group + "/signingKey").toString());
    profile.insert("useGit",
                   getInstance()->value(group + "/useGit").toString());
    profile.insert("autoPush",
                   getInstance()->value(group + "/autoPush").toString());
    profile.insert("autoPull",
                   getInstance()->value(group + "/autoPull").toString());
    profiles.insert(group, profile);
  }

  getInstance()->endGroup();

  return profiles;
}

/**
 * @brief Stores the profile settings in the application's configuration.
 * @example
 * QtPassSettings::setProfiles(profiles);
 *
 * @param profiles - A hash of profile names mapped to their key-value settings,
 * such as "path" and "signingKey".
 * @return void - This method does not return a value.
 */
void QtPassSettings::setProfiles(
    const QHash<QString, QHash<QString, QString>> &profiles) {
  getInstance()->remove(SettingsConstants::profile);
  getInstance()->beginGroup(SettingsConstants::profile);

  QHash<QString, QHash<QString, QString>>::const_iterator i = profiles.begin();
  for (; i != profiles.end(); ++i) {
    getInstance()->setValue(i.key() + "/path", i.value().value("path"));
    getInstance()->setValue(i.key() + "/signingKey",
                            i.value().value("signingKey"));
    getInstance()->setValue(i.key() + "/useGit", i.value().value("useGit"));
    getInstance()->setValue(i.key() + "/autoPush", i.value().value("autoPush"));
    getInstance()->setValue(i.key() + "/autoPull", i.value().value("autoPull"));
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

auto QtPassSettings::getPos(const QPoint &defaultValue) -> QPoint {
  QPoint pos =
      getInstance()->value(SettingsConstants::pos, defaultValue).toPoint();
  if (pos == QPoint(0, 0)) {
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
      screen = QGuiApplication::primaryScreen();
    if (screen)
      pos = screen->geometry().center();
  }
  return pos;
}
void QtPassSettings::setPos(const QPoint &pos) {
  if (pos == QPoint(0, 0))
    return;
  getInstance()->setValue(SettingsConstants::pos, pos);
}

auto QtPassSettings::getSize(const QSize &defaultValue) -> QSize {
  return getInstance()->value(SettingsConstants::size, defaultValue).toSize();
}
void QtPassSettings::setSize(const QSize &size) {
  getInstance()->setValue(SettingsConstants::size, size);
}

void QtPassSettings::setMaximized(const bool &maximized) {
  getInstance()->setValue(SettingsConstants::maximized, maximized);
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

auto QtPassSettings::getDialogPos(const QString &key,
                                  const QPoint &defaultValue) -> QPoint {
  return getInstance()
      ->value(SettingsConstants::dialogPos + "/" + key, defaultValue)
      .toPoint();
}
void QtPassSettings::setDialogPos(const QString &key, const QPoint &pos) {
  getInstance()->setValue(SettingsConstants::dialogPos + "/" + key, pos);
}

auto QtPassSettings::getDialogSize(const QString &key,
                                   const QSize &defaultValue) -> QSize {
  return getInstance()
      ->value(SettingsConstants::dialogSize + "/" + key, defaultValue)
      .toSize();
}
void QtPassSettings::setDialogSize(const QString &key, const QSize &size) {
  getInstance()->setValue(SettingsConstants::dialogSize + "/" + key, size);
}

auto QtPassSettings::isDialogMaximized(const QString &key,
                                       const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::dialogMaximized + "/" + key, defaultValue)
      .toBool();
}
void QtPassSettings::setDialogMaximized(const QString &key,
                                        const bool &maximized) {
  getInstance()->setValue(SettingsConstants::dialogMaximized + "/" + key,
                          maximized);
}

void QtPassSettings::setUsePass(const bool &usePass) {
  getInstance()->setValue(SettingsConstants::usePass, usePass);
  // Backend selection changed: force re-selection on next getPass().
  PassBackendFactory::invalidate();
}

void QtPassSettings::setClipBoardType(const int &clipBoardType) {
  getInstance()->setValue(SettingsConstants::clipBoardType, clipBoardType);
}

void QtPassSettings::setUseSelection(const bool &useSelection) {
  getInstance()->setValue(SettingsConstants::useSelection, useSelection);
}

void QtPassSettings::setUseAutoclear(const bool &useAutoclear) {
  getInstance()->setValue(SettingsConstants::useAutoclear, useAutoclear);
}

auto QtPassSettings::getAutoclearSeconds(const int &defaultValue) -> int {
  return getInstance()
      ->value(SettingsConstants::autoclearSeconds, defaultValue)
      .toInt();
}
void QtPassSettings::setAutoclearSeconds(const int &autoClearSeconds) {
  getInstance()->setValue(SettingsConstants::autoclearSeconds,
                          autoClearSeconds);
}

void QtPassSettings::setUseAutoclearPanel(const bool &useAutoclearPanel) {
  getInstance()->setValue(SettingsConstants::useAutoclearPanel,
                          useAutoclearPanel);
}

auto QtPassSettings::getAutoclearPanelSeconds(const int &defaultValue) -> int {
  return getInstance()
      ->value(SettingsConstants::autoclearPanelSeconds, defaultValue)
      .toInt();
}
void QtPassSettings::setAutoclearPanelSeconds(
    const int &autoClearPanelSeconds) {
  getInstance()->setValue(SettingsConstants::autoclearPanelSeconds,
                          autoClearPanelSeconds);
}

void QtPassSettings::setHidePassword(const bool &hidePassword) {
  getInstance()->setValue(SettingsConstants::hidePassword, hidePassword);
}

void QtPassSettings::setHideContent(const bool &hideContent) {
  getInstance()->setValue(SettingsConstants::hideContent, hideContent);
}

void QtPassSettings::setUseMonospace(const bool &useMonospace) {
  getInstance()->setValue(SettingsConstants::useMonospace, useMonospace);
}

void QtPassSettings::setDisplayAsIs(const bool &displayAsIs) {
  getInstance()->setValue(SettingsConstants::displayAsIs, displayAsIs);
}

void QtPassSettings::setNoLineWrapping(const bool &noLineWrapping) {
  getInstance()->setValue(SettingsConstants::noLineWrapping, noLineWrapping);
}

void QtPassSettings::setAddGPGId(const bool &addGPGId) {
  getInstance()->setValue(SettingsConstants::addGPGId, addGPGId);
}

/**
 * @brief Retrieves the password store path, normalizes it, and ensures the
 * directory exists.
 * @example
 * QString passStore =
 * QtPassSettings::getPassStore("/home/user/.password-store"); qDebug() <<
 * passStore; // Expected output: "/home/user/.password-store/"
 *
 * @param defaultValue - Fallback path used when no password store is
 * configured.
 * @return QString - The normalized absolute password store path, guaranteed to
 * end with a path separator.
 */
auto QtPassSettings::getPassStore(const QString &defaultValue) -> QString {
  QString returnValue = getInstance()
                            ->value(SettingsConstants::passStore, defaultValue)
                            .toString();

  // Normalize the path string
  returnValue = QDir(returnValue).absolutePath();

  // ensure directory exists if never used pass or misconfigured.
  // otherwise process->setWorkingDirectory(passStore); will fail on execution.
  if (!QDir(returnValue).exists()) {
    if (!QDir().mkdir(returnValue)) {
      qWarning() << "Failed to create password store directory:" << returnValue;
    }
  }

  // ensure path ends in /
  if (!returnValue.endsWith("/") && !returnValue.endsWith(QDir::separator())) {
    returnValue += QDir::separator();
  }

  return returnValue;
}
void QtPassSettings::setPassStore(const QString &passStore) {
  getInstance()->setValue(SettingsConstants::passStore, passStore);
}

auto QtPassSettings::getPassSigningKey(const QString &defaultValue) -> QString {
  return getInstance()
      ->value(SettingsConstants::passSigningKey, defaultValue)
      .toString();
}
void QtPassSettings::setPassSigningKey(const QString &passSigningKey) {
  getInstance()->setValue(SettingsConstants::passSigningKey, passSigningKey);
}

/**
 * @brief Initializes executable paths for Pass, Git, GPG, and Pwgen by locating
 * them in the system PATH.
 * @example
 * QtPassSettings::initExecutables();
 *
 * @return void - This method does not return a value.
 */
void QtPassSettings::initExecutables() {
  const AppSettings s = QtPassSettings::load();
  QtPassSettings::setPassExecutable(s.passExecutable.isEmpty()
                                        ? Util::findBinaryInPath("pass")
                                        : s.passExecutable);
  QtPassSettings::setGitExecutable(s.gitExecutable.isEmpty()
                                       ? Util::findBinaryInPath("git")
                                       : s.gitExecutable);
  QtPassSettings::setGpgExecutable(s.gpgExecutable.isEmpty()
                                       ? Util::findBinaryInPath("gpg2")
                                       : s.gpgExecutable);
  QtPassSettings::setPwgenExecutable(s.pwgenExecutable.isEmpty()
                                         ? Util::findBinaryInPath("pwgen")
                                         : s.pwgenExecutable);
}
auto QtPassSettings::getPassExecutable(const QString &defaultValue) -> QString {
  return getInstance()
      ->value(SettingsConstants::passExecutable, defaultValue)
      .toString();
}
void QtPassSettings::setPassExecutable(const QString &passExecutable) {
  getInstance()->setValue(SettingsConstants::passExecutable, passExecutable);
}

auto QtPassSettings::getSshAuthSockOverride(const QString &defaultValue)
    -> QString {
  return getInstance()
      ->value(SettingsConstants::sshAuthSockOverride, defaultValue)
      .toString();
}
void QtPassSettings::setSshAuthSockOverride(const QString &value) {
  getInstance()->setValue(SettingsConstants::sshAuthSockOverride, value);
}

void QtPassSettings::setGitExecutable(const QString &gitExecutable) {
  getInstance()->setValue(SettingsConstants::gitExecutable, gitExecutable);
}

void QtPassSettings::setGpgExecutable(const QString &gpgExecutable) {
  getInstance()->setValue(SettingsConstants::gpgExecutable, gpgExecutable);
}

auto QtPassSettings::getPwgenExecutable(const QString &defaultValue)
    -> QString {
  return getInstance()
      ->value(SettingsConstants::pwgenExecutable, defaultValue)
      .toString();
}
void QtPassSettings::setPwgenExecutable(const QString &pwgenExecutable) {
  getInstance()->setValue(SettingsConstants::pwgenExecutable, pwgenExecutable);
}

auto QtPassSettings::getGpgHome(const QString &defaultValue) -> QString {
  return getInstance()
      ->value(SettingsConstants::gpgHome, defaultValue)
      .toString();
}

auto QtPassSettings::isUseWebDav(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::useWebDav, defaultValue)
      .toBool();
}
void QtPassSettings::setUseWebDav(const bool &useWebDav) {
  getInstance()->setValue(SettingsConstants::useWebDav, useWebDav);
}

void QtPassSettings::setWebDavUrl(const QString &webDavUrl) {
  getInstance()->setValue(SettingsConstants::webDavUrl, webDavUrl);
}

void QtPassSettings::setWebDavUser(const QString &webDavUser) {
  getInstance()->setValue(SettingsConstants::webDavUser, webDavUser);
}

void QtPassSettings::setWebDavPassword(const QString &webDavPassword) {
  getInstance()->setValue(SettingsConstants::webDavPassword, webDavPassword);
}

auto QtPassSettings::getProfile(const QString &defaultValue) -> QString {
  return getInstance()
      ->value(SettingsConstants::profile, defaultValue)
      .toString();
}
void QtPassSettings::setProfile(const QString &profile) {
  getInstance()->setValue(SettingsConstants::profile, profile);
}

/**
 * @brief Gets the useGit setting for a specific profile.
 * @param profileName The profile name.
 * @param defaultValue The default value if not set.
 * @return The useGit setting for the profile.
 */
auto QtPassSettings::getProfileUseGit(const QString &profileName,
                                      const bool &defaultValue) -> bool {
  QString stored =
      getInstance()
          ->value(SettingsConstants::profile + "/" + profileName + "/useGit")
          .toString();
  // If empty or not set, return default (migration-friendly fallback)
  if (stored.isEmpty()) {
    return defaultValue;
  }
  return stored == "true";
}

/**
 * @brief Sets the useGit setting for a specific profile.
 * @param profileName The profile name.
 * @param useGit The useGit value to set.
 */
void QtPassSettings::setProfileUseGit(const QString &profileName,
                                      const bool &useGit) {
  getInstance()->setValue(SettingsConstants::profile + "/" + profileName +
                              "/useGit",
                          useGit ? "true" : "false");
}

/**
 * @brief Gets the autoPush setting for a specific profile.
 * @param profileName The profile name.
 * @param defaultValue The default value if not set.
 * @return The autoPush setting for the profile.
 */
auto QtPassSettings::getProfileAutoPush(const QString &profileName,
                                        const bool &defaultValue) -> bool {
  QString stored =
      getInstance()
          ->value(SettingsConstants::profile + "/" + profileName + "/autoPush")
          .toString();
  if (stored.isEmpty()) {
    return defaultValue;
  }
  return stored == "true";
}

/**
 * @brief Sets the autoPush setting for a specific profile.
 * @param profileName The profile name.
 * @param autoPush The autoPush value to set.
 */
void QtPassSettings::setProfileAutoPush(const QString &profileName,
                                        const bool &autoPush) {
  getInstance()->setValue(SettingsConstants::profile + "/" + profileName +
                              "/autoPush",
                          autoPush ? "true" : "false");
}

/**
 * @brief Gets the autoPull setting for a specific profile.
 * @param profileName The profile name.
 * @param defaultValue The default value if not set.
 * @return The autoPull setting for the profile.
 */
auto QtPassSettings::getProfileAutoPull(const QString &profileName,
                                        const bool &defaultValue) -> bool {
  QString stored =
      getInstance()
          ->value(SettingsConstants::profile + "/" + profileName + "/autoPull")
          .toString();
  if (stored.isEmpty()) {
    return defaultValue;
  }
  return stored == "true";
}

/**
 * @brief Sets the autoPull setting for a specific profile.
 * @param profileName The profile name.
 * @param autoPull The autoPull value to set.
 */
void QtPassSettings::setProfileAutoPull(const QString &profileName,
                                        const bool &autoPull) {
  getInstance()->setValue(SettingsConstants::profile + "/" + profileName +
                              "/autoPull",
                          autoPull ? "true" : "false");
}

/**
 * @brief Determines whether Git should be used for the current QtPass settings.
 * @example
 * bool result = QtPassSettings::isUseGit(true);
 * std::cout << result << std::endl; // Expected output: true or false
 *
 * @param const bool &defaultValue - The fallback value used when no explicit
 * setting is stored.
 * @return bool - True if Git usage is enabled, otherwise false.
 */
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
void QtPassSettings::setUseGit(const bool &useGit) {
  getInstance()->setValue(SettingsConstants::useGit, useGit);
}

auto QtPassSettings::isUseGrepSearch(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::useGrepSearch, defaultValue)
      .toBool();
}

void QtPassSettings::setUseGrepSearch(const bool &useGrepSearch) {
  getInstance()->setValue(SettingsConstants::useGrepSearch, useGrepSearch);
}

auto QtPassSettings::isUseOtp(const bool &defaultValue) -> bool {
  return getInstance()->value(SettingsConstants::useOtp, defaultValue).toBool();
}

void QtPassSettings::setUseOtp(const bool &useOtp) {
  getInstance()->setValue(SettingsConstants::useOtp, useOtp);
}

void QtPassSettings::setUseQrencode(const bool &useQrencode) {
  getInstance()->setValue(SettingsConstants::useQrencode, useQrencode);
}

void QtPassSettings::setQrencodeExecutable(const QString &qrencodeExecutable) {
  getInstance()->setValue(SettingsConstants::qrencodeExecutable,
                          qrencodeExecutable);
}

void QtPassSettings::setUsePwgen(const bool &usePwgen) {
  getInstance()->setValue(SettingsConstants::usePwgen, usePwgen);
}

void QtPassSettings::setAvoidCapitals(const bool &avoidCapitals) {
  getInstance()->setValue(SettingsConstants::avoidCapitals, avoidCapitals);
}

void QtPassSettings::setAvoidNumbers(const bool &avoidNumbers) {
  getInstance()->setValue(SettingsConstants::avoidNumbers, avoidNumbers);
}

void QtPassSettings::setLessRandom(const bool &lessRandom) {
  getInstance()->setValue(SettingsConstants::lessRandom, lessRandom);
}

void QtPassSettings::setUseSymbols(const bool &useSymbols) {
  getInstance()->setValue(SettingsConstants::useSymbols, useSymbols);
}

void QtPassSettings::setPasswordLength(const int &passwordLength) {
  getInstance()->setValue(SettingsConstants::passwordLength, passwordLength);
}
void QtPassSettings::setPasswordCharsSelection(
    const int &passwordCharsSelection) {
  getInstance()->setValue(SettingsConstants::passwordCharsSelection,
                          passwordCharsSelection);
}
void QtPassSettings::setPasswordChars(const QString &passwordChars) {
  getInstance()->setValue(SettingsConstants::passwordChars, passwordChars);
}

void QtPassSettings::setUseTrayIcon(const bool &useTrayIcon) {
  getInstance()->setValue(SettingsConstants::useTrayIcon, useTrayIcon);
}

auto QtPassSettings::isHideOnClose(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::hideOnClose, defaultValue)
      .toBool();
}
void QtPassSettings::setHideOnClose(const bool &hideOnClose) {
  getInstance()->setValue(SettingsConstants::hideOnClose, hideOnClose);
}

void QtPassSettings::setStartMinimized(const bool &startMinimized) {
  getInstance()->setValue(SettingsConstants::startMinimized, startMinimized);
}

auto QtPassSettings::isAlwaysOnTop(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::alwaysOnTop, defaultValue)
      .toBool();
}
void QtPassSettings::setAlwaysOnTop(const bool &alwaysOnTop) {
  getInstance()->setValue(SettingsConstants::alwaysOnTop, alwaysOnTop);
}

void QtPassSettings::setAutoPull(const bool &autoPull) {
  getInstance()->setValue(SettingsConstants::autoPull, autoPull);
}

auto QtPassSettings::isAutoPush(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::autoPush, defaultValue)
      .toBool();
}
void QtPassSettings::setAutoPush(const bool &autoPush) {
  getInstance()->setValue(SettingsConstants::autoPush, autoPush);
}

auto QtPassSettings::getPassTemplate(const QString &defaultValue) -> QString {
  return getInstance()
      ->value(SettingsConstants::passTemplate, defaultValue)
      .toString();
}
void QtPassSettings::setPassTemplate(const QString &passTemplate) {
  getInstance()->setValue(SettingsConstants::passTemplate, passTemplate);
}

void QtPassSettings::setUseTemplate(const bool &useTemplate) {
  getInstance()->setValue(SettingsConstants::useTemplate, useTemplate);
}

void QtPassSettings::setTemplateAllFields(const bool &templateAllFields) {
  getInstance()->setValue(SettingsConstants::templateAllFields,
                          templateAllFields);
}

auto QtPassSettings::isShowProcessOutput(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::showProcessOutput, defaultValue)
      .toBool();
}
void QtPassSettings::setShowProcessOutput(const bool &showProcessOutput) {
  getInstance()->setValue(SettingsConstants::showProcessOutput,
                          showProcessOutput);
}

auto QtPassSettings::getRealPass() -> RealPass * {
  return PassBackendFactory::getRealPass();
}
auto QtPassSettings::getImitatePass() -> ImitatePass * {
  return PassBackendFactory::getImitatePass();
}
