// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Dialog that shows an ASCII-armored public key and lets the user copy
 * it or save it as a `.asc` file.
 */
#ifndef SRC_EXPORTPUBLICKEYDIALOG_H_
#define SRC_EXPORTPUBLICKEYDIALOG_H_

#include <QDialog>
#include <QScopedPointer>
#include <QString>

namespace Ui {
class ExportPublicKeyDialog;
}

/**
 * @class ExportPublicKeyDialog
 * @ingroup keys
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
  /** @brief Destroys the dialog and its generated UI. */
  ~ExportPublicKeyDialog() override;

  /**
   * @brief Reduce a key identifier to a filesystem-safe token.
   * @param keyId Raw identifier, possibly containing whitespace-separated
   *              IDs or characters not valid in a filename.
   * @return The first whitespace-separated token of keyId with anything
   *         outside [A-Za-z0-9_-] stripped. Empty if no characters survive.
   */
  static auto sanitizeKeyIdForFilename(const QString &keyId) -> QString;

  /**
   * @brief Write the armored key to @p fileName, refusing a symbolic link
   * there.
   *
   * QSaveFile writes to the target of a link at the chosen name. A folder in
   * a shared store can hold such a link, pushed by a co-writer under the name
   * the dialog proposes (the key's fingerprint), so the key would land
   * outside the store. A dangling link counts too.
   * @param fileName The chosen file.
   * @param text The armored key.
   * @param error Set to a message for the user when the save fails.
   * @return Whether the key was written.
   */
  static auto saveArmoredKey(const QString &fileName, const QString &text,
                             QString *error) -> bool;

private slots:
  /**
   * @brief Copy the armored key text to the system clipboard; the button
   * reads "Copied!" for 1.5 s as feedback. Public keys are not secrets, so
   * this bypasses ClipboardManager and its autoclear.
   */
  void on_copyButton_clicked();

  /**
   * @brief Prompt for a destination and write the armored key to that file
   * through saveArmoredKey(). The suggested name is
   * `<sanitizeKeyIdForFilename(keyId)>.asc`, or `public_key.asc` when nothing
   * of the key id survives. Failures are reported in a warning box.
   */
  void on_saveButton_clicked();

private:
  /** @brief Generated form (`exportpublickeydialog.ui`); owned. */
  QScopedPointer<Ui::ExportPublicKeyDialog> ui;
  /** @brief Key id as passed to the constructor; the basis of the suggested
   * file name. */
  QString m_keyId;
  /** @brief The copy button's label from the form, restored after the
   * "Copied!" flash. */
  QString m_copyButtonOriginalText;
};

#endif // SRC_EXPORTPUBLICKEYDIALOG_H_
