#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>

namespace Ui {
class Dialog;
}

class Dialog : public QDialog
{
    Q_OBJECT

public:
    explicit Dialog(QWidget *parent = 0);
    ~Dialog();
    void setPassPath(QString);
    void setGitPath(QString);
    void setGpgPath(QString);
    void setStorePath(QString);
    void usePass(bool);
    QString getPassPath();
    QString getGitPath();
    QString getGpgPath();
    QString getStorePath();
    bool usePass();

private slots:
    void on_radioButtonNative_clicked();

    void on_radioButtonPass_clicked();

private:
    Ui::Dialog *ui;
    void setGroupBoxState();
};

#endif // DIALOG_H
