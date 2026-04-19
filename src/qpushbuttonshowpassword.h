// SPDX-FileCopyrightText: 2020 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_QPUSHBUTTONSHOWPASSWORD_H_
#define SRC_QPUSHBUTTONSHOWPASSWORD_H_

#include <QLineEdit>
#include <QPushButton>

/**
 * @class QPushButtonShowPassword
 * @brief QPushButton that toggles visibility of an associated QLineEdit.
 */
class QWidget;
class QPushButtonShowPassword : public QPushButton {
  Q_OBJECT

public:
  /**
   * @brief Construct a show-password button linked to the given QLineEdit.
   * @param line QLineEdit whose contents are shown or hidden by this button.
   * @param parent Optional parent widget.
   */
  explicit QPushButtonShowPassword(QLineEdit *line, QWidget *parent = nullptr);

signals:
  /**
   * @brief Emitted on button activation with the current text of the line edit.
   * @param text Current text of the associated QLineEdit.
   */
  void clicked(const QString &text);

private slots:
  void buttonClicked(bool);

private:
  QIcon iconEdit;
  QIcon iconEditPushed;
  QLineEdit *line;
};

#endif // SRC_QPUSHBUTTONSHOWPASSWORD_H_
