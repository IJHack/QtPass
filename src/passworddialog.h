// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_PASSWORDDIALOG_H_
#define SRC_PASSWORDDIALOG_H_

#include "passwordconfiguration.h"
#include <QDialog>

namespace Ui {
class PasswordDialog;
}

class QLineEdit;
class QWidget;

/*!
    \class PasswordDialog
    \brief PasswordDialog Handles the inserting and editing of passwords.

    Includes templated views.
 */
class PasswordDialog : public QDialog {
  Q_OBJECT

public:
  explicit PasswordDialog(PasswordConfiguration passConfig,
                          QWidget *parent = nullptr);
  PasswordDialog(QString file, const bool &isNew, QWidget *parent = nullptr);
  ~PasswordDialog() override;

  /*! Sets content in the password field in the interface.
      \param password the password as a QString
      \sa getPassword
   */
  void setPassword(const QString &password);

  /*! Returns the password as set in the password field in the interface.
      \return password as a QString
      \sa setPassword
   */
  auto getPassword() -> QString;

  /*! Sets content in the template for the interface.
      \param rawFields is the template as a QString
      \param useTemplate whether the template is used
   */
  void setTemplate(const QString &rawFields, bool useTemplate);

  void templateAll(bool templateAll);
  void setLength(int l);
  void setPasswordCharTemplate(int t);
  void usePwgen(bool usePwgen);

public Q_SLOTS:
  void setPass(const QString &output);

private Q_SLOTS:
  void on_checkBoxShow_stateChanged(int arg1);
  void on_createPasswordButton_clicked();
  void on_accepted();
  void on_rejected();

private:
  Ui::PasswordDialog *ui;
  PasswordConfiguration m_passConfig;
  QStringList m_fields;
  QString m_file;
  bool m_templating{};
  bool m_allFields{};
  bool m_isNew;
  QList<QLineEdit *> templateLines;
  QList<QLineEdit *> otherLines;
};

#endif // SRC_PASSWORDDIALOG_H_
