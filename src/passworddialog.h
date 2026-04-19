// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
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

/**
 * PasswordDialog — Dialog for inserting and editing passwords, optionally using
 * templates.
 *
 * Manages UI for entering, generating, and templating password-related fields.
 */

/**
 * Set the text shown in the dialog's password field.
 * @param password Password text to display.
 */

/**
 * Retrieve the current text from the dialog's password field.
 * @return Current password as a QString.
 */

/**
 * Configure the template content used by the dialog and whether templating is
 * enabled.
 * @param rawFields Template text containing field definitions.
 * @param useTemplate Set to true to enable using the provided template, false
 * to disable.
 */

/**
 * Enable or disable applying the template to all applicable fields.
 * @param templateAll If true, apply templating to all fields; otherwise apply
 * selectively.
 */

/**
 * Set the desired password length used by the dialog (e.g., for generation or
 * validation).
 * @param l Desired password length.
 */

/**
 * Set the password character template identifier that controls allowed/required
 * character classes.
 * @param t Identifier describing the character template to use.
 */

/**
 * Enable or disable using the pwgen-style password generator mode.
 * @param usePwgen If true, use the pwgen generator behavior; otherwise do not.
 */

/**
 * Populate the dialog's password field with the provided output (slot).
 * @param output Password text to place into the dialog.
 */

/**
 * Handler for the "show password" checkbox state change (slot).
 * @param arg1 New state value passed by the checkbox.
 */

/**
 * Handler invoked when the create-password button is clicked (slot).
 */

/**
 * Handler invoked when the dialog is accepted (e.g., OK pressed) (slot).
 */

/**
 * Handler invoked when the dialog is rejected (e.g., Cancel pressed) (slot).
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
  void setLength(int length);
  void setPasswordCharTemplate(int templateIndex);
  void usePwgen(bool usePwgen);

public slots:
  void setPass(const QString &output);

private slots:
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
