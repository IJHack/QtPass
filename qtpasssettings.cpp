#include "qtpasssettings.h"
#include "settingsconstants.h"

QtPassSettings::QtPassSettings(QObject *parent) : QObject(parent)
{

}

bool QtPassSettings::initialized = false;

QScopedPointer<QSettings> QtPassSettings::settings;
QHash<QString, QString> QtPassSettings::stringSettings;
QHash<QString, QByteArray> QtPassSettings::byteArraySettings;
QHash<QString, QPoint> QtPassSettings::pointSettings;
QHash<QString, QSize> QtPassSettings::sizeSettings;
QHash<QString, int> QtPassSettings::intSettings;
QHash<QString, bool> QtPassSettings::boolSettings;

void QtPassSettings::saveAllSettings()
{

    for(QHash<QString, QString>::iterator i=stringSettings.begin(); i != QtPassSettings::stringSettings.end(); ++i)
    {
        getSettings().setValue(i.key(),i.value());
    }
    for(QHash<QString, QByteArray>::iterator i=QtPassSettings::byteArraySettings.begin(); i != QtPassSettings::byteArraySettings.end(); ++i)
    {
        getSettings().setValue(i.key(),i.value());
    }
    for(QHash<QString, QPoint>::iterator i=QtPassSettings::pointSettings.begin(); i != QtPassSettings::pointSettings.end(); ++i)
    {
        getSettings().setValue(i.key(),i.value());
    }
    for(QHash<QString, QSize>::iterator i=QtPassSettings::sizeSettings.begin(); i != QtPassSettings::sizeSettings.end(); ++i)
    {
        getSettings().setValue(i.key(),i.value());
    }
    for(QHash<QString, int>::iterator i=QtPassSettings::intSettings.begin(); i != QtPassSettings::intSettings.end(); ++i)
    {
        getSettings().setValue(i.key(),i.value());
    }
    for(QHash<QString, bool>::iterator i=QtPassSettings::boolSettings.begin(); i !=QtPassSettings::boolSettings.end(); ++i)
    {
        getSettings().setValue(i.key(),i.value());
    }
}

QString QtPassSettings::getVersion(const QString &defaultValue)
{
    return getStringValue(SettingsConstants::version, defaultValue);
}

void QtPassSettings::setVersion(const QString &version)
{
    setStringValue(SettingsConstants::version, version);
}

QByteArray QtPassSettings::getGeometry(const QByteArray &defaultValue)
{
    beginMainwindowGroup();
    QByteArray returnValue = getByteArrayValue("geometry", defaultValue);
    endSettingsGroup();
    return returnValue;
}

void QtPassSettings::setGeometry(const QByteArray &geometry)
{
    setByteArrayValue("geometry", geometry);
}

QByteArray QtPassSettings::getSavestate(const QByteArray &defaultValue)
{
    beginMainwindowGroup();
    QByteArray returnValue = getByteArrayValue("savestate", defaultValue);
    endSettingsGroup();
    return returnValue;
}

void QtPassSettings::setSavestate(const QByteArray &saveState)
{
    getByteArrayValue("savestate", saveState);
}

QPoint QtPassSettings::getPos(const QPoint &defaultValue)
{
    beginMainwindowGroup();
    QPoint returnValue = getPointValue("pos", defaultValue);
    endSettingsGroup();
    return returnValue;
}

void QtPassSettings::setPos(const QPoint &pos)
{
    setPointValue("pos", pos);
}

QSize QtPassSettings::getSize(const QSize &defaultValue)
{
    beginMainwindowGroup();
    QSize returnValue = getSizeValue("size", defaultValue);
    endSettingsGroup();
    return returnValue;
}

void QtPassSettings::setSize(const QSize &size)
{
    setSizeValue("size", size);
}

int QtPassSettings::getSplitterLeft(const int &defaultValue)
{
    beginMainwindowGroup();
    int returnValue = getIntValue("splitterLeft", defaultValue);
    endSettingsGroup();
    return returnValue;
}

void QtPassSettings::setSplitterLeft(const int &splitterLeft)
{
    setIntValue("splitterLeft", splitterLeft);
}

int QtPassSettings::getSplitterRight(const int &defaultValue)
{
    beginMainwindowGroup();
    int returnValue = getIntValue("splitterRight", defaultValue);
    endSettingsGroup();
    return returnValue;
}

void QtPassSettings::setSplitterRight(const int &splitterRight)
{
    setIntValue("splitterRight", splitterRight);
}

bool QtPassSettings::isMaximized(const bool &defaultValue)
{
    beginMainwindowGroup();
    bool returnValue = getBoolValue("maximized", defaultValue);
    endSettingsGroup();
    return returnValue;
}

void QtPassSettings::setMaximized(const bool &maximized)
{
    setBoolValue("maximized", maximized);
}

bool QtPassSettings::isUsePass(const bool &defaultValue)
{
    return getBoolValue("usePass", defaultValue);
}

void QtPassSettings::setUsePass(const bool &usePass)
{
    setBoolValue("usePass", usePass);
}

Enums::clipBoardType QtPassSettings::getClipBoardType(const Enums::clipBoardType &defaultvalue)
{
    return static_cast<Enums::clipBoardType>(getIntValue("clipBoardType",static_cast<int>(defaultvalue)));
}

void QtPassSettings::setClipBoardType(const Enums::clipBoardType &clipBoardType)
{
    setIntValue("clipBoardType",static_cast<int>(clipBoardType));
}

bool QtPassSettings::isUseAutoclear(const bool &defaultValue)
{
    return getBoolValue("useAutoclear", defaultValue);
}

void QtPassSettings::setUseAutoclear(const bool &useAutoclear)
{
    setBoolValue("useAutoclear", useAutoclear);
}

int QtPassSettings::getAutoclearSeconds(const int &defaultValue)
{
    return getIntValue("autoclearSeconds", defaultValue);
}

void QtPassSettings::setAutoclearSeconds(const int &autoClearSeconds)
{
    setIntValue("autoclearSeconds", autoClearSeconds);
}

bool QtPassSettings::isUseAutoclearPanel(const bool &defaultValue)
{
    return getBoolValue("useAutoclearPanel", defaultValue);
}

void QtPassSettings::setUseAutoclearPanel(const bool &useAutoclearPanel)
{
    setBoolValue("useAutoclearPanel", useAutoclearPanel);
}

int QtPassSettings::getAutoclearPanelSeconds(const int &defaultValue)
{
    return getIntValue("autoclearPanelSeconds", defaultValue);
}

void QtPassSettings::setAutoclearPanelSeconds(const int &autoClearPanelSeconds)
{
    setIntValue("autoclearPanelSeconds", autoClearPanelSeconds);
}

bool QtPassSettings::isHidePassword(const bool &defaultValue)
{
    return getBoolValue("hidePassword", defaultValue);
}

void QtPassSettings::setHidePassword(const bool &hidePassword)
{
    setBoolValue("hidePassword", hidePassword);
}

bool QtPassSettings::isHideContent(const bool &defaultValue)
{
    return getBoolValue("hideContent", defaultValue);
}

void QtPassSettings::setHideContent(const bool &hideContent)
{
    setBoolValue("hideContent", hideContent);
}

bool QtPassSettings::isAddGPGId(const bool &defaultValue)
{
    return getBoolValue("addGPGId", defaultValue);
}

void QtPassSettings::setAddGPGId(const bool &addGPGId)
{
    setBoolValue("addGPGId", addGPGId);
}

QString QtPassSettings::getPassStore(const QString &defaultValue)
{
    return getStringValue("passStore", defaultValue);
}

void QtPassSettings::setPassStore(const QString &passStore)
{
    setStringValue("passStore", passStore);
}

QString QtPassSettings::getPassExecutable(const QString &defaultValue)
{
    return getStringValue("passExecutable", defaultValue);
}

void QtPassSettings::setPassExecutable(const QString &passExecutable)
{
    setStringValue("passExecutable", passExecutable);
}

QString QtPassSettings::getGitExecutable(const QString &defaultValue)
{
    return getStringValue("gitExecutable", defaultValue);
}

void QtPassSettings::setGitExecutable(const QString &gitExecutable)
{
    setStringValue("gitExecutable", gitExecutable);
}

QString QtPassSettings::getGpgExecutable(const QString &defaultValue)
{
    return getStringValue("gpgExecutable", defaultValue);
}

void QtPassSettings::setGpgExecutable(const QString &gpgExecutable)
{
    setStringValue("gpgExecutable", gpgExecutable);
}

QString QtPassSettings::getPwgenExecutable(const QString &defaultValue)
{
    return getStringValue("pwgenExecutable", defaultValue);
}

void QtPassSettings::setPwgenExecutable(const QString &pwgenExecutable)
{
    setStringValue("pwgenExecutable", pwgenExecutable);
}

QString QtPassSettings::getGpgHome(const QString &defaultValue)
{
    return getStringValue("gpgHome", defaultValue);
}

void QtPassSettings::setGpgHome(const QString &gpgHome)
{
    setStringValue("gpgHome", gpgHome);
}

bool QtPassSettings::isUseWebDav(const bool &defaultValue)
{
    return getBoolValue("useWebDav", defaultValue);
}

void QtPassSettings::setUseWebDav(const bool &useWebDav)
{
    setBoolValue("useWebDav", useWebDav);
}

QString QtPassSettings::getWebDavUrl(const QString &defaultValue)
{
    return getStringValue("webDavUrl", defaultValue);
}

void QtPassSettings::setWebDavUrl(const QString &webDavUrl)
{
    setStringValue("webDavUrl", webDavUrl);
}

QString QtPassSettings::getWebDavUser(const QString &defaultValue)
{
    return getStringValue("webDavUser", defaultValue);
}

void QtPassSettings::setWebDavUser(const QString &webDavUser)
{
    setStringValue("webDavUser", webDavUser);
}

QString QtPassSettings::getWebDavPassword(const QString &defaultValue)
{
    return getStringValue("webDavPassword", defaultValue);
}

void QtPassSettings::setWebDavPassword(const QString &webDavPassword)
{
    setStringValue("webDavPassword", webDavPassword);
}

QString QtPassSettings::getProfile(const QString &defaultValue)
{
    return getStringValue("profile", defaultValue);
}

void QtPassSettings::setProfile(const QString &profile)
{
    setStringValue("profile", profile);
}

bool QtPassSettings::isUseGit(const bool &defaultValue)
{
    return getBoolValue("useGit", defaultValue);
}

void QtPassSettings::setUseGit(const bool &useGit)
{
    setBoolValue("useGit", useGit);
}

bool QtPassSettings::isUsePwgen(const bool &defaultValue)
{
    return getBoolValue("usePwgen", defaultValue);
}

void QtPassSettings::setUsePwgen(const bool &usePwgen)
{
    setBoolValue("usePwgen", usePwgen);
}

bool QtPassSettings::isAvoidCapitals(const bool &defaultValue)
{
    return getBoolValue("avoidCapitals", defaultValue);
}

void QtPassSettings::setAvoidCapitals(const bool &avoidCapitals)
{
    setBoolValue("avoidCapitals", avoidCapitals);
}

bool QtPassSettings::isAvoidNumbers(const bool &defaultValue)
{
    return getBoolValue("avoidNumbers", defaultValue);
}

void QtPassSettings::setAvoidNumbers(const bool &avoidNumbers)
{
    setBoolValue("avoidNumbers", avoidNumbers);
}

bool QtPassSettings::isLessRandom(const bool &defaultValue)
{
    return getBoolValue("lessRandom", defaultValue);
}

void QtPassSettings::setLessRandom(const bool &lessRandom)
{
    setBoolValue("lessRandom", lessRandom);
}

bool QtPassSettings::isUseSymbols(const bool &defaultValue)
{
    return getBoolValue("useSymbols", defaultValue);
}

void QtPassSettings::setUseSymbols(const bool &useSymbols)
{
    setBoolValue("useSymbols", useSymbols);
}

int QtPassSettings::getPasswordCharsSelected(const int &defaultValue)
{
    return getIntValue("passwordCharsSelected", defaultValue);
}

void QtPassSettings::setPasswordCharsSelected(const int &passwordCharsSelected)
{
    setIntValue("passwordCharsSelected", passwordCharsSelected);
}

int QtPassSettings::getPasswordLength(const int &defaultValue)
{
    return getIntValue("passwordLength", defaultValue);
}

void QtPassSettings::setPasswordLength(const int &passwordLength)
{
    setIntValue("passwordLength", passwordLength);
}

int QtPassSettings::getPasswordCharsselection(const int &defaultValue)
{
    return getIntValue("passwordCharsselection", defaultValue);
}

void QtPassSettings::setPasswordCharsselection(const int &passwordCharsselection)
{
    setIntValue("passwordCharsselection", passwordCharsselection);
}

QString QtPassSettings::getPasswordChars(const QString &defaultValue)
{
    return getStringValue("passwordChars", defaultValue);
}

void QtPassSettings::setPasswordChars(const QString &passwordChars)
{
    setStringValue("passwordChars", passwordChars);
}

bool QtPassSettings::isUseTrayIcon(const bool &defaultValue)
{
    return getBoolValue("useTrayIcon", defaultValue);
}

void QtPassSettings::setUseTrayIcon(const bool &useTrayIcon)
{
    setBoolValue("useTrayIcon", useTrayIcon);
}

bool QtPassSettings::isHideOnClose(const bool &defaultValue)
{
    return getBoolValue("hideOnClose", defaultValue);
}

void QtPassSettings::setHideOnClose(const bool &hideOnClose)
{
    setBoolValue("hideOnClose", hideOnClose);
}

bool QtPassSettings::isStartMinimized(const bool &defaultValue)
{
    return getBoolValue("startMinimized", defaultValue);
}

void QtPassSettings::setStartMinimized(const bool &startMinimized)
{
    setBoolValue("startMinimized", startMinimized);
}

bool QtPassSettings::isAlwaysOnTop(const bool &defaultValue)
{
    return getBoolValue("alwaysOnTop", defaultValue);
}

void QtPassSettings::setAlwaysOnTop(const bool &alwaysOnTop)
{
    setBoolValue("alwaysOnTop", alwaysOnTop);
}

bool QtPassSettings::isAutoPull(const bool &defaultValue)
{
    return getBoolValue("autoPull", defaultValue);
}

void QtPassSettings::setAutoPull(const bool &autoPull)
{
    setBoolValue("autoPull", autoPull);
}

bool QtPassSettings::isAutoPush(const bool &defaultValue)
{
    return getBoolValue("autoPush", defaultValue);
}

void QtPassSettings::setAutoPush(const bool &autoPush)
{
    setBoolValue("autoPush", autoPush);
}

QString QtPassSettings::getPassTemplate(const QString &defaultValue)
{
    return getStringValue("passTemplate", defaultValue);
}

void QtPassSettings::setPassTemplate(const QString &passTemplate)
{
    setStringValue("passTemplate", passTemplate);
}

bool QtPassSettings::isUseTemplate(const bool &defaultValue)
{
    return getBoolValue("useTemplate", defaultValue);
}

void QtPassSettings::setUseTemplate(const bool &useTemplate)
{
    setBoolValue("useTemplate", useTemplate);
}

bool QtPassSettings::isTemplateAllFields(const bool &defaultValue)
{
    return getBoolValue("templateAllFields", defaultValue);
}

void QtPassSettings::setTemplateAllFields(const bool &templateAllFields)
{
    setBoolValue("templateAllFields", templateAllFields);
}

QStringList QtPassSettings::getChildKeysFromCurrentGroup()
{
    return getSettings().childKeys();
}

QHash<QString, QString> QtPassSettings::getProfiles()
{
    beginProfilesGroup();
    QStringList childrenKeys = getChildKeysFromCurrentGroup();
    QHash<QString, QString> profiles;
    foreach (QString key, childrenKeys){
      profiles.insert(key, getSetting(key).toString());
    }
    endSettingsGroup();
    return profiles;
}

QSettings &QtPassSettings::getSettings() {
  if (!QtPassSettings::initialized) {
    QString portable_ini = QCoreApplication::applicationDirPath() +
                           QDir::separator() + "qtpass.ini";
    // qDebug() << "Settings file: " + portable_ini;
    if (QFile(portable_ini).exists()) {
      // qDebug() << "Settings file exists, loading it in";
      settings.reset(new QSettings(portable_ini, QSettings::IniFormat));
    } else {
      // qDebug() << "Settings file does not exist, use defaults";
      settings.reset(new QSettings("IJHack", "QtPass"));
    }
  }
  initialized = true;
  return *settings;
}


QString QtPassSettings::getStringValue(const QString &key, const QString &defaultValue)
{
    QString stringValue;
    if(stringSettings.contains(key)){
        stringValue = stringSettings.take(key);
    }else{
        stringValue = getSettings().value(key, defaultValue).toString();
        stringSettings.insert(key, stringValue);
    }
    return stringValue;
}

int QtPassSettings::getIntValue(const QString &key, const int &defaultValue)
{
    int intValue;
    if(intSettings.contains(key)){
        intValue = intSettings.take(key);
    }else{
        intValue = getSettings().value(key, defaultValue).toInt();
        intSettings.insert(key, intValue);
    }
    return intValue;
}

bool QtPassSettings::getBoolValue(const QString &key, const bool &defaultValue)
{
    bool boolValue;
    if(boolSettings.contains(key)){
        boolValue = boolSettings.take(key);
    }else{
        boolValue = getSettings().value(key, defaultValue).toBool();
        boolSettings.insert(key, boolValue);
    }
    return boolValue;
}

QByteArray QtPassSettings::getByteArrayValue(const QString &key, const QByteArray &defaultValue)
{
    QByteArray byteArrayValue;
    if(byteArraySettings.contains(key)){
        byteArrayValue = byteArraySettings.take(key);
    }else{
        byteArrayValue = getSettings().value(key, defaultValue).toByteArray();
        byteArraySettings.insert(key, byteArrayValue);
    }
    return byteArrayValue;
}

QPoint QtPassSettings::getPointValue(const QString &key, const QPoint &defaultValue)
{
    QPoint pointValue;
    if(pointSettings.contains(key)){
        pointValue = pointSettings.take(key);
    }else{
        pointValue = getSettings().value(key, defaultValue).toPoint();
        pointSettings.insert(key, pointValue);
    }
    return pointValue;
}

QSize QtPassSettings::getSizeValue(const QString &key, const QSize &defaultValue)
{
    QSize sizeValue;
    if(sizeSettings.contains(key)){
        sizeValue = sizeSettings.take(key);
    }else{
        sizeValue = getSettings().value(key, defaultValue).toSize();
        sizeSettings.insert(key, sizeValue);
    }
    return sizeValue;
}

void QtPassSettings::setStringValue(const QString &key, const QString &stringValue)
{
    stringSettings.insert(key, stringValue);
    getSettings().setValue(key, stringValue);
}

void QtPassSettings::setIntValue(const QString &key, const int &intValue)
{
    intSettings.insert(key, intValue);
    getSettings().setValue(key, intValue);
}

void QtPassSettings::setBoolValue(const QString &key, const bool &boolValue)
{
    boolSettings.insert(key, boolValue);
    getSettings().setValue(key, boolValue);
}

void QtPassSettings::setByteArrayValue(const QString &key, const QByteArray &byteArrayValue)
{
    byteArraySettings.insert(key, byteArrayValue);
    getSettings().setValue(key, byteArrayValue);
}

void QtPassSettings::setPointValue(const QString &key, const QPoint &pointValue)
{
    pointSettings.insert(key, pointValue);
    getSettings().setValue(key, pointValue);
}

void QtPassSettings::setSizeValue(const QString &key, const QSize &sizeValue)
{
    sizeSettings.insert(key, sizeValue);
    getSettings().setValue(key, sizeValue);
}

void QtPassSettings::beginSettingsGroup(const QString &groupName)
{
    getSettings().beginGroup(groupName);
}

void QtPassSettings::endSettingsGroup()
{
    getSettings().endGroup();
}

void QtPassSettings::beginMainwindowGroup()
{
    getSettings().beginGroup("mainwindow");

}

void QtPassSettings::beginProfilesGroup()
{
    getSettings().beginGroup("profiles");

}

QVariant QtPassSettings::getSetting(const QString &key, const QVariant &defalutValue)
{
    return getSettings().value(key, defalutValue);
}
