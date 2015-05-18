#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <QFileDialog>

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
    void setProfiles(QHash<QString, QString>, QString);
    void usePass(bool);
    void useClipboard(bool);
    void useAutoclear(bool);
    void setAutoclear(int);
    void hidePassword(bool);
    void hideContent(bool);
    void addGPGId(bool);
    QString getPassPath();
    QString getGitPath();
    QString getGpgPath();
    QString getStorePath();
    QHash<QString,QString> getProfiles();
    bool usePass();
    bool useClipboard();
    bool useAutoclear();
    int getAutoclear();
    bool hidePassword();
    bool hideContent();
    bool addGPGId();

private slots:
    void on_radioButtonNative_clicked();
    void on_radioButtonPass_clicked();
    void on_toolButtonGit_clicked();
    void on_toolButtonGpg_clicked();
    void on_toolButtonPass_clicked();
    void on_toolButtonStore_clicked();
    void on_checkBoxClipboard_clicked();
    void on_checkBoxAutoclear_clicked();

private:
    QScopedPointer<Ui::Dialog> ui;
    void setGroupBoxState();
    QString selectExecutable();
    QString selectFolder();
};

#endif // DIALOG_H
