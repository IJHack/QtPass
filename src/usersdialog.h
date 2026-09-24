// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_USERSDIALOG_H_
#define SRC_USERSDIALOG_H_

#include "appsettings.h"
#include "userinfo.h"

#include <QDialog>
#include <QList>
#include <QRegularExpression>
#include <QScopedPointer>

namespace Ui {
class UsersDialog;
}

class Pass;
class QKeyEvent;
class QListWidgetItem;

/**
 * @class UsersDialog
 * @brief Dialog for selecting GPG recipients for password encryption.
 *
 * UsersDialog allows users to select which GPG keys to encrypt passwords to.
 * It displays available keys, supports filtering, and shows key expiration
 * status.
 */
class UsersDialog : public QDialog {
  Q_OBJECT

public:
  /**
   * @brief Construct users dialog.
   * @param pass Active Pass backend.
   * @param s Application settings snapshot.
   * @param dir Password store directory path.
   * @param parent Parent widget.
   */
  explicit UsersDialog(Pass *pass, const AppSettings &s, QString dir,
                       QWidget *parent = nullptr);
  /**
   * @brief Destructor.
   */
  ~UsersDialog() override;

  /**
   * @brief Choose whether OK runs Pass::Init on the folder.
   *
   * On by default. Off, the dialog only collects the selection; the caller
   * reads selectedUsers() and initialises the folder itself (new profiles,
   * which must not go through the backend of the active store).
   * @param initOnAccept false to skip Pass::Init.
   */
  void setInitOnAccept(bool initOnAccept) { m_initOnAccept = initOnAccept; }

  /**
   * @brief The key list with the user's choice in each entry's enabled flag.
   * @return All listed users; the selected ones have enabled == true.
   */
  [[nodiscard]] auto selectedUsers() const -> QList<UserInfo> {
    return m_userList;
  }

  /**
   * @brief Whether at least one key is ticked.
   * @return true if Init would get a recipient.
   */
  [[nodiscard]] auto hasSelection() const -> bool;

public slots:
  /**
   * @brief Handle dialog acceptance.
   */
  void accept() override;

protected:
  /**
   * @brief Handle key press.
   * @param event Key event.
   */
  void keyPressEvent(QKeyEvent *event) override;

private slots:
  /**
   * @brief Handle user selection change.
   * @param item Changed list item.
   */
  void itemChange(QListWidgetItem *item);
  /**
   * @brief Filter user list.
   * @param filter Filter text.
   */
  void on_lineEdit_textChanged(const QString &filter);
  /**
   * @brief Handle select all checkbox.
   */
  void on_checkBox_clicked();
  /**
   * @brief Import a GPG key and refresh the list.
   */
  void on_importKeyButton_clicked();
  /**
   * @brief Make the key gpg just imported the current row. gpg reports a
   * 16-character long ID (IMPORTED) or a 40-character fingerprint
   * (IMPORT_OK), so the two are compared both ways, by suffix.
   * @param importedKey What gpg reported.
   */
  void selectImportedKey(const QString &importedKey);

private:
  QScopedPointer<Ui::UsersDialog> ui;
  Pass *m_pass;               /**< Active Pass backend */
  QString m_passStore;        /**< Password store root path */
  QString m_gpgExe;           /**< GPG executable path */
  QList<UserInfo> m_userList; /**< List of available GPG users */
  bool m_initOnAccept{true};  /**< Whether accept() runs Pass::Init. */
  void updateOkButton();
  QString m_dir;                         /**< Password store directory */
  QString m_lastFilter;                  /**< Last filter text for caching */
  QString m_cachedPatternString;         /**< Cached pattern string */
  QRegularExpression m_cachedNameFilter; /**< Cached regex filter */
  mutable QDateTime m_cachedCurrentDateTime; /**< Cached current date/time for
                                                     expiry checks */
  mutable bool m_cachedDateTimeValid =
      false; /**< Whether cached date/time is valid */

  void restoreDialogState();

  /**
   * @brief Connect dialog signals.
   */
  void connectSignals();

  /**
   * @brief Load GPG keys and determine secret key status.
   * @return true if successful, false if keys could not be loaded.
   */
  auto loadGpgKeys() -> bool;

  /**
   * @brief Mark which keys have secret counterparts.
   * @param users List of users to mark.
   */
  void markSecretKeys(QList<UserInfo> &users);

  /**
   * @brief Load recipients and handle missing keys.
   */
  void loadRecipients();
  /**
   * @brief Show @p text above the key list in bold red, for a recipient list
   * that could not be verified.
   * @param text The warning, plain text.
   */
  void showRecipientWarning(const QString &text);
  /**
   * @brief Tick the keys the folder is encrypted to, and list recipients no
   * key in the keyring answers to, so saving does not drop them silently.
   * @param recipients The folder's recipients, as listed in its `.gpg-id`.
   */
  void selectRecipients(const QStringList &recipients);

  /**
   * @brief Populate user list.
   * @param filter Optional filter text.
   */
  void populateList(const QString &filter = QString());
  /**
   * @brief Check if user passes filter.
   * @param user User to check.
   * @param filter Text filter.
   * @param nameFilter Regex filter.
   * @return true if user passes.
   */
  bool passesFilter(const UserInfo &user, const QString &filter,
                    const QRegularExpression &nameFilter) const;
  /**
   * @brief Check if GPG key is expired.
   * @param user User to check.
   * @return true if key is expired.
   */
  auto isUserExpired(const UserInfo &user) const -> bool;
  /**
   * @brief Build display text for user.
   * @param user User info.
   * @return Formatted text.
   */
  QString buildUserText(const UserInfo &user) const;
  /**
   * @brief Apply visual styling to user item.
   * @param item List item.
   * @param user User info.
   */
  void applyUserStyling(QListWidgetItem *item, const UserInfo &user) const;
};

#endif // SRC_USERSDIALOG_H_
