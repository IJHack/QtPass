// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_PASSWORDDIALOG_H_
#define SRC_PASSWORDDIALOG_H_

#include "appsettings.h"

#include <QDialog>

namespace Ui {
class PasswordDialog;
}

class Pass;
class QAction;
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
   * @param pass Backend used to show, insert, and generate passwords.
   * @param s Application settings snapshot (password config, template, pwgen).
   * @param file Path to the password file being edited.
   * @param isNew true if creating a new entry, false if editing existing.
   * @param parent Optional parent widget.
   */
  PasswordDialog(Pass *pass, const AppSettings &s, QString file,
                 const bool &isNew, QWidget *parent = nullptr);
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

  /**
   * @brief Set available templates from file and select default.
   * @param templates Hash of template name to field list.
   * @param defaultTemplate Name of default template.
   */
  void setAvailableTemplates(const QHash<QString, QStringList> &templates,
                             const QString &defaultTemplate);

public slots:
  /**
   * @brief Cycle to next template (Ctrl+T).
   */
  void cycleTemplate();

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
  /**
   * @brief Locate the field that holds the one-time password configuration.
   * @return The matching QLineEdit, or nullptr when the entry has no OTP field.
   */
  [[nodiscard]] auto otpLineEdit() const -> QLineEdit *;
  /**
   * @brief Connect validation and normalisation to the OTP field, if present.
   *
   * Called whenever the field widgets are rebuilt, since both setTemplate()
   * and setPassword() recreate them.
   */
  void hookOtpField();
  /**
   * @brief Flag an OTP value that is neither a URI nor valid base32.
   */
  void validateOtpField();
  /**
   * @brief Rewrite the OTP field as a canonical otpauth URI.
   *
   * A value that cannot be parsed is left exactly as the user typed it, so
   * nothing is silently destroyed.
   */
  void normalizeOtpField();

  Ui::PasswordDialog *ui;
  PasswordConfiguration m_passConfig;
  Pass *m_pass{nullptr};
  QStringList m_fields;
  QString m_file;
  bool m_templating{};
  bool m_isNew{};
  /// True once the existing entry's decrypted content has been loaded, so
  /// on_accepted() can refuse to overwrite it with empty fields before the
  /// asynchronous Show completes.
  bool m_contentLoaded{};
  QList<QLineEdit *> m_templateLines;
  QList<QLineEdit *> m_otherLines;
  QHash<QString, QStringList> m_availableTemplates;
  QString m_currentTemplateName;
  /// Warning indicator shown inside the OTP field; owned by that field.
  QAction *m_otpWarning{nullptr};

  void applyTemplate(const QString &templateName);
};

#endif // SRC_PASSWORDDIALOG_H_
