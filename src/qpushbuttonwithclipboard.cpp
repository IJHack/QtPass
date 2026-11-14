// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Implements QPushButtonWithClipboard: click re-emit and the 500 ms
 * icon flash.
 */
#include "qpushbuttonwithclipboard.h"
#include <QTimer>
#include <utility>

QPushButtonWithClipboard::QPushButtonWithClipboard(QString textToCopy,
                                                   QWidget *parent)
    : QPushButton(parent), textToCopy(std::move(textToCopy)),
      iconEdit(QIcon::fromTheme("edit-copy", QIcon(":/icons/edit-copy.svg"))),
      iconEditPushed(
          QIcon::fromTheme("document-new", QIcon(":/icons/document-new.svg"))) {
  setIcon(iconEdit);
  // Icon-only: give screen readers and hovering users a name.
  setToolTip(tr("Copy to clipboard"));
  setAccessibleName(tr("Copy to clipboard"));
  setForegroundRole(QPalette::ButtonText);
  connect(this, &QPushButton::clicked, this,
          &QPushButtonWithClipboard::buttonClicked);
}

auto QPushButtonWithClipboard::getTextToCopy() const -> QString {
  return textToCopy;
}

void QPushButtonWithClipboard::setTextToCopy(const QString &value) {
  textToCopy = value;
}

void QPushButtonWithClipboard::buttonClicked(bool /*unused*/) {
  setIcon(iconEditPushed);
  QTimer::singleShot(500, this, &QPushButtonWithClipboard::changeIconDefault);
  emit clicked(textToCopy);
}

void QPushButtonWithClipboard::changeIconDefault() { setIcon(iconEdit); }
