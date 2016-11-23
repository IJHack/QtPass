#ifndef QTPASSSETTINGS_H
#define QTPASSSETTINGS_H

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QObject>
#include <QPoint>
#include <QSettings>
#include <QSize>
#include <QVariant>

#include "enums.h"
#include "mainwindow.h"

class QtPassSettings {

public:
  static QString
  getVersion(const QString &defaultValue = QVariant().toString());
  static void setVersion(const QString &version);

  static QByteArray
  getGeometry(const QByteArray &defaultValue = QVariant().toByteArray());
  static void setGeometry(const QByteArray &geometry);

  static QByteArray
  getSavestate(const QByteArray &defaultValue = QVariant().toByteArray());
  static void setSavestate(const QByteArray &saveState);

  static QPoint getPos(const QPoint &defaultValue = QVariant().toPoint());
  static void setPos(const QPoint &pos);

  static QSize getSize(const QSize &defaultValue = QVariant().toSize());
  static void setSize(const QSize &size);

  static int getSplitterLeft(const int &defaultValue = QVariant().toInt());
  static void setSplitterLeft(const int &splitterLeft);

  static int getSplitterRight(const int &defaultValue = QVariant().toInt());
  static void setSplitterRight(const int &splitterRight);

  static bool isMaximized(const bool &defaultValue = QVariant().toBool());
  static void setMaximized(const bool &maximized);

  static bool isUsePass(const bool &defaultValue = QVariant().toBool());
  static void setUsePass(const bool &usePass);

  static Enums::clipBoardType getClipBoardType(
      const Enums::clipBoardType &defaultvalue = Enums::CLIPBOARD_NEVER);
  static void setClipBoardType(const Enums::clipBoardType &clipBoardType);

  static bool isUseAutoclear(const bool &defaultValue = QVariant().toBool());
  static void setUseAutoclear(const bool &useAutoclear);

  static int getAutoclearSeconds(const int &defaultValue = QVariant().toInt());
  static void setAutoclearSeconds(const int &autoClearSeconds);

  static bool
  isUseAutoclearPanel(const bool &defaultValue = QVariant().toBool());
  static void setUseAutoclearPanel(const bool &useAutoclearPanel);

  static int
  getAutoclearPanelSeconds(const int &defaultValue = QVariant().toInt());
  static void setAutoclearPanelSeconds(const int &autoClearPanelSeconds);

  static bool isHidePassword(const bool &defaultValue = QVariant().toBool());
  static void setHidePassword(const bool &hidePassword);

  static bool isHideContent(const bool &defaultValue = QVariant().toBool());
  static void setHideContent(const bool &hideContent);

  static bool isAddGPGId(const bool &defaultValue = QVariant().toBool());
  static void setAddGPGId(const bool &addGPGId);

  static QString
  getPassStore(const QString &defaultValue = QVariant().toString());
  static void setPassStore(const QString &passStore);

  static QString
  getPassExecutable(const QString &defaultValue = QVariant().toString());
  static void setPassExecutable(const QString &passExecutable);

  static QString
  getGitExecutable(const QString &defaultValue = QVariant().toString());
  static void setGitExecutable(const QString &gitExecutable);

  static QString
  getGpgExecutable(const QString &defaultValue = QVariant().toString());
  static void setGpgExecutable(const QString &gpgExecutable);

  static QString
  getPwgenExecutable(const QString &defaultValue = QVariant().toString());
  static void setPwgenExecutable(const QString &pwgenExecutable);

  static QString
  getGpgHome(const QString &defaultValue = QVariant().toString());
  static void setGpgHome(const QString &gpgHome);

  static bool isUseWebDav(const bool &defaultValue = QVariant().toBool());
  static void setUseWebDav(const bool &useWebDav);

  static QString
  getWebDavUrl(const QString &defaultValue = QVariant().toString());
  static void setWebDavUrl(const QString &webDavUrl);

  static QString
  getWebDavUser(const QString &defaultValue = QVariant().toString());
  static void setWebDavUser(const QString &webDavUser);

  static QString
  getWebDavPassword(const QString &defaultValue = QVariant().toString());
  static void setWebDavPassword(const QString &webDavPassword);

  static QString
  getProfile(const QString &defaultValue = QVariant().toString());
  static void setProfile(const QString &profile);

  static bool isUseGit(const bool &defaultValue = QVariant().toBool());
  static void setUseGit(const bool &useGit);

  static bool isUsePwgen(const bool &defaultValue = QVariant().toBool());
  static void setUsePwgen(const bool &usePwgen);

  static bool isAvoidCapitals(const bool &defaultValue = QVariant().toBool());
  static void setAvoidCapitals(const bool &avoidCapitals);

  static bool isAvoidNumbers(const bool &defaultValue = QVariant().toBool());
  static void setAvoidNumbers(const bool &avoidNumbers);

  static bool isLessRandom(const bool &defaultValue = QVariant().toBool());
  static void setLessRandom(const bool &lessRandom);

  static bool isUseSymbols(const bool &defaultValue = QVariant().toBool());
  static void setUseSymbols(const bool &useSymbols);

  static int
  getPasswordCharsSelected(const int &defaultValue = QVariant().toInt());
  static void setPasswordCharsSelected(const int &passwordCharsSelected);

  static int getPasswordLength(const int &defaultValue = QVariant().toInt());
  static void setPasswordLength(const int &passwordLength);

  static int
  getPasswordCharsselection(const int &defaultValue = QVariant().toInt());
  static void setPasswordCharsselection(const int &passwordCharsselection);

  static QString
  getPasswordChars(const QString &defaultValue = QVariant().toString());
  static void setPasswordChars(const QString &passwordChars);

  static bool isUseTrayIcon(const bool &defaultValue = QVariant().toBool());
  static void setUseTrayIcon(const bool &useTrayIcon);

  static bool isHideOnClose(const bool &defaultValue = QVariant().toBool());
  static void setHideOnClose(const bool &hideOnClose);

  static bool isStartMinimized(const bool &defaultValue = QVariant().toBool());
  static void setStartMinimized(const bool &startMinimized);

  static bool isAlwaysOnTop(const bool &defaultValue = QVariant().toBool());
  static void setAlwaysOnTop(const bool &alwaysOnTop);

  static bool isAutoPull(const bool &defaultValue = QVariant().toBool());
  static void setAutoPull(const bool &autoPull);

  static bool isAutoPush(const bool &defaultValue = QVariant().toBool());
  static void setAutoPush(const bool &autoPush);

  static QString
  getPassTemplate(const QString &defaultValue = QVariant().toString());
  static void setPassTemplate(const QString &passTemplate);

  static bool isUseTemplate(const bool &defaultValue = QVariant().toBool());
  static void setUseTemplate(const bool &useTemplate);

  static bool
  isTemplateAllFields(const bool &defaultValue = QVariant().toBool());
  static void setTemplateAllFields(const bool &templateAllFields);

  static QHash<QString, QString> getProfiles();
  static void setProfiles(const QHash<QString, QString> &profiles);

signals:

public slots:

private:
  // constructor
  explicit QtPassSettings();

  static bool initialized;
  // member
  static QScopedPointer<QSettings> settings;

  static QHash<QString, QString> stringSettings;
  static QHash<QString, QByteArray> byteArraySettings;
  static QHash<QString, QPoint> pointSettings;
  static QHash<QString, QSize> sizeSettings;
  static QHash<QString, int> intSettings;
  static QHash<QString, bool> boolSettings;

  // functions
  static QSettings &getSettings();

  static QString getStringValue(const QString &key,
                                const QString &defaultValue);
  static int getIntValue(const QString &key, const int &defaultValue);
  static bool getBoolValue(const QString &key, const bool &defaultValue);
  static QByteArray getByteArrayValue(const QString &key,
                                      const QByteArray &defaultValue);
  static QPoint getPointValue(const QString &key, const QPoint &defaultValue);
  static QSize getSizeValue(const QString &key, const QSize &defaultValue);

  static void setStringValue(const QString &key, const QString &stringValue);
  static void setIntValue(const QString &key, const int &intValue);
  static void setBoolValue(const QString &key, const bool &boolValue);
  static void setByteArrayValue(const QString &key,
                                const QByteArray &byteArrayValue);
  static void setPointValue(const QString &key, const QPoint &pointValue);
  static void setSizeValue(const QString &key, const QSize &sizeValue);

  static QStringList getChildKeysFromCurrentGroup();
  static void beginSettingsGroup(const QString &groupName);
  static void endSettingsGroup();

  static void beginMainwindowGroup();
  static void beginProfilesGroup();

  static QVariant getSetting(const QString &key,
                             const QVariant &defaultValue = QVariant());
  static void setSetting(const QString &key, const QVariant &value);
};

#endif // QTPASSSETTINGS_H
