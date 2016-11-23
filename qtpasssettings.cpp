#include "qtpasssettings.h"
#include "settingsconstants.h"

QtPassSettings::QtPassSettings() {}

bool QtPassSettings::initialized = false;

QScopedPointer<QSettings> QtPassSettings::settings;
QHash<QString, QString> QtPassSettings::stringSettings;
QHash<QString, QByteArray> QtPassSettings::byteArraySettings;
QHash<QString, QPoint> QtPassSettings::pointSettings;
QHash<QString, QSize> QtPassSettings::sizeSettings;
QHash<QString, int> QtPassSettings::intSettings;
QHash<QString, bool> QtPassSettings::boolSettings;

QString QtPassSettings::getVersion(const QString &defaultValue) {
  return getStringValue(SettingsConstants::version, defaultValue);
}

void QtPassSettings::setVersion(const QString &version) {
  setStringValue(SettingsConstants::version, version);
}

QByteArray QtPassSettings::getGeometry(const QByteArray &defaultValue) {
  beginMainwindowGroup();
  QByteArray returnValue =
      getByteArrayValue(SettingsConstants::geometry, defaultValue);
  endSettingsGroup();
  return returnValue;
}

void QtPassSettings::setGeometry(const QByteArray &geometry) {
  beginMainwindowGroup();
  setByteArrayValue(SettingsConstants::geometry, geometry);
  endSettingsGroup();
}

QByteArray QtPassSettings::getSavestate(const QByteArray &defaultValue) {
  beginMainwindowGroup();
  QByteArray returnValue =
      getByteArrayValue(SettingsConstants::savestate, defaultValue);
  endSettingsGroup();
  return returnValue;
}

void QtPassSettings::setSavestate(const QByteArray &saveState) {
  beginMainwindowGroup();
  setByteArrayValue(SettingsConstants::savestate, saveState);
  endSettingsGroup();
}

QPoint QtPassSettings::getPos(const QPoint &defaultValue) {
  beginMainwindowGroup();
  QPoint returnValue = getPointValue(SettingsConstants::pos, defaultValue);
  endSettingsGroup();
  return returnValue;
}

void QtPassSettings::setPos(const QPoint &pos) {
  beginMainwindowGroup();
  setPointValue(SettingsConstants::pos, pos);
  endSettingsGroup();
}

QSize QtPassSettings::getSize(const QSize &defaultValue) {
  beginMainwindowGroup();
  QSize returnValue = getSizeValue(SettingsConstants::size, defaultValue);
  endSettingsGroup();
  return returnValue;
}

void QtPassSettings::setSize(const QSize &size) {
  beginMainwindowGroup();
  setSizeValue(SettingsConstants::size, size);
  endSettingsGroup();
}

int QtPassSettings::getSplitterLeft(const int &defaultValue) {
  beginMainwindowGroup();
  int returnValue = getIntValue(SettingsConstants::splitterLeft, defaultValue);
  endSettingsGroup();
  return returnValue;
}

void QtPassSettings::setSplitterLeft(const int &splitterLeft) {
  beginMainwindowGroup();
  setIntValue(SettingsConstants::splitterLeft, splitterLeft);
  endSettingsGroup();
}

int QtPassSettings::getSplitterRight(const int &defaultValue) {
  beginMainwindowGroup();
  int returnValue = getIntValue(SettingsConstants::splitterRight, defaultValue);
  endSettingsGroup();
  return returnValue;
}

void QtPassSettings::setSplitterRight(const int &splitterRight) {
  beginMainwindowGroup();
  setIntValue(SettingsConstants::splitterRight, splitterRight);
  endSettingsGroup();
}

bool QtPassSettings::isMaximized(const bool &defaultValue) {
  beginMainwindowGroup();
  bool returnValue = getBoolValue(SettingsConstants::maximized, defaultValue);
  endSettingsGroup();
  return returnValue;
}

void QtPassSettings::setMaximized(const bool &maximized) {
  beginMainwindowGroup();
  setBoolValue(SettingsConstants::maximized, maximized);
  endSettingsGroup();
}

bool QtPassSettings::isUsePass(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::usePass, defaultValue);
}

void QtPassSettings::setUsePass(const bool &usePass) {
  setBoolValue(SettingsConstants::usePass, usePass);
}

Enums::clipBoardType
QtPassSettings::getClipBoardType(const Enums::clipBoardType &defaultvalue) {
  return static_cast<Enums::clipBoardType>(getIntValue(
      SettingsConstants::clipBoardType, static_cast<int>(defaultvalue)));
}

void QtPassSettings::setClipBoardType(
    const Enums::clipBoardType &clipBoardType) {
  setIntValue(SettingsConstants::clipBoardType,
              static_cast<int>(clipBoardType));
}

bool QtPassSettings::isUseAutoclear(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::useAutoclear, defaultValue);
}

void QtPassSettings::setUseAutoclear(const bool &useAutoclear) {
  setBoolValue(SettingsConstants::useAutoclear, useAutoclear);
}

int QtPassSettings::getAutoclearSeconds(const int &defaultValue) {
  return getIntValue(SettingsConstants::autoclearSeconds, defaultValue);
}

void QtPassSettings::setAutoclearSeconds(const int &autoClearSeconds) {
  setIntValue(SettingsConstants::autoclearSeconds, autoClearSeconds);
}

bool QtPassSettings::isUseAutoclearPanel(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::useAutoclearPanel, defaultValue);
}

void QtPassSettings::setUseAutoclearPanel(const bool &useAutoclearPanel) {
  setBoolValue(SettingsConstants::useAutoclearPanel, useAutoclearPanel);
}

int QtPassSettings::getAutoclearPanelSeconds(const int &defaultValue) {
  return getIntValue(SettingsConstants::autoclearPanelSeconds, defaultValue);
}

void QtPassSettings::setAutoclearPanelSeconds(
    const int &autoClearPanelSeconds) {
  setIntValue(SettingsConstants::autoclearPanelSeconds, autoClearPanelSeconds);
}

bool QtPassSettings::isHidePassword(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::hidePassword, defaultValue);
}

void QtPassSettings::setHidePassword(const bool &hidePassword) {
  setBoolValue(SettingsConstants::hidePassword, hidePassword);
}

bool QtPassSettings::isHideContent(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::hideContent, defaultValue);
}

void QtPassSettings::setHideContent(const bool &hideContent) {
  setBoolValue(SettingsConstants::hideContent, hideContent);
}

bool QtPassSettings::isAddGPGId(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::addGPGId, defaultValue);
}

void QtPassSettings::setAddGPGId(const bool &addGPGId) {
  setBoolValue(SettingsConstants::addGPGId, addGPGId);
}

QString QtPassSettings::getPassStore(const QString &defaultValue) {
  QString returnValue =
      getStringValue(SettingsConstants::passStore, defaultValue);
  // ensure directory exists if never used pass or misconfigured.
  // otherwise process->setWorkingDirectory(passStore); will fail on execution.
  QDir().mkdir(returnValue);
  return returnValue;
}

void QtPassSettings::setPassStore(const QString &passStore) {
  setStringValue(SettingsConstants::passStore, passStore);
}

QString QtPassSettings::getPassExecutable(const QString &defaultValue) {
  return getStringValue(SettingsConstants::passExecutable, defaultValue);
}

void QtPassSettings::setPassExecutable(const QString &passExecutable) {
  setStringValue(SettingsConstants::passExecutable, passExecutable);
}

QString QtPassSettings::getGitExecutable(const QString &defaultValue) {
  return getStringValue(SettingsConstants::gitExecutable, defaultValue);
}

void QtPassSettings::setGitExecutable(const QString &gitExecutable) {
  setStringValue(SettingsConstants::gitExecutable, gitExecutable);
}

QString QtPassSettings::getGpgExecutable(const QString &defaultValue) {
  return getStringValue(SettingsConstants::gpgExecutable, defaultValue);
}

void QtPassSettings::setGpgExecutable(const QString &gpgExecutable) {
  setStringValue(SettingsConstants::gpgExecutable, gpgExecutable);
}

QString QtPassSettings::getPwgenExecutable(const QString &defaultValue) {
  return getStringValue(SettingsConstants::pwgenExecutable, defaultValue);
}

void QtPassSettings::setPwgenExecutable(const QString &pwgenExecutable) {
  setStringValue(SettingsConstants::pwgenExecutable, pwgenExecutable);
}

QString QtPassSettings::getGpgHome(const QString &defaultValue) {
  return getStringValue(SettingsConstants::gpgHome, defaultValue);
}

void QtPassSettings::setGpgHome(const QString &gpgHome) {
  setStringValue(SettingsConstants::gpgHome, gpgHome);
}

bool QtPassSettings::isUseWebDav(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::useWebDav, defaultValue);
}

void QtPassSettings::setUseWebDav(const bool &useWebDav) {
  setBoolValue(SettingsConstants::useWebDav, useWebDav);
}

QString QtPassSettings::getWebDavUrl(const QString &defaultValue) {
  return getStringValue(SettingsConstants::webDavUrl, defaultValue);
}

void QtPassSettings::setWebDavUrl(const QString &webDavUrl) {
  setStringValue(SettingsConstants::webDavUrl, webDavUrl);
}

QString QtPassSettings::getWebDavUser(const QString &defaultValue) {
  return getStringValue(SettingsConstants::webDavUser, defaultValue);
}

void QtPassSettings::setWebDavUser(const QString &webDavUser) {
  setStringValue(SettingsConstants::webDavUser, webDavUser);
}

QString QtPassSettings::getWebDavPassword(const QString &defaultValue) {
  return getStringValue(SettingsConstants::webDavPassword, defaultValue);
}

void QtPassSettings::setWebDavPassword(const QString &webDavPassword) {
  setStringValue(SettingsConstants::webDavPassword, webDavPassword);
}

QString QtPassSettings::getProfile(const QString &defaultValue) {
  return getStringValue(SettingsConstants::profile, defaultValue);
}

void QtPassSettings::setProfile(const QString &profile) {
  setStringValue(SettingsConstants::profile, profile);
}

bool QtPassSettings::isUseGit(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::useGit, defaultValue);
}

void QtPassSettings::setUseGit(const bool &useGit) {
  setBoolValue(SettingsConstants::useGit, useGit);
}

bool QtPassSettings::isUsePwgen(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::usePwgen, defaultValue);
}

void QtPassSettings::setUsePwgen(const bool &usePwgen) {
  setBoolValue(SettingsConstants::usePwgen, usePwgen);
}

bool QtPassSettings::isAvoidCapitals(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::avoidCapitals, defaultValue);
}

void QtPassSettings::setAvoidCapitals(const bool &avoidCapitals) {
  setBoolValue(SettingsConstants::avoidCapitals, avoidCapitals);
}

bool QtPassSettings::isAvoidNumbers(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::avoidNumbers, defaultValue);
}

void QtPassSettings::setAvoidNumbers(const bool &avoidNumbers) {
  setBoolValue(SettingsConstants::avoidNumbers, avoidNumbers);
}

bool QtPassSettings::isLessRandom(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::lessRandom, defaultValue);
}

void QtPassSettings::setLessRandom(const bool &lessRandom) {
  setBoolValue(SettingsConstants::lessRandom, lessRandom);
}

bool QtPassSettings::isUseSymbols(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::useSymbols, defaultValue);
}

void QtPassSettings::setUseSymbols(const bool &useSymbols) {
  setBoolValue(SettingsConstants::useSymbols, useSymbols);
}

int QtPassSettings::getPasswordCharsSelected(const int &defaultValue) {
  return getIntValue(SettingsConstants::passwordCharsSelected, defaultValue);
}

void QtPassSettings::setPasswordCharsSelected(
    const int &passwordCharsSelected) {
  setIntValue(SettingsConstants::passwordCharsSelected, passwordCharsSelected);
}

int QtPassSettings::getPasswordLength(const int &defaultValue) {
  return getIntValue(SettingsConstants::passwordLength, defaultValue);
}

void QtPassSettings::setPasswordLength(const int &passwordLength) {
  setIntValue(SettingsConstants::passwordLength, passwordLength);
}

int QtPassSettings::getPasswordCharsselection(const int &defaultValue) {
  return getIntValue(SettingsConstants::passwordCharsselection, defaultValue);
}

void QtPassSettings::setPasswordCharsselection(
    const int &passwordCharsselection) {
  setIntValue(SettingsConstants::passwordCharsselection,
              passwordCharsselection);
}

QString QtPassSettings::getPasswordChars(const QString &defaultValue) {
  return getStringValue(SettingsConstants::passwordChars, defaultValue);
}

void QtPassSettings::setPasswordChars(const QString &passwordChars) {
  setStringValue(SettingsConstants::passwordChars, passwordChars);
}

bool QtPassSettings::isUseTrayIcon(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::useTrayIcon, defaultValue);
}

void QtPassSettings::setUseTrayIcon(const bool &useTrayIcon) {
  setBoolValue(SettingsConstants::useTrayIcon, useTrayIcon);
}

bool QtPassSettings::isHideOnClose(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::hideOnClose, defaultValue);
}

void QtPassSettings::setHideOnClose(const bool &hideOnClose) {
  setBoolValue(SettingsConstants::hideOnClose, hideOnClose);
}

bool QtPassSettings::isStartMinimized(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::startMinimized, defaultValue);
}

void QtPassSettings::setStartMinimized(const bool &startMinimized) {
  setBoolValue(SettingsConstants::startMinimized, startMinimized);
}

bool QtPassSettings::isAlwaysOnTop(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::alwaysOnTop, defaultValue);
}

void QtPassSettings::setAlwaysOnTop(const bool &alwaysOnTop) {
  setBoolValue(SettingsConstants::alwaysOnTop, alwaysOnTop);
}

bool QtPassSettings::isAutoPull(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::autoPull, defaultValue);
}

void QtPassSettings::setAutoPull(const bool &autoPull) {
  setBoolValue(SettingsConstants::autoPull, autoPull);
}

bool QtPassSettings::isAutoPush(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::autoPush, defaultValue);
}

void QtPassSettings::setAutoPush(const bool &autoPush) {
  setBoolValue(SettingsConstants::autoPush, autoPush);
}

QString QtPassSettings::getPassTemplate(const QString &defaultValue) {
  return getStringValue(SettingsConstants::passTemplate, defaultValue);
}

void QtPassSettings::setPassTemplate(const QString &passTemplate) {
  setStringValue(SettingsConstants::passTemplate, passTemplate);
}

bool QtPassSettings::isUseTemplate(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::useTemplate, defaultValue);
}

void QtPassSettings::setUseTemplate(const bool &useTemplate) {
  setBoolValue(SettingsConstants::useTemplate, useTemplate);
}

bool QtPassSettings::isTemplateAllFields(const bool &defaultValue) {
  return getBoolValue(SettingsConstants::templateAllFields, defaultValue);
}

void QtPassSettings::setTemplateAllFields(const bool &templateAllFields) {
  setBoolValue(SettingsConstants::templateAllFields, templateAllFields);
}

QStringList QtPassSettings::getChildKeysFromCurrentGroup() {
  return getSettings().childKeys();
}

QHash<QString, QString> QtPassSettings::getProfiles() {
  beginProfilesGroup();
  QStringList childrenKeys = getChildKeysFromCurrentGroup();
  QHash<QString, QString> profiles;
  foreach (QString key, childrenKeys) {
    profiles.insert(key, getSetting(key).toString());
  }
  endSettingsGroup();
  return profiles;
}

void QtPassSettings::setProfiles(const QHash<QString, QString> &profiles) {
  getSettings().remove(SettingsConstants::groupProfiles);
  beginProfilesGroup();
  QHash<QString, QString>::const_iterator i = profiles.begin();
  for (; i != profiles.end(); ++i) {
    setSetting(i.key(), i.value());
  }
  endSettingsGroup();
}

QSettings &QtPassSettings::getSettings() {
  if (!QtPassSettings::initialized) {
    QString portable_ini = QCoreApplication::applicationDirPath() +
                           QDir::separator() + "qtpass.ini";
    if (QFile(portable_ini).exists()) {
      settings.reset(new QSettings(portable_ini, QSettings::IniFormat));
    } else {
      settings.reset(new QSettings("IJHack", "QtPass"));
    }
  }
  initialized = true;
  return *settings;
}

QString QtPassSettings::getStringValue(const QString &key,
                                       const QString &defaultValue) {
  QString stringValue;
  if (stringSettings.contains(key)) {
    stringValue = stringSettings.take(key);
  } else {
    stringValue = getSettings().value(key, defaultValue).toString();
    stringSettings.insert(key, stringValue);
  }
  return stringValue;
}

int QtPassSettings::getIntValue(const QString &key, const int &defaultValue) {
  int intValue;
  if (intSettings.contains(key)) {
    intValue = intSettings.take(key);
  } else {
    intValue = getSettings().value(key, defaultValue).toInt();
    intSettings.insert(key, intValue);
  }
  return intValue;
}

bool QtPassSettings::getBoolValue(const QString &key,
                                  const bool &defaultValue) {
  bool boolValue;
  if (boolSettings.contains(key)) {
    boolValue = boolSettings.take(key);
  } else {
    boolValue = getSettings().value(key, defaultValue).toBool();
    boolSettings.insert(key, boolValue);
  }
  return boolValue;
}

QByteArray QtPassSettings::getByteArrayValue(const QString &key,
                                             const QByteArray &defaultValue) {
  QByteArray byteArrayValue;
  if (byteArraySettings.contains(key)) {
    byteArrayValue = byteArraySettings.take(key);
  } else {
    byteArrayValue = getSettings().value(key, defaultValue).toByteArray();
    byteArraySettings.insert(key, byteArrayValue);
  }
  return byteArrayValue;
}

QPoint QtPassSettings::getPointValue(const QString &key,
                                     const QPoint &defaultValue) {
  QPoint pointValue;
  if (pointSettings.contains(key)) {
    pointValue = pointSettings.take(key);
  } else {
    pointValue = getSettings().value(key, defaultValue).toPoint();
    pointSettings.insert(key, pointValue);
  }
  return pointValue;
}

QSize QtPassSettings::getSizeValue(const QString &key,
                                   const QSize &defaultValue) {
  QSize sizeValue;
  if (sizeSettings.contains(key)) {
    sizeValue = sizeSettings.take(key);
  } else {
    sizeValue = getSettings().value(key, defaultValue).toSize();
    sizeSettings.insert(key, sizeValue);
  }
  return sizeValue;
}

void QtPassSettings::setStringValue(const QString &key,
                                    const QString &stringValue) {
  stringSettings.insert(key, stringValue);
  getSettings().setValue(key, stringValue);
}

void QtPassSettings::setIntValue(const QString &key, const int &intValue) {
  intSettings.insert(key, intValue);
  getSettings().setValue(key, intValue);
}

void QtPassSettings::setBoolValue(const QString &key, const bool &boolValue) {
  boolSettings.insert(key, boolValue);
  getSettings().setValue(key, boolValue);
}

void QtPassSettings::setByteArrayValue(const QString &key,
                                       const QByteArray &byteArrayValue) {
  byteArraySettings.insert(key, byteArrayValue);
  getSettings().setValue(key, byteArrayValue);
}

void QtPassSettings::setPointValue(const QString &key,
                                   const QPoint &pointValue) {
  pointSettings.insert(key, pointValue);
  getSettings().setValue(key, pointValue);
}

void QtPassSettings::setSizeValue(const QString &key, const QSize &sizeValue) {
  sizeSettings.insert(key, sizeValue);
  getSettings().setValue(key, sizeValue);
}

void QtPassSettings::beginSettingsGroup(const QString &groupName) {
  getSettings().beginGroup(groupName);
}

void QtPassSettings::endSettingsGroup() { getSettings().endGroup(); }

void QtPassSettings::beginMainwindowGroup() {
  getSettings().beginGroup(SettingsConstants::groupMainwindow);
}

void QtPassSettings::beginProfilesGroup() {
  getSettings().beginGroup(SettingsConstants::groupProfiles);
}

QVariant QtPassSettings::getSetting(const QString &key,
                                    const QVariant &defaultValue) {
  return getSettings().value(key, defaultValue);
}

void QtPassSettings::setSetting(const QString &key, const QVariant &value) {
  getSettings().setValue(key, value);
}
