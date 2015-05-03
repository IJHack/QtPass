#include "usersdialog.h"
#include "ui_usersdialog.h"
#include <QRegExp>

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
    userList = users;
    populateList("");
}

void UsersDialog::populateList(const QString &filter)
{
    QRegExp nameFilter("*"+filter+"*");
    nameFilter.setPatternSyntax(QRegExp::Wildcard);
    nameFilter.setCaseSensitivity(Qt::CaseInsensitive);
    ui->listWidget->clear();
    if (userList) {
        for (QList<UserInfo>::iterator it = userList->begin(); it != userList->end(); ++it) {
            UserInfo &user(*it);
            if (filter.isEmpty() || nameFilter.exactMatch(user.name)) {
                QListWidgetItem *item = new QListWidgetItem(user.name + "\n" + user.key_id, ui->listWidget);
                item->setCheckState(user.enabled ? Qt::Checked : Qt::Unchecked);
                item->setData(Qt::UserRole, QVariant::fromValue(&user));
                ui->listWidget->addItem(item);
            }
        }
    }
}

void UsersDialog::on_clearButton_clicked()
{
    ui->lineEdit->clear();
    on_lineEdit_textChanged("");
}

void UsersDialog::on_lineEdit_textChanged(const QString &filter)
{
    populateList(filter);
}
