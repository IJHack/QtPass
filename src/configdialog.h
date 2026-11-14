// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Settings dialog: every page of the configuration window and the
 * profile table, with the validation that gates OK.
 */
#ifndef SRC_CONFIGDIALOG_H_
#define SRC_CONFIGDIALOG_H_

#include "appsettings.h"
#include "enums.h"
#include "passwordconfiguration.h"
#include "profile.h"

#include <QDialog>
#include <QHash>
#include <QProcessEnvironment>
#include <QSet>
#include <optional>

namespace Ui {
struct UserInfo;

class ConfigDialog;
} // namespace Ui

class QLabel;

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
  /**
   * @brief Forget the signing keys gpg did not know while the dialog is in
   * the background, and check them again once it is back in front.
   * @param event The change; only activation changes matter here.
   */
  void changeEvent(QEvent *event) override;

private slots:
  /**
   * @brief On OK: warn about a dubious SSH_AUTH_SOCK override, save the
   * settings and the profiles, let the active profile follow a rename, and
   * create and initialise the stores of profiles added in this session.
   */
  void on_accepted();
  /**
   * @brief Fill the pass, gpg (gpg2 first), git and pwgen paths from PATH,
   * keeping a field as it is when nothing is found, and choose the pass
   * backend when pass was found.
   */
  void on_autodetectButton_clicked();
  /// Native (gpg/git) backend chosen: enable its group box, disable pass's.
  void on_radioButtonNative_clicked();
  /// pass backend chosen: enable its group box, disable the native one.
  void on_radioButtonPass_clicked();
  /**
   * @brief Pick the git executable in a file dialog; a cancelled dialog
   * turns Git off and disables its checkbox, as a missing git does.
   */
  void on_toolButtonGit_clicked();
  /// Pick the gpg executable in a file dialog; cancel keeps the field.
  void on_toolButtonGpg_clicked();
  /// Open KeygenDialog for the gpg shown in the dialog.
  void on_pushButtonGenerateKey_clicked();
  /**
   * @brief Pick the pwgen executable; a choice enables "use pwgen", a
   * cancelled dialog unticks and disables it.
   */
  void on_toolButtonPwgen_clicked();
  /// Pick the pass executable in a file dialog; cancel keeps the field.
  void on_toolButtonPass_clicked();
  /// Pick the password store folder for the pass backend's store path.
  void on_toolButtonStore_clicked();
  /**
   * @brief Enable the controls that only mean something with a clipboard
   * (selection, hiding, autoclear) unless "No clipboard" is chosen.
   * @param index The combo box row; 0 is "No clipboard".
   */
  void on_comboBoxClipboard_activated(int index);
  /**
   * @brief Show the chosen character set's characters in the line edit,
   * editable only for the custom set.
   * @param index A PasswordConfiguration::CharacterSet value.
   */
  void on_passwordCharTemplateSelector_activated(int index);
  /// Re-apply the clipboard gating for the current clipboard mode.
  void on_checkBoxSelection_clicked();
  /**
   * @brief Append a "New profile" row pointing at the global store path,
   * select it and put the cursor in the name field.
   */
  void on_addButton_clicked();
  /**
   * @brief Remove the selected profile row (a message when none is); the
   * list's currentRowChanged re-points the form at a neighbour.
   */
  void on_deleteButton_clicked();
  /// Pick the selected profile's store folder and store it in its entry.
  void on_profilePathBrowse_clicked();
  /// Hide-on-close and start-minimised are only offered with a tray icon.
  void on_checkBoxUseTrayIcon_clicked();
  /**
   * @brief Enable the Git-dependent boxes (add .gpg-id, auto pull/push, and
   * in native mode the parent-repository switch) with "use Git".
   */
  void on_checkBoxUseGit_clicked();
  /**
   * @brief Enable pwgen's flags or the character-set controls, whichever
   * the "use pwgen" box selects; nothing in pass mode, where pass generates.
   */
  void on_checkBoxUsePwgen_clicked();
  /**
   * @brief The profile list's current row changed: show that profile.
   * @param row The new row, -1 for none.
   */
  void onProfileSelected(int row);
  /**
   * @brief The name field was typed in: store it in the entry and the list
   * item, then validate (duplicates, empty).
   * @param name The text as typed.
   */
  void onProfileNameEdited(const QString &name);
  /**
   * @brief The path field was typed in: store it in the entry, validate and
   * refresh the status line.
   * @param path The text as typed.
   */
  void onProfilePathEdited(const QString &path);
  /**
   * @brief The signing-key field was typed in: store it in the entry and
   * validate (fingerprints only, shared keys).
   * @param key The text as typed.
   */
  void onProfileSigningKeyEdited(const QString &key);
  /**
   * @brief One of the profile's Git boxes was clicked: all three flags
   * become the profile's own (no longer following the globals), and push
   * and pull are enabled only with Git.
   */
  void onProfileGitToggled();

private:
  /// The secret-fields and every-field checkboxes only while passwords are
  /// hidden; the secret words and "Default" only while secret fields are
  /// hidden and every field is not.
  void updateSecretFieldsEnabled();
  /**
   * @brief One row of the profile list while the dialog is open. A QList
   * rather than the Profiles map so a profile can pass through an empty or
   * duplicate name while it is being typed; getProfiles() builds the map.
   */
  struct ProfileEntry {
    QString name;    ///< The name as typed; may be empty or a duplicate.
    Profile profile; ///< Path, signing key and Git flags as edited.
    /// The name the profile had when the dialog opened (empty for one added
    /// here), so a rename of the active profile can follow it.
    QString originalName;
  };
  /// What is wrong with one profile's fields; empty strings where nothing is.
  struct ProfileProblems {
    QString name; ///< Empty or duplicate name; empty when fine.
    QString path; ///< Missing store path; empty when fine.
    QString key;  ///< Signing key that is no fingerprint list; empty when fine.
    /// Not a problem, a warning: other profiles sign with the same key.
    /// It neither blocks OK nor marks the row.
    QString sharedKey;
    /**
     * @brief Whether all three fields are fine.
     * @return true when name, path and key have no complaint.
     */
    [[nodiscard]] auto none() const -> bool {
      return name.isEmpty() && path.isEmpty() && key.isEmpty();
    }
    /**
     * @brief The first complaint, for a tooltip on the profile's row.
     * @return The name, path or key complaint, in that order; empty if none.
     */
    [[nodiscard]] auto first() const -> QString {
      return !name.isEmpty() ? name : !path.isEmpty() ? path : key;
    }
  };
  /// Designer tooltip of the profile-name field, put back when validate()
  /// has no complaint about it.
  QString m_nameTip;
  /// Designer tooltip of the store-path field, put back when validate() has
  /// no complaint about it.
  QString m_pathTip;
  /// Designer tooltip of the signing-key field, put back when validate() has
  /// no complaint about it.
  QString m_keyTip;
  /// Below the signing key: the profiles that share it, if any.
  QLabel *m_sharedKeyWarning = nullptr;
  /// In the pass box: the stores pass would commit outside of, if any.
  QLabel *m_passGitWarning = nullptr;
  /// Primary fingerprint per gpg and signing-key token (empty: gpg does
  /// not know it), so each key is looked up once, not on every keystroke.
  mutable QHash<QString, QString> m_primaryOf;
  /// An unknown key was forgotten in the background: validate() again on
  /// return.
  bool m_recheckSigners = false;
  /// The settings naming the GPG home those lookups run in, read on first
  /// use.
  mutable std::optional<AppSettings> m_gpgSettings;
  /// The profiles as shown; index == row in profileList.
  QList<ProfileEntry> m_entries;
  /// Row whose fields the form shows, -1 for none.
  int m_currentEntry = -1;
  /// True while loadProfileForm() sets the fields, so their edited-signals
  /// do not write back.
  bool m_loadingForm = false;

  /**
   * @brief Show row @p row's profile in the form (or clear and disable it
   * for -1), with m_loadingForm set so the fields' edited-signals do not
   * write back. The Git boxes show the profile's own flags, else the
   * global ones.
   * @param row Row in m_entries, or -1 for none.
   */
  void loadProfileForm(int row);
  /**
   * @brief The entry the form is editing.
   * @return A pointer into m_entries, or nullptr when no row is current.
   */
  auto currentEntry() -> ProfileEntry *;
  /**
   * @brief Refresh the status line under the form: the profile's name and
   * cleaned path, marked "New" while the folder does not exist, or a
   * reminder to fill in the required fields.
   */
  void updateProfileStatus();
  /// The widgets from configdialog.ui.
  QScopedPointer<Ui::ConfigDialog> ui;

  /**
   * @brief Show @p path as the git executable; an empty path disables and
   * turns off "use Git".
   * @param path The git executable, or empty for none.
   */
  void setGitPath(const QString &path);
  /**
   * @brief Fill the profile list and m_entries from @p profiles, dropping
   * the empty-named leftover older versions wrote, select
   * @p currentProfile and validate.
   * @param profiles The saved profiles.
   * @param currentProfile The active profile's name, to select its row.
   */
  void setProfiles(Profiles profiles, const QString &currentProfile);
  /**
   * @brief Tick the pass or the native radio button and enable the matching
   * group box.
   * @param usePass true for the pass backend.
   */
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

  /**
   * @brief Enable the group box of the chosen backend and, in pass mode,
   * disable the password-generation controls pass takes care of; in native
   * mode hand them back to on_checkBoxUsePwgen_clicked().
   */
  void setGroupBoxState();
  /**
   * @brief Ask for an existing file in a read-only file dialog.
   * @return The chosen file, empty when cancelled.
   */
  auto selectExecutable() -> QString;
  /**
   * @brief Ask for a directory in a directories-only file dialog.
   * @return The chosen folder, empty when cancelled.
   */
  auto selectFolder() -> QString;
  /**
   * @brief QMessageBox::critical with a single OK button. Once a workaround
   * for crashes in Qt 5.4.1 before QApplication::exec(); no caller today.
   * @param title The box's title.
   * @param text The message.
   */
  void criticalMessage(const QString &title, const QString &text);

  /**
   * @brief Whether qrencode can run: @p configuredPath when it is an
   * executable file, else a `qrencode` on Util's PATH, which is then saved
   * as the qrencode executable. Always false on Windows.
   * @param configuredPath The saved qrencode executable, possibly empty.
   * @return true when QR codes can be shown.
   */
  auto isQrencodeAvailable(const QString &configuredPath) -> bool;
  /**
   * @brief Check every profile (problemsOf(), sharingTheKey()), mark the
   * rows and the form, enable OK only when no profile has a problem, and
   * refresh the pass Git warning.
   */
  void validate();
  /// Shows m_passGitWarning for the stores Util::repositoryPassClimbsTo()
  /// names, while the pass backend is chosen.
  void updatePassGitWarning();
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
  /// Per profile row: its store as the file system resolves it, and its
  /// signing-key fingerprints (upper case).
  struct SigningStores {
    QStringList stores;        ///< Canonical (or cleaned) store path.
    QList<QSet<QString>> keys; ///< Fingerprints of the signing key.
  };
  /**
   * @brief Resolve every profile's store and fingerprints once.
   * @return One entry per row of m_entries.
   */
  [[nodiscard]] auto signingStores() const -> SigningStores;
  /**
   * @brief One profile's signing keys as signers: each key as written, plus
   * the primary fingerprint gpg gives for a complete key ID or (sub)key
   * fingerprint.
   * @param signingKey The profile's signing-key setting.
   * @param env The environment gpg looks keys up in (its GPG home).
   * @return Upper-case fingerprints and key IDs.
   */
  [[nodiscard]] auto signerKeys(const QString &signingKey,
                                const QProcessEnvironment &env) const
      -> QSet<QString>;
  /**
   * @brief The other profiles, for other stores, whose signing keys share a
   * fingerprint with row @p row's.
   * @param row The profile's row.
   * @param resolved What signingStores() gave for this validation.
   * @return Their names, sorted; empty when none or no key is set.
   */
  [[nodiscard]] auto sharingTheKey(int row, const SigningStores &resolved) const
      -> QStringList;

  /**
   * @brief For every saved profile whose store was not a profile's store
   * when the dialog opened: offer to create the folder and, when it has no
   * `.gpg-id`, ask for recipients and initialise it.
   * @param existingProfiles The profiles as they were when the dialog opened.
   */
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
