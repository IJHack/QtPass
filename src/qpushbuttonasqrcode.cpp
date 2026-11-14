// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Implements QPushButtonAsQRCode: icon setup and the click re-emit.
 */
#include "qpushbuttonasqrcode.h"
#include <QTimer>
#include <utility>

QPushButtonAsQRCode::QPushButtonAsQRCode(QString textToCopy, QWidget *parent)
    : QPushButton(parent), textToCopy(std::move(textToCopy)),
      iconEdit(QIcon::fromTheme("qrcode", QIcon(":/icons/qrcode.svg"))) {
  setIcon(iconEdit);
  // Icon-only: give screen readers and hovering users a name.
  setToolTip(tr("Show as QR code"));
  setAccessibleName(tr("Show as QR code"));
  setForegroundRole(QPalette::ButtonText);
  connect(this, &QPushButton::clicked, this,
          &QPushButtonAsQRCode::buttonClicked);
}

auto QPushButtonAsQRCode::getTextToCopy() const -> QString {
  return textToCopy;
}

void QPushButtonAsQRCode::setTextToCopy(const QString &value) {
  textToCopy = value;
}

void QPushButtonAsQRCode::buttonClicked(bool /*unused*/) {
  emit clicked(textToCopy);
}
