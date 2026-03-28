// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "usersdialog.h"
#include "qtpasssettings.h"
#include "ui_usersdialog.h"
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSet>
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
    reject();
  }

  QList<UserInfo> secret_keys = QtPassSettings::getPass()->listKeys("", true);
  QSet<QString> secretKeyIds;
  for (const UserInfo &sec : secret_keys) {
    secretKeyIds.insert(sec.key_id);
  }
  for (auto &user : users) {
    if (secretKeyIds.contains(user.key_id)) {
      user.have_secret = true;
    }
  }

  QList<UserInfo> selected_users;
  int count = 0;

  QStringList recipients = QtPassSettings::getPass()->getRecipientString(
      m_dir.isEmpty() ? "" : m_dir, " ", &count);
  if (!recipients.isEmpty()) {
    selected_users = QtPassSettings::getPass()->listKeys(recipients);
  }
  QSet<QString> selectedKeyIds;
  for (const UserInfo &sel : selected_users) {
    selectedKeyIds.insert(sel.key_id);
  }
  for (auto &user : users) {
    if (selectedKeyIds.contains(user.key_id)) {
      user.enabled = true;
    }
  }

  if (count > selected_users.size()) {
    // Some keys seem missing from keyring, add them separately
    QStringList allRecipients = QtPassSettings::getPass()->getRecipientList(
        m_dir.isEmpty() ? "" : m_dir);
    for (const QString &recipient : allRecipients) {
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

  ui->lineEdit->setClearButtonEnabled(true);
}

/**
 * @brief UsersDialog::~UsersDialog basic destructor.
 */
UsersDialog::~UsersDialog() { delete ui; }

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
Q_DECLARE_METATYPE(UserInfo *)
Q_DECLARE_METATYPE(const UserInfo *)
#endif

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
  // TODO(annejan): save window size or something
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
  if (!item) {
    return;
  }
  auto *info = item->data(Qt::UserRole).value<UserInfo *>();
  if (!info) {
    return;
  }
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

  for (const auto &user : m_userList) {
    if (!passesFilter(user, filter, nameFilter)) {
      continue;
    }

    auto *item = new QListWidgetItem(buildUserText(user), ui->listWidget);
    applyUserStyling(item, user);
    item->setCheckState(user.enabled ? Qt::Checked : Qt::Unchecked);
    item->setData(Qt::UserRole, QVariant::fromValue(&user));
    ui->listWidget->addItem(item);
  }
}

bool UsersDialog::passesFilter(const UserInfo &user, const QString &filter,
                               const QRegularExpression &nameFilter) const {
  if (!filter.isEmpty() && !nameFilter.match(user.name).hasMatch()) {
    return false;
  }
  if (!user.isValid() && !ui->checkBox->isChecked()) {
    return false;
  }
  const bool expired = isUserExpired(user);
  return !(expired && !ui->checkBox->isChecked());
}

bool UsersDialog::isUserExpired(const UserInfo &user) const {
  return user.expiry.toSecsSinceEpoch() > 0 &&
         QDateTime::currentDateTime() > user.expiry;
}

QString UsersDialog::buildUserText(const UserInfo &user) const {
  QString text = user.name + "\n" + user.key_id;
  if (user.created.toSecsSinceEpoch() > 0) {
    text += " " + tr("created") + " " +
            QLocale::system().toString(user.created, QLocale::ShortFormat);
  }
  if (user.expiry.toSecsSinceEpoch() > 0) {
    text += " " + tr("expires") + " " +
            QLocale::system().toString(user.expiry, QLocale::ShortFormat);
  }
  return text;
}

void UsersDialog::applyUserStyling(QListWidgetItem *item,
                                   const UserInfo &user) const {
  if (user.have_secret) {
    item->setForeground(Qt::blue);
    QFont font = item->font();
    font.setBold(true);
    item->setFont(font);
  } else if (!user.isValid()) {
    item->setBackground(Qt::darkRed);
    item->setForeground(Qt::white);
  } else if (isUserExpired(user)) {
    item->setForeground(Qt::darkRed);
  } else if (!user.fullyValid()) {
    item->setBackground(Qt::darkYellow);
    item->setForeground(Qt::white);
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
