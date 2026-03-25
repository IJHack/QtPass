// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
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

/*!
    \class UsersDialog
    \brief Handles listing and editing of GPG users.

    Selection of whom to encrypt to.
 */
class UsersDialog : public QDialog {
  Q_OBJECT

public:
  explicit UsersDialog(QString dir, QWidget *parent = nullptr);
  ~UsersDialog() override;

public Q_SLOTS:
  void accept() override;

protected:
  void closeEvent(QCloseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private Q_SLOTS:
  void itemChange(QListWidgetItem *item);
  void on_lineEdit_textChanged(const QString &filter);
  void on_checkBox_clicked();

private:
  Ui::UsersDialog *ui;
  QList<UserInfo> m_userList;
  QString m_dir;

  void populateList(const QString &filter = QString());
};

#endif // SRC_USERSDIALOG_H_
