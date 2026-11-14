// SPDX-FileCopyrightText: 2020 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Eye-icon button that toggles a QLineEdit between password and
 * plain-text echo mode.
 */
#ifndef SRC_QPUSHBUTTONSHOWPASSWORD_H_
#define SRC_QPUSHBUTTONSHOWPASSWORD_H_

#include <QLineEdit>
#include <QPushButton>

class QWidget;
/**
 * @class QPushButtonShowPassword
 * @brief QPushButton that toggles visibility of an associated QLineEdit.
 *
 * The button is icon-only and bound to one QLineEdit for its lifetime. Each
 * click flips that line edit between `QLineEdit::Password` and
 * `QLineEdit::Normal` echo mode and swaps the icon, tooltip and accessible
 * name between "Show password" and "Hide password" to match. It is driven
 * entirely by its own `QPushButton::clicked` signal; the owner only has to
 * create it next to the line edit (`PasswordDisplayPanel` does this for the
 * masked password row of a decrypted entry).
 */
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
  /**
   * @brief Connected to `QPushButton::clicked(bool)`; flips the echo mode of
   * @ref line and updates icon, tooltip and accessible name accordingly.
   */
  void buttonClicked(bool);

private:
  /** @brief `view-visible` icon shown while the password is hidden. */
  QIcon iconEdit;
  /** @brief `view-hidden` icon shown while the password is revealed. */
  QIcon iconEditPushed;
  /** @brief The line edit being toggled; owned by the caller, must outlive
   * this button. */
  QLineEdit *line;
};

#endif // SRC_QPUSHBUTTONSHOWPASSWORD_H_
