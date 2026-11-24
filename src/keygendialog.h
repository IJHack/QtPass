// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_KEYGENDIALOG_H_
#define SRC_KEYGENDIALOG_H_

#include <QDialog>
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

  /**
   * @brief Whether gpg reads @p passphrase back as typed from a batch
   * `Passphrase:` line.
   *
   * gpg skips white space after the colon and cuts spaces and tabs off the
   * end of a value, so a passphrase with a blank at either end would protect
   * the key without it, and the passphrase typed twice is then refused. Only
   * ASCII white space counts: gpg keeps a no-break space.
   * @param passphrase The passphrase typed; empty means none.
   * @return False when it starts or ends with ASCII white space.
   */
  static bool isBatchPassphrase(const QString &passphrase);
  /**
   * @brief isWholeText whether a field's text is valid UTF-16, without half
   * of a character (a lone surrogate, as half of a pasted emoji).
   *
   * gpg gets no such text, only a replacement character for it: a name would
   * not be the one typed, and a passphrase would protect the key with
   * another one.
   * @param value A field's text.
   * @return False when it holds a surrogate without its other half.
   */
  static bool isWholeText(const QString &value);

protected:
  /**
   * @brief Handle dialog close, emitting appropriate signals.
   * @param event The close event.
   */
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
  ConfigDialog *dialog;
  std::unique_ptr<QProgressIndicator> m_progressIndicator;
};

#endif // SRC_KEYGENDIALOG_H_
