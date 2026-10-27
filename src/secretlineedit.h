// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_SECRETLINEEDIT_H_
#define SRC_SECRETLINEEDIT_H_

#include <QClipboard>
#include <QLineEdit>
#include <functional>

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
 * buildClipboardMimeData() on the clipboard instead, and the key and mouse
 * handlers keep QLineEdit from filling the primary selection at all (it runs
 * them in Password echo mode) and fill it with the marked data instead, so
 * plain text never gets there, not even for a moment. Each copy emits copied()
 * so the text is cleared with the rest when the autoclear timer runs. Dragging
 * is off, as QLineEdit has it by default.
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
   *
   * The Copy action is named "edit-copy" on every Qt version (Qt 6.7 and
   * later name it so themselves).
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
   * @brief Ctrl+C copies with the hints; any other key is handled without
   * QLineEdit's plain-text copy to the primary selection, which then gets
   * the marked selection.
   * @param event The key event.
   */
  void keyPressEvent(QKeyEvent *event) override;

  /**
   * @brief Fills the primary selection with the marked selection on a
   * left-button release, instead of QLineEdit's plain-text copy.
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
  void withoutPlainSelectionCopy(const std::function<void()> &handler);
  void publishSelection();
};

#endif // SRC_SECRETLINEEDIT_H_
