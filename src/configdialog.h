// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_CONFIGDIALOG_H_
#define SRC_CONFIGDIALOG_H_

#include "appsettings.h"
#include "enums.h"
#include "passwordconfiguration.h"
#include "profile.h"

#include <QDialog>

namespace Ui {
struct UserInfo;

class ConfigDialog;
} // namespace Ui

/**
 * @class ConfigDialog
 * @brief The ConfigDialog handles the configuration interface.
 *
 * Presents controls for profiles, generation tools (pwgen/pass),
 * GPG/git/OTP/qrencode integration, template handling, tray icon and
 * autoclear options, and exposes methods to read and update those settings.
 */
class ConfigDialog : public QDialog {
  Q_OBJECT

public:
  /**
   * @brief Construct a ConfigDialog associated with the given main window.
   * @param parent Parent widget, usually the main window.
   */
  explicit ConfigDialog(QWidget *parent);
  ~ConfigDialog() override;

  /**
   * @brief Whether a signing-key setting holds only full key fingerprints
   * (40 or 64 hexadecimal characters each, space separated); empty passes.
   * @param setting The profile's signing key field.
   * @return true when every token is a fingerprint.
   */
  static auto isFingerprintList(const QString &setting) -> bool;

  /**
   * @brief Enable or disable clipboard selection mode.
   * @param useSelection true to enable selection mode.
   */
  void useSelection(bool useSelection);

  /**
   * @brief Return all configured profiles.
   * @return Hash of profile name to key-value settings map.
   */
  auto getProfiles() -> Profiles;

  /**
   * @brief Show or hide the system tray icon.
   * @param useSystray true to enable the tray icon.
   */
  void useTrayIcon(bool useSystray);

  /**
   * @brief Enable or disable git integration.
   * @param useGit true to enable git.
   */
  void useGit(bool useGit);

  /**
   * @brief Enable or disable OTP support.
   * @param useOtp true to enable OTP.
   */
  void useOtp(bool useOtp);

  /**
   * @brief Enable or disable grep content search.
   * @param useGrepSearch true to enable grep search.
   */
  void useGrepSearch(bool useGrepSearch);

  /**
   * @brief Enable or disable QR code display.
   * @param useQrencode true to enable qrencode.
   */
  void useQrencode(bool useQrencode);

  /**
   * @brief Set the path to the pwgen executable.
   * @param path Path to pwgen.
   */
  void setPwgenPath(const QString &path);

  /**
   * @brief Enable or disable pwgen-based password generation.
   * @param usePwgen true to use pwgen.
   */
  void usePwgen(bool usePwgen);

  /**
   * @brief Apply a password configuration to the dialog controls.
   * @param config Configuration to display.
   */
  void setPasswordConfiguration(const PasswordConfiguration &config);

  /**
   * @brief Read the current password configuration from the dialog controls.
   * @return Current PasswordConfiguration.
   */
  auto getPasswordConfiguration() -> PasswordConfiguration;

protected:
private slots:
  void on_accepted();
  void on_autodetectButton_clicked();
  void on_radioButtonNative_clicked();
  void on_radioButtonPass_clicked();
  void on_toolButtonGit_clicked();
  void on_toolButtonGpg_clicked();
  void on_pushButtonGenerateKey_clicked();
  void on_toolButtonPwgen_clicked();
  void on_toolButtonPass_clicked();
  void on_toolButtonStore_clicked();
  void on_comboBoxClipboard_activated(int);
  void on_passwordCharTemplateSelector_activated(int);
  void on_checkBoxSelection_clicked();
  void on_addButton_clicked();
  void on_deleteButton_clicked();
  void on_profilePathBrowse_clicked();
  void on_checkBoxUseTrayIcon_clicked();
  void on_checkBoxUseGit_clicked();
  void on_checkBoxUsePwgen_clicked();
  void onProfileSelected(int row);
  void onProfileNameEdited(const QString &name);
  void onProfilePathEdited(const QString &path);
  void onProfileSigningKeyEdited(const QString &key);
  void onProfileGitToggled();

private:
  /**
   * @brief One row of the profile list while the dialog is open. A QList
   * rather than the Profiles map so a profile can pass through an empty or
   * duplicate name while it is being typed; getProfiles() builds the map.
   */
  struct ProfileEntry {
    QString name;
    Profile profile;
    /// The name the profile had when the dialog opened (empty for one added
    /// here), so a rename of the active profile can follow it.
    QString originalName;
  };
  /// What is wrong with one profile's fields; empty strings where nothing is.
  struct ProfileProblems {
    QString name;
    QString path;
    QString key;
    /// Whether all three fields are fine.
    [[nodiscard]] auto none() const -> bool {
      return name.isEmpty() && path.isEmpty() && key.isEmpty();
    }
    /// The first complaint, for a tooltip on the profile's row.
    [[nodiscard]] auto first() const -> QString {
      return !name.isEmpty() ? name : !path.isEmpty() ? path : key;
    }
  };
  /// Designer tooltips of the form fields, put back when validate() has no
  /// complaint about them.
  QString m_nameTip, m_pathTip, m_keyTip;
  /// The profiles as shown; index == row in profileList.
  QList<ProfileEntry> m_entries;
  /// Row whose fields the form shows, -1 for none.
  int m_currentEntry = -1;
  /// True while loadProfileForm() sets the fields, so their edited-signals
  /// do not write back.
  bool m_loadingForm = false;

  void loadProfileForm(int row);
  auto currentEntry() -> ProfileEntry *;
  void updateProfileStatus();
  QScopedPointer<Ui::ConfigDialog> ui;

  void setGitPath(const QString &);
  void setProfiles(Profiles, const QString &);
  void usePass(bool usePass);

  /**
   * @brief Load the dialog's value widgets from an AppSettings struct.
   *
   * Mirrors the legacy per-getter constructor loads. Sets values only; the
   * constructor still handles environment-dependent gating (tray/OTP/qrencode
   * availability, clipboard combo population, primary-selection support) and
   * the profile table. Password-generation and pwgen-path widgets are
   * intentionally not loaded here, matching the previous behaviour.
   * @param settings Values to display.
   */
  void applySettings(const AppSettings &settings);
  /**
   * @brief Build an AppSettings struct from the dialog's widgets.
   *
   * Starts from the currently persisted settings and overwrites only the
   * fields this dialog owns, so unrelated keys (window geometry,
   * profiles, etc.) are preserved on save.
   * @return Settings reflecting the dialog state.
   */
  auto readSettings() -> AppSettings;

  void setGroupBoxState();
  auto selectExecutable() -> QString;
  auto selectFolder() -> QString;
  // QMessageBox::critical with hack to avoid crashes with
  // Qt 5.4.1 when QApplication::exec was not yet called
  void criticalMessage(const QString &title, const QString &text);

  auto isQrencodeAvailable(const QString &configuredPath) -> bool;
  void validate();
  /**
   * @brief What is wrong with @p entry's name, store path and signing keys.
   * @param entry The profile to check.
   * @param names How many profiles carry each name, for duplicates.
   * @return The complaints, empty where a field is fine.
   */
  auto problemsOf(const ProfileEntry &entry,
                  const QHash<QString, int> &names) const -> ProfileProblems;
  /**
   * @brief Mark row @p row of the profile list, and the form fields when it
   * is the row being edited, with @p problems.
   * @param row Row in the profile list.
   * @param problems What problemsOf() found for it.
   */
  void showProblems(int row, const ProfileProblems &problems);

  void initializeNewProfiles(const Profiles &existingProfiles);
  /**
   * @brief Offer to create a new profile's store folder when it is missing.
   * @param path The folder, cleaned.
   * @return Whether the folder exists now.
   */
  auto ensureProfileFolder(const QString &path) -> bool;
  /**
   * @brief Ask for a new profile's recipients and initialise its store with
   * them, keeping the active backend's settings and queue on the active
   * store (#1774).
   * @param name The profile's name, for titles and messages.
   * @param profile The profile.
   * @param path Its store folder, cleaned.
   */
  void initialiseProfileStore(const QString &name, const Profile &profile,
                              const QString &path);
  /// Warn, without refusing, when the SSH_AUTH_SOCK override does not look
  /// like a socket; the value is saved as entered either way.
  void warnAboutSshAuthSockOverride();
  /**
   * @brief Point the active profile at its new name after a rename (or at
   * none after a deletion), and let its own Git flags replace the global
   * ones, as switching to it does.
   * @param profiles The profiles as saved.
   */
  void followActiveProfile(const Profiles &profiles);

  /// User-defined custom charset, retained while a builtin set is selected so
  /// it is not lost when the line edit shows the builtin's characters instead.
  QString m_customPasswordChars;
};

#endif // SRC_CONFIGDIALOG_H_
