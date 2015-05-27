#ifndef KEYGENDIALOG_H
#define KEYGENDIALOG_H

#include <QDialog>
#include "dialog.h"

namespace Ui {
class KeygenDialog;
}

class KeygenDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KeygenDialog(Dialog *parent = 0);
    ~KeygenDialog();

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
    Dialog *dialog;

};

#endif // KEYGENDIALOG_H
