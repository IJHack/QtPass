#ifndef KEYGENDIALOG_H
#define KEYGENDIALOG_H

#include <QDialog>
#include <QCloseEvent>
#include "configdialog.h"

namespace Ui {
class KeygenDialog;
}

class KeygenDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KeygenDialog(ConfigDialog *parent = 0);
    ~KeygenDialog();

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void on_passphrase1_textChanged(const QString &arg1);
    void on_passphrase2_textChanged(const QString &arg1);
    void on_checkBox_stateChanged(int arg1);
    void on_email_textChanged(const QString &arg1);
    void on_name_textChanged(const QString &arg1);

private:
    Ui::KeygenDialog *ui;
    void replace(QString, QString);
    void done(int r);
    void no_protection(bool enable);
    ConfigDialog *dialog;

};

#endif // KEYGENDIALOG_H
