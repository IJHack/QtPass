// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_KEYGENDIALOG_H_
#define SRC_KEYGENDIALOG_H_

#include <QDialog>

namespace Ui {
class KeygenDialog;
}

/*!
    \class KeygenDialog
    \brief Handles GPG keypair generation.
 */
class ConfigDialog;
class QCloseEvent;
/**
 * Construct the KeygenDialog.
 * @param parent Optional parent ConfigDialog used to return to the configuration UI.
 */

/**
 * Destroy the KeygenDialog and release associated resources.
 */

/**
 * Handle the widget close event to perform any cleanup or confirm actions before closing.
 * @param event Close event provided by Qt.
 */

/**
 * Update internal state when the first passphrase text changes.
 * @param arg1 New passphrase text from the first input field.
 */

/**
 * Update internal state when the confirmation passphrase text changes.
 * @param arg1 New passphrase text from the confirmation input field.
 */

/**
 * React to changes of the checkbox controlling protection options.
 * @param arg1 New checkbox state (value corresponds to Qt::CheckState).
 */

/**
 * Update internal state when the email text changes.
 * @param arg1 New email text from the email input field.
 */

/**
 * Update internal state when the name text changes.
 * @param arg1 New name text from the name input field.
 */

/**
 * Perform a text replacement within the dialog using the provided strings.
 * @param from Text to be replaced.
 * @param to Replacement text.
 */

/**
 * Finalize the dialog with the given result code and perform any necessary teardown.
 * @param r Result code passed to QDialog::done.
 */

/**
 * Enable or disable "no protection" behavior for generated keys or inputs.
 * @param enable `true` to enable no-protection mode, `false` to disable it.
 */
class KeygenDialog : public QDialog {
  Q_OBJECT

public:
  explicit KeygenDialog(ConfigDialog *parent = nullptr);
  ~KeygenDialog() override;

protected:
  void closeEvent(QCloseEvent *event) override;

private slots:
  void on_passphrase1_textChanged(const QString &arg1);
  void on_passphrase2_textChanged(const QString &arg1);
  void on_checkBox_stateChanged(int arg1);
  void on_email_textChanged(const QString &arg1);
  void on_name_textChanged(const QString &arg1);

private:
  Ui::KeygenDialog *ui;
  void replace(const QString &, const QString &);
  void done(int r) override;
  void no_protection(bool enable);
  ConfigDialog *dialog;
};

#endif // SRC_KEYGENDIALOG_H_
