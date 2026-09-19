// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_PROCESSOUTPUTPANEL_H_
#define SRC_PROCESSOUTPUTPANEL_H_

#include "enums.h"
#include <QDockWidget>
#include <QString>

class QTextEdit;
class QToolButton;

/**
 * @class ProcessOutputPanel
 * @brief The dockable "Process output" console: numbered, colour-coded
 *        lines of git/pass/gpg output with a line cap and sticky
 *        auto-scroll.
 *
 * A QDockWidget at the bottom dock area is the conventional place for an
 * IDE-style console and gives users detach/move for free. The panel knows
 * nothing about settings or the tree; MainWindow decides when to feed it
 * and when to show it.
 */
class ProcessOutputPanel : public QDockWidget {
  Q_OBJECT

public:
  /// Oldest lines are dropped once the document holds more than this.
  static constexpr int MaxLines = 1000;

  /**
   * @brief Build the console (Clear button + read-only text edit).
   * @param parent The main window; it adds the dock to an area itself.
   */
  explicit ProcessOutputPanel(QWidget *parent = nullptr);

  /**
   * @brief Append output, one numbered line per non-empty input line.
   *
   * Lines are right-trimmed (CR and trailing whitespace), empty ones are
   * skipped, and each gets the running counter and, when given, the
   * command prefix so multi-line output stays attributed. Errors are red.
   * Keeps the view at the bottom unless the user scrolled up.
   * @param output Raw stdout/stderr text.
   * @param isError true for stderr styling.
   * @param linePrefix Command label, e.g. "git push"; empty for none.
   */
  void append(const QString &output, bool isError,
              const QString &linePrefix = QString());

  /**
   * @brief Number of lines appended since the last clear().
   * @return The running counter shown as the line number.
   */
  [[nodiscard]] auto lineCount() const -> int { return m_counter; }

  /**
   * @brief Whether new output scrolls the view to the bottom.
   * @return false after the user scrolled up, true again at the bottom or
   *         after clear().
   */
  [[nodiscard]] auto autoScroll() const -> bool { return m_autoScroll; }

  /**
   * @brief Human-readable command label for a process id.
   * @param pid Process identifier.
   * @return e.g. "git push"; empty for processes that are never shown.
   */
  static auto processName(Enums::PROCESS pid) -> QString;

  /**
   * @brief Whether a process's output may carry secrets and must never
   *        reach this long-lived panel (pass show, pass grep, pass insert).
   * @param pid Process identifier.
   * @return true when the output must be dropped.
   */
  static auto isSensitiveProcess(Enums::PROCESS pid) -> bool;

public slots:
  /**
   * @brief Drop all output, reset the counter, re-arm auto-scroll.
   */
  void clear();

private:
  void limitLines();

  QTextEdit *m_edit = nullptr;
  QToolButton *m_clearButton = nullptr;
  int m_counter = 0;
  bool m_autoScroll = true;
};

#endif // SRC_PROCESSOUTPUTPANEL_H_
