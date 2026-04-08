// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_USERSDIALOG_H_
#define SRC_USERSDIALOG_H_

#include "userinfo.h"

#include <QDialog>
#include <QList>

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
  explicit UsersDialog(QString dir, QWidget *parent = nullptr);
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

private:
  Ui::UsersDialog *ui;
  QList<UserInfo> m_userList; /**< List of available GPG users */
  QString m_dir;              /**< Password store directory */

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
