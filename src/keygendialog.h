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

class Pass;

/**
 * @class KeygenDialog
 * @brief Handles GPG keypair generation.
 */
class KeygenDialog : public QDialog {
  Q_OBJECT

public:
  /**
   * @brief Construct a KeygenDialog.
   * @param gpgExe Path to the gpg executable (for the default template).
   * @param pass Backend that runs `gpg --gen-key`; the dialog accepts on its
   *             finishedGenerateGPGKeys() and shows generateGPGKeysFailed().
   * @param parent Parent widget, or nullptr.
   */
  explicit KeygenDialog(const QString &gpgExe, Pass *pass,
                        QWidget *parent = nullptr);
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
private slots:
  void on_passphrase1_textChanged(const QString &arg1);
  void on_passphrase2_textChanged(const QString &arg1);
  /**
   * @brief Ticking "no passphrase" clears and disables the passphrase
   * fields and makes OK available; unticking it asks for one again.
   * @param checked The checkbox state.
   */
  void setNoPassphrase(bool checked);
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
  /**
   * @brief OK is available when a passphrase is typed twice the same, or
   * when no passphrase was asked for explicitly. Empty fields alone are not
   * a choice: the key would be stored unprotected without anyone saying so.
   */
  void updateOkState();
  /**
   * @brief Whether the passphrase question has an answer: typed twice the
   * same, or waived with the checkbox. Empty fields alone are neither.
   */
  auto passphraseChosen() const -> bool;
  QScopedPointer<Ui::KeygenDialog> ui;
  void replace(const QString &, const QString &);
  void done(int r) override;
  void startGeneration(const QString &batch);
  void generationFailed(const QString &error);
  Pass *m_pass;
  QMetaObject::Connection m_finished;
  QMetaObject::Connection m_failed;
  std::unique_ptr<QProgressIndicator> m_progressIndicator;
};

#endif // SRC_KEYGENDIALOG_H_
