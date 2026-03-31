// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_QPUSHBUTTONWITHCLIPBOARD_H_
#define SRC_QPUSHBUTTONWITHCLIPBOARD_H_

#include <QPushButton>

/*!
    \class QPushButtonWithClipboard
    \brief Stylish widget to allow copying of password and account details
*/
class QWidget;
/**
 * A QPushButton subclass that stores a text payload and emits it when
 * activated.
 *
 * The button is intended for copying or emitting password/account details and
 * toggles between default and pushed icon states.
 */

/**
 * Construct a QPushButtonWithClipboard and initialize the stored text.
 * @param textToCopy Initial text to store and emit when the button is
 * activated.
 * @param parent Optional parent widget.
 */

/**
 * Retrieve the stored text used for copying or emission.
 * @returns QString containing the current stored text.
 */

/**
 * Update the stored text used for copying or emission.
 * @param value The new text to store.
 */

/**
 * Emitted when the button is activated with its associated text.
 * @param text The stored text associated with this button.
 */

/**
 * Restore the button's icon to its default appearance.
 */

/**
 * Handle the button's clicked/pressed state and emit the stored text when
 * appropriate.
 * @param checked Whether the button is in the pressed/checked state.
 */
class QPushButtonWithClipboard : public QPushButton {
  Q_OBJECT

public:
  explicit QPushButtonWithClipboard(QString textToCopy = "",
                                    QWidget *parent = nullptr);

  [[nodiscard]] auto getTextToCopy() const -> QString;
  void setTextToCopy(const QString &value);

signals:
  void clicked(const QString &);

private slots:
  void changeIconDefault();
  void buttonClicked(bool);

private:
  QString textToCopy;
  QIcon iconEdit;
  QIcon iconEditPushed;
};

#endif // SRC_QPUSHBUTTONWITHCLIPBOARD_H_
