#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QFileDialog>
#include "mainwindow.h"
#include <QTableWidgetItem>
#include <QCloseEvent>

namespace Ui {
struct UserInfo;

class ConfigDialog;
}

class ConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigDialog(MainWindow *parent);
    ~ConfigDialog();
    void setPassPath(QString);
    void setGitPath(QString);
    void setGpgPath(QString);
    void setStorePath(QString);
    void setProfiles(QHash<QString, QString>, QString);
    void usePass(bool);
    void useClipboard(MainWindow::clipBoardType);
    void useAutoclear(bool);
    void setAutoclear(int);
    void useAutoclearPanel(bool);
    void setAutoclearPanel(int);
    void hidePassword(bool);
    void hideContent(bool);
    void addGPGId(bool);
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
    void useTrayIcon(bool);
    void hideOnClose(bool);
    void startMinimized(bool);
    void useGit(bool);
    bool useGit();
    QString getPwgenPath();
    void setPwgenPath(QString);
    void usePwgen(bool);
    void useSymbols(bool);
    void setPasswordLength(int);
    void setPasswordChars(QString);
    bool usePwgen();
    bool useSymbols();
    int getPasswordLength();
    QString getPasswordChars();
    bool useTemplate();
    void useTemplate(bool);
    QString getTemplate();
    void setTemplate(QString);
    void templateAllFields(bool);
    bool templateAllFields();
    bool autoPull();
    void autoPull(bool);
    bool autoPush();
    void autoPush(bool);

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

#endif // DIALOG_H
