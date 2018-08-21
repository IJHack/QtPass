#include "usersdialog.h"
#include "qtpasssettings.h"
#include "ui_usersdialog.h"
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMessageBox>
#include <QRegExp>
#include <QWidget>

#ifdef QT_DEBUG
#include "debughelper.h"
#endif
/**
 * @brief UsersDialog::UsersDialog basic constructor
 * @param parent
 */
UsersDialog::UsersDialog(QString dir, QWidget *parent)
    : QDialog(parent), ui(new Ui::UsersDialog), m_dir(dir) {

  ui->setupUi(this);

  QList<UserInfo> users = QtPassSettings::getPass()->listKeys();
  if (users.isEmpty()) {
    QMessageBox::critical(parent, tr("Can not get key list"),
                          tr("Unable to get list of available gpg keys"));
    return;
  }

  QList<UserInfo> secret_keys = QtPassSettings::getPass()->listKeys("", true);
  foreach (const UserInfo &sec, secret_keys) {
    for (QList<UserInfo>::iterator it = users.begin(); it != users.end(); ++it)
      if (sec.key_id == it->key_id)
        it->have_secret = true;
  }

  QList<UserInfo> selected_users;
  int count = 0;

  QStringList recipients = QtPassSettings::getPass()->getRecipientString(
      m_dir.isEmpty() ? "" : m_dir, " ", &count);
  if (!recipients.isEmpty())
    selected_users = QtPassSettings::getPass()->listKeys(recipients);
  foreach (const UserInfo &sel, selected_users) {
    for (QList<UserInfo>::iterator it = users.begin(); it != users.end(); ++it)
      if (sel.key_id == it->key_id)
        it->enabled = true;
  }

  if (count > selected_users.size()) {
    // Some keys seem missing from keyring, add them separately
    QStringList recipients = QtPassSettings::getPass()->getRecipientList(
        m_dir.isEmpty() ? "" : m_dir);
    foreach (const QString recipient, recipients) {
      if (QtPassSettings::getPass()->listKeys(recipient).size() < 1) {
        UserInfo i;
        i.enabled = true;
        i.key_id = recipient;
        i.name = " ?? " + tr("Key not found in keyring");
        users.append(i);
      }
    }
  }

  m_userList = users;
  populateList();

  connect(ui->buttonBox, &QDialogButtonBox::accepted, this,
          &UsersDialog::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(ui->listWidget, &QListWidget::itemChanged, this,
          &UsersDialog::itemChange);

#if QT_VERSION >= 0x050200
  ui->lineEdit->setClearButtonEnabled(true);
#endif
}

/**
 * @brief UsersDialog::~UsersDialog basic destructor.
 */
UsersDialog::~UsersDialog() { delete ui; }

Q_DECLARE_METATYPE(UserInfo *)

void UsersDialog::accept() {
  QtPassSettings::getPass()->Init(m_dir, m_userList);

  QDialog::accept();
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
 * @brief UsersDialog::populateList update the view based on filter options
 * (such as searching).
 * @param filter
 */
void UsersDialog::populateList(const QString &filter) {
  QRegExp nameFilter("*" + filter + "*");
  nameFilter.setPatternSyntax(QRegExp::Wildcard);
  nameFilter.setCaseSensitivity(Qt::CaseInsensitive);
  ui->listWidget->clear();
  if (!m_userList.isEmpty()) {
    for (QList<UserInfo>::iterator it = m_userList.begin();
         it != m_userList.end(); ++it) {
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
 * @brief UsersDialog::on_checkBox_clicked filtering.
 */
void UsersDialog::on_checkBox_clicked() { populateList(ui->lineEdit->text()); }
