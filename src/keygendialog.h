// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_KEYGENDIALOG_H_
#define SRC_KEYGENDIALOG_H_

#include <QDialog>

namespace Ui {
class KeygenDialog;
}

class ConfigDialog;
class QCloseEvent;

/**
 * @class KeygenDialog
 * @brief Handles GPG keypair generation.
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
