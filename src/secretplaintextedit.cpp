// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Implements SecretPlainTextEdit::createMimeDataFromSelection(), the
 * single hook through which all copies leave the editor.
 */
#include "secretplaintextedit.h"
#include "clipboardmanager.h"
#include <QTextCursor>
#include <QTextDocumentFragment>

SecretPlainTextEdit::SecretPlainTextEdit(QWidget *parent)
    : QPlainTextEdit(parent) {}

auto SecretPlainTextEdit::createMimeDataFromSelection() const -> QMimeData * {
  const QString text = textCursor().selection().toPlainText();
  // copied() is a notification, not a change to this widget; the signal is
  // declared non-const as all signals are.
  emit const_cast<SecretPlainTextEdit *>(this)->copied(text);
  return buildClipboardMimeData(text);
}
