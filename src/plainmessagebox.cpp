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
/// Lines and characters of a message a box shows whole.
constexpr qsizetype kMaxLines = 40;
/// See kMaxLines.
constexpr qsizetype kMaxChars = 4000;
/// Characters of a message kept whole under "Show Details...".
constexpr qsizetype kMaxDetailChars = 256 * 1024;

/**
 * @brief At most @p lines lines and @p chars characters from the start of
 * @p text, or from its end with @p fromEnd.
 * @param text The text.
 * @param lines The most lines.
 * @param chars The most characters.
 * @param fromEnd Take the end, not the start.
 * @return That part of @p text.
 */
auto part(const QString &text, qsizetype lines, qsizetype chars, bool fromEnd)
    -> QString {
  QString piece = fromEnd ? text.right(chars) : text.left(chars);
  qsizetype seen = 0;
  if (fromEnd) {
    for (qsizetype i = piece.size() - 1; i >= 0; --i) {
      if (piece.at(i) == QLatin1Char('\n') && ++seen == lines) {
        return piece.mid(i + 1);
      }
    }
  } else {
    for (qsizetype i = 0; i < piece.size(); ++i) {
      if (piece.at(i) == QLatin1Char('\n') && ++seen == lines) {
        return piece.left(i);
      }
    }
  }
  return piece;
}

/**
 * @brief @p text as a box shows it: whole up to kMaxLines lines and
 * kMaxChars characters, else its start and its end with an ellipsis
 * between.
 *
 * A message quoting git's or gpg's error output is as long as that output,
 * and a pushed .gitattributes with many bad lines decides its length: laid
 * out whole, the box froze the GUI. The end stays because that is where git
 * puts what went wrong ("fatal: ..."), after its warnings.
 * @param text The message.
 * @param cut Set to whether anything was left out.
 * @return The message to show.
 */
auto bounded(const QString &text, bool *cut) -> QString {
  *cut = text.size() > kMaxChars || text.count(QLatin1Char('\n')) >= kMaxLines;
  if (!*cut) {
    return text;
  }
  return part(text, kMaxLines / 2, kMaxChars / 2, false) +
         QStringLiteral("\n\u2026\n") +
         part(text, kMaxLines / 2, kMaxChars / 2, true);
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
  bool cut = false;
  box.setText(bounded(shown, &cut));
  if (cut) {
    // All of it under "Show Details...": plain text in a scrolling field,
    // which does not lay it out at once as the label did. Not all tool
    // output reaches the Process Output panel (the set-up runs git while
    // the window waits), so this is where the cause can be read. Past
    // 256 KB its start and end, for the same reason as the box.
    box.setDetailedText(shown.size() <= kMaxDetailChars
                            ? shown
                            : shown.left(kMaxDetailChars / 2) +
                                  QStringLiteral("\n\u2026\n") +
                                  shown.right(kMaxDetailChars / 2));
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
