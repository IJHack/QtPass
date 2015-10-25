#include "passworddialog.h"
#include "ui_passworddialog.h"
#include <QLabel>
#include <QLineEdit>
#include <QDebug>

PasswordDialog::PasswordDialog(MainWindow *parent) :
    QDialog(parent),
    ui(new Ui::PasswordDialog)
{
    mainWindow = parent;
    ui->setupUi(this);
}

PasswordDialog::~PasswordDialog()
{
    delete ui;
}

void PasswordDialog::on_checkBoxShow_stateChanged(int arg1)
{
    if (arg1) {
        ui->lineEditPassword->setEchoMode(QLineEdit::Normal);
    } else {
        ui->lineEditPassword->setEchoMode(QLineEdit::Password);
    }
}

void PasswordDialog::on_createPasswordButton_clicked()
{
    ui->widget->setEnabled(false);
    ui->lineEditPassword->setText(mainWindow->generatePassword());
    ui->widget->setEnabled(true);
}

void PasswordDialog::setPassword(QString password)
{
    QStringList tokens =  password.split("\n");
    ui->lineEditPassword->setText(tokens[0]);
    tokens.pop_front();
    if (templating) {
        for (int i = 0; i < ui->formLayout->rowCount(); i++) {
            QLayoutItem *item = ui->formLayout->itemAt(i, QFormLayout::FieldRole);
            if (item == NULL) {
                continue;
            }
            QWidget *widget = item->widget();
            for (int j = 0; j < tokens.length(); j++) {
                QString token = tokens.at(j);
                if (token.startsWith(widget->objectName()+':')) {
                    tokens.removeAt(j);
                    QString value = token.remove(0, widget->objectName().length()+1);
                    ((QLineEdit*)widget)->setText(value);
                }
            }
        }
        if (allFields) {
            for (int j = 0; j < tokens.length(); j++) {
                QString token = tokens.at(j);
                if (token.contains(':')) {
                    int colon = token.indexOf(':');
                    QString field = token.left(colon);
                    QString value = token.right(token.length()-colon-1);
                    if (!passTemplate.contains(field) && value.startsWith("//")) {
                        continue;   // colon is probably from a url
                    }
                    QLineEdit *line = new QLineEdit();
                    line->setObjectName(field);
                    line->setText(value);
                    ui->formLayout->addRow(new QLabel(field), line);
                    tokens.removeAt(j);
                    j--; // tokens.length() also got shortened by the remove..
                }
            }
        }
    }
    ui->plainTextEdit->insertPlainText(tokens.join("\n"));
}

QString PasswordDialog::getPassword()
{
    QString passFile = ui->lineEditPassword->text() + "\n";
    for (int i = 0; i < ui->formLayout->rowCount(); i++) {
        QLayoutItem *item = ui->formLayout->itemAt(i, QFormLayout::FieldRole);
        if (item == NULL) {
            continue;
        }
        QWidget *widget = item->widget();
        QString text = ((QLineEdit*)widget)->text();
        if (text.isEmpty()) {
            continue;
        }
        passFile += widget->objectName() + ":" + text + "\n";
    }
    passFile += ui->plainTextEdit->toPlainText();
    return passFile;
}

void PasswordDialog::setTemplate(QString rawFields) {
    fields = rawFields.split('\n');
    foreach (QString field, fields) {
        if (field.isEmpty()) {
            continue;
        }
        QLineEdit *line = new QLineEdit();
        line->setObjectName(field);
        ui->formLayout->addRow(new QLabel(field), line);
    }
}

void PasswordDialog::setFile(QString file) {
    this->setWindowTitle(this->windowTitle()+" "+file);
}

void PasswordDialog::templateAll(bool templateAll) {
    allFields = templateAll;
}

void PasswordDialog::useTemplate(bool useTemplate) {
    templating = useTemplate;
}
