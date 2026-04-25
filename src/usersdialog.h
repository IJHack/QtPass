// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_USERSDIALOG_H_
#define SRC_USERSDIALOG_H_

#include "userinfo.h"

#include <QDialog>
#include <QList>
#include <QRegularExpression>

namespace Ui {
class UsersDialog;
}

class QCloseEvent;
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
   * @param dir Password store directory path.
   * @param parent Parent widget.
   */
  explicit UsersDialog(const QString &dir, QWidget *parent = nullptr);
  /**
   * @brief Destructor.
   */
  ~UsersDialog() override;

public slots:
  /**
   * @brief Handle dialog acceptance.
   */
  void accept() override;

protected:
  /**
   * @brief Handle close event.
   * @param event Close event.
   */
  void closeEvent(QCloseEvent *event) override;
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

private slots:
  /**
   * @brief Import a GPG key and refresh the list.
   */
  void on_importKeyButton_clicked();

private:
  Ui::UsersDialog *ui;
  QList<UserInfo> m_userList;            /**< List of available GPG users */
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
