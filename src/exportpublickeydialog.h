// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_EXPORTPUBLICKEYDIALOG_H_
#define SRC_EXPORTPUBLICKEYDIALOG_H_

#include <QDialog>
#include <QString>

namespace Ui {
class ExportPublicKeyDialog;
}

/**
 * @class ExportPublicKeyDialog
 * @brief Dialog showing an ASCII-armored public GPG key for sharing.
 *
 * Displays the armored key text and offers Copy-to-Clipboard and
 * Save-to-File actions so the user can hand the key to teammates.
 */
class ExportPublicKeyDialog : public QDialog {
  Q_OBJECT

public:
  /**
   * @brief Construct an ExportPublicKeyDialog.
   * @param keyId GPG key identifier or fingerprint shown in the header
   *              and used as the default file name when saving.
   * @param armoredKey ASCII-armored public key text to display.
   * @param parent Optional parent widget.
   */
  explicit ExportPublicKeyDialog(const QString &keyId,
                                 const QString &armoredKey,
                                 QWidget *parent = nullptr);
  ~ExportPublicKeyDialog() override;

  /**
   * @brief Reduce a key identifier to a filesystem-safe token.
   * @param keyId Raw identifier, possibly containing whitespace-separated
   *              IDs or characters not valid in a filename.
   * @return The first whitespace-separated token of keyId with anything
   *         outside [A-Za-z0-9_-] stripped. Empty if no characters survive.
   */
  static auto sanitizeKeyIdForFilename(const QString &keyId) -> QString;

private slots:
  /**
   * @brief Copy the armored key text to the system clipboard.
   */
  void on_copyButton_clicked();

  /**
   * @brief Prompt for a destination and write the armored key to that file.
   */
  void on_saveButton_clicked();

private:
  Ui::ExportPublicKeyDialog *ui;
  QString m_keyId;
  QString m_copyButtonOriginalText;
};

#endif // SRC_EXPORTPUBLICKEYDIALOG_H_
