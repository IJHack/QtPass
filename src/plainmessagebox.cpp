// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Implements the PlainMessageBox helpers; the text format is set before
 * the text so it is never laid out as rich text.
 */
#include "plainmessagebox.h"
#include "util.h"

namespace {
/// Lines and characters of a message a box shows at most.
constexpr qsizetype kMaxLines = 40;
/// See kMaxLines.
constexpr qsizetype kMaxChars = 4000;
/// Characters of a cut message kept under "Show Details...".
constexpr qsizetype kMaxDetailChars = 256 * 1024;

/**
 * @brief @p text cut to kMaxLines lines and kMaxChars characters, with an
 * ellipsis where it was cut.
 *
 * A message quoting git's or gpg's error output is as long as that output,
 * and a pushed .gitattributes with many bad lines decides its length: laid
 * out whole, the box froze the GUI. The rest goes under "Show Details...".
 * @param text The message.
 * @return The message, at most that long.
 */
auto bounded(const QString &text) -> QString {
  QString shown = text.left(kMaxChars);
  qsizetype lines = 0;
  for (qsizetype i = 0; i < shown.size(); ++i) {
    if (shown.at(i) == QLatin1Char('\n') && ++lines == kMaxLines) {
      shown.truncate(i);
      break;
    }
  }
  if (shown.size() < text.size()) {
    shown += QStringLiteral("\n\u2026");
  }
  return shown;
}
} // namespace

auto PlainMessageBox::show(QWidget *parent, QMessageBox::Icon icon,
                           const QString &title, const QString &text,
                           QMessageBox::StandardButtons buttons)
    -> QMessageBox::StandardButton {
  QMessageBox box(parent);
  box.setIcon(icon);
  // The text quotes names from the store: a bidi override in a folder name
  // mirrored the rest of a warning, a C1 control cut it short.
  box.setWindowTitle(Util::displayable(title, false));
  // Before the text: set afterwards, the text would already have been laid
  // out as rich text once.
  box.setTextFormat(Qt::PlainText);
  const QString shown = Util::displayable(text);
  const QString cut = bounded(shown);
  box.setText(cut);
  if (cut.size() != shown.size()) {
    // The rest under "Show Details...": plain text in a scrolling field,
    // which does not lay it out at once as the label did. Not all tool
    // output reaches the Process Output panel (the set-up runs git while
    // the window waits), so this is where the cause can be read.
    box.setDetailedText(shown.left(kMaxDetailChars));
  }
  box.setStandardButtons(buttons);
  return static_cast<QMessageBox::StandardButton>(box.exec());
}

auto PlainMessageBox::askYes(QWidget *parent, const QString &title,
                             const QString &text) -> bool {
  return show(parent, QMessageBox::Question, title, text,
              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}

void PlainMessageBox::warning(QWidget *parent, const QString &title,
                              const QString &text) {
  show(parent, QMessageBox::Warning, title, text);
}

void PlainMessageBox::critical(QWidget *parent, const QString &title,
                               const QString &text) {
  show(parent, QMessageBox::Critical, title, text);
}
