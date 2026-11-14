// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Implements SecretLineEdit: the four copy paths (Ctrl+C, Cut, context
 * menu, X11 primary selection) routed through buildClipboardMimeData().
 */
#include "secretlineedit.h"
#include "clipboardmanager.h"
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QValidator>

namespace {
/**
 * @brief Keeps a line edit to the one line it stands for.
 *
 * QLineEdit takes the CR and LF of a paste, a drop or an input method as they
 * come, draws them as blanks and returns them from text(). In the password
 * dialog a field's line break then ends its line in the entry: a password
 * pasted as "\nhunter2" was saved as an empty first line. QLineEdit runs its
 * validator on every change before it reports the new text, and validate()
 * may correct what it is given.
 */
class SingleLineValidator : public QValidator {
public:
  using QValidator::QValidator;

  /**
   * @brief Remove CR and LF from @p input.
   * @param input The text the field would hold; corrected in place.
   * @param pos The cursor position; kept behind the character it was behind.
   * @return Always Acceptable: every text is one line once corrected.
   */
  auto validate(QString &input, int &pos) const -> State override {
    if (!input.contains(u'\n') && !input.contains(u'\r')) {
      return Acceptable;
    }
    const QStringView head = QStringView(input).left(pos);
    pos -= static_cast<int>(head.count(u'\n') + head.count(u'\r'));
    input.remove(u'\n').remove(u'\r');
    return Acceptable;
  }
};
} // namespace

SecretLineEdit::SecretLineEdit(QWidget *parent) : QLineEdit(parent) {
  setValidator(new SingleLineValidator(this));
}

auto SecretLineEdit::createSecretContextMenu() -> QMenu * {
  QMenu *menu = createStandardContextMenu();
  for (QAction *action : menu->actions()) {
    // The standard actions call QLineEdit::copy() and cut(), which are not
    // virtual.
    if (action->objectName() == QLatin1String("edit-copy")) {
      disconnect(action, &QAction::triggered, this, nullptr);
      connect(action, &QAction::triggered, this,
              [this] { copySelection(QClipboard::Clipboard); });
    } else if (action->objectName() == QLatin1String("edit-cut")) {
      disconnect(action, &QAction::triggered, this, nullptr);
      connect(action, &QAction::triggered, this, [this] { cutSelection(); });
    } else if (action->objectName() == QLatin1String("edit-paste")) {
      disconnect(action, &QAction::triggered, this, nullptr);
      connect(action, &QAction::triggered, this,
              [this] { pasteOneLine(QClipboard::Clipboard); });
    }
  }
  return menu;
}

void SecretLineEdit::keyPressEvent(QKeyEvent *event) {
  if (event->matches(QKeySequence::Copy)) {
    copySelection(QClipboard::Clipboard);
    event->accept();
    return;
  }
  if (event->matches(QKeySequence::Cut)) {
    cutSelection();
    event->accept();
    return;
  }
  if (event->matches(QKeySequence::Paste)) {
    pasteOneLine(QClipboard::Clipboard);
    event->accept();
    return;
  }
  // QLineEdit copies what these delete (Ctrl+K and Ctrl+U with the X11 key
  // bindings), as a cut.
  const bool toEnd = event->matches(QKeySequence::DeleteEndOfLine);
  if (toEnd || event->matches(QKeySequence::DeleteCompleteLine)) {
    if (!isReadOnly()) {
      const int from = toEnd ? cursorPosition() : 0;
      setSelection(from, static_cast<int>(text().size()) - from);
      cutSelection();
    }
    event->accept();
    return;
  }
  withoutPlainSelectionCopy([this, event] { QLineEdit::keyPressEvent(event); });
  // Where QLineEdit would have copied: after a key it handled, not after a
  // lone modifier or another key it ignores.
  if (event->isAccepted()) {
    publishSelection();
  }
}

void SecretLineEdit::mouseReleaseEvent(QMouseEvent *event) {
  // QLineEdit pastes the primary selection here on a middle click.
  if (event->button() == Qt::MiddleButton && !isReadOnly() &&
      QApplication::clipboard()->supportsSelection()) {
    deselect();
    pasteOneLine(QClipboard::Selection);
    event->accept();
    return;
  }
  withoutPlainSelectionCopy(
      [this, event] { QLineEdit::mouseReleaseEvent(event); });
  if (event->button() == Qt::LeftButton) {
    publishSelection();
  }
}

void SecretLineEdit::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu = createSecretContextMenu();
  menu->setAttribute(Qt::WA_DeleteOnClose);
  menu->popup(event->globalPos());
}

void SecretLineEdit::copySelection(QClipboard::Mode mode) {
  // As QLineEdit itself: nothing leaves a field that shows dots.
  if (echoMode() != QLineEdit::Normal || !hasSelectedText()) {
    return;
  }
  const QString text = selectedText();
  QApplication::clipboard()->setMimeData(buildClipboardMimeData(text), mode);
  emit copied(text);
}

void SecretLineEdit::cutSelection() {
  // As QLineEdit::cut(): a field that shows dots deletes without copying.
  if (isReadOnly() || !hasSelectedText()) {
    return;
  }
  copySelection(QClipboard::Clipboard);
  del();
}

void SecretLineEdit::withoutPlainSelectionCopy(
    const std::function<void()> &handler) {
  // QLineEdit fills the primary selection with plain text itself, at the end
  // of every key it handles and on a left-button release, but only in Normal
  // echo mode (QWidgetLineControl::copy()). Replacing that afterwards still
  // leaves a moment in which a clipboard manager could take the unmarked
  // text, so the base handler runs in Password mode instead: same length,
  // so cursor and selection stay where they are, and no copy.
  // Editable, the field keeps Normal mode (see the class description) and
  // publishSelection() replaces the plain copy.
  if (echoMode() != QLineEdit::Normal || !isReadOnly()) {
    handler();
    return;
  }
  setEchoMode(QLineEdit::Password);
  handler();
  setEchoMode(QLineEdit::Normal);
}

void SecretLineEdit::publishSelection() {
  if (QApplication::clipboard()->supportsSelection()) {
    copySelection(QClipboard::Selection);
  }
}

void SecretLineEdit::pasteOneLine(QClipboard::Mode mode) {
  if (isReadOnly()) {
    return;
  }
  QString text = QApplication::clipboard()->text(mode);
  text.remove(u'\n').remove(u'\r');
  // insert() replaces the selection and records the edit for Undo; with no
  // line break left the validator changes nothing, so the history stays.
  insert(text);
}
