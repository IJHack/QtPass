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

/*!
    \class ConfigDialog
    \brief The ConfigDialog handles the configuration interface.

    This class should also take the handling from the MainWindow class.
*/
class MainWindow;
class QCloseEvent;
class QTableWidgetItem;
/**
 * Configuration dialog providing UI and accessors for application settings.
 *
 * Presents controls for profiles, generation tools (pwgen/pass),
 * GPG/git/OTP/qrencode integration, template handling, tray icon and
 * autoclear options, and exposes methods to read and update those settings.
 * Also overrides closeEvent to perform dialog-close handling.
 */
class ConfigDialog : public QDialog {
  Q_OBJECT

public:
  explicit ConfigDialog(MainWindow *parent);
  ~ConfigDialog() override;

  void useSelection(bool useSelection);
  void useAutoclear(bool useAutoclear);
  void useAutoclearPanel(bool useAutoclearPanel);
  auto getProfiles() -> QHash<QString, QHash<QString, QString>>;
  void wizard();
  void genKey(const QString &, QDialog *);
  void useTrayIcon(bool useSystray);
  void useGit(bool useGit);
  void useOtp(bool useOtp);
  void useGrepSearch(bool useGrepSearch);
  void useQrencode(bool useQrencode);
  void setPwgenPath(const QString &);
  void usePwgen(bool usePwgen);
  void setPasswordConfiguration(const PasswordConfiguration &config);
  auto getPasswordConfiguration() -> PasswordConfiguration;
  void useTemplate(bool useTemplate);

protected:
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

private:
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
};

#endif // SRC_CONFIGDIALOG_H_
