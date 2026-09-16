// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_KEYGENDIALOG_H_
#define SRC_KEYGENDIALOG_H_

#include <QDialog>
#include <QScopedPointer>
#include <memory>

#include "qprogressindicator.h"

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
  /**
   * @brief Construct a KeygenDialog.
   * @param gpgExe Path to the gpg executable.
   * @param parent Parent dialog, or nullptr.
   */
  explicit KeygenDialog(const QString &gpgExe, ConfigDialog *parent = nullptr);
  ~KeygenDialog() override;

  /**
   * @brief Splice a passphrase into a GPG batch template.
   *
   * Replaces %no-protection (or an existing Passphrase: line) with
   * "Passphrase: <passphrase>"; an empty passphrase yields %no-protection.
   * @param batch GPG batch template text.
   * @param passphrase Key passphrase, empty for an unprotected key.
   * @return Batch text ready for gpg --gen-key --batch.
   */
  static QString applyPassphrase(const QString &batch,
                                 const QString &passphrase);

protected:
  /**
   * @brief Handle dialog close, emitting appropriate signals.
   * @param event The close event.
   */
  void closeEvent(QCloseEvent *event) override;

private slots:
  void on_passphrase1_textChanged(const QString &arg1);
  void on_passphrase2_textChanged(const QString &arg1);
  /**
   * @brief Enable or disable expert mode (the editable batch template).
   *
   * Connected explicitly to QCheckBox::toggled in the constructor; the box
   * is two-state, so the deprecated stateChanged(int) is not needed.
   * @param checked true when the Expert box is checked.
   */
  void setExpertMode(bool checked);
  void on_email_textChanged(const QString &arg1);
  void on_name_textChanged(const QString &arg1);

private:
  QScopedPointer<Ui::KeygenDialog> ui;
  void replace(const QString &, const QString &);
  void done(int r) override;
  ConfigDialog *dialog;
  std::unique_ptr<QProgressIndicator> m_progressIndicator;
};

#endif // SRC_KEYGENDIALOG_H_
