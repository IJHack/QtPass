#include "qtpasssettings.h"
#include "pass.h"

#include "util.h"

#include <QCoreApplication>

bool QtPassSettings::initialized = false;

Pass *QtPassSettings::pass;
// Go via pointer to avoid dynamic initialization,
// due to "random" initialization order realtive to other
// globals, especially around QObject emtadata dynamic initialization
// can lead to crashes depending on compiler, linker etc.
QScopedPointer<RealPass> QtPassSettings::realPass;
QScopedPointer<ImitatePass> QtPassSettings::imitatePass;

QtPassSettings *QtPassSettings::m_instance = nullptr;
QtPassSettings *QtPassSettings::getInstance() {
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

PasswordConfiguration QtPassSettings::getPasswordConfiguration() {
  PasswordConfiguration config;

  config.length =
      getInstance()->value(SettingsConstants::passwordLength, 0).toInt();
  config.selected = static_cast<PasswordConfiguration::characterSet>(
      getInstance()
          ->value(SettingsConstants::passwordCharsselection, 0)
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

QHash<QString, QHash<QString, QString>> QtPassSettings::getProfiles() {
  getInstance()->beginGroup(SettingsConstants::profile);
  QHash<QString, QHash<QString, QString>> profiles;

  // migration from version <= v1.3.2: profiles datastructure
  QStringList childKeys = getInstance()->childKeys();
  if (!childKeys.empty()) {
    foreach (QString key, childKeys) {
      QHash<QString, QString> profile;
      profile.insert("path", getInstance()->value(key).toString());
      profile.insert("signingKey", "");
      profiles.insert(key, profile);
    }
  }
  // /migration from version <= v1.3.2

  QStringList childGroups = getInstance()->childGroups();
  foreach (QString group, childGroups) {
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

Pass *QtPassSettings::getPass() {
  if (!pass) {
    if (isUsePass()) {
      QtPassSettings::pass = getRealPass();
    } else {
      QtPassSettings::pass = getImitatePass();
    }
    pass->init();
  }
  return pass;
}

QString QtPassSettings::getVersion(const QString &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::version, defaultValue)
      .toString();
}
void QtPassSettings::setVersion(const QString &version) {
  getInstance()->setValue(SettingsConstants::version, version);
}

QByteArray QtPassSettings::getGeometry(const QByteArray &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::geometry, defaultValue)
      .toByteArray();
}
void QtPassSettings::setGeometry(const QByteArray &geometry) {
  getInstance()->setValue(SettingsConstants::geometry, geometry);
}

QByteArray QtPassSettings::getSavestate(const QByteArray &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::savestate, defaultValue)
      .toByteArray();
}
void QtPassSettings::setSavestate(const QByteArray &saveState) {
  getInstance()->setValue(SettingsConstants::savestate, saveState);
}

QPoint QtPassSettings::getPos(const QPoint &defaultValue) {
  return getInstance()->value(SettingsConstants::pos, defaultValue).toPoint();
}
void QtPassSettings::setPos(const QPoint &pos) {
  getInstance()->setValue(SettingsConstants::pos, pos);
}

QSize QtPassSettings::getSize(const QSize &defaultValue) {
  return getInstance()->value(SettingsConstants::size, defaultValue).toSize();
}
void QtPassSettings::setSize(const QSize &size) {
  getInstance()->setValue(SettingsConstants::size, size);
}

bool QtPassSettings::isMaximized(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::maximized, defaultValue)
      .toBool();
}
void QtPassSettings::setMaximized(const bool &maximized) {
  getInstance()->setValue(SettingsConstants::maximized, maximized);
}

bool QtPassSettings::isUsePass(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::usePass, defaultValue)
      .toBool();
}
void QtPassSettings::setUsePass(const bool &usePass) {
  if (usePass) {
    QtPassSettings::pass = getRealPass();
  } else {
    QtPassSettings::pass = getImitatePass();
  }
  getInstance()->setValue(SettingsConstants::usePass, usePass);
}

int QtPassSettings::getClipBoardTypeRaw(
    const Enums::clipBoardType &defaultvalue) {
  return getInstance()
      ->value(SettingsConstants::clipBoardType, static_cast<int>(defaultvalue))
      .toInt();
}

Enums::clipBoardType
QtPassSettings::getClipBoardType(const Enums::clipBoardType &defaultvalue) {
  return static_cast<Enums::clipBoardType>(getClipBoardTypeRaw(defaultvalue));
}
void QtPassSettings::setClipBoardType(const int &clipBoardType) {
  getInstance()->setValue(SettingsConstants::clipBoardType, clipBoardType);
}

bool QtPassSettings::isUseSelection(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::useSelection, defaultValue)
      .toBool();
}
void QtPassSettings::setUseSelection(const bool &useSelection) {
  getInstance()->setValue(SettingsConstants::useSelection, useSelection);
}

bool QtPassSettings::isUseAutoclear(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::useAutoclear, defaultValue)
      .toBool();
}
void QtPassSettings::setUseAutoclear(const bool &useAutoclear) {
  getInstance()->setValue(SettingsConstants::useAutoclear, useAutoclear);
}

int QtPassSettings::getAutoclearSeconds(const int &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::autoclearSeconds, defaultValue)
      .toInt();
}
void QtPassSettings::setAutoclearSeconds(const int &autoClearSeconds) {
  getInstance()->setValue(SettingsConstants::autoclearSeconds,
                          autoClearSeconds);
}

bool QtPassSettings::isUseAutoclearPanel(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::useAutoclearPanel, defaultValue)
      .toBool();
}
void QtPassSettings::setUseAutoclearPanel(const bool &useAutoclearPanel) {
  getInstance()->setValue(SettingsConstants::useAutoclearPanel,
                          useAutoclearPanel);
}

int QtPassSettings::getAutoclearPanelSeconds(const int &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::autoclearPanelSeconds, defaultValue)
      .toInt();
}
void QtPassSettings::setAutoclearPanelSeconds(
    const int &autoClearPanelSeconds) {
  getInstance()->setValue(SettingsConstants::autoclearPanelSeconds,
                          autoClearPanelSeconds);
}

bool QtPassSettings::isHidePassword(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::hidePassword, defaultValue)
      .toBool();
}
void QtPassSettings::setHidePassword(const bool &hidePassword) {
  getInstance()->setValue(SettingsConstants::hidePassword, hidePassword);
}

bool QtPassSettings::isHideContent(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::hideContent, defaultValue)
      .toBool();
}
void QtPassSettings::setHideContent(const bool &hideContent) {
  getInstance()->setValue(SettingsConstants::hideContent, hideContent);
}

bool QtPassSettings::isUseMonospace(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::useMonospace, defaultValue)
      .toBool();
}
void QtPassSettings::setUseMonospace(const bool &useMonospace) {
  getInstance()->setValue(SettingsConstants::useMonospace, useMonospace);
}

bool QtPassSettings::isDisplayAsIs(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::displayAsIs, defaultValue)
      .toBool();
}
void QtPassSettings::setDisplayAsIs(const bool &displayAsIs) {
  getInstance()->setValue(SettingsConstants::displayAsIs, displayAsIs);
}

bool QtPassSettings::isNoLineWrapping(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::noLineWrapping, defaultValue)
      .toBool();
}
void QtPassSettings::setNoLineWrapping(const bool &noLineWrapping) {
  getInstance()->setValue(SettingsConstants::noLineWrapping, noLineWrapping);
}

bool QtPassSettings::isAddGPGId(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::addGPGId, defaultValue)
      .toBool();
}
void QtPassSettings::setAddGPGId(const bool &addGPGId) {
  getInstance()->setValue(SettingsConstants::addGPGId, addGPGId);
}

QString QtPassSettings::getPassStore(const QString &defaultValue) {
  QString returnValue = getInstance()
                            ->value(SettingsConstants::passStore, defaultValue)
                            .toString();

  // Normalize the path string
  returnValue = QDir(returnValue).absolutePath();

  // ensure directory exists if never used pass or misconfigured.
  // otherwise process->setWorkingDirectory(passStore); will fail on execution.
  if (!QDir(returnValue).exists()) {
    QDir().mkdir(returnValue);
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

QString QtPassSettings::getPassSigningKey(const QString &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::passSigningKey, defaultValue)
      .toString();
}
void QtPassSettings::setPassSigningKey(const QString &passSigningKey) {
  getInstance()->setValue(SettingsConstants::passSigningKey, passSigningKey);
}

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
QString QtPassSettings::getPassExecutable(const QString &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::passExecutable, defaultValue)
      .toString();
}
void QtPassSettings::setPassExecutable(const QString &passExecutable) {
  getInstance()->setValue(SettingsConstants::passExecutable, passExecutable);
}

QString QtPassSettings::getGitExecutable(const QString &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::gitExecutable, defaultValue)
      .toString();
}
void QtPassSettings::setGitExecutable(const QString &gitExecutable) {
  getInstance()->setValue(SettingsConstants::gitExecutable, gitExecutable);
}

QString QtPassSettings::getGpgExecutable(const QString &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::gpgExecutable, defaultValue)
      .toString();
}
void QtPassSettings::setGpgExecutable(const QString &gpgExecutable) {
  getInstance()->setValue(SettingsConstants::gpgExecutable, gpgExecutable);
}

QString QtPassSettings::getPwgenExecutable(const QString &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::pwgenExecutable, defaultValue)
      .toString();
}
void QtPassSettings::setPwgenExecutable(const QString &pwgenExecutable) {
  getInstance()->setValue(SettingsConstants::pwgenExecutable, pwgenExecutable);
}

QString QtPassSettings::getGpgHome(const QString &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::gpgHome, defaultValue)
      .toString();
}

bool QtPassSettings::isUseWebDav(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::useWebDav, defaultValue)
      .toBool();
}
void QtPassSettings::setUseWebDav(const bool &useWebDav) {
  getInstance()->setValue(SettingsConstants::useWebDav, useWebDav);
}

QString QtPassSettings::getWebDavUrl(const QString &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::webDavUrl, defaultValue)
      .toString();
}
void QtPassSettings::setWebDavUrl(const QString &webDavUrl) {
  getInstance()->setValue(SettingsConstants::webDavUrl, webDavUrl);
}

QString QtPassSettings::getWebDavUser(const QString &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::webDavUser, defaultValue)
      .toString();
}
void QtPassSettings::setWebDavUser(const QString &webDavUser) {
  getInstance()->setValue(SettingsConstants::webDavUser, webDavUser);
}

QString QtPassSettings::getWebDavPassword(const QString &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::webDavPassword, defaultValue)
      .toString();
}
void QtPassSettings::setWebDavPassword(const QString &webDavPassword) {
  getInstance()->setValue(SettingsConstants::webDavPassword, webDavPassword);
}

QString QtPassSettings::getProfile(const QString &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::profile, defaultValue)
      .toString();
}
void QtPassSettings::setProfile(const QString &profile) {
  getInstance()->setValue(SettingsConstants::profile, profile);
}

bool QtPassSettings::isUseGit(const bool &defaultValue) {
  return getInstance()->value(SettingsConstants::useGit, defaultValue).toBool();
}
void QtPassSettings::setUseGit(const bool &useGit) {
  getInstance()->setValue(SettingsConstants::useGit, useGit);
}

bool QtPassSettings::isUseOtp(const bool &defaultValue) {
  return getInstance()->value(SettingsConstants::useOtp, defaultValue).toBool();
}

void QtPassSettings::setUseOtp(const bool &useOtp) {
  getInstance()->setValue(SettingsConstants::useOtp, useOtp);
}

bool QtPassSettings::isUseQrencode(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::useQrencode, defaultValue)
      .toBool();
}

void QtPassSettings::setUseQrencode(const bool &useQrencode) {
  getInstance()->setValue(SettingsConstants::useQrencode, useQrencode);
}

QString QtPassSettings::getQrencodeExecutable(const QString &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::qrencodeExecutable, defaultValue)
      .toString();
}
void QtPassSettings::setQrencodeExecutable(const QString &qrencodeExecutable) {
  getInstance()->setValue(SettingsConstants::qrencodeExecutable,
                          qrencodeExecutable);
}

bool QtPassSettings::isUsePwgen(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::usePwgen, defaultValue)
      .toBool();
}
void QtPassSettings::setUsePwgen(const bool &usePwgen) {
  getInstance()->setValue(SettingsConstants::usePwgen, usePwgen);
}

bool QtPassSettings::isAvoidCapitals(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::avoidCapitals, defaultValue)
      .toBool();
}
void QtPassSettings::setAvoidCapitals(const bool &avoidCapitals) {
  getInstance()->setValue(SettingsConstants::avoidCapitals, avoidCapitals);
}

bool QtPassSettings::isAvoidNumbers(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::avoidNumbers, defaultValue)
      .toBool();
}
void QtPassSettings::setAvoidNumbers(const bool &avoidNumbers) {
  getInstance()->setValue(SettingsConstants::avoidNumbers, avoidNumbers);
}

bool QtPassSettings::isLessRandom(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::lessRandom, defaultValue)
      .toBool();
}
void QtPassSettings::setLessRandom(const bool &lessRandom) {
  getInstance()->setValue(SettingsConstants::lessRandom, lessRandom);
}

bool QtPassSettings::isUseSymbols(const bool &defaultValue) {
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
void QtPassSettings::setPasswordCharsselection(
    const int &passwordCharsselection) {
  getInstance()->setValue(SettingsConstants::passwordCharsselection,
                          passwordCharsselection);
}
void QtPassSettings::setPasswordChars(const QString &passwordChars) {
  getInstance()->setValue(SettingsConstants::passwordChars, passwordChars);
}

bool QtPassSettings::isUseTrayIcon(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::useTrayIcon, defaultValue)
      .toBool();
}
void QtPassSettings::setUseTrayIcon(const bool &useTrayIcon) {
  getInstance()->setValue(SettingsConstants::useTrayIcon, useTrayIcon);
}

bool QtPassSettings::isHideOnClose(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::hideOnClose, defaultValue)
      .toBool();
}
void QtPassSettings::setHideOnClose(const bool &hideOnClose) {
  getInstance()->setValue(SettingsConstants::hideOnClose, hideOnClose);
}

bool QtPassSettings::isStartMinimized(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::startMinimized, defaultValue)
      .toBool();
}
void QtPassSettings::setStartMinimized(const bool &startMinimized) {
  getInstance()->setValue(SettingsConstants::startMinimized, startMinimized);
}

bool QtPassSettings::isAlwaysOnTop(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::alwaysOnTop, defaultValue)
      .toBool();
}
void QtPassSettings::setAlwaysOnTop(const bool &alwaysOnTop) {
  getInstance()->setValue(SettingsConstants::alwaysOnTop, alwaysOnTop);
}

bool QtPassSettings::isAutoPull(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::autoPull, defaultValue)
      .toBool();
}
void QtPassSettings::setAutoPull(const bool &autoPull) {
  getInstance()->setValue(SettingsConstants::autoPull, autoPull);
}

bool QtPassSettings::isAutoPush(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::autoPush, defaultValue)
      .toBool();
}
void QtPassSettings::setAutoPush(const bool &autoPush) {
  getInstance()->setValue(SettingsConstants::autoPush, autoPush);
}

QString QtPassSettings::getPassTemplate(const QString &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::passTemplate, defaultValue)
      .toString();
}
void QtPassSettings::setPassTemplate(const QString &passTemplate) {
  getInstance()->setValue(SettingsConstants::passTemplate, passTemplate);
}

bool QtPassSettings::isUseTemplate(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::useTemplate, defaultValue)
      .toBool();
}
void QtPassSettings::setUseTemplate(const bool &useTemplate) {
  getInstance()->setValue(SettingsConstants::useTemplate, useTemplate);
}

bool QtPassSettings::isTemplateAllFields(const bool &defaultValue) {
  return getInstance()
      ->value(SettingsConstants::templateAllFields, defaultValue)
      .toBool();
}
void QtPassSettings::setTemplateAllFields(const bool &templateAllFields) {
  getInstance()->setValue(SettingsConstants::templateAllFields,
                          templateAllFields);
}

RealPass *QtPassSettings::getRealPass() {
  if (realPass.isNull())
    realPass.reset(new RealPass());
  return realPass.data();
}
ImitatePass *QtPassSettings::getImitatePass() {
  if (imitatePass.isNull())
    imitatePass.reset(new ImitatePass());
  return imitatePass.data();
}
