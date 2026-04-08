// SPDX-FileCopyrightText: 2020 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_QPUSHBUTTONSHOWPASSWORD_H_
#define SRC_QPUSHBUTTONSHOWPASSWORD_H_

#include <QLineEdit>
#include <QPushButton>

/*!
    \class QPushButtonAsQRCode
    \brief Stylish widget to display the field as QR Code
*/
class QWidget;
/**
 * QPushButton that controls visibility of the associated QLineEdit's contents
 * (e.g., show/hide password).
 *
 * @param line QLineEdit whose contents are shown or hidden by this button.
 * @param parent Optional parent widget.
 */
/**
 * Emitted when the button action occurs, carrying the current text of the
 * associated QLineEdit.
 *
 * @param The current text of the associated QLineEdit.
 */
/**
 * Handles the button activation state and updates the associated QLineEdit
 * accordingly.
 *
 * @param checked The new checked/toggled state of the button.
 */
class QPushButtonShowPassword : public QPushButton {
  Q_OBJECT

public:
  explicit QPushButtonShowPassword(QLineEdit *line, QWidget *parent = nullptr);

signals:
  void clicked(const QString &);

private slots:
  void buttonClicked(bool);

private:
  QIcon iconEdit;
  QIcon iconEditPushed;
  QLineEdit *line;
};

#endif // SRC_QPUSHBUTTONSHOWPASSWORD_H_
