// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_QPUSHBUTTONASQRCODE_H_
#define SRC_QPUSHBUTTONASQRCODE_H_

#include <QPushButton>

/*!
    \class QPushButtonAsQRCode
    \brief Stylish widget to display the field as QR Code
*/
class QWidget;
/**
 * Stylish QPushButton that represents a field as a QR code and holds a text
 * payload to copy or transmit.
 */

/**
 * Create a QPushButton configured to present text as a QR-code-related action.
 * @param textToCopy Initial text stored for copying or QR-code use (may be
 * empty).
 * @param parent Optional parent widget.
 */

/**
 * Return the stored text used for copy or QR-code actions.
 * @returns The current stored text.
 */

/**
 * Update the stored text used for copy or QR-code actions.
 * @param value New text to store.
 */

/**
 * Emitted when the button is activated, delivering the current text payload.
 * @param text The current stored text.
 */

/**
 * Reset the button's icon to its default appearance.
 */

/**
 * Handle a change/click event reported as a boolean state.
 * @param checked True if the button is in the checked/active state, false
 * otherwise.
 */
class QPushButtonAsQRCode : public QPushButton {
  Q_OBJECT

public:
  explicit QPushButtonAsQRCode(QString textToCopy = "",
                               QWidget *parent = nullptr);

  [[nodiscard]] auto getTextToCopy() const -> QString;
  void setTextToCopy(const QString &value);

signals:
  void clicked(const QString &);

private:
  void buttonClicked(bool);

private:
  QString textToCopy;
  QIcon iconEdit;
};

#endif // SRC_QPUSHBUTTONASQRCODE_H_
