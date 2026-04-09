// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "qpushbuttonasqrcode.h"
#include <QTimer>
#include <utility>

/**
 * @brief QPushButtonAsQRCode::QPushButtonAsQRCode
 *  basic constructor
 * @param textToCopy
 *  the text to display as qrcode
 * @param parent
 *  the parent window
 */
QPushButtonAsQRCode::QPushButtonAsQRCode(QString textToCopy, QWidget *parent)
    : QPushButton(parent), textToCopy(std::move(textToCopy)),
      iconEdit(QIcon::fromTheme("qrcode", QIcon(":/icons/qrcode.svg"))) {
  setIcon(iconEdit);
  setForegroundRole(QPalette::ButtonText);
  connect(this, &QPushButton::clicked, this,
          &QPushButtonAsQRCode::buttonClicked);
}

/**
 * @brief QPushButtonAsQRCode::getTextToCopy returns the text of
 * associated text field
 * @return QString textToCopy
 */
auto QPushButtonAsQRCode::getTextToCopy() const -> QString {
  return textToCopy;
}

/**
 * @brief QPushButtonAsQRCode::setTextToCopy sets text from associated
 * text field
 * @param value QString text to be copied
 */
void QPushButtonAsQRCode::setTextToCopy(const QString &value) {
  textToCopy = value;
}

/**
 * @brief QPushButtonAsQRCode::buttonClicked handles clicked event by
 * emitting clicked(QString) with string provided to constructor
 */
void QPushButtonAsQRCode::buttonClicked(bool /*unused*/) {
  emit clicked(textToCopy);
}
