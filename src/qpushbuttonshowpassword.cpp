// SPDX-FileCopyrightText: 2020 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Implements QPushButtonShowPassword: the echo-mode toggle and the
 * matching icon and tooltip swap.
 */
#include "qpushbuttonshowpassword.h"
#include <QTimer>

QPushButtonShowPassword::QPushButtonShowPassword(QLineEdit *line,
                                                 QWidget *parent)
    : QPushButton(parent),
      iconEdit(QIcon::fromTheme("view-visible", QIcon(":/icons/view.svg"))),
      iconEditPushed(
          QIcon::fromTheme("view-hidden", QIcon(":/icons/hide.svg"))) {
  setIcon(iconEdit);
  // Icon-only: give screen readers and hovering users a name.
  setToolTip(tr("Show password"));
  setAccessibleName(tr("Show password"));
  setForegroundRole(QPalette::ButtonText);
  connect(this, &QPushButton::clicked, this,
          &QPushButtonShowPassword::buttonClicked);
  this->line = line;
}

void QPushButtonShowPassword::buttonClicked(bool /*unused*/) {
  if (this->line->echoMode() == QLineEdit::Password) {
    this->line->setEchoMode(QLineEdit::Normal);
    setIcon(iconEditPushed);
    setToolTip(tr("Hide password"));
    setAccessibleName(tr("Hide password"));
  } else {
    this->line->setEchoMode(QLineEdit::Password);
    setIcon(iconEdit);
    setToolTip(tr("Show password"));
    setAccessibleName(tr("Show password"));
  }
}
