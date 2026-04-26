// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_CONFIGDIALOG_H_
#define SRC_CONFIGDIALOG_H_

#include "enums.h"
#include "passwordconfiguration.h"

#include <QDialog>

namespace Ui {
struct UserInfo;

class ConfigDialog;
} // namespace Ui

class MainWindow;
class QCloseEvent;
class QTableWidgetItem;

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
   * @param parent The application's MainWindow.
   */
  explicit ConfigDialog(MainWindow *parent);
  ~ConfigDialog() override;

  /**
   * @brief Enable or disable clipboard selection mode.
   * @param useSelection true to enable selection mode.
   */
  void useSelection(bool useSelection);

  /**
   * @brief Enable or disable autoclear of the clipboard.
   * @param useAutoclear true to enable autoclear.
   */
  void useAutoclear(bool useAutoclear);

  /**
   * @brief Enable or disable autoclear of the panel.
   * @param useAutoclearPanel true to enable panel autoclear.
   */
  void useAutoclearPanel(bool useAutoclearPanel);

  /**
   * @brief Return all configured profiles.
   * @return Hash of profile name to key-value settings map.
   */
  auto getProfiles() -> QHash<QString, QHash<QString, QString>>;

  /**
   * @brief Run the first-time setup wizard.
   */
  void wizard();

  /**
   * @brief Start GPG key generation.
   * @param batch GPG batch parameter string.
   * @param dialog Parent dialog to return to after generation.
   */
  void genKey(const QString &batch, QDialog *dialog);

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

  /**
   * @brief Enable or disable password file templating.
   * @param useTemplate true to enable the template.
   */
  void useTemplate(bool useTemplate);

protected:
  /**
   * @brief Save settings and clean up on close.
   * @param event The close event.
   */
  void closeEvent(QCloseEvent *event) override;

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
  void on_checkBoxAutoclear_clicked();
  void on_checkBoxAutoclearPanel_clicked();
  void on_addButton_clicked();
  void on_deleteButton_clicked();
  void on_profileTable_cellDoubleClicked(int row, int column);
  void on_checkBoxUseTrayIcon_clicked();
  void on_checkBoxUseGit_clicked();
  void on_checkBoxUsePwgen_clicked();
  void on_checkBoxUseTemplate_clicked();
  void onProfileTableItemChanged(QTableWidgetItem *item);
  void onProfileTableSelectionChanged();

private:
  void updateProfileStatus(int row);
  void loadGitSettingsForProfile(
      const QString &profileName,
      const QHash<QString, QHash<QString, QString>> &profiles);
  QScopedPointer<Ui::ConfigDialog> ui;

  auto getSecretKeys() -> QStringList;

  void setGitPath(const QString &);
  void setProfiles(QHash<QString, QHash<QString, QString>>, const QString &);
  void usePass(bool usePass);

  void setGroupBoxState();
  auto selectExecutable() -> QString;
  auto selectFolder() -> QString;
  // QMessageBox::critical with hack to avoid crashes with
  // Qt 5.4.1 when QApplication::exec was not yet called
  void criticalMessage(const QString &title, const QString &text);

  auto isPassOtpAvailable() -> bool;
  auto isQrencodeAvailable() -> bool;
  void validate(QTableWidgetItem *item = nullptr);

  auto checkGpgExistence() -> bool;
  auto checkSecretKeys() -> bool;
  auto checkPasswordStore() -> bool;
  void handleGpgIdFile();
  void initializeNewProfiles(
      const QHash<QString, QHash<QString, QString>> &existingProfiles);

  MainWindow *mainWindow;
  QHash<QString, QHash<QString, QString>> m_profiles;
};

#endif // SRC_CONFIGDIALOG_H_
