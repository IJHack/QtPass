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
#include <QFont>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QWidget>
#include <algorithm>
#include <utility>

#include "qtpasslogging.h"
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

void UsersDialog::showRecipientWarning(const QString &text) {
  auto *banner = new QLabel(text, this);
  banner->setObjectName(QStringLiteral("recipientWarning"));
  banner->setWordWrap(true);
  banner->setTextFormat(Qt::PlainText);
  // Palette, not a stylesheet colour, so it reads on dark themes too; red as
  // ProcessOutputPanel paints error lines.
  QFont bold = banner->font();
  bold.setBold(true);
  banner->setFont(bold);
  QPalette palette = banner->palette();
  palette.setColor(QPalette::WindowText, QColor(Qt::red));
  banner->setPalette(palette);
  banner->setContentsMargins(4, 4, 4, 4);
  ui->verticalLayout->insertWidget(1, banner);
}

namespace {

/// A key ID as written anywhere: without a 0x prefix, upper case.
auto normalisedKeyId(const QString &id) -> QString {
  const QString trimmed = id.trimmed();
  return (trimmed.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)
              ? trimmed.mid(2)
              : trimmed)
      .toUpper();
}

/// Whether @p recipient, as a .gpg-id lists it, names @p key the way gpg's
/// -r resolves it: a key ID in any length gpg accepts (the key carries its
/// fingerprint, the list may carry a long or short ID, a suffix of it, or
/// the fingerprint; eight characters is the shortest), or else a name,
/// matched case-insensitively as a substring of the UID after gpg's
/// "=", "<...>", "@" and "*" markers.
auto answersTo(const UserInfo &key, const QString &recipient) -> bool {
  static const QRegularExpression hexId(
      QStringLiteral("^(0x)?[0-9A-Fa-f]{8,}$"));
  if (hexId.match(recipient.trimmed()).hasMatch()) {
    const QString id = normalisedKeyId(recipient);
    const QString keyId = normalisedKeyId(key.key_id);
    return keyId.endsWith(id) || (keyId.size() >= 8 && id.endsWith(keyId));
  }
  QString term = recipient.trimmed();
  if (term.startsWith(u'=') || term.startsWith(u'@') || term.startsWith(u'*')) {
    term.remove(0, 1);
  } else if (term.startsWith(u'<') && term.endsWith(u'>')) {
    term = term.mid(1, term.size() - 2);
  }
  return !term.isEmpty() && key.name.contains(term, Qt::CaseInsensitive);
}

} // namespace

void UsersDialog::selectRecipients(const QStringList &recipients) {
  // One gpg call resolves every recipient, by key ID or by email/UID.
  const QList<UserInfo> resolved = m_pass->listKeys(recipients);
  for (UserInfo &user : m_userList) {
    if (std::any_of(resolved.cbegin(), resolved.cend(),
                    [&user](const UserInfo &key) {
                      return key.key_id == user.key_id;
                    })) {
      user.enabled = true;
    }
  }
  for (const QString &recipient : recipients) {
    // The match is a fast path, not gpg's resolution: a secondary UID or a
    // subkey ID resolves in gpg without showing in what it lists. Only what
    // gpg itself cannot find is reported missing.
    const bool found = std::any_of(resolved.cbegin(), resolved.cend(),
                                   [&recipient](const UserInfo &key) {
                                     return answersTo(key, recipient);
                                   }) ||
                       !m_pass->listKeys(recipient).isEmpty();
    if (!found) {
      UserInfo missing;
      missing.enabled = true;
      missing.key_id = recipient;
      missing.name = " ?? " + tr("Key not found in keyring");
      m_userList.append(missing);
    }
  }
}

void UsersDialog::loadRecipients() {
  // Through the backend: with a signing key only a verified list may be
  // preselected, since OK signs whatever is selected.
  const Pass::RecipientsForEditing loaded =
      m_pass->recipientsForEditing(m_dir, m_passStore);
  if (!loaded.warning.isEmpty()) {
    showRecipientWarning(loaded.warning);
  }
  // A folder without .gpg-id (new store, new profile) has no recipients yet;
  // listKeys() with no filter would return the whole keyring, preselected.
  if (loaded.state != Pass::RecipientsForEditing::State::Rejected &&
      !loaded.recipients.isEmpty()) {
    selectRecipients(loaded.recipients);
  }
}

UsersDialog::~UsersDialog() = default;

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

void UsersDialog::keyPressEvent(QKeyEvent *event) {
  switch (event->key()) {
  case Qt::Key_Escape:
    ui->lineEdit->clear();
    break;
  default:
    break;
  }
}

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

auto UsersDialog::isUserExpired(const UserInfo &user) const -> bool {
  if (!m_cachedDateTimeValid) {
    m_cachedCurrentDateTime = QDateTime::currentDateTime();
    m_cachedDateTimeValid = true;
  }
  return user.expiry.isValid() && m_cachedCurrentDateTime > user.expiry;
}

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

void UsersDialog::on_lineEdit_textChanged(const QString &filter) {
  populateList(filter);
}

void UsersDialog::on_checkBox_clicked() { populateList(ui->lineEdit->text()); }

void UsersDialog::on_importKeyButton_clicked() {
  ImportKeyDialog dialog(m_gpgExe, this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  // Non-empty after Accepted, but an empty id would make the endsWith()
  // below match the first listed key.
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

  // Match the stored key_id, not the item text. gpg reports a 16-char long
  // id (IMPORTED) or a 40-char fingerprint (IMPORT_OK): compare both ways.
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
    // Suffixes shorter than 16 chars would give false positives.
    if ((keyId.length() >= 16 &&
         importedKey.endsWith(keyId, Qt::CaseInsensitive)) ||
        (importedKey.length() >= 16 &&
         keyId.endsWith(importedKey, Qt::CaseInsensitive))) {
      ui->listWidget->setCurrentItem(item);
      break;
    }
  }
}
