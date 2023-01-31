#include "usersdialog.h"
#include "qtpasssettings.h"
#include "ui_usersdialog.h"
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMessageBox>
#include <QRegularExpression>
#include <QWidget>
#include <utility>

#ifdef QT_DEBUG
#include "debughelper.h"
#endif
/**
 * @brief UsersDialog::UsersDialog basic constructor
 * @param parent
 */
UsersDialog::UsersDialog(QString dir, QWidget *parent)
    : QDialog(parent), ui(new Ui::UsersDialog), m_dir(std::move(dir)) {

  ui->setupUi(this);

  QList<UserInfo> users = QtPassSettings::getPass()->listKeys();
  if (users.isEmpty()) {
    QMessageBox::critical(parent, tr("Keylist missing"),
                          tr("Could not fetch list of available GPG keys"));
    return;
  }

  QList<UserInfo> secret_keys = QtPassSettings::getPass()->listKeys("", true);
  foreach (const UserInfo &sec, secret_keys) {
    for (auto &user : users)
      if (sec.key_id == user.key_id)
        user.have_secret = true;
  }

  QList<UserInfo> selected_users;
  int count = 0;

  QStringList recipients = QtPassSettings::getPass()->getRecipientString(
      m_dir.isEmpty() ? "" : m_dir, " ", &count);
  if (!recipients.isEmpty())
    selected_users = QtPassSettings::getPass()->listKeys(recipients);
  foreach (const UserInfo &sel, selected_users) {
    for (auto &user : users)
      if (sel.key_id == user.key_id)
        user.enabled = true;
  }

  if (count > selected_users.size()) {
    // Some keys seem missing from keyring, add them separately
    QStringList recipients = QtPassSettings::getPass()->getRecipientList(
        m_dir.isEmpty() ? "" : m_dir);
    foreach (const QString recipient, recipients) {
      if (QtPassSettings::getPass()->listKeys(recipient).empty()) {
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

/**
 * @brief UsersDialog::accept
 */
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
  auto *info = item->data(Qt::UserRole).value<UserInfo *>();
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
  QRegularExpression nameFilter(
      QRegularExpression::wildcardToRegularExpression("*" + filter + "*"),
      QRegularExpression::CaseInsensitiveOption);
  ui->listWidget->clear();
  if (!m_userList.isEmpty()) {
    for (auto &user : m_userList) {
      if (filter.isEmpty() || nameFilter.match(user.name).hasMatch()) {
        if (!user.isValid() && !ui->checkBox->isChecked())
          continue;
        if (user.expiry.toSecsSinceEpoch() > 0 &&
            user.expiry.daysTo(QDateTime::currentDateTime()) > 0 &&
            !ui->checkBox->isChecked())
          continue;
        QString userText = user.name + "\n" + user.key_id;
        if (user.created.toSecsSinceEpoch() > 0) {
          userText +=
              " " + tr("created") + " " +
              QLocale::system().toString(user.created, QLocale::ShortFormat);
        }
        if (user.expiry.toSecsSinceEpoch() > 0)
          userText +=
              " " + tr("expires") + " " +
              QLocale::system().toString(user.expiry, QLocale::ShortFormat);
        auto *item = new QListWidgetItem(userText, ui->listWidget);
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
        } else if (user.expiry.toSecsSinceEpoch() > 0 &&
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
