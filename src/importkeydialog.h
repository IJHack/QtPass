// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Dialog that imports an ASCII-armored GPG public key from a file or
 * the clipboard by running `gpg --import`.
 */
#ifndef SRC_IMPORTKEYDIALOG_H_
#define SRC_IMPORTKEYDIALOG_H_

#include <QDialog>
#include <QScopedPointer>
#include <QString>

namespace Ui {
class ImportKeyDialog;
}

/**
 * @class ImportKeyDialog
 * @ingroup keys
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
   * @param gpgExe Path to the gpg executable (falls back to "gpg" if empty).
   * @param parent Optional parent widget.
   */
  explicit ImportKeyDialog(const QString &gpgExe, QWidget *parent = nullptr);
  /** @brief Destroys the dialog and its generated UI. */
  ~ImportKeyDialog() override;

  /**
   * @brief Get the imported key id on success.
   * @return The imported key id (or fingerprint when GnuPG status output
   *         provides one), or empty if import failed.
   */
  auto importedKeyId() const -> QString;

  /**
   * @brief Parse GPG import output to extract a key id or fingerprint.
   *
   * Prefers locale-independent status lines (\c [GNUPG:] IMPORT_OK and
   * \c [GNUPG:] IMPORTED) when present, then falls back to the
   * human-readable \c "gpg: key ...: imported" line (English locale only).
   *
   * @param output Combined stdout/stderr from `gpg --import --status-fd 1`.
   * @return The imported key id or fingerprint, or empty if not found.
   */
  static auto parseGpgImportOutput(const QString &output) -> QString;

private slots:
  /**
   * @brief "From file" button: picks a file, reads it as a regular file of at
   * most 16 MiB, requires the `-----BEGIN PGP` armor header and loads the
   * text into the input field. Binary keyrings are refused with a hint to
   * `gpg --armor --export` first.
   */
  void on_fileButton_clicked();

  /**
   * @brief "From clipboard" button: replaces the input field with the
   * clipboard text, unless the clipboard is empty.
   */
  void on_pasteButton_clicked();

  /**
   * @brief "Import" button: runs importFromString() on the trimmed input and
   * closes the dialog with `accept()` on success.
   */
  void on_importButton_clicked();

  /**
   * @brief Enables the Import button only while the input field holds
   * non-blank text.
   */
  void on_inputTextEdit_textChanged();

private:
  /** @brief Generated form (`importkeydialog.ui`); owned. */
  QScopedPointer<Ui::ImportKeyDialog> ui;
  /** @brief Key id or fingerprint of the last successful import; empty until
   * then. Read back through importedKeyId(). */
  QString m_importedKeyId;
  /** @brief Path of the gpg binary to run; empty means plain `gpg` on PATH. */
  QString m_gpgExe;

  /**
   * @brief Feeds @p input to `gpg --status-fd 1 --import --batch --yes` via
   * Executor::executeBlocking() and records the imported key id.
   * @param input Armored key text, trimmed.
   * @return true when gpg exited with 0 and a key id could be parsed from its
   *         output; false after showError() otherwise.
   */
  auto importFromString(const QString &input) -> bool;
  /**
   * @brief Shows @p message in a plain-text warning box (gpg's stderr quotes
   * user IDs, which must not be read as markup).
   * @param message The error text.
   */
  void showError(const QString &message);
  /**
   * @brief Shows the "imported, now verify the fingerprint" information box.
   * @param keyId The imported key id or fingerprint.
   */
  void showSuccess(const QString &keyId);
};
#endif // SRC_IMPORTKEYDIALOG_H_
