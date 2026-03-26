// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_QTPASSSETTINGS_H_
#define SRC_QTPASSSETTINGS_H_

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

  ~QtPassSettings() override = default;

  static bool initialized;
  static QtPassSettings *m_instance;

  static Pass *pass;
  static QScopedPointer<RealPass> realPass;
  static QScopedPointer<ImitatePass> imitatePass;

public:
  static auto getInstance() -> QtPassSettings *;

  static auto getVersion(const QString &defaultValue = QVariant().toString())
      -> QString;
  static void setVersion(const QString &version);

  static auto
  getGeometry(const QByteArray &defaultValue = QVariant().toByteArray())
      -> QByteArray;
  static void setGeometry(const QByteArray &geometry);

  static auto
  getSavestate(const QByteArray &defaultValue = QVariant().toByteArray())
      -> QByteArray;
  static void setSavestate(const QByteArray &saveState);

  static auto getPos(const QPoint &defaultValue = QVariant().toPoint())
      -> QPoint;
  static void setPos(const QPoint &pos);

  static auto getSize(const QSize &defaultValue = QVariant().toSize()) -> QSize;
  static void setSize(const QSize &size);

  static auto isMaximized(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setMaximized(const bool &maximized);

  static auto isUsePass(const bool &defaultValue = QVariant().toBool()) -> bool;
  static void setUsePass(const bool &usePass);

  static auto getClipBoardTypeRaw(
      const Enums::clipBoardType &defaultvalue = Enums::CLIPBOARD_NEVER) -> int;
  static auto getClipBoardType(
      const Enums::clipBoardType &defaultvalue = Enums::CLIPBOARD_NEVER)
      -> Enums::clipBoardType;
  static void setClipBoardType(const int &clipBoardType);

  static auto isUseSelection(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setUseSelection(const bool &useSelection);

  static auto isUseAutoclear(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setUseAutoclear(const bool &useAutoclear);

  static auto getAutoclearSeconds(const int &defaultValue = QVariant().toInt())
      -> int;
  static void setAutoclearSeconds(const int &autoClearSeconds);

  static auto
  isUseAutoclearPanel(const bool &defaultValue = QVariant().toBool()) -> bool;
  static void setUseAutoclearPanel(const bool &useAutoclearPanel);

  static auto
  getAutoclearPanelSeconds(const int &defaultValue = QVariant().toInt()) -> int;
  static void setAutoclearPanelSeconds(const int &autoClearPanelSeconds);

  static auto isHidePassword(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setHidePassword(const bool &hidePassword);

  static auto isHideContent(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setHideContent(const bool &hideContent);

  static auto isUseMonospace(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setUseMonospace(const bool &useMonospace);

  static auto isDisplayAsIs(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setDisplayAsIs(const bool &displayAsIs);

  static auto isNoLineWrapping(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setNoLineWrapping(const bool &noLineWrapping);

  static auto isAddGPGId(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setAddGPGId(const bool &addGPGId);

  static auto getPassStore(const QString &defaultValue = QVariant().toString())
      -> QString;
  static void setPassStore(const QString &passStore);

  static auto
  getPassSigningKey(const QString &defaultValue = QVariant().toString())
      -> QString;
  static void setPassSigningKey(const QString &passSigningKey);

  static void initExecutables();
  static auto
  getPassExecutable(const QString &defaultValue = QVariant().toString())
      -> QString;
  static void setPassExecutable(const QString &passExecutable);

  static auto
  getGitExecutable(const QString &defaultValue = QVariant().toString())
      -> QString;
  static void setGitExecutable(const QString &gitExecutable);

  static auto
  getGpgExecutable(const QString &defaultValue = QVariant().toString())
      -> QString;
  static void setGpgExecutable(const QString &gpgExecutable);

  static auto
  getPwgenExecutable(const QString &defaultValue = QVariant().toString())
      -> QString;
  static void setPwgenExecutable(const QString &pwgenExecutable);

  static auto getGpgHome(const QString &defaultValue = QVariant().toString())
      -> QString;

  static auto isUseWebDav(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setUseWebDav(const bool &useWebDav);

  static auto getWebDavUrl(const QString &defaultValue = QVariant().toString())
      -> QString;
  static void setWebDavUrl(const QString &webDavUrl);

  static auto getWebDavUser(const QString &defaultValue = QVariant().toString())
      -> QString;
  static void setWebDavUser(const QString &webDavUser);

  static auto
  getWebDavPassword(const QString &defaultValue = QVariant().toString())
      -> QString;
  static void setWebDavPassword(const QString &webDavPassword);

  static auto getProfile(const QString &defaultValue = QVariant().toString())
      -> QString;
  static void setProfile(const QString &profile);

  static auto isUseGit(const bool &defaultValue = QVariant().toBool()) -> bool;
  static void setUseGit(const bool &useGit);

  static auto isUseOtp(const bool &defaultValue = QVariant().toBool()) -> bool;
  static void setUseOtp(const bool &useOtp);

  static auto isUseQrencode(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setUseQrencode(const bool &useQrencode);

  static auto
  getQrencodeExecutable(const QString &defaultValue = QVariant().toString())
      -> QString;
  static void setQrencodeExecutable(const QString &qrencodeExecutable);

  static auto isUsePwgen(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setUsePwgen(const bool &usePwgen);

  static auto isAvoidCapitals(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setAvoidCapitals(const bool &avoidCapitals);

  static auto isAvoidNumbers(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setAvoidNumbers(const bool &avoidNumbers);

  static auto isLessRandom(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setLessRandom(const bool &lessRandom);

  static auto isUseSymbols(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setUseSymbols(const bool &useSymbols);

  static auto getPasswordConfiguration() -> PasswordConfiguration;
  static void setPasswordConfiguration(const PasswordConfiguration &config);
  static void setPasswordLength(const int &passwordLength);
  static void setPasswordCharsselection(const int &passwordCharsselection);
  static void setPasswordChars(const QString &passwordChars);

  static auto isUseTrayIcon(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setUseTrayIcon(const bool &useTrayIcon);

  static auto isHideOnClose(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setHideOnClose(const bool &hideOnClose);

  static auto isStartMinimized(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setStartMinimized(const bool &startMinimized);

  static auto isAlwaysOnTop(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setAlwaysOnTop(const bool &alwaysOnTop);

  static auto isAutoPull(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setAutoPull(const bool &autoPull);

  static auto isAutoPush(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setAutoPush(const bool &autoPush);

  static auto
  getPassTemplate(const QString &defaultValue = QVariant().toString())
      -> QString;
  static void setPassTemplate(const QString &passTemplate);

  static auto isUseTemplate(const bool &defaultValue = QVariant().toBool())
      -> bool;
  static void setUseTemplate(const bool &useTemplate);

  static auto
  isTemplateAllFields(const bool &defaultValue = QVariant().toBool()) -> bool;
  static void setTemplateAllFields(const bool &templateAllFields);

  static auto getProfiles() -> QHash<QString, QHash<QString, QString>>;
  static void
  setProfiles(const QHash<QString, QHash<QString, QString>> &profiles);

  static auto getPass() -> Pass *;
  static auto getRealPass() -> RealPass *;
  static auto getImitatePass() -> ImitatePass *;
};

#endif // SRC_QTPASSSETTINGS_H_
