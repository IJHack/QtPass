// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_QPUSHBUTTONASQRCODE_H_
#define SRC_QPUSHBUTTONASQRCODE_H_

#include <QPushButton>

/**
 * @class QPushButtonAsQRCode
 * @brief QPushButton that presents a text payload as a QR code action.
 */
class QWidget;
class QPushButtonAsQRCode : public QPushButton {
  Q_OBJECT

public:
  /**
   * @brief Construct a QPushButtonAsQRCode with an optional initial text.
   * @param textToCopy Initial text stored for QR-code use (may be empty).
   * @param parent Optional parent widget.
   */
  explicit QPushButtonAsQRCode(QString textToCopy = "",
                               QWidget *parent = nullptr);

  /**
   * @brief Return the stored text used for QR-code actions.
   * @return The current stored text.
   */
  [[nodiscard]] auto getTextToCopy() const -> QString;
  /**
   * @brief Update the stored text used for QR-code actions.
   * @param value New text to store.
   */
  void setTextToCopy(const QString &value);

signals:
  /**
   * @brief Emitted when the button is activated with the current text payload.
   * @param text The stored text.
   */
  void clicked(const QString &text);

private:
  void buttonClicked(bool);

private:
  QString textToCopy;
  QIcon iconEdit;
};

#endif // SRC_QPUSHBUTTONASQRCODE_H_
