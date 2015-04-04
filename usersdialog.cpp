#include "usersdialog.h"
#include "ui_usersdialog.h"

UsersDialog::UsersDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UsersDialog)
{
    ui->setupUi(this);
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(ui->listWidget, SIGNAL(itemChanged(QListWidgetItem *)), this, SLOT(itemChange(QListWidgetItem *)));
}

UsersDialog::~UsersDialog()
{
    delete ui;
}

Q_DECLARE_METATYPE(UserInfo *)

void UsersDialog::itemChange(QListWidgetItem *item)
{
    if (!item) return;
    UserInfo *info = item->data(Qt::UserRole).value<UserInfo *>();
    if (!info) return;
    info->enabled = item->checkState() == Qt::Checked;
}

void UsersDialog::setUsers(QList<UserInfo> *users)
{
    ui->listWidget->clear();
    if (users) {
        for (QList<UserInfo>::iterator it = users->begin(); it != users->end(); ++it) {
            UserInfo &user(*it);
            QListWidgetItem *item = new QListWidgetItem(user.name + "\n" + user.key_id, ui->listWidget);
            item->setCheckState(user.enabled ? Qt::Checked : Qt::Unchecked);
            item->setData(Qt::UserRole, QVariant::fromValue(&user));
            ui->listWidget->addItem(item);
        }
    }
}
