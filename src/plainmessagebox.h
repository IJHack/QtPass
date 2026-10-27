// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_PLAINMESSAGEBOX_H_
#define SRC_PLAINMESSAGEBOX_H_

#include <QMessageBox>

/**
 * @brief Message boxes whose text is shown as the text it is.
 *
 * QMessageBox's static helpers read their text as rich text when it looks
 * like it (Qt::AutoText). A message quoting a name from the store would then
 * be markup that other writers of a shared store control: a folder named
 * `<img src="` with `dev/zero">.gpg` beneath it has Qt read /dev/zero
 * without end. These set Qt::PlainText before the text, for every message
 * that quotes a path, a field or anything else from the store.
 */
namespace PlainMessageBox {

/**
 * @brief Show a modal message box with plain text and wait for it.
 * @param parent The parent widget.
 * @param icon The icon.
 * @param title The window title.
 * @param text The message, shown as it is.
 * @param buttons The buttons offered.
 * @return The button clicked.
 */
auto show(QWidget *parent, QMessageBox::Icon icon, const QString &title,
          const QString &text,
          QMessageBox::StandardButtons buttons = QMessageBox::Ok)
    -> QMessageBox::StandardButton;

/**
 * @brief Ask a yes/no question in plain text.
 * @param parent The parent widget.
 * @param title The window title.
 * @param text The question, shown as it is.
 * @return Whether Yes was clicked.
 */
auto askYes(QWidget *parent, const QString &title, const QString &text) -> bool;

/**
 * @brief Show a warning in plain text.
 * @param parent The parent widget.
 * @param title The window title.
 * @param text The warning, shown as it is.
 */
void warning(QWidget *parent, const QString &title, const QString &text);

/**
 * @brief Show an error in plain text.
 * @param parent The parent widget.
 * @param title The window title.
 * @param text The error, shown as it is.
 */
void critical(QWidget *parent, const QString &title, const QString &text);

} // namespace PlainMessageBox

#endif // SRC_PLAINMESSAGEBOX_H_
