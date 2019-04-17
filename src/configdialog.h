#ifndef CONFIGDIALOG_H_
#define CONFIGDIALOG_H_

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
class ConfigDialog : public QDialog {
  Q_OBJECT

public:
  explicit ConfigDialog(MainWindow *parent);
  ~ConfigDialog();

  void useSelection(bool useSelection);
  void useAutoclear(bool useAutoclear);
  void useAutoclearPanel(bool useAutoclearPanel);
  QHash<QString, QString> getProfiles();
  void wizard();
  void genKey(QString, QDialog *);
  void useTrayIcon(bool useTrayIdon);
  void useGit(bool useGit);
  void useOtp(bool useOtp);
  void useQrencode(bool useQrencode);
  void setPwgenPath(QString);
  void usePwgen(bool usePwgen);
  void setPasswordConfiguration(const PasswordConfiguration &config);
  PasswordConfiguration getPasswordConfiguration();
  void useTemplate(bool useTemplate);

protected:
  void closeEvent(QCloseEvent *event);

private slots:
  void on_accepted();
  void on_autodetectButton_clicked();
  void on_radioButtonNative_clicked();
  void on_radioButtonPass_clicked();
  void on_toolButtonGit_clicked();
  void on_toolButtonGpg_clicked();
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
  void on_checkBoxUseTrayIcon_clicked();
  void on_checkBoxUseGit_clicked();
  void on_checkBoxUsePwgen_clicked();
  void on_checkBoxUseTemplate_clicked();
  void onProfileTableItemChanged(QTableWidgetItem *item);

private:
  QScopedPointer<Ui::ConfigDialog> ui;

  QStringList getSecretKeys();

  void setGitPath(QString);
  void setProfiles(QHash<QString, QString>, QString);
  void usePass(bool usePass);

  void setGroupBoxState();
  QString selectExecutable();
  QString selectFolder();
  // QMessageBox::critical with hack to avoid crashes with
  // Qt 5.4.1 when QApplication::exec was not yet called
  void criticalMessage(const QString &title, const QString &text);

  bool isPassOtpAvailable();
  bool isQrencodeAvailable();
  void validate(QTableWidgetItem *item = nullptr);

  MainWindow *mainWindow;
};

#endif // CONFIGDIALOG_H_
