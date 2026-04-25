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
#include <QScopedPointer>
#include <QSettings>
#include <QSize>
#include <QVariant>

/**
 * @class QtPassSettings
 * @brief Singleton that stores QtPass settings, handles persistence and
 * defaults.
 *
 * QtPassSettings manages all application configuration including:
 * - Window geometry (position, size, maximized state)
 * - Password store configuration (path, GPG settings)
 * - Executable paths (pass, git, gpg, pwgen)
 * - UI preferences (clipboard, autoclear, display options)
 * - gopass configuration
 *
 * Uses QSettings for persistence with a singleton pattern.
 * All getters accept a default value returned when setting is not yet saved.
 */
class QtPassSettings : public QSettings {
private:
  /**
   * @brief Private default constructor to enforce singleton usage and prevent
   * direct instantiation.
   */
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
  /**
   * @brief Get the singleton instance.
   * @return Pointer to the QtPassSettings instance.
   */
  static auto getInstance() -> QtPassSettings *;

  // Window settings
  /**
   * @brief Get saved application version.
   * @param defaultValue String returned if no version saved.
   * @return Application version string.
   */
  static auto getVersion(const QString &defaultValue = QVariant().toString())
      -> QString;
  /**
   * @brief Save application version.
   * @param version Version string to save.
   */
  static void setVersion(const QString &version);

  /**
   * @brief Get saved window geometry.
   * @param defaultValue Byte array returned if no geometry saved.
   * @return Window geometry as QByteArray.
   */
  static auto
  getGeometry(const QByteArray &defaultValue = QVariant().toByteArray())
      -> QByteArray;
  /**
   * @brief Save window geometry.
   * @param geometry Window geometry to save.
   */
  static void setGeometry(const QByteArray &geometry);

  /**
   * @brief Get saved window state.
   * @param defaultValue Byte array returned if no state saved.
   * @return Window state as QByteArray.
   */
  static auto
  getSavestate(const QByteArray &defaultValue = QVariant().toByteArray())
      -> QByteArray;
  /**
   * @brief Save window state.
   * @param saveState Window state to save.
   */
  static void setSavestate(const QByteArray &saveState);

  /**
   * @brief Get saved window position.
   * @param defaultValue Point returned if no position saved.
   * @return Window position as QPoint.
   */
  static auto getPos(const QPoint &defaultValue = QVariant().toPoint())
      -> QPoint;
  /**
   * @brief Save window position.
   * @param pos Window position to save.
   */
  static void setPos(const QPoint &pos);

  /**
   * @brief Get saved window size.
   * @param defaultValue Size returned if no size saved.
   * @return Window size as QSize.
   */
  static auto getSize(const QSize &defaultValue = QVariant().toSize()) -> QSize;
  /**
   * @brief Save window size.
   * @param size Window size to save.
   */
  static void setSize(const QSize &size);

  /**
   * @brief Get maximized state.
   * @param defaultValue Boolean returned if not saved.
   * @return true if window is maximized.
   */
  static auto isMaximized(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save maximized state.
   * @param maximized Maximized state to save.
   */
  static void setMaximized(const bool &maximized);

  // Dialog-specific settings
  /**
   * @brief Get saved dialog geometry.
   * @param key Dialog identifier (e.g., "usersDialog").
   * @param defaultValue Byte array returned if not saved.
   * @return Dialog geometry as QByteArray.
   */
  static auto
  getDialogGeometry(const QString &key,
                    const QByteArray &defaultValue = QVariant().toByteArray())
      -> QByteArray;
  /**
   * @brief Save dialog geometry.
   * @param key Dialog identifier.
   * @param geometry Dialog geometry to save.
   */
  static void setDialogGeometry(const QString &key, const QByteArray &geometry);

  /**
   * @brief Get saved dialog position.
   * @param key Dialog identifier.
   * @param defaultValue Point returned if not saved.
   * @return Dialog position as QPoint.
   */
  static auto getDialogPos(const QString &key,
                           const QPoint &defaultValue = QVariant().toPoint())
      -> QPoint;
  /**
   * @brief Save dialog position.
   * @param key Dialog identifier.
   * @param pos Dialog position to save.
   */
  static void setDialogPos(const QString &key, const QPoint &pos);

  /**
   * @brief Get saved dialog size.
   * @param key Dialog identifier.
   * @param defaultValue Size returned if not saved.
   * @return Dialog size as QSize.
   */
  static auto getDialogSize(const QString &key,
                            const QSize &defaultValue = QVariant().toSize())
      -> QSize;
  /**
   * @brief Save dialog size.
   * @param key Dialog identifier.
   * @param size Dialog size to save.
   */
  static void setDialogSize(const QString &key, const QSize &size);

  /**
   * @brief Get dialog maximized state.
   * @param key Dialog identifier.
   * @param defaultValue Boolean returned if not saved.
   * @return true if dialog is maximized.
   */
  static auto isDialogMaximized(const QString &key,
                                const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save dialog maximized state.
   * @param key Dialog identifier.
   * @param maximized Maximized state to save.
   */
  static void setDialogMaximized(const QString &key, const bool &maximized);

  // Password store settings
  /**
   * @brief Get whether to use pass (true) or GPG (false).
   * @param defaultValue Boolean returned if not saved (defaults to false).
   * @return true if using pass, false if using GPG.
   */
  static auto isUsePass(const bool &defaultValue = QVariant().toBool()) -> bool;
  /**
   * @brief Save use pass setting.
   * @param usePass true to use pass, false for GPG.
   */
  static void setUsePass(const bool &usePass);

  /**
   * @brief Get clipboard type preference.
   * @param defaultValue Default value returned if not saved.
   * @return Clipboard type as Enums::clipBoardType.
   */
  static auto getClipBoardTypeRaw(
      const Enums::clipBoardType &defaultValue = Enums::CLIPBOARD_NEVER) -> int;
  /**
   * @brief Get clipboard type as enum.
   * @param defaultValue Default value returned if not saved.
   * @return Clipboard type as Enums::clipBoardType.
   */
  static auto getClipBoardType(
      const Enums::clipBoardType &defaultValue = Enums::CLIPBOARD_NEVER)
      -> Enums::clipBoardType;
  /**
   * @brief Save clipboard type.
   * @param clipBoardType Clipboard type to save.
   */
  static void setClipBoardType(const int &clipBoardType);

  // UI settings
  /**
   * @brief Get whether to use selection (X11) for clipboard.
   * @param defaultValue Boolean returned if not saved.
   * @return true if using selection.
   */
  static auto isUseSelection(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save use selection setting.
   * @param useSelection true to use selection.
   */
  static void setUseSelection(const bool &useSelection);

  /**
   * @brief Get whether to use autoclear for clipboard.
   * @param defaultValue Boolean returned if not saved.
   * @return true if autoclear is enabled.
   */
  static auto isUseAutoclear(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save use autoclear setting.
   * @param useAutoclear true to enable autoclear.
   */
  static void setUseAutoclear(const bool &useAutoclear);

  /**
   * @brief Get autoclear delay in seconds.
   * @param defaultValue Integer returned if not saved.
   * @return Seconds before clipboard is cleared.
   */
  static auto getAutoclearSeconds(const int &defaultValue = QVariant().toInt())
      -> int;
  /**
   * @brief Save autoclear seconds.
   * @param autoClearSeconds Seconds to wait before clearing.
   */
  static void setAutoclearSeconds(const int &autoClearSeconds);

  /**
   * @brief Get whether to use panel autoclear.
   * @param defaultValue Boolean returned if not saved.
   * @return true if panel autoclear is enabled.
   */
  static auto
  isUseAutoclearPanel(const bool &defaultValue = QVariant().toBool()) -> bool;
  /**
   * @brief Save use autoclear panel setting.
   * @param useAutoclearPanel true to enable panel autoclear.
   */
  static void setUseAutoclearPanel(const bool &useAutoclearPanel);

  /**
   * @brief Get panel autoclear delay in seconds.
   * @param defaultValue Integer returned if not saved.
   * @return Seconds before panel is cleared.
   */
  static auto
  getAutoclearPanelSeconds(const int &defaultValue = QVariant().toInt()) -> int;
  /**
   * @brief Save panel autoclear seconds.
   * @param autoClearPanelSeconds Seconds to wait before clearing panel.
   */
  static void setAutoclearPanelSeconds(const int &autoClearPanelSeconds);

  /**
   * @brief Get whether to hide password in UI.
   * @param defaultValue Boolean returned if not saved.
   * @return true if password is hidden.
   */
  static auto isHidePassword(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save hide password setting.
   * @param hidePassword true to hide password.
   */
  static void setHidePassword(const bool &hidePassword);

  /**
   * @brief Get whether to hide content (password + username).
   * @param defaultValue Boolean returned if not saved.
   * @return true if content is hidden.
   */
  static auto isHideContent(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save hide content setting.
   * @param hideContent true to hide content.
   */
  static void setHideContent(const bool &hideContent);

  /**
   * @brief Get whether to use monospace font.
   * @param defaultValue Boolean returned if not saved.
   * @return true if using monospace font.
   */
  static auto isUseMonospace(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save use monospace setting.
   * @param useMonospace true to use monospace font.
   */
  static void setUseMonospace(const bool &useMonospace);

  /**
   * @brief Get whether to display password as-is (no modification).
   * @param defaultValue Boolean returned if not saved.
   * @return true if displaying as-is.
   */
  static auto isDisplayAsIs(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save display as-is setting.
   * @param displayAsIs true to display unmodified.
   */
  static void setDisplayAsIs(const bool &displayAsIs);

  /**
   * @brief Get whether to disable line wrapping.
   * @param defaultValue Boolean returned if not saved.
   * @return true if line wrapping disabled.
   */
  static auto isNoLineWrapping(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save no line wrapping setting.
   * @param noLineWrapping true to disable wrapping.
   */
  static void setNoLineWrapping(const bool &noLineWrapping);

  /**
   * @brief Get whether to auto-add GPG ID when receiving files.
   * @param defaultValue Boolean returned if not saved.
   * @return true if auto-adding is enabled.
   */
  static auto isAddGPGId(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save add GPG ID setting.
   * @param addGPGId true to auto-add GPG IDs.
   */
  static void setAddGPGId(const bool &addGPGId);

  // Pass store path
  /**
   * @brief Get password store directory path.
   * @param defaultValue String returned if not saved.
   * @return Path to password store.
   */
  static auto getPassStore(const QString &defaultValue = QVariant().toString())
      -> QString;
  /**
   * @brief Save password store path.
   * @param passStore Path to password store.
   */
  static void setPassStore(const QString &passStore);

  /**
   * @brief Get GPG signing key for pass.
   * @param defaultValue String returned if not saved.
   * @return GPG key ID.
   */
  static auto
  getPassSigningKey(const QString &defaultValue = QVariant().toString())
      -> QString;
  /**
   * @brief Save GPG signing key.
   * @param passSigningKey Key ID to use for signing.
   */
  static void setPassSigningKey(const QString &passSigningKey);

  // Executable paths
  /**
   * @brief Initialize executable paths by auto-detecting them.
   */
  static void initExecutables();
  /**
   * @brief Get pass executable path.
   * @param defaultValue String returned if not saved.
   * @return Path to pass executable.
   */
  static auto
  getPassExecutable(const QString &defaultValue = QVariant().toString())
      -> QString;
  /**
   * @brief Save pass executable path.
   * @param passExecutable Path to pass.
   */
  static void setPassExecutable(const QString &passExecutable);

  /**
   * @brief Get git executable path.
   * @param defaultValue String returned if not saved.
   * @return Path to git executable.
   */
  static auto
  getGitExecutable(const QString &defaultValue = QVariant().toString())
      -> QString;
  /**
   * @brief Save git executable path.
   * @param gitExecutable Path to git.
   */
  static void setGitExecutable(const QString &gitExecutable);

  /**
   * @brief Get GPG executable path.
   * @param defaultValue String returned if not saved.
   * @return Path to GPG executable.
   */
  static auto
  getGpgExecutable(const QString &defaultValue = QVariant().toString())
      -> QString;
  /**
   * @brief Save GPG executable path.
   * @param gpgExecutable Path to GPG executable.
   */
  static void setGpgExecutable(const QString &gpgExecutable);

  /**
   * @brief Get pwgen executable path.
   * @param defaultValue String returned if not saved.
   * @return Path to pwgen executable.
   */
  static auto
  getPwgenExecutable(const QString &defaultValue = QVariant().toString())
      -> QString;
  /**
   * @brief Save pwgen executable path.
   * @param pwgenExecutable Path to pwgen.
   */
  static void setPwgenExecutable(const QString &pwgenExecutable);

  /**
   * @brief Get GPG home directory.
   * @param defaultValue String returned if not saved.
   * @return Path to the GPG home directory.
   */
  static auto getGpgHome(const QString &defaultValue = QVariant().toString())
      -> QString;

  /**
   * @brief Check whether WebDAV integration is enabled.
   * @param defaultValue Value returned if not saved.
   * @return True if WebDAV is enabled.
   */
  static auto isUseWebDav(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save WebDAV integration flag.
   * @param useWebDav Whether to enable WebDAV.
   */
  static void setUseWebDav(const bool &useWebDav);

  /**
   * @brief Get WebDAV URL.
   * @param defaultValue String returned if not saved.
   * @return WebDAV URL.
   */
  static auto getWebDavUrl(const QString &defaultValue = QVariant().toString())
      -> QString;
  /**
   * @brief Save WebDAV URL.
   * @param webDavUrl WebDAV URL.
   */
  static void setWebDavUrl(const QString &webDavUrl);

  /**
   * @brief Get WebDAV username.
   * @param defaultValue String returned if not saved.
   * @return WebDAV username.
   */
  static auto getWebDavUser(const QString &defaultValue = QVariant().toString())
      -> QString;
  /**
   * @brief Save WebDAV username.
   * @param webDavUser WebDAV username.
   */
  static void setWebDavUser(const QString &webDavUser);

  /**
   * @brief Get WebDAV password.
   * @param defaultValue String returned if not saved.
   * @return WebDAV password.
   */
  static auto
  getWebDavPassword(const QString &defaultValue = QVariant().toString())
      -> QString;
  /**
   * @brief Save WebDAV password.
   * @param webDavPassword WebDAV password.
   */
  static void setWebDavPassword(const QString &webDavPassword);

  /**
   * @brief Get active profile name.
   * @param defaultValue String returned if not saved.
   * @return Active profile name.
   */
  static auto getProfile(const QString &defaultValue = QVariant().toString())
      -> QString;
  /**
   * @brief Save active profile name.
   * @param profile Profile name.
   */
  static void setProfile(const QString &profile);

  /**
   * @brief Get Git setting for a specific profile.
   * @param profileName Name of the profile.
   * @param defaultValue Value if not set.
   * @return Git setting for profile.
   */
  static auto getProfileUseGit(const QString &profileName,
                               const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Set Git setting for a specific profile.
   * @param profileName Name of the profile.
   * @param useGit Git setting value.
   */
  static void setProfileUseGit(const QString &profileName, const bool &useGit);

  /**
   * @brief Get autoPush setting for a specific profile.
   * @param profileName Name of the profile.
   * @param defaultValue Value if not set.
   * @return autoPush setting for profile.
   */
  static auto getProfileAutoPush(const QString &profileName,
                                 const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Set autoPush setting for a specific profile.
   * @param profileName Name of the profile.
   * @param autoPush autoPush setting value.
   */
  static void setProfileAutoPush(const QString &profileName,
                                 const bool &autoPush);

  /**
   * @brief Get autoPull setting for a specific profile.
   * @param profileName Name of the profile.
   * @param defaultValue Value if not set.
   * @return autoPull setting for profile.
   */
  static auto getProfileAutoPull(const QString &profileName,
                                 const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Set autoPull setting for a specific profile.
   * @param profileName Name of the profile.
   * @param autoPull autoPull setting value.
   */
  static void setProfileAutoPull(const QString &profileName,
                                 const bool &autoPull);

  /**
   * @brief Check whether Git integration is enabled.
   * @param defaultValue Value returned if not saved.
   * @return True if Git integration is enabled.
   */
  static auto isUseGit(const bool &defaultValue = QVariant().toBool()) -> bool;
  /**
   * @brief Save Git integration flag.
   * @param useGit Whether to enable Git integration.
   */
  static void setUseGit(const bool &useGit);

  /**
   * @brief Check whether content search (pass grep) is enabled.
   * @param defaultValue Value returned if not saved (defaults to false).
   * @return True if grep search is enabled.
   */
  static auto isUseGrepSearch(const bool &defaultValue = false) -> bool;
  /**
   * @brief Save content search flag.
   * @param useGrepSearch Whether to enable grep search.
   */
  static void setUseGrepSearch(const bool &useGrepSearch);

  /**
   * @brief Check whether OTP support is enabled.
   * @param defaultValue Value returned if not saved.
   * @return True if OTP support is enabled.
   */
  static auto isUseOtp(const bool &defaultValue = QVariant().toBool()) -> bool;
  /**
   * @brief Save OTP support flag.
   * @param useOtp Whether to enable OTP support.
   */
  static void setUseOtp(const bool &useOtp);

  /**
   * @brief Check whether qrencode support is enabled.
   * @param defaultValue Value returned if not saved.
   * @return True if qrencode support is enabled.
   */
  static auto isUseQrencode(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save qrencode support flag.
   * @param useQrencode Whether to enable qrencode support.
   */
  static void setUseQrencode(const bool &useQrencode);

  /**
   * @brief Get qrencode executable path.
   * @param defaultValue String returned if not saved.
   * @return Path to qrencode executable.
   */
  static auto
  getQrencodeExecutable(const QString &defaultValue = QVariant().toString())
      -> QString;
  /**
   * @brief Save qrencode executable path.
   * @param qrencodeExecutable Path to qrencode executable.
   */
  static void setQrencodeExecutable(const QString &qrencodeExecutable);

  /**
   * @brief Check whether pwgen support is enabled.
   * @param defaultValue Value returned if not saved.
   * @return True if pwgen support is enabled.
   */
  static auto isUsePwgen(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save pwgen support flag.
   * @param usePwgen Whether to enable pwgen support.
   */
  static void setUsePwgen(const bool &usePwgen);

  /**
   * @brief Check whether uppercase characters should be avoided.
   * @param defaultValue Value returned if not saved.
   * @return True if uppercase characters are avoided.
   */
  static auto isAvoidCapitals(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save uppercase avoidance flag.
   * @param avoidCapitals Whether uppercase characters should be avoided.
   */
  static void setAvoidCapitals(const bool &avoidCapitals);

  /**
   * @brief Check whether numeric characters should be avoided.
   * @param defaultValue Value returned if not saved.
   * @return True if numeric characters are avoided.
   */
  static auto isAvoidNumbers(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save numeric avoidance flag.
   * @param avoidNumbers Whether numeric characters should be avoided.
   */
  static void setAvoidNumbers(const bool &avoidNumbers);

  /**
   * @brief Check whether less random password generation is enabled.
   * @param defaultValue Value returned if not saved.
   * @return True if less random generation is enabled.
   */
  static auto isLessRandom(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save less-random generation flag.
   * @param lessRandom Whether less-random generation is enabled.
   */
  static void setLessRandom(const bool &lessRandom);

  /**
   * @brief Check whether symbol characters are enabled.
   * @param defaultValue Value returned if not saved.
   * @return True if symbol characters are enabled.
   */
  static auto isUseSymbols(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save symbol usage flag.
   * @param useSymbols Whether symbol characters are enabled.
   */
  static void setUseSymbols(const bool &useSymbols);

  /**
   * @brief Get complete password generation configuration.
   * @return Password generation configuration.
   */
  static auto getPasswordConfiguration() -> PasswordConfiguration;
  /**
   * @brief Save complete password generation configuration.
   * @param config Password generation configuration.
   */
  static void setPasswordConfiguration(const PasswordConfiguration &config);
  /**
   * @brief Save password length setting.
   * @param passwordLength Password length.
   */
  static void setPasswordLength(const int &passwordLength);
  /**
   * @brief Save password character selection mode.
   * @param passwordCharsSelection Password character selection mode.
   */
  static void setPasswordCharsSelection(const int &passwordCharsSelection);
  /**
   * @brief Save custom password characters.
   * @param passwordChars Custom characters used for password generation.
   */
  static void setPasswordChars(const QString &passwordChars);

  /**
   * @brief Check whether tray icon support is enabled.
   * @param defaultValue Value returned if not saved.
   * @return True if tray icon support is enabled.
   */
  static auto isUseTrayIcon(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save tray icon support flag.
   * @param useTrayIcon Whether tray icon support is enabled.
   */
  static void setUseTrayIcon(const bool &useTrayIcon);

  /**
   * @brief Check whether closing the window hides the application.
   * @param defaultValue Value returned if not saved.
   * @return True if close action hides the application.
   */
  static auto isHideOnClose(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save hide-on-close flag.
   * @param hideOnClose Whether close action should hide the application.
   */
  static void setHideOnClose(const bool &hideOnClose);

  /**
   * @brief Check whether application should start minimized.
   * @param defaultValue Value returned if not saved.
   * @return True if application should start minimized.
   */
  static auto isStartMinimized(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save start-minimized flag.
   * @param startMinimized Whether application should start minimized.
   */
  static void setStartMinimized(const bool &startMinimized);

  /**
   * @brief Check whether main window should stay always on top.
   * @param defaultValue Value returned if not saved.
   * @return True if always-on-top is enabled.
   */
  static auto isAlwaysOnTop(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save always-on-top flag.
   * @param alwaysOnTop Whether always-on-top should be enabled.
   */
  static void setAlwaysOnTop(const bool &alwaysOnTop);

  /**
   * @brief Check whether automatic pull is enabled.
   * @param defaultValue Value returned if not saved.
   * @return True if automatic pull is enabled.
   */
  static auto isAutoPull(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save automatic pull flag.
   * @param autoPull Whether automatic pull should be enabled.
   */
  static void setAutoPull(const bool &autoPull);

  /**
   * @brief Check whether automatic push is enabled.
   * @param defaultValue Value returned if not saved.
   * @return True if automatic push is enabled.
   */
  static auto isAutoPush(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save automatic push flag.
   * @param autoPush Whether automatic push should be enabled.
   */
  static void setAutoPush(const bool &autoPush);

  /**
   * @brief Get pass entry template.
   * @param defaultValue String returned if not saved.
   * @return Pass template.
   */
  static auto
  getPassTemplate(const QString &defaultValue = QVariant().toString())
      -> QString;
  /**
   * @brief Save pass entry template.
   * @param passTemplate Pass template.
   */
  static void setPassTemplate(const QString &passTemplate);

  /**
   * @brief Check whether template usage is enabled.
   * @param defaultValue Value returned if not saved.
   * @return True if template usage is enabled.
   */
  static auto isUseTemplate(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Save template usage flag.
   * @param useTemplate Whether template usage should be enabled.
   */
  static void setUseTemplate(const bool &useTemplate);

  /**
   * @brief Check whether template applies to all fields.
   * @param defaultValue Value returned if not saved.
   * @return True if template applies to all fields.
   */
  static auto
  isTemplateAllFields(const bool &defaultValue = QVariant().toBool()) -> bool;
  /**
   * @brief Save template-all-fields flag.
   * @param templateAllFields Whether template should apply to all fields.
   */
  static void setTemplateAllFields(const bool &templateAllFields);

  /**
   * @brief Get showProcessOutput setting.
   * @param defaultValue Default value if not set.
   * @return Whether process output should be displayed.
   */
  static auto isShowProcessOutput(const bool &defaultValue = false) -> bool;
  /**
   * @brief Save showProcessOutput setting.
   * @param showProcessOutput Whether to show process output.
   */
  static void setShowProcessOutput(const bool &showProcessOutput);

  /**
   * @brief Get all configured profiles.
   * @return Profile map keyed by profile name.
   */
  static auto getProfiles() -> QHash<QString, QHash<QString, QString>>;
  /**
   * @brief Save all configured profiles.
   * @param profiles Profile map keyed by profile name.
   */
  static void
  setProfiles(const QHash<QString, QHash<QString, QString>> &profiles);

  /**
   * @brief Get currently active pass backend instance.
   * @return Pointer to pass backend.
   */
  static auto getPass() -> Pass *;
  /**
   * @brief Get real pass backend instance.
   * @return Pointer to real pass backend.
   */
  static auto getRealPass() -> RealPass *;
  /**
   * @brief Get imitate pass backend instance.
   * @return Pointer to imitate pass backend.
   */
  static auto getImitatePass() -> ImitatePass *;
};

#endif // SRC_QTPASSSETTINGS_H_
