// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief QPlainTextEdit for editing decrypted entry text that marks everything
 * copied out of it as a secret.
 */
#ifndef SRC_SECRETPLAINTEXTEDIT_H_
#define SRC_SECRETPLAINTEXTEDIT_H_

#include <QPlainTextEdit>

/**
 * @class SecretPlainTextEdit
 * @ingroup secrets
 * @brief A QPlainTextEdit for decrypted entry text being edited: whatever is
 * copied or cut out of it is marked as a secret and reported for autoclear.
 *
 * As in SecretTextBrowser, every way text leaves the editor (Ctrl+C, Ctrl+X,
 * the context menu, a drag, and the X11 primary selection a mouse selection
 * fills) goes through createMimeDataFromSelection().
 */
class SecretPlainTextEdit : public QPlainTextEdit {
  Q_OBJECT

public:
  /**
   * @brief Constructs the editor.
   * @param parent Owner.
   */
  explicit SecretPlainTextEdit(QWidget *parent = nullptr);

signals:
  /**
   * @brief Selected text was copied or cut out of the editor.
   * @param text The text, as put on the clipboard, in the selection or in
   * the drag.
   */
  void copied(const QString &text);

protected:
  /**
   * @brief The selection as secret-marked plain text.
   * @return MIME data from buildClipboardMimeData(); the caller owns it.
   */
  [[nodiscard]] auto createMimeDataFromSelection() const
      -> QMimeData * override;
};

#endif // SRC_SECRETPLAINTEXTEDIT_H_
