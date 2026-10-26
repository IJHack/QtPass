// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "secretlineedit.h"
#include "clipboardmanager.h"
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
  QLineEdit::keyPressEvent(event);
  reclaimPrimarySelection();
}

void SecretLineEdit::mouseReleaseEvent(QMouseEvent *event) {
  QLineEdit::mouseReleaseEvent(event);
  if (event->button() == Qt::LeftButton) {
    reclaimPrimarySelection();
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

void SecretLineEdit::reclaimPrimarySelection() {
  QClipboard *clipboard = QApplication::clipboard();
  // Only what QLineEdit just put there: its own plain-text copy of the
  // selection, which this replaces with the marked one.
  if (clipboard->supportsSelection() && hasSelectedText() &&
      clipboard->text(QClipboard::Selection) == selectedText()) {
    copySelection(QClipboard::Selection);
  }
}
