#ifndef PASSWORDDIALOG_H
#define PASSWORDDIALOG_H

#include <QDialog>
#include "mainwindow.h"

namespace Ui {
class PasswordDialog;
}

class PasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PasswordDialog(MainWindow *parent = 0);
    ~PasswordDialog();
    void setPassword(QString);
    QString getPassword();
    void setTemplate(QString);

private slots:
    void on_checkBoxShow_stateChanged(int arg1);
    void on_createPasswordButton_clicked();

private:
    Ui::PasswordDialog *ui;
    MainWindow *mainWindow;
    QString passTemplate;
    void addFields();

};

#endif // PASSWORDDIALOG_H
