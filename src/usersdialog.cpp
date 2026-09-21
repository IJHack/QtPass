// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "usersdialog.h"
#include "importkeydialog.h"
#include "pass.h"
#include "qtpasssettings.h"
#include "ui_usersdialog.h"
#include "windowstatestore.h"
#include <QApplication>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QWidget>
#include <algorithm>
#include <utility>

#include "qtpasslogging.h"
/**
 * @brief UsersDialog::UsersDialog basic constructor
 * @param pass Active Pass backend.
 * @param s Application settings snapshot.
 * @param dir Password directory
 * @param parent
 */
UsersDialog::UsersDialog(Pass *pass, const AppSettings &s, QString dir,
                         QWidget *parent)
    : QDialog(parent), ui(new Ui::UsersDialog), m_pass(pass),
      m_passStore(s.passStore), m_gpgExe(s.gpgExecutable),
      m_dir(std::move(dir)) {
  Q_ASSERT(pass);
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
  // importKeyButton is wired via Qt's automatic on_<name>_<signal> mechanism
  // already triggered by setupUi().

  ui->lineEdit->setClearButtonEnabled(true);
}

/**
 * @brief Restore dialog geometry from settings.
 */
void UsersDialog::restoreDialogState() {
  WindowStateStore::attach(*this, QStringLiteral("usersDialog"));
}

auto UsersDialog::loadGpgKeys() -> bool {
  QList<UserInfo> users = m_pass->listKeys();
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
  QList<UserInfo> secret_keys = m_pass->listKeys("", true);
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
  // Through the backend: with a signing key only a verified list may be
  // preselected, since OK signs whatever is selected.
  const Pass::RecipientsForEditing loaded =
      m_pass->recipientsForEditing(m_dir, m_passStore);
  const QStringList recipients =
      loaded.state == Pass::RecipientsForEditing::State::Rejected
          ? QStringList()
          : loaded.recipients;
  if (!loaded.warning.isEmpty()) {
    auto *banner = new QLabel(loaded.warning, this);
    banner->setObjectName(QStringLiteral("recipientWarning"));
    banner->setWordWrap(true);
    banner->setTextFormat(Qt::PlainText);
    banner->setStyleSheet(QStringLiteral(
        "QLabel { font-weight: bold; color: #b00020; padding: 4px; }"));
    ui->verticalLayout->insertWidget(1, banner);
  }
  if (recipients.isEmpty()) {
    // A folder without .gpg-id (new store, new profile) has no recipients
    // yet. Without this, listKeys() with no filter returned the whole
    // keyring and every key in it came up preselected.
    return;
  }
  const int count = static_cast<int>(recipients.size());

  QList<UserInfo> selectedUsers = m_pass->listKeys(recipients);
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
    const QStringList &allRecipients = recipients;

    // Use bulk lookup to resolve all recipients at once (single gpg call)
    // This preserves the original email/UID resolution behavior
    QList<UserInfo> resolvedKeys = m_pass->listKeys(allRecipients);
    // Track resolved recipients by their resolved key_id
    QSet<QString> resolvedKeyIds;
    for (const UserInfo &key : resolvedKeys) {
      resolvedKeyIds.insert(key.key_id);
    }

    // Accept a recipient as resolved if GPG returned a key for it
    // (either its exact key_id, or GPG resolved email/UID/fingerprint to it)
    QSet<QString> resolvedRecipients;
    for (const UserInfo &key : resolvedKeys) {
      resolvedRecipients.insert(key.key_id);
      // Also add the name (email/UID) of resolved keys as valid resolved tokens
      // since GPG matched them to this key
      if (!key.name.isEmpty()) {
        resolvedRecipients.insert(
            key.name.section('@', 0, 0));    // email local part
        resolvedRecipients.insert(key.name); // full email/UID
      }
    }

    for (const QString &recipient : allRecipients) {
      if (!resolvedKeyIds.contains(recipient) &&
          !resolvedRecipients.contains(recipient) &&
          !selectedKeyIds.contains(recipient)) {
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
UsersDialog::~UsersDialog() = default;

/**
 * @brief UsersDialog::accept
 */
void UsersDialog::accept() {
  if (!hasSelection()) {
    // Nothing to encrypt to: an empty .gpg-id would make every insert fail
    // and the wizard never offers this dialog again once the file exists.
    return;
  }
  if (m_initOnAccept) {
    m_pass->Init(m_dir, m_userList);
  }

  QDialog::accept();
}

auto UsersDialog::hasSelection() const -> bool {
  return std::any_of(m_userList.cbegin(), m_userList.cend(),
                     [](const UserInfo &user) { return user.enabled; });
}

void UsersDialog::updateOkButton() {
  if (auto *ok = ui->buttonBox->button(QDialogButtonBox::Ok)) {
    ok->setEnabled(hasSelection());
  }
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
    qCWarning(lcQtPass)
        << "UsersDialog::itemChange: invalid user index data for item";
    return;
  }
  if (index < 0 || index >= m_userList.size()) {
    qCWarning(lcQtPass) << "UsersDialog::itemChange: user index out of range:"
                        << index << "valid range is [0,"
                        << (m_userList.size() - 1) << "]";
    return;
  }
  m_userList[index].enabled = item->checkState() == Qt::Checked;
  updateOkButton();
}

/**
 * @brief UsersDialog::populateList update the view based on filter options
 * (such as searching).
 * @param filter
 */
void UsersDialog::populateList(const QString &filter) {
  // Invalidate cached datetime so expiry checks use fresh current time
  m_cachedDateTimeValid = false;

  QString patternString = "*" + filter + "*";
  if (m_cachedPatternString != patternString) {
    QRegularExpression re(
        QRegularExpression::wildcardToRegularExpression(patternString),
        QRegularExpression::CaseInsensitiveOption);
    if (re.isValid()) {
      m_cachedNameFilter = re;
      m_cachedPatternString = patternString;
    }
  }
  const QRegularExpression &nameFilter = m_cachedNameFilter;
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
  updateOkButton();
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
  if (!m_cachedDateTimeValid) {
    m_cachedCurrentDateTime = QDateTime::currentDateTime();
    m_cachedDateTimeValid = true;
  }
  return user.expiry.isValid() && m_cachedCurrentDateTime > user.expiry;
}

/**
 * @brief Builds display text for a user.
 * @param user User to format
 * @return Formatted user text
 */
QString UsersDialog::buildUserText(const UserInfo &user) const {
  QString text = user.name + "\n" + user.key_id;
  if (user.created.isValid()) {
    text += " " + tr("created") + " " +
            QLocale::system().toString(user.created, QLocale::ShortFormat);
  }
  if (user.expiry.isValid()) {
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
  // Status badge first: an own key can be expired too, and then the marker
  // matters more than the "you can decrypt with this" colour.
  bool badged = true;
  if (!user.isValid()) {
    item->setBackground(Qt::darkRed);
    item->setForeground(Qt::white);
    item->setText(tr("[INVALID] ") + originalText);
  } else if (isUserExpired(user)) {
    // Same treatment as invalid: a lone dark-red foreground is unreadable on
    // dark themes, and gpg refuses to encrypt to expired keys anyway.
    item->setBackground(Qt::darkRed);
    item->setForeground(Qt::white);
    item->setText(tr("[EXPIRED] ") + originalText);
  } else if (!user.fullyValid()) {
    item->setBackground(Qt::darkYellow);
    item->setForeground(Qt::white);
    item->setText(tr("[PARTIAL] ") + originalText);
  } else {
    item->setText(originalText);
    badged = false;
  }
  if (user.have_secret) {
    if (!badged) {
      item->setForeground(QApplication::palette().color(QPalette::Link));
    }
    QFont font = item->font();
    font.setBold(true);
    item->setFont(font);
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

void UsersDialog::on_importKeyButton_clicked() {
  ImportKeyDialog dialog(m_gpgExe, this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  // dialog.exec() == Accepted is only reachable after a successful import
  // (see ImportKeyDialog::importFromString), so importedKeyId() is non-empty
  // by construction. Guard anyway: an empty value would make the
  // bidirectional endsWith() below match the first listed key.
  const QString importedKey = dialog.importedKeyId();
  if (importedKey.isEmpty()) {
    return;
  }

  if (!loadGpgKeys()) {
    return;
  }

  // Clear the filter so the just-imported key is visible. setText("") still
  // emits textChanged once, so don't double-populate.
  {
    const QSignalBlocker blocker(ui->lineEdit);
    ui->lineEdit->clear();
  }
  populateList(QString());

  // Match against the user's stored key_id, not the visible item text.
  // gpg can return a 16-char long key id (IMPORTED status line) or a
  // 40-char fingerprint (IMPORT_OK status line); compare both directions
  // so a long-id match works against a fingerprint hit and vice versa.
  for (int i = 0; i < ui->listWidget->count(); ++i) {
    QListWidgetItem *item = ui->listWidget->item(i);
    if (item == nullptr) {
      continue;
    }
    bool ok = false;
    const int idx = item->data(Qt::UserRole).toInt(&ok);
    if (!ok || idx < 0 || idx >= m_userList.size()) {
      continue;
    }
    const QString &keyId = m_userList[idx].key_id;
    if (keyId.isEmpty()) {
      continue;
    }
    // Only perform endsWith checks when the suffix being matched is at least
    // 16 chars to avoid false positives with short IDs.
    if ((keyId.length() >= 16 &&
         importedKey.endsWith(keyId, Qt::CaseInsensitive)) ||
        (importedKey.length() >= 16 &&
         keyId.endsWith(importedKey, Qt::CaseInsensitive))) {
      ui->listWidget->setCurrentItem(item);
      break;
    }
  }
}
