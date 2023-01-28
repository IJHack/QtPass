#ifndef QTPASSSETTINGS_H
#define QTPASSSETTINGS_H

#include "enums.h"
#include "imitatepass.h"
#include "passwordconfiguration.h"
#include "realpass.h"
#include "settingsconstants.h"

#include <QByteArray>
#include <QHash>
#include <QPoint>
#include <QSettings>
#include <QSize>
#include <QVariant>

/*!
    \class QtPassSettings
    \brief Singleton that stores qtpass' settings, saves and loads config
*/
class QtPassSettings : public QSettings {
private:
  explicit QtPassSettings();

  QtPassSettings(const QString &organization, const QSettings::Format format)
      : QSettings(organization, format) {}
  QtPassSettings(const QString &organization, const QString &application)
      : QSettings(organization, application) {}

  virtual ~QtPassSettings() {}

  static bool initialized;
  static QtPassSettings *m_instance;

  static Pass *pass;
  static QScopedPointer<RealPass> realPass;
  static QScopedPointer<ImitatePass> imitatePass;

public:
  static QtPassSettings *getInstance();

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

  static bool isMaximized(const bool &defaultValue = QVariant().toBool());
  static void setMaximized(const bool &maximized);

  static bool isUsePass(const bool &defaultValue = QVariant().toBool());
  static void setUsePass(const bool &usePass);

  static int getClipBoardTypeRaw(
      const Enums::clipBoardType &defaultvalue = Enums::CLIPBOARD_NEVER);
  static Enums::clipBoardType getClipBoardType(
      const Enums::clipBoardType &defaultvalue = Enums::CLIPBOARD_NEVER);
  static void setClipBoardType(const int &clipBoardType);

  static bool isUseSelection(const bool &defaultValue = QVariant().toBool());
  static void setUseSelection(const bool &useSelection);

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

  static bool isUseMonospace(const bool &defaultValue = QVariant().toBool());
  static void setUseMonospace(const bool &useMonospace);

  static bool isDisplayAsIs(const bool &defaultValue = QVariant().toBool());
  static void setDisplayAsIs(const bool &displayAsIs);

  static bool isNoLineWrapping(const bool &defaultValue = QVariant().toBool());
  static void setNoLineWrapping(const bool &noLineWrapping);

  static bool isAddGPGId(const bool &defaultValue = QVariant().toBool());
  static void setAddGPGId(const bool &addGPGId);

  static QString
  getPassStore(const QString &defaultValue = QVariant().toString());
  static void setPassStore(const QString &passStore);

  static void initExecutables();
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

  static bool isUseOtp(const bool &defaultValue = QVariant().toBool());
  static void setUseOtp(const bool &useOtp);

  static bool isUseQrencode(const bool &defaultValue = QVariant().toBool());
  static void setUseQrencode(const bool &useQrencode);

  static QString
  getQrencodeExecutable(const QString &defaultValue = QVariant().toString());
  static void setQrencodeExecutable(const QString &qrencodeExecutable);

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

  static PasswordConfiguration getPasswordConfiguration();
  static void setPasswordConfiguration(const PasswordConfiguration &config);
  static void setPasswordLength(const int &passwordLength);
  static void setPasswordCharsselection(const int &passwordCharsselection);
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

  static Pass *getPass();
  static RealPass *getRealPass();
  static ImitatePass *getImitatePass();
};

#endif // QTPASSSETTINGS_H
