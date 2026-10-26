// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "secrettextbrowser.h"
#include "clipboardmanager.h"
#include <QTextDocumentFragment>

SecretTextBrowser::SecretTextBrowser(QWidget *parent) : QTextBrowser(parent) {}

auto SecretTextBrowser::createMimeDataFromSelection() const -> QMimeData * {
  // Plain text only: the default would also put the selection's HTML on the
  // clipboard, without the hints.
  const QString text = textCursor().selection().toPlainText();
  // copied() is a notification, not a change to this widget; the signal is
  // declared non-const as all signals are.
  emit const_cast<SecretTextBrowser *>(this)->copied(text);
  return buildClipboardMimeData(text);
}
