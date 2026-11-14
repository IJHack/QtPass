// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief QLineEdit whose copies, cuts and primary-selection fills carry the
 * secret clipboard hints and are reported for autoclear.
 */
#ifndef SRC_SECRETLINEEDIT_H_
#define SRC_SECRETLINEEDIT_H_

#include <QClipboard>
#include <QLineEdit>
#include <functional>

class QMenu;

/**
 * @class SecretLineEdit
 * @ingroup secrets
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
 *
 * In an editable field (the password editor) Ctrl+X, Cut, and the keys
 * that delete to the end of the line or the whole line copy the same way
 * before they delete. Password mode would turn off undo, make Ctrl+arrow
 * jump to the ends and change the input method hints there, so the base
 * handlers run as they are and the marked selection replaces QLineEdit's
 * plain one at once, in the same event: X11 and Wayland hand selection data
 * over only when another program asks for it, which reaches QtPass through
 * its event loop, after the replacement.
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
   * @brief The standard context menu, with Copy and Cut going through the
   * secret copy.
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
   * @brief Ctrl+C copies with the hints, Ctrl+X as well before it deletes;
   * after any other key the primary selection gets the marked selection, not
   * QLineEdit's plain-text copy.
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
  /**
   * @brief Puts the selection on the clipboard or in the primary selection
   * as secret-marked MIME data and emits copied(). Does nothing in Password
   * echo mode or without a selection, as QLineEdit::copy() itself.
   * @param mode Clipboard for Ctrl+C and Copy, Selection for the X11 primary
   * selection.
   */
  void copySelection(QClipboard::Mode mode);
  /**
   * @brief Copies the selection through copySelection() and then deletes it.
   * Does nothing when read-only or without a selection, as QLineEdit::cut().
   */
  void cutSelection();
  /**
   * @brief Runs a base-class event handler so that it cannot fill the primary
   * selection with plain text: in a read-only field in Normal echo mode the
   * handler runs under Password mode, which makes QLineEdit skip the copy
   * without moving cursor or selection. Editable fields run the handler as is
   * and rely on publishSelection() replacing the plain copy at once.
   * @param handler The base handler call, e.g. `QLineEdit::keyPressEvent`.
   */
  void withoutPlainSelectionCopy(const std::function<void()> &handler);
  /**
   * @brief Fills the X11 primary selection with the marked selection, on
   * platforms that have one; called after a handled key or a left-button
   * release, where QLineEdit would have copied plain text.
   */
  void publishSelection();
};

#endif // SRC_SECRETLINEEDIT_H_
