// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_IMPORTKEYDIALOG_H_
#define SRC_IMPORTKEYDIALOG_H_

#include <QDialog>
#include <QString>

namespace Ui {
class ImportKeyDialog;
}

/**
 * @class ImportKeyDialog
 * @brief Dialog for importing GPG keys from file or clipboard.
 *
 * Allows users to import GPG public keys without leaving QtPass.
 * Offers two import paths: from file or from clipboard text.
 */
class ImportKeyDialog : public QDialog {
  Q_OBJECT

public:
  /**
   * @brief Construct an ImportKeyDialog.
   * @param parent Optional parent widget.
   */
  explicit ImportKeyDialog(QWidget *parent = nullptr);
  ~ImportKeyDialog() override;

  /**
   * @brief Get the imported key id on success.
   * @return The imported key id, or empty if import failed.
   */
  QString importedKeyFingerprint() const;

  /**
   * @brief Parse GPG import output to extract key id.
   * @param output Output from gpg --import command.
   * @return The imported key id, or empty if not found.
   */
  static auto parseGpgImportOutput(const QString &output) -> QString;

private slots:
  /**
   * @brief Handle file picker button click.
   */
  void on_fileButton_clicked();

  /**
   * @brief Handle paste button click.
   */
  void on_pasteButton_clicked();

  /**
   * @brief Handle import confirmation.
   */
  void on_importButton_clicked();

  /**
   * @brief Update UI state based on input field.
   */
  void on_inputTextEdit_textChanged();

private:
  Ui::ImportKeyDialog *ui;
  QString m_importedKeyId;

  auto importFromString(const QString &input) -> bool;
  void showError(const QString &message);
  void showSuccess(const QString &keyId);
};
#endif // SRC_IMPORTKEYDIALOG_H_