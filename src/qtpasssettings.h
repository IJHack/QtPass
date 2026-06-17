// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_QTPASSSETTINGS_H_
#define SRC_QTPASSSETTINGS_H_

#include "appsettings.h"
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
  explicit QtPassSettings() = delete;

  QtPassSettings(const QString &organization, const QSettings::Format format)
      : QSettings(organization, format) {}
  QtPassSettings(const QString &organization, const QString &application)
      : QSettings(organization, application) {}

  ~QtPassSettings() override = default;

  static bool initialized;
  static QtPassSettings *m_instance;

public:
  /**
   * @brief Get the singleton instance.
   * @return Pointer to the QtPassSettings instance.
   */
  static auto getInstance() -> QtPassSettings *;

  /**
   * @brief Load all flat settings into an AppSettings value object.
   *
   * Convenience facade over SettingsSerializer using the singleton store.
   * Note: nested/keyed settings (profiles, per-dialog geometry) are not part
   * of AppSettings and still use their dedicated accessors.
   * @return Current settings.
   */
  static auto load() -> AppSettings;
  /**
   * @brief Persist an AppSettings value object to the singleton store.
   *
   * Writes via SettingsSerializer and invalidates the cached Pass backend so a
   * changed "use pass" mode takes effect on the next getPass().
   * @param settings Values to save.
   */
  static void save(const AppSettings &settings);

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
   * @brief Save use pass setting.
   * @param usePass true to use pass, false for GPG.
   */
  static void setUsePass(const bool &usePass);

  // UI settings

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
   * @brief Save panel autoclear seconds.
   * @param autoClearPanelSeconds Seconds to wait before clearing panel.
   */
  static void setAutoclearPanelSeconds(const int &autoClearPanelSeconds);

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
   * @brief Check whether WebDAV integration is enabled.
   * @param defaultValue Value returned if not saved.
   * @return True if WebDAV is enabled.
   */
  static auto isUseWebDav(const bool &defaultValue = QVariant().toBool())
      -> bool;
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
   * @brief Check whether content search (pass grep) is enabled.
   * @param defaultValue Value returned if not saved (defaults to false).
   * @return True if grep search is enabled.
   */
  static auto isUseGrepSearch(const bool &defaultValue = false) -> bool;
  /**
   * @brief Check whether OTP support is enabled.
   * @param defaultValue Value returned if not saved.
   * @return True if OTP support is enabled.
   */
  static auto isUseOtp(const bool &defaultValue = QVariant().toBool()) -> bool;
  /**
   * @brief Save qrencode executable path.
   * @param qrencodeExecutable Path to qrencode executable.
   */
  static void setQrencodeExecutable(const QString &qrencodeExecutable);

  /**
   * @brief Save pwgen support flag.
   * @param usePwgen Whether to enable pwgen support.
   */
  static void setUsePwgen(const bool &usePwgen);

  /**
   * @brief Get complete password generation configuration.
   * @return Password generation configuration.
   */
  static auto getPasswordConfiguration() -> PasswordConfiguration;
  /**
   * @brief Check whether closing the window hides the application.
   * @param defaultValue Value returned if not saved.
   * @return True if close action hides the application.
   */
  static auto isHideOnClose(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Check whether main window should stay always on top.
   * @param defaultValue Value returned if not saved.
   * @return True if always-on-top is enabled.
   */
  static auto isAlwaysOnTop(const bool &defaultValue = QVariant().toBool())
      -> bool;
  /**
   * @brief Check whether automatic push is enabled.
   * @param defaultValue Value returned if not saved.
   * @return True if automatic push is enabled.
   */
  static auto isAutoPush(const bool &defaultValue = QVariant().toBool())
      -> bool;
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
   * @brief Get showProcessOutput setting.
   * @param defaultValue Default value if not set.
   * @return Whether process output should be displayed.
   */
  static auto isShowProcessOutput(const bool &defaultValue = false) -> bool;
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
