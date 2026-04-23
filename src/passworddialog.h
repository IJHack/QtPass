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
 * @class PasswordDialog
 * @brief Dialog for inserting and editing passwords, optionally using
 * templates.
 *
 * Manages UI for entering, generating, and templating password-related fields.
 */
class PasswordDialog : public QDialog {
  Q_OBJECT

public:
  /**
   * @brief Construct a PasswordDialog for entering a new password.
   * @param passConfig Password generation configuration.
   * @param parent Optional parent widget.
   */
  explicit PasswordDialog(PasswordConfiguration passConfig,
                          QWidget *parent = nullptr);
  /**
   * @brief Construct a PasswordDialog for editing an existing password file.
   * @param file Path to the password file being edited.
   * @param isNew true if creating a new entry, false if editing existing.
   * @param parent Optional parent widget.
   */
  PasswordDialog(QString file, const bool &isNew, QWidget *parent = nullptr);
  ~PasswordDialog() override;

  /**
   * @brief Populate the dialog's password field with the given text.
   * @param password Password text to display.
   * @sa getPassword
   */
  void setPassword(const QString &password);

  /**
   * @brief Retrieve the current text from the dialog's password field.
   * @return Current password as a QString.
   * @sa setPassword
   */
  auto getPassword() -> QString;

  /**
   * @brief Set the template fields and whether templating is enabled.
   * @param rawFields Template text containing field definitions.
   * @param useTemplate true to enable the template, false to disable.
   */
  void setTemplate(const QString &rawFields, bool useTemplate);

  /**
   * @brief Set the desired password length shown in the dialog.
   * @param length Desired password length.
   */
  void setLength(int length);

  /**
   * @brief Set the password character template index.
   * @param templateIndex Index identifying the character template to use.
   */
  void setPasswordCharTemplate(int templateIndex);

  /**
   * @brief Enable or disable pwgen-style password generation mode.
   * @param usePwgen true to enable pwgen mode, false to disable.
   */
  void usePwgen(bool usePwgen);

public slots:
  /**
   * @brief Populate the dialog's password field from pass show output.
   * @param output Output from the pass show command.
   */
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
  bool m_isNew;
  QList<QLineEdit *> templateLines;
  QList<QLineEdit *> otherLines;
};

#endif // SRC_PASSWORDDIALOG_H_