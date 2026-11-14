// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Icon button that asks for a stored secret to be shown as a QR code.
 */
#ifndef SRC_QPUSHBUTTONASQRCODE_H_
#define SRC_QPUSHBUTTONASQRCODE_H_

#include <QPushButton>

/**
 * @class QPushButtonAsQRCode
 * @ingroup secrets
 * @brief QPushButton that presents a text payload as a QR code action.
 *
 * The button is icon-only (theme icon `qrcode`) and carries the text of the
 * field it sits next to. It does not render the QR code itself: on activation
 * it re-emits `clicked(const QString&)` with the stored text. The owning
 * PasswordDisplayPanel passes that on, and QtPass::showTextAsQRCode() opens
 * the QR dialog.
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
  /**
   * @brief Connected to `QPushButton::clicked(bool)`; re-emits the overload
   * carrying the stored text so receivers do not need to look it up.
   * @param checked Unused; the button is not checkable.
   */
  void buttonClicked(bool checked);

private:
  /** @brief Text to be rendered as a QR code; set by the owning panel. */
  QString textToCopy;
  /** @brief The `qrcode` icon shown on the button. */
  QIcon iconEdit;
};

#endif // SRC_QPUSHBUTTONASQRCODE_H_
