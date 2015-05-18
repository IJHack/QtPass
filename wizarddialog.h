#ifndef WIZARDDIALOG_H
#define WIZARDDIALOG_H

#include <QDialog>

namespace Ui {
class WizardDialog;
}

class WizardDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WizardDialog(QWidget *parent = 0);
    ~WizardDialog();

private:
    Ui::WizardDialog *ui;
};

#endif // WIZARDDIALOG_H
