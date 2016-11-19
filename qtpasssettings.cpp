#include "qtpasssettings.h"

QtPassSettings::QtPassSettings(QObject *parent) : QObject(parent)
{

}

QString QtPassSettings::getVersion(const QString &defaultValue)
{
    return getStringValue("version", defaultValue);
}

QString QtPassSettings::setVersion(const QString &version)
{

}

QByteArray QtPassSettings::getGeometry(const QByteArray &defaultValue)
{
    return getByteArrayValue("geometry", defaultValue);
}

QByteArray QtPassSettings::setGeometry(const QByteArray &geometry)
{

}

QByteArray QtPassSettings::getSavestate(const QByteArray &defaultValue)
{
    return getByteArrayValue("savestate", defaultValue);
}

QByteArray QtPassSettings::setSavestate(const QByteArray &saveState)
{

}

QPoint QtPassSettings::getPos(const QPoint &defaultValue)
{
    return getPointValue("pos", defaultValue);
}

QPoint QtPassSettings::setPos(const QPoint &pos)
{

}

QSize QtPassSettings::getSize(const QSize &defaultValue)
{
    return getSizeValue("size", defaultValue);
}

QSize QtPassSettings::setSize(const QSize &size)
{

}

int QtPassSettings::getSplitterLeft(const int &defaultValue)
{
    return getIntValue("splitterLeft", defaultValue);
}

int QtPassSettings::setSplitterLeft(const int &splitterLeft)
{

}

int QtPassSettings::getSplitterRight(const int &defaultValue)
{
    return getIntValue("splitterRight", defaultValue);
}

int QtPassSettings::setSplitterRight(const int &splitterRight)
{

}

bool QtPassSettings::isMaximized(const bool &defaultValue)
{
    return getBoolValue("maximized", defaultValue);
}

bool QtPassSettings::setMaximized(const bool &maximized)
{

}

bool QtPassSettings::isUsePass(const bool &defaultValue)
{
    return getBoolValue("usePass", defaultValue);
}

bool QtPassSettings::setUsePass(const bool &usePass)
{

}

Enums::clipBoardType QtPassSettings::getClipBoardType(const Enums::clipBoardType &defaultvalue)
{
    return static_cast<Enums::clipBoardType>(getIntValue("clipBoardType",static_cast<int>(defaultvalue)));
}

Enums::clipBoardType QtPassSettings::setClipBoardType(const Enums::clipBoardType &clipBoardType)
{

}

bool QtPassSettings::isUseAutoclear(const bool &defaultValue)
{
    return getBoolValue("useAutoclear", defaultValue);
}

bool QtPassSettings::setUseAutoclear(const bool &useAutoclear)
{

}

int QtPassSettings::getAutoclearSeconds(const int &defaultValue)
{
    return getIntValue("autoclearSeconds", defaultValue);
}

int QtPassSettings::setAutoclearSeconds(const int &autoClearSeconds)
{

}

bool QtPassSettings::isUseAutoclearPanel(const bool &defaultValue)
{
    return getBoolValue("useAutoclearPanel", defaultValue);
}

bool QtPassSettings::setUseAutoclearPanel(const bool &useAutoclearPanel)
{

}

int QtPassSettings::getAutoclearPanelSeconds(const int &defaultValue)
{
    return getIntValue("autoclearPanelSeconds", defaultValue);
}

int QtPassSettings::setAutoclearPanelSeconds(const int &autoClearPanelSeconds)
{

}

bool QtPassSettings::isHidePassword(const bool &defaultValue)
{
    return getBoolValue("hidePassword", defaultValue);
}

bool QtPassSettings::setHidePassword(const bool &hidePassword)
{

}

bool QtPassSettings::isHideContent(const bool &defaultValue)
{
    return getBoolValue("hideContent", defaultValue);
}

bool QtPassSettings::setHideContent(const bool &hideContent)
{

}

bool QtPassSettings::isAddGPGId(const bool &defaultValue)
{
    return getBoolValue("addGPGId", defaultValue);
}

bool QtPassSettings::setAddGPGId(const bool &addGPGId)
{

}

QString QtPassSettings::getPassStore(const QString &defaultValue)
{
    return getStringValue("passStore", defaultValue);
}

QString QtPassSettings::setPassStore(const QString &passStore)
{

}

QString QtPassSettings::getPassExecutable(const QString &defaultValue)
{
    return getStringValue("passExecutable", defaultValue);
}

QString QtPassSettings::setPassExecutable(const QString &passExecutable)
{

}

QString QtPassSettings::getGitExecutable(const QString &defaultValue)
{
    return getStringValue("gitExecutable", defaultValue);
}

QString QtPassSettings::setGitExecutable(const QString &gitExecutable)
{

}

QString QtPassSettings::getGpgExecutable(const QString &defaultValue)
{
    return getStringValue("gpgExecutable", defaultValue);
}

QString QtPassSettings::setGpgExecutable(const QString &gpgExecutable)
{

}

QString QtPassSettings::getPwgenExecutable(const QString &defaultValue)
{
    return getStringValue("pwgenExecutable", defaultValue);
}

QString QtPassSettings::setPwgenExecutable(const QString &pwgenExecutable)
{

}

QString QtPassSettings::getGpgHome(const QString &defaultValue)
{
    return getStringValue("gpgHome", defaultValue);
}

QString QtPassSettings::setGpgHome(const QString &gpgHome)
{

}

bool QtPassSettings::isUseWebDav(const bool &defaultValue)
{
    return getBoolValue("useWebDav", defaultValue);
}

bool QtPassSettings::setUseWebDav(const bool &useWebDav)
{

}

QString QtPassSettings::getWebDavUrl(const QString &defaultValue)
{
    return getStringValue("webDavUrl", defaultValue);
}

QString QtPassSettings::setWebDavUrl(const QString &webDavUrl)
{

}

QString QtPassSettings::getWebDavUser(const QString &defaultValue)
{
    return getStringValue("webDavUser", defaultValue);
}

QString QtPassSettings::setWebDavUser(const QString &webDavUser)
{

}

QString QtPassSettings::getWebDavPassword(const QString &defaultValue)
{
    return getStringValue("webDavPassword", defaultValue);
}

QString QtPassSettings::setWebDavPassword(const QString &webDavPassword)
{

}

QString QtPassSettings::getProfile(const QString &defaultValue)
{
    return getStringValue("profile", defaultValue);
}

QString QtPassSettings::setProfile(const QString &profile)
{

}

bool QtPassSettings::isUseGit(const bool &defaultValue)
{
    return getBoolValue("useGit", defaultValue);
}

bool QtPassSettings::setUseGit(const bool &useGit)
{

}

bool QtPassSettings::isUsePwgen(const bool &defaultValue)
{
    return getBoolValue("usePwgen", defaultValue);
}

bool QtPassSettings::setUsePwgen(const bool &usePwgen)
{

}

bool QtPassSettings::isAvoidCapitals(const bool &defaultValue)
{
    return getBoolValue("avoidCapitals", defaultValue);
}

bool QtPassSettings::setAvoidCapitals(const bool &avoidCapitals)
{

}

bool QtPassSettings::isAvoidNumbers(const bool &defaultValue)
{
    return getBoolValue("avoidNumbers", defaultValue);
}

bool QtPassSettings::setAvoidNumbers(const bool &AvoidNumbers)
{

}

bool QtPassSettings::isLessRandom(const bool &defaultValue)
{
    return getBoolValue("lessRandom", defaultValue);
}

bool QtPassSettings::setLessRandom(const bool &lessRandom)
{

}

bool QtPassSettings::isUseSymbols(const bool &defaultValue)
{
    return getBoolValue("useSymbols", defaultValue);
}

bool QtPassSettings::setUseSymbols(const bool &useSymbols)
{

}

int QtPassSettings::getPasswordCharsSelected(const int &defaultValue)
{
    return getIntValue("passwordCharsSelected", defaultValue);
}

int QtPassSettings::setPasswordCharsSelected(const int &passwordCharsSelected)
{

}

int QtPassSettings::getPasswordLength(const int &defaultValue)
{
    return getIntValue("passwordLength", defaultValue);
}

int QtPassSettings::setPasswordLength(const int &passwordLength)
{

}

int QtPassSettings::getPasswordCharsselection(const int &defaultValue)
{
    return getIntValue("passwordCharsselection", defaultValue);
}

int QtPassSettings::setPasswordCharsselection(const int &passwordCharsselection)
{

}

QString QtPassSettings::getPasswordChars(const QString &defaultValue)
{
    return getStringValue("passwordChars", defaultValue);
}

QString QtPassSettings::setPasswordChars(const QString &passwordChars)
{

}

bool QtPassSettings::isUseTrayIcon(const bool &defaultValue)
{
    return getBoolValue("useTrayIcon", defaultValue);
}

bool QtPassSettings::setUseTrayIcon(const bool &useTrayIcon)
{

}

bool QtPassSettings::isHideOnClose(const bool &defaultValue)
{
    return getBoolValue("hideOnClose", defaultValue);
}

bool QtPassSettings::setHideOnClose(const bool &hideOnClose)
{

}

bool QtPassSettings::isStartMinimized(const bool &defaultValue)
{
    return getBoolValue("startMinimized", defaultValue);
}

bool QtPassSettings::setStartMinimized(const bool &startMinimized)
{

}

bool QtPassSettings::isAlwaysOnTop(const bool &defaultValue)
{
    return getBoolValue("alwaysOnTop", defaultValue);
}

bool QtPassSettings::setAlwaysOnTop(const bool &alwaysOnTop)
{

}

bool QtPassSettings::isAutoPull(const bool &defaultValue)
{
    return getBoolValue("autoPull", defaultValue);
}

bool QtPassSettings::setAutoPull(const bool &autoPull)
{

}

bool QtPassSettings::isAutoPush(const bool &defaultValue)
{
    return getBoolValue("autoPush", defaultValue);
}

bool QtPassSettings::setAutoPush(const bool &autoPush)
{

}

QString QtPassSettings::getPassTemplate(const QString &defaultValue)
{
    return getStringValue("passTemplate", defaultValue);
}

QString QtPassSettings::setPassTemplate(const QString &passTemplate)
{

}

bool QtPassSettings::isUseTemplate(const bool &defaultValue)
{
    return getBoolValue("useTemplate", defaultValue);
}

bool QtPassSettings::setUseTemplate(const bool &useTemplate)
{

}

bool QtPassSettings::isTemplateAllFields(const bool &defaultValue)
{
    return getBoolValue("templateAllFields", defaultValue);
}

bool QtPassSettings::setTemplateAllFields(const bool &templateAllFields)
{

}


QSettings &QtPassSettings::getSettings() {

  if (!settings) {
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
  return *settings;
}

QString QtPassSettings::getStringValue(const QString &key, const QString &defaultValue)
{
    return getSettings().value(key, defaultValue).toString();
}

int QtPassSettings::getIntValue(const QString &key, const int &defaultValue)
{
    return getSettings().value(key, defaultValue).toInt();
}

bool QtPassSettings::getBoolValue(const QString &key, const bool &defaultValue)
{
    return getSettings().value(key, defaultValue).toBool();
}

QByteArray QtPassSettings::getByteArrayValue(const QString &key, const QByteArray &defaultValue)
{
    return getSettings().value(key, defaultValue).toByteArray();
}

QPoint QtPassSettings::getPointValue(const QString &key, const QPoint &defaultValue)
{
    return getSettings().value(key, defaultValue).toPoint();
}

QSize QtPassSettings::getSizeValue(const QString &key, const QSize &defaultValue)
{
    return getSettings().value(key, defaultValue).toSize();
}

