#ifndef USERSDIALOG_H
#define USERSDIALOG_H

#include <QDialog>
#include <QList>
#include <QStandardItemModel>
#include <QCloseEvent>

namespace Ui {
class UsersDialog;
}

class QListWidgetItem;

struct UserInfo {
    UserInfo() : validity('-'), have_secret(false), enabled(false) {}
    QString name;
    QString key_id;
    char validity;
    bool have_secret;
    bool enabled;
};

class UsersDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UsersDialog(QWidget *parent = 0);
    ~UsersDialog();
    void setUsers(QList<UserInfo> *);

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void itemChange(QListWidgetItem *);
    void on_clearButton_clicked();
    void on_lineEdit_textChanged(const QString &filter);

private:
    Ui::UsersDialog *ui;
    QList<UserInfo> *userList;
    void populateList(const QString &filter);
};

#endif // USERSDIALOG_H
