// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_SECRETLINEEDIT_H_
#define SRC_SECRETLINEEDIT_H_

#include <QClipboard>
#include <QLineEdit>

class QMenu;

/**
 * @class SecretLineEdit
 * @brief A QLineEdit for a password that can be shown: whatever is copied out
 * of it is marked as a secret and reported for autoclear.
 *
 * QLineEdit copies nothing in Password echo mode, but once a "show" button
 * switches it to Normal it copies plain text, without the hints, on four
 * paths: Ctrl+C, the context menu's Copy, and the X11 primary selection after
 * a mouse selection or a key. Unlike QTextEdit it has no
 * createMimeDataFromSelection() to hook. Here Ctrl+C and Copy put
 * buildClipboardMimeData() on the clipboard instead, so plain text never gets
 * there; the primary selection, which QLineEdit fills itself, is replaced by
 * the same data before the event returns. Each copy emits copied() so the
 * text is cleared with the rest when the autoclear timer runs. Dragging is
 * off, as QLineEdit has it by default.
 */
class SecretLineEdit : public QLineEdit {
  Q_OBJECT

public:
  /**
   * @brief Constructs the line edit.
   * @param parent Owner.
   */
  explicit SecretLineEdit(QWidget *parent = nullptr);

  /**
   * @brief The standard context menu, with Copy going through the secret copy.
   * @return A new menu; the caller owns it.
   */
  [[nodiscard]] auto createSecretContextMenu() -> QMenu *;

signals:
  /**
   * @brief Selected text was copied out of the line edit.
   * @param text The text, as put on the clipboard or in the selection.
   */
  void copied(const QString &text);

protected:
  /**
   * @brief Ctrl+C copies with the hints; after any other key, the primary
   * selection QLineEdit may have filled is replaced.
   * @param event The key event.
   */
  void keyPressEvent(QKeyEvent *event) override;

  /**
   * @brief Replaces the primary selection QLineEdit fills on release.
   * @param event The mouse event.
   */
  void mouseReleaseEvent(QMouseEvent *event) override;

  /**
   * @brief Shows createSecretContextMenu().
   * @param event The context menu event.
   */
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  void copySelection(QClipboard::Mode mode);
  void reclaimPrimarySelection();
};

#endif // SRC_SECRETLINEEDIT_H_
