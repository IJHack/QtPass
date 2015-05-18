#include "wizarddialog.h"
#include "ui_wizarddialog.h"

WizardDialog::WizardDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WizardDialog)
{
    ui->setupUi(this);
}

WizardDialog::~WizardDialog()
{
    delete ui;
}
