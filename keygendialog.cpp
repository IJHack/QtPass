#include "keygendialog.h"
#include "ui_keygendialog.h"
#include <QDebug>
#include <QMessageBox>

KeygenDialog::KeygenDialog(Dialog *parent) :
    QDialog(parent),
    ui(new Ui::KeygenDialog)
{
    ui->setupUi(this);
    dialog = parent;
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
        if (arg1 == "") {
            no_protection(true);
        } else {
            no_protection(false);
        }
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
        if (key == "Passphrase") {
            line.replace("%no-protection", "Passphrase: " + value);
        }
        clear.append(line);
    }
    ui->plainTextEdit->setPlainText(clear.join("\n"));
}

/**
 * @brief KeygenDialog::no_protection
 * @param enable
 */\
void KeygenDialog::no_protection(bool enable)
{
    QStringList clear;
    QString expert  = ui->plainTextEdit->toPlainText();
    QStringList lines = expert.split(QRegExp("[\r\n]"),QString::SkipEmptyParts);
    foreach (QString line, lines) {
        bool remove = false;
        if (!enable) {
            if (line.indexOf("%no-protection") == 0) {
                remove = true;
            }
        } else {
            if (line.indexOf("Passphrase") == 0) {
                line = "%no-protection";
            }
        }
        if (!remove) {
            clear.append(line);
        }
    }
    ui->plainTextEdit->setPlainText(clear.join("\n"));
}

/**
 * @brief KeygenDialog::done
 * @param r
 */
void KeygenDialog::done(int r)
{
    if(QDialog::Accepted == r)  // ok was pressed
    {
        ui->widget->setEnabled(false);
        ui->buttonBox->setEnabled(false);
        ui->checkBox->setEnabled(false);
        ui->plainTextEdit->setEnabled(false);
        // some kind of animation or at-least explanation needed here
        // people don't like wating :D
        dialog->genKey(ui->plainTextEdit->toPlainText(), this);
//            QMessageBox::critical(this, tr("GPG gen-key error"),
//                tr("Something went wrong, I guess"));
//            ui->widget->setEnabled(true);
//            ui->buttonBox->setEnabled(true);
//            ui->checkBox->setEnabled(true);
//            on_checkBox_stateChanged(ui->checkBox->isChecked());
//            // something went wrong, explain things?
//            return;
    }
    else    // cancel, close or exc was pressed
    {
        QDialog::done(r);
        return;
    }
}
