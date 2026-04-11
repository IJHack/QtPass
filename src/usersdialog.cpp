// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "usersdialog.h"
#include "qtpasssettings.h"
#include "ui_usersdialog.h"
#include <QApplication>
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

  restoreDialogState();
  if (!loadGpgKeys()) {
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    return;
  }

  loadRecipients();
  populateList();

  connectSignals();
}

void UsersDialog::connectSignals() {
  connect(ui->buttonBox, &QDialogButtonBox::accepted, this,
          &UsersDialog::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(ui->listWidget, &QListWidget::itemChanged, this,
          &UsersDialog::itemChange);

  ui->lineEdit->setClearButtonEnabled(true);
}

void UsersDialog::restoreDialogState() {
  /**
   * @brief Restore dialog geometry from settings.
   */
  QByteArray savedGeometry = QtPassSettings::getDialogGeometry("usersDialog");
  bool hasSavedGeometry = !savedGeometry.isEmpty();
  if (hasSavedGeometry) {
    restoreGeometry(savedGeometry);
  }
  if (QtPassSettings::isDialogMaximized("usersDialog")) {
    showMaximized();
  } else if (hasSavedGeometry) {
    move(QtPassSettings::getDialogPos("usersDialog"));
    resize(QtPassSettings::getDialogSize("usersDialog"));
  }
}

auto UsersDialog::loadGpgKeys() -> bool {
  /**
   * @brief Load GPG keys and determine secret key status.
   * @return true if successful, false if keys could not be loaded.
   */
  QList<UserInfo> users = QtPassSettings::getPass()->listKeys();
  if (users.isEmpty()) {
    QMessageBox::critical(parentWidget(), tr("Keylist missing"),
                          tr("Could not fetch list of available GPG keys"));
    reject();
    return false;
  }

  markSecretKeys(users);

  m_userList = users;
  return true;
}

void UsersDialog::markSecretKeys(QList<UserInfo> &users) {
  /**
   * @brief Mark which keys have secret counterparts.
   * @param users List of users to mark.
   */
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
}

void UsersDialog::loadRecipients() {
  /**
   * @brief Load recipients and handle missing keys.
   */
  int count = 0;
  QStringList recipients = QtPassSettings::getPass()->getRecipientString(
      m_dir.isEmpty() ? "" : m_dir, " ", &count);

  QList<UserInfo> selectedUsers =
      QtPassSettings::getPass()->listKeys(recipients);
  QSet<QString> selectedKeyIds;
  for (const UserInfo &sel : selectedUsers) {
    selectedKeyIds.insert(sel.key_id);
  }
  for (auto &user : m_userList) {
    if (selectedKeyIds.contains(user.key_id)) {
      user.enabled = true;
    }
  }

  if (count > selectedUsers.size()) {
    QStringList allRecipients = QtPassSettings::getPass()->getRecipientList(
        m_dir.isEmpty() ? "" : m_dir);
    QSet<QString> missingKeyRecipients;
    for (const QString &recipient : allRecipients) {
      if (!missingKeyRecipients.contains(recipient) &&
          QtPassSettings::getPass()->listKeys(recipient).empty()) {
        missingKeyRecipients.insert(recipient);
        UserInfo i;
        i.enabled = true;
        i.key_id = recipient;
        i.name = " ?? " + tr("Key not found in keyring");
        m_userList.append(i);
      }
    }
  }
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
 * @brief UsersDialog::closeEvent save window state on close.
 * @param event
 */
void UsersDialog::closeEvent(QCloseEvent *event) {
  QtPassSettings::setDialogGeometry("usersDialog", saveGeometry());
  if (!isMaximized()) {
    QtPassSettings::setDialogPos("usersDialog", pos());
    QtPassSettings::setDialogSize("usersDialog", size());
  }
  QtPassSettings::setDialogMaximized("usersDialog", isMaximized());
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
  bool ok = false;
  const int index = item->data(Qt::UserRole).toInt(&ok);
  if (!ok) {
#ifdef QT_DEBUG
    qWarning() << "UsersDialog::itemChange: invalid user index data for item";
#endif
    return;
  }
  if (index < 0 || index >= m_userList.size()) {
#ifdef QT_DEBUG
    qWarning() << "UsersDialog::itemChange: user index out of range:" << index
               << "valid range is [0," << (m_userList.size() - 1) << "]";
#endif
    return;
  }
  m_userList[index].enabled = item->checkState() == Qt::Checked;
}

/**
 * @brief UsersDialog::populateList update the view based on filter options
 * (such as searching).
 * @param filter
 */
void UsersDialog::populateList(const QString &filter) {
  static QString lastFilter;
  static QRegularExpression cachedNameFilter;
  if (filter != lastFilter) {
    lastFilter = filter;
    cachedNameFilter = QRegularExpression(
        QRegularExpression::wildcardToRegularExpression("*" + filter + "*"),
        QRegularExpression::CaseInsensitiveOption);
  }
  const QRegularExpression &nameFilter = cachedNameFilter;
  ui->listWidget->clear();

  for (int i = 0; i < m_userList.size(); ++i) {
    const auto &user = m_userList.at(i);
    if (!passesFilter(user, filter, nameFilter)) {
      continue;
    }

    auto *item = new QListWidgetItem(buildUserText(user), ui->listWidget);
    applyUserStyling(item, user);
    item->setCheckState(user.enabled ? Qt::Checked : Qt::Unchecked);
    item->setData(Qt::UserRole, QVariant::fromValue(i));
    ui->listWidget->addItem(item);
  }
}

/**
 * @brief Checks if a user passes the filter criteria.
 * @param user User to check
 * @param filter Filter string
 * @param nameFilter Compiled name filter regex
 * @return true if user passes filter
 */
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

/**
 * @brief Checks if a user's key has expired.
 * @param user User to check
 * @return true if user's key is expired
 */
auto UsersDialog::isUserExpired(const UserInfo &user) const -> bool {
  return user.expiry.toSecsSinceEpoch() > 0 &&
         QDateTime::currentDateTime() > user.expiry;
}

/**
 * @brief Builds display text for a user.
 * @param user User to format
 * @return Formatted user text
 */
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

/**
 * @brief Applies visual styling to a user list item based on key status.
 * @param item List widget item to style
 * @param user User whose status determines styling
 */
void UsersDialog::applyUserStyling(QListWidgetItem *item,
                                   const UserInfo &user) const {
  const QString originalText = item->text();
  if (user.have_secret) {
    const QPalette palette = QApplication::palette();
    item->setForeground(palette.color(QPalette::Link));
    QFont font = item->font();
    font.setBold(true);
    item->setFont(font);
  } else if (!user.isValid()) {
    item->setBackground(Qt::darkRed);
    item->setForeground(Qt::white);
    item->setText(tr("[INVALID] ") + originalText);
  } else if (isUserExpired(user)) {
    item->setForeground(Qt::darkRed);
    item->setText(tr("[EXPIRED] ") + originalText);
  } else if (!user.fullyValid()) {
    item->setBackground(Qt::darkYellow);
    item->setForeground(Qt::white);
    item->setText(tr("[PARTIAL] ") + originalText);
  } else {
    item->setText(originalText);
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
