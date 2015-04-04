#ifndef USERSDIALOG_H
#define USERSDIALOG_H

//#include <QAbstractListModel>
#include <QDialog>
#include <QList>
#include <QStandardItemModel>

namespace Ui {
class UsersDialog;
}

class QListWidgetItem;

struct UserInfo {
    UserInfo() : enabled(false) {}
    QString name;
    QString key_id;
    bool enabled;
};

class UsersDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UsersDialog(QWidget *parent = 0);
    ~UsersDialog();
    void setUsers(QList<UserInfo> *);

private slots:
    void itemChange(QListWidgetItem *);

private:
    Ui::UsersDialog *ui;
};

#endif // USERSDIALOG_H
