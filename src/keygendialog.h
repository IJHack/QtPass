// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Dialog that collects name, e-mail and passphrase for a new GPG
 * keypair and drives `gpg --gen-key --batch` through the Pass backend.
 */
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
 * @ingroup keys
 * @brief Handles GPG keypair generation.
 *
 * The form asks for a name, an e-mail address and a passphrase typed twice
 * (or an explicit "no passphrase"); an Expert checkbox reveals the editable
 * GPG batch template that the name and e-mail fields are mirrored into. The
 * passphrase is never written into that template: on OK, done() validates
 * the fields, splices the passphrase in with applyPassphrase() and hands the
 * batch to Pass::GenerateGPGKeys(), replacing the form with a progress
 * indicator. The dialog owns nothing of the backend; it connects to
 * Pass::finishedGenerateGPGKeys (accept) and Pass::generateGPGKeysFailed
 * (show the error and give the form back) for the duration of one attempt
 * only, so an `exec()` caller such as the first-run wizard sees `Accepted`
 * exactly when a key exists.
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
  /** @brief Destroys the dialog and its generated UI. */
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

  /**
   * @brief Whether @p value can go into a GPG batch line as one value.
   *
   * The batch is line-oriented, and gpg reads a line as a C string: a line
   * break or NUL in a name or passphrase would start a directive of its own
   * (%no-protection, a Passphrase: of the paster's choosing).
   * @param value A field's text.
   * @return False when it holds a CR, LF or NUL.
   */
  [[nodiscard]] static auto isBatchValue(const QString &value) -> bool;

protected:
private slots:
  /**
   * @brief First passphrase field edited: re-evaluates whether OK may be
   * enabled. The text itself stays out of the template box.
   * @param arg1 The new field text (unused).
   */
  void on_passphrase1_textChanged(const QString &arg1);
  /**
   * @brief Confirmation field edited: same as on_passphrase1_textChanged().
   * @param arg1 The new field text (unused).
   */
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
  /**
   * @brief Mirrors the e-mail field into the template's `Name-Email:` line.
   * @param arg1 The new field text.
   */
  void on_email_textChanged(const QString &arg1);
  /**
   * @brief Mirrors the name field into the template's `Name-Real:` line.
   * @param arg1 The new field text.
   */
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
   * @return true when generation may proceed as far as the passphrase goes.
   */
  auto passphraseChosen() const -> bool;
  /** @brief Generated form (`keygendialog.ui`); owned. */
  QScopedPointer<Ui::KeygenDialog> ui;
  /**
   * @brief Rewrites every `<key>: ...` line of the batch template to carry
   * @p value. Values that fail isBatchValue() are ignored so a pasted line
   * break can never become a directive of its own.
   * @param key Batch parameter name, e.g. `Name-Real`.
   * @param value New value for that parameter.
   */
  void replace(const QString &key, const QString &value);
  /**
   * @brief On `Accepted`, validates the form (passphrase chosen, no line
   * breaks, name of 5+ characters, plausible e-mail), swaps the form for the
   * progress indicator and calls startGeneration(); the dialog only closes
   * once the backend reports success. Any other result disconnects the
   * backend signals and closes normally.
   * @param r The QDialog result code.
   */
  void done(int r) override;
  /**
   * @brief Connects to the backend's finished/failed signals for this one
   * attempt and calls Pass::GenerateGPGKeys(). With no backend the attempt
   * fails at once.
   * @param batch Complete batch text, passphrase already spliced in.
   */
  void startGeneration(const QString &batch);
  /**
   * @brief Stops and hides the progress indicator, re-enables the form under
   * the usual OK rule and shows @p error in the status label, so the user can
   * correct the input and try again.
   * @param error Backend error text; HTML-escaped before display.
   */
  void generationFailed(const QString &error);
  /** @brief Backend that runs gpg; not owned, may be nullptr. */
  Pass *m_pass;
  /** @brief Connection to Pass::finishedGenerateGPGKeys, live only during an
   * attempt. */
  QMetaObject::Connection m_finished;
  /** @brief Connection to Pass::generateGPGKeysFailed, live only during an
   * attempt. */
  QMetaObject::Connection m_failed;
  /** @brief Spinner shown while gpg works; created lazily on the first
   * attempt, reused (hidden) across retries. */
  std::unique_ptr<QProgressIndicator> m_progressIndicator;
};

#endif // SRC_KEYGENDIALOG_H_
