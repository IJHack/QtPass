// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_QPUSHBUTTONWITHCLIPBOARD_H_
#define SRC_QPUSHBUTTONWITHCLIPBOARD_H_

#include <QPushButton>

/**
 * @class QPushButtonWithClipboard
 * @brief QPushButton that stores a text payload and emits it when activated.
 */
class QWidget;
class QPushButtonWithClipboard : public QPushButton {
  Q_OBJECT

public:
  /**
   * @brief Construct a QPushButtonWithClipboard with an optional initial text.
   * @param textToCopy Initial text to store and emit on activation.
   * @param parent Optional parent widget.
   */
  explicit QPushButtonWithClipboard(QString textToCopy = "",
                                    QWidget *parent = nullptr);

  /**
   * @brief Return the stored text used for copying.
   * @return The current stored text.
   */
  [[nodiscard]] auto getTextToCopy() const -> QString;
  /**
   * @brief Update the stored text used for copying.
   * @param value The new text to store.
   */
  void setTextToCopy(const QString &value);

signals:
  /**
   * @brief Emitted when the button is activated with the stored text.
   * @param text The stored text associated with this button.
   */
  void clicked(const QString &text);

private slots:
  void changeIconDefault();
  void buttonClicked(bool);

private:
  QString textToCopy;
  QIcon iconEdit;
  QIcon iconEditPushed;
};

#endif // SRC_QPUSHBUTTONWITHCLIPBOARD_H_
