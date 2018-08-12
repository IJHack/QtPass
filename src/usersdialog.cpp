#include "usersdialog.h"
#include "ui_usersdialog.h"
#include <QCloseEvent>
#include <QKeyEvent>
#include <QRegExp>
#include <QWidget>

#ifdef QT_DEBUG
#include "debughelper.h"
#endif
/**
 * @brief UsersDialog::UsersDialog basic constructor
 * @param parent
 */
UsersDialog::UsersDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::UsersDialog) {
  ui->setupUi(this);
  connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(ui->listWidget, &QListWidget::itemChanged, this,
          &UsersDialog::itemChange);
  userList = nullptr;

#if QT_VERSION >= 0x050200
  ui->lineEdit->setClearButtonEnabled(true);
#endif
}

/**
 * @brief UsersDialog::~UsersDialog basic destructor.
 */
UsersDialog::~UsersDialog() { delete ui; }

Q_DECLARE_METATYPE(UserInfo *)

/**
 * @brief UsersDialog::itemChange update the item information.
 * @param item
 */
void UsersDialog::itemChange(QListWidgetItem *item) {
  if (!item)
    return;
  UserInfo *info = item->data(Qt::UserRole).value<UserInfo *>();
  if (!info)
    return;
  info->enabled = item->checkState() == Qt::Checked;
}

/**
 * @brief UsersDialog::setUsers update all the users.
 * @param users
 */
void UsersDialog::setUsers(QList<UserInfo> *users) {
  userList = users;
  populateList("");
}

/**
 * @brief UsersDialog::populateList update the view based on filter options
 * (such as searching).
 * @param filter
 */
void UsersDialog::populateList(const QString &filter) {
  QRegExp nameFilter("*" + filter + "*");
  nameFilter.setPatternSyntax(QRegExp::Wildcard);
  nameFilter.setCaseSensitivity(Qt::CaseInsensitive);
  ui->listWidget->clear();
  if (userList) {
    for (QList<UserInfo>::iterator it = userList->begin();
         it != userList->end(); ++it) {
      UserInfo &user(*it);
      if (filter.isEmpty() || nameFilter.exactMatch(user.name)) {
        if (!user.isValid() && !ui->checkBox->isChecked())
          continue;
        if (user.expiry.toTime_t() > 0 &&
            user.expiry.daysTo(QDateTime::currentDateTime()) > 0 &&
            !ui->checkBox->isChecked())
          continue;
        QString userText = user.name + "\n" + user.key_id;
        if (user.created.toTime_t() > 0) {
          userText += " " + tr("created") + " " +
                      user.created.toString(Qt::SystemLocaleShortDate);
        }
        if (user.expiry.toTime_t() > 0)
          userText += " " + tr("expires") + " " +
                      user.expiry.toString(Qt::SystemLocaleShortDate);
        QListWidgetItem *item = new QListWidgetItem(userText, ui->listWidget);
        item->setCheckState(user.enabled ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, QVariant::fromValue(&user));
        if (user.have_secret) {
          // item->setForeground(QColor(32, 74, 135));
          item->setForeground(Qt::blue);
          QFont font;
          font.setFamily(font.defaultFamily());
          font.setBold(true);
          item->setFont(font);
        } else if (!user.isValid()) {
          item->setBackground(QColor(164, 0, 0));
          item->setForeground(Qt::white);
        } else if (user.expiry.toTime_t() > 0 &&
                   user.expiry.daysTo(QDateTime::currentDateTime()) > 0) {
          item->setForeground(QColor(164, 0, 0));
        } else if (!user.fullyValid()) {
          item->setBackground(QColor(164, 80, 0));
          item->setForeground(Qt::white);
        }

        ui->listWidget->addItem(item);
      }
    }
  }
}

/**
 * @brief UsersDialog::on_lineEdit_textChanged typing in the searchbox.
 * @param filter
 */
void UsersDialog::on_lineEdit_textChanged(const QString &filter) {
  populateList(filter);
}

/**
 * @brief UsersDialog::closeEvent might have to store size and location if that
 * is wanted.
 * @param event
 */
void UsersDialog::closeEvent(QCloseEvent *event) {
  // TODO(annejan) save window size or something
  event->accept();
}

/**
 * @brief UsersDialog::on_checkBox_clicked filtering.
 */
void UsersDialog::on_checkBox_clicked() { populateList(ui->lineEdit->text()); }

/**
 * @brief UsersDialog::keyPressEvent clear the lineEdit when escape is pressed.
 * No action for Enter currently.
 * @param event
 */
void UsersDialog::keyPressEvent(QKeyEvent *event) {
  switch (event->key()) {
  case Qt::Key_Escape:
    ui->lineEdit->clear();
    break;
  default:
    break;
  }
}
