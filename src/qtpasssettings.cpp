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

#include "util.h"

#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <utility>

bool QtPassSettings::initialized = false;

Pass *QtPassSettings::pass;
// Go via pointer to avoid dynamic initialization,
// due to "random" initialization order relative to other
// globals, especially around QObject metadata dynamic initialization
// can lead to crashes depending on compiler, linker etc.
QScopedPointer<RealPass> QtPassSettings::realPass;
QScopedPointer<ImitatePass> QtPassSettings::imitatePass;

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
  getInstance()->setValue(SettingsConstants::passwordCharsselection,
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
    // profiles.insert(group, getInstance()->value(group).toString());
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
  }

  getInstance()->endGroup();
}

auto QtPassSettings::getPass() -> Pass * {
  if (!pass) {
    if (isUsePass()) {
      pass = getRealPass();
    } else {
      pass = getImitatePass();
    }
    if (pass) {
      pass->init();
    }
  }
  return pass;
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

auto QtPassSettings::isMaximized(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::maximized, defaultValue)
      .toBool();
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

auto QtPassSettings::isUsePass(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::usePass, defaultValue)
      .toBool();
}
void QtPassSettings::setUsePass(const bool &usePass) {
  getInstance()->setValue(SettingsConstants::usePass, usePass);
  pass = nullptr;
}

auto QtPassSettings::getClipBoardTypeRaw(
    const Enums::clipBoardType &defaultValue) -> int {
  return getInstance()
      ->value(SettingsConstants::clipBoardType, static_cast<int>(defaultValue))
      .toInt();
}

auto QtPassSettings::getClipBoardType(const Enums::clipBoardType &defaultValue)
    -> Enums::clipBoardType {
  return static_cast<Enums::clipBoardType>(getClipBoardTypeRaw(defaultValue));
}
void QtPassSettings::setClipBoardType(const int &clipBoardType) {
  getInstance()->setValue(SettingsConstants::clipBoardType, clipBoardType);
}

auto QtPassSettings::isUseSelection(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::useSelection, defaultValue)
      .toBool();
}
void QtPassSettings::setUseSelection(const bool &useSelection) {
  getInstance()->setValue(SettingsConstants::useSelection, useSelection);
}

auto QtPassSettings::isUseAutoclear(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::useAutoclear, defaultValue)
      .toBool();
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

auto QtPassSettings::isUseAutoclearPanel(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::useAutoclearPanel, defaultValue)
      .toBool();
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

auto QtPassSettings::isHidePassword(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::hidePassword, defaultValue)
      .toBool();
}
void QtPassSettings::setHidePassword(const bool &hidePassword) {
  getInstance()->setValue(SettingsConstants::hidePassword, hidePassword);
}

auto QtPassSettings::isHideContent(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::hideContent, defaultValue)
      .toBool();
}
void QtPassSettings::setHideContent(const bool &hideContent) {
  getInstance()->setValue(SettingsConstants::hideContent, hideContent);
}

auto QtPassSettings::isUseMonospace(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::useMonospace, defaultValue)
      .toBool();
}
void QtPassSettings::setUseMonospace(const bool &useMonospace) {
  getInstance()->setValue(SettingsConstants::useMonospace, useMonospace);
}

auto QtPassSettings::isDisplayAsIs(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::displayAsIs, defaultValue)
      .toBool();
}
void QtPassSettings::setDisplayAsIs(const bool &displayAsIs) {
  getInstance()->setValue(SettingsConstants::displayAsIs, displayAsIs);
}

auto QtPassSettings::isNoLineWrapping(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::noLineWrapping, defaultValue)
      .toBool();
}
void QtPassSettings::setNoLineWrapping(const bool &noLineWrapping) {
  getInstance()->setValue(SettingsConstants::noLineWrapping, noLineWrapping);
}

auto QtPassSettings::isAddGPGId(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::addGPGId, defaultValue)
      .toBool();
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
  QString passExecutable =
      QtPassSettings::getPassExecutable(Util::findBinaryInPath("pass"));
  QtPassSettings::setPassExecutable(passExecutable);

  QString gitExecutable =
      QtPassSettings::getGitExecutable(Util::findBinaryInPath("git"));
  QtPassSettings::setGitExecutable(gitExecutable);

  QString gpgExecutable =
      QtPassSettings::getGpgExecutable(Util::findBinaryInPath("gpg2"));
  QtPassSettings::setGpgExecutable(gpgExecutable);

  QString pwgenExecutable =
      QtPassSettings::getPwgenExecutable(Util::findBinaryInPath("pwgen"));
  QtPassSettings::setPwgenExecutable(pwgenExecutable);
}
auto QtPassSettings::getPassExecutable(const QString &defaultValue) -> QString {
  return getInstance()
      ->value(SettingsConstants::passExecutable, defaultValue)
      .toString();
}
void QtPassSettings::setPassExecutable(const QString &passExecutable) {
  getInstance()->setValue(SettingsConstants::passExecutable, passExecutable);
}

auto QtPassSettings::getGitExecutable(const QString &defaultValue) -> QString {
  return getInstance()
      ->value(SettingsConstants::gitExecutable, defaultValue)
      .toString();
}
void QtPassSettings::setGitExecutable(const QString &gitExecutable) {
  getInstance()->setValue(SettingsConstants::gitExecutable, gitExecutable);
}

auto QtPassSettings::getGpgExecutable(const QString &defaultValue) -> QString {
  return getInstance()
      ->value(SettingsConstants::gpgExecutable, defaultValue)
      .toString();
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

auto QtPassSettings::getWebDavUrl(const QString &defaultValue) -> QString {
  return getInstance()
      ->value(SettingsConstants::webDavUrl, defaultValue)
      .toString();
}
void QtPassSettings::setWebDavUrl(const QString &webDavUrl) {
  getInstance()->setValue(SettingsConstants::webDavUrl, webDavUrl);
}

auto QtPassSettings::getWebDavUser(const QString &defaultValue) -> QString {
  return getInstance()
      ->value(SettingsConstants::webDavUser, defaultValue)
      .toString();
}
void QtPassSettings::setWebDavUser(const QString &webDavUser) {
  getInstance()->setValue(SettingsConstants::webDavUser, webDavUser);
}

auto QtPassSettings::getWebDavPassword(const QString &defaultValue) -> QString {
  return getInstance()
      ->value(SettingsConstants::webDavPassword, defaultValue)
      .toString();
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

auto QtPassSettings::isUseQrencode(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::useQrencode, defaultValue)
      .toBool();
}

void QtPassSettings::setUseQrencode(const bool &useQrencode) {
  getInstance()->setValue(SettingsConstants::useQrencode, useQrencode);
}

auto QtPassSettings::getQrencodeExecutable(const QString &defaultValue)
    -> QString {
  return getInstance()
      ->value(SettingsConstants::qrencodeExecutable, defaultValue)
      .toString();
}
void QtPassSettings::setQrencodeExecutable(const QString &qrencodeExecutable) {
  getInstance()->setValue(SettingsConstants::qrencodeExecutable,
                          qrencodeExecutable);
}

auto QtPassSettings::isUsePwgen(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::usePwgen, defaultValue)
      .toBool();
}
void QtPassSettings::setUsePwgen(const bool &usePwgen) {
  getInstance()->setValue(SettingsConstants::usePwgen, usePwgen);
}

auto QtPassSettings::isAvoidCapitals(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::avoidCapitals, defaultValue)
      .toBool();
}
void QtPassSettings::setAvoidCapitals(const bool &avoidCapitals) {
  getInstance()->setValue(SettingsConstants::avoidCapitals, avoidCapitals);
}

auto QtPassSettings::isAvoidNumbers(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::avoidNumbers, defaultValue)
      .toBool();
}
void QtPassSettings::setAvoidNumbers(const bool &avoidNumbers) {
  getInstance()->setValue(SettingsConstants::avoidNumbers, avoidNumbers);
}

auto QtPassSettings::isLessRandom(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::lessRandom, defaultValue)
      .toBool();
}
void QtPassSettings::setLessRandom(const bool &lessRandom) {
  getInstance()->setValue(SettingsConstants::lessRandom, lessRandom);
}

auto QtPassSettings::isUseSymbols(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::useSymbols, defaultValue)
      .toBool();
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

auto QtPassSettings::isUseTrayIcon(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::useTrayIcon, defaultValue)
      .toBool();
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

auto QtPassSettings::isStartMinimized(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::startMinimized, defaultValue)
      .toBool();
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

auto QtPassSettings::isAutoPull(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::autoPull, defaultValue)
      .toBool();
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

auto QtPassSettings::isUseTemplate(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::useTemplate, defaultValue)
      .toBool();
}
void QtPassSettings::setUseTemplate(const bool &useTemplate) {
  getInstance()->setValue(SettingsConstants::useTemplate, useTemplate);
}

auto QtPassSettings::isTemplateAllFields(const bool &defaultValue) -> bool {
  return getInstance()
      ->value(SettingsConstants::templateAllFields, defaultValue)
      .toBool();
}
void QtPassSettings::setTemplateAllFields(const bool &templateAllFields) {
  getInstance()->setValue(SettingsConstants::templateAllFields,
                          templateAllFields);
}

auto QtPassSettings::getRealPass() -> RealPass * {
  if (realPass.isNull()) {
    realPass.reset(new RealPass());
  }
  return realPass.data();
}
auto QtPassSettings::getImitatePass() -> ImitatePass * {
  if (imitatePass.isNull()) {
    imitatePass.reset(new ImitatePass());
  }
  return imitatePass.data();
}
