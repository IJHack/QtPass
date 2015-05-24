#include "keygendialog.h"
#include "ui_keygendialog.h"
#include <QDebug>

KeygenDialog::KeygenDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::KeygenDialog)
{
    ui->setupUi(this);
}

KeygenDialog::~KeygenDialog()
{
    delete ui;
}

void KeygenDialog::on_passphrase1_textChanged(const QString &arg1)
{
    if (ui->passphrase1->text() == ui->passphrase2->text()) {
        ui->buttonBox->setEnabled(true);
        replace("Passphrase", arg1);
    } else {
        ui->buttonBox->setEnabled(false);
    }
}

void KeygenDialog::on_passphrase2_textChanged(const QString &arg1)
{
    on_passphrase1_textChanged(arg1);
}

void KeygenDialog::on_checkBox_stateChanged(int arg1)
{
    if (arg1) {
        ui->plainTextEdit->setReadOnly(false);
        ui->plainTextEdit->setEnabled(true);
    } else {
        ui->plainTextEdit->setReadOnly(true);
        ui->plainTextEdit->setEnabled(false);
    }
}

void KeygenDialog::on_email_textChanged(const QString &arg1)
{
    replace("Name-Email", arg1);
}

void KeygenDialog::on_name_textChanged(const QString &arg1)
{
    replace("Name-Real", arg1);
}

/**
 * @brief KeygenDialog::replace
 * @param key
 * @param value
 */
void KeygenDialog::replace(QString key, QString value)
{
    QStringList clear;
    QString expert  = ui->plainTextEdit->toPlainText();
    QStringList lines = expert.split(QRegExp("[\r\n]"),QString::SkipEmptyParts);
    foreach (QString line, lines) {
        line.replace(QRegExp(key+":.*"), key + ": " + value);
        clear.append(line);
    }
    ui->plainTextEdit->setPlainText(clear.join("\n"));
}

void KeygenDialog::done(int r)
{
    if(QDialog::Accepted == r)  // ok was pressed
    {
        bool status = false;

        // TODO call for keygen

        if(status)   // validate the data somehow
        {
            QDialog::done(r);
            return;
        }
        else
        {
            // something went wrong?
            return;
        }
    }
    else    // cancel, close or exc was pressed
    {
        QDialog::done(r);
        return;
    }
}
