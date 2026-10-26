// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "secretlineedit.h"
#include "qtpass.h"
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>

SecretLineEdit::SecretLineEdit(QWidget *parent) : QLineEdit(parent) {}

auto SecretLineEdit::createSecretContextMenu() -> QMenu * {
  QMenu *menu = createStandardContextMenu();
  for (QAction *action : menu->actions()) {
    if (action->objectName() == QLatin1String("edit-copy")) {
      // The standard action calls QLineEdit::copy(), which is not virtual.
      disconnect(action, &QAction::triggered, this, nullptr);
      connect(action, &QAction::triggered, this,
              [this] { copySelection(QClipboard::Clipboard); });
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
  withoutPlainSelectionCopy([this, event] { QLineEdit::keyPressEvent(event); });
  // Where QLineEdit would have copied: after a key it handled, not after a
  // lone modifier or another key it ignores.
  if (event->isAccepted()) {
    publishSelection();
  }
}

void SecretLineEdit::mouseReleaseEvent(QMouseEvent *event) {
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

void SecretLineEdit::withoutPlainSelectionCopy(
    const std::function<void()> &handler) {
  // QLineEdit fills the primary selection with plain text itself, at the end
  // of every key it handles and on a left-button release, but only in Normal
  // echo mode (QWidgetLineControl::copy()). Replacing that afterwards still
  // leaves a moment in which a clipboard manager could take the unmarked
  // text, so the base handler runs in Password mode instead: same length,
  // so cursor and selection stay where they are, and no copy.
  if (echoMode() != QLineEdit::Normal) {
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
