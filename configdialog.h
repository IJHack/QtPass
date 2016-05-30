#ifndef CONFIGDIALOG_H_
#define CONFIGDIALOG_H_

#include "mainwindow.h"
#include <QCloseEvent>
#include <QDialog>
#include <QFileDialog>
#include <QTableWidgetItem>

namespace Ui {
struct UserInfo;

class ConfigDialog;
}

class ConfigDialog : public QDialog {
  Q_OBJECT

public:
  explicit ConfigDialog(MainWindow *parent);
  ~ConfigDialog();
  void setPassPath(QString);
  void setGitPath(QString);
  void setGpgPath(QString);
  void setStorePath(QString);
  void setProfiles(QHash<QString, QString>, QString);
  void usePass(bool usePass);
  void useClipboard(MainWindow::clipBoardType);
  void useAutoclear(bool useAutoclear);
  void setAutoclear(int seconds);
  void useAutoclearPanel(bool useAutoclearPanel);
  void setAutoclearPanel(int seconds);
  void hidePassword(bool hidePassword);
  void hideContent(bool hideContent);
  void addGPGId(bool addGPGId);
  QString getPassPath();
  QString getGitPath();
  QString getGpgPath();
  QString getStorePath();
  QHash<QString, QString> getProfiles();
  bool usePass();
  MainWindow::clipBoardType useClipboard();
  bool useAutoclear();
  int getAutoclear();
  bool useAutoclearPanel();
  int getAutoclearPanel();
  bool hidePassword();
  bool hideContent();
  bool addGPGId();
  void wizard();
  void genKey(QString, QDialog *);
  bool useTrayIcon();
  bool hideOnClose();
  bool startMinimized();
  void useTrayIcon(bool useTrayIdon);
  void hideOnClose(bool hideOnClose);
  void startMinimized(bool startMinimized);
  void useGit(bool useGit);
  bool useGit();
  QString getPwgenPath();
  void setPwgenPath(QString);
  void usePwgen(bool usePwgen);
  void avoidCapitals(bool avoidCapitals);
  void avoidNumbers(bool avoidNumbers);
  void lessRandom(bool lessRandom);
  void useSymbols(bool useSymbols);
  void setPasswordLength(int pwLen);
  void setPasswordChars(QString);
  bool usePwgen();
  bool avoidCapitals();
  bool avoidNumbers();
  bool lessRandom();
  bool useSymbols();
  int getPasswordLength();
  QString getPasswordChars();
  bool useTemplate();
  void useTemplate(bool useTemplate);
  QString getTemplate();
  void setTemplate(QString);
  void templateAllFields(bool templateAllFields);
  bool templateAllFields();
  bool autoPull();
  void autoPull(bool autoPull);
  bool autoPush();
  void autoPush(bool autoPush);
  bool alwaysOnTop();
  void alwaysOnTop(bool alwaysOnTop);

protected:
  void closeEvent(QCloseEvent *event);

private slots:
  void on_radioButtonNative_clicked();
  void on_radioButtonPass_clicked();
  void on_toolButtonGit_clicked();
  void on_toolButtonGpg_clicked();
  void on_toolButtonPwgen_clicked();
  void on_toolButtonPass_clicked();
  void on_toolButtonStore_clicked();
  void on_comboBoxClipboard_activated();
  void on_checkBoxAutoclear_clicked();
  void on_checkBoxAutoclearPanel_clicked();
  void on_addButton_clicked();
  void on_deleteButton_clicked();
  void on_checkBoxUseTrayIcon_clicked();
  void on_checkBoxUseGit_clicked();
  void on_checkBoxUsePwgen_clicked();
  void on_checkBoxUseTemplate_clicked();

private:
  QScopedPointer<Ui::ConfigDialog> ui;
  void setGroupBoxState();
  QString selectExecutable();
  QString selectFolder();
  // QMessageBox::critical with hack to avoid crashes with
  // Qt 5.4.1 when QApplication::exec was not yet called
  void criticalMessage(const QString &title, const QString &text);
  MainWindow *mainWindow;
};

#endif // CONFIGDIALOG_H_
