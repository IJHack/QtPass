// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "plainmessagebox.h"

auto PlainMessageBox::show(QWidget *parent, QMessageBox::Icon icon,
                           const QString &title, const QString &text,
                           QMessageBox::StandardButtons buttons)
    -> QMessageBox::StandardButton {
  QMessageBox box(parent);
  box.setIcon(icon);
  box.setWindowTitle(title);
  // Before the text: set afterwards, the text would already have been laid
  // out as rich text once.
  box.setTextFormat(Qt::PlainText);
  box.setText(text);
  box.setStandardButtons(buttons);
  return static_cast<QMessageBox::StandardButton>(box.exec());
}

auto PlainMessageBox::askYes(QWidget *parent, const QString &title,
                             const QString &text) -> bool {
  return show(parent, QMessageBox::Question, title, text,
              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}

void PlainMessageBox::warning(QWidget *parent, const QString &title,
                              const QString &text) {
  show(parent, QMessageBox::Warning, title, text);
}

void PlainMessageBox::critical(QWidget *parent, const QString &title,
                               const QString &text) {
  show(parent, QMessageBox::Critical, title, text);
}
