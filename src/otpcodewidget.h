// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_OTPCODEWIDGET_H_
#define SRC_OTPCODEWIDGET_H_

#include "totp.h"

#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButtonWithClipboard;

/**
 * @class OtpCodeWidget
 * @brief Live one-time password display: code, copy button and countdown.
 *
 * Rendered by PasswordDisplayPanel as the value side of the "OTP Code" row. It
 * is a widget rather than a set of loose children so that its refresh timer is
 * a child of the thing it updates: PasswordDisplayPanel::clear() deletes the
 * row's widgets, which destroys this widget and therefore the timer. A timer
 * owned by the panel instead could fire after its label had been deleted.
 *
 * The code is recomputed from the wall clock on every tick rather than from an
 * internal counter, so a machine that suspends across a period boundary
 * self-corrects on the first tick after resume.
 */
class OtpCodeWidget : public QWidget {
  Q_OBJECT

public:
  /**
   * @brief Build a live OTP code display and start its refresh timer.
   * @param settings Validated TOTP settings for the selected entry.
   * @param withCopyButton Add the clipboard button. Pass false when the
   *        clipboard type is Enums::CLIPBOARD_NEVER.
   * @param monospace Render the code in a monospace font.
   * @param parent Parent widget.
   */
  OtpCodeWidget(Totp::Settings settings, bool withCopyButton, bool monospace,
                QWidget *parent = nullptr);

  /**
   * @brief Recompute the code and countdown for an explicit point in time.
   *
   * Called once per second by the internal timer; exposed so tests can pin the
   * clock instead of racing it.
   * @param unixTime Seconds since the Unix epoch (UTC).
   */
  void refresh(quint64 unixTime);

  /**
   * @brief The current code, ungrouped, exactly as it is copied.
   * @return The current OTP code.
   */
  [[nodiscard]] auto code() const -> QString;

  /**
   * @brief Seconds left on the current code, as last rendered.
   * @return A value in 1..Totp::Settings::step.
   */
  [[nodiscard]] auto secondsRemaining() const -> uint;

signals:
  /**
   * @brief Emitted when the copy button is activated.
   * @param text The current code.
   */
  void copyRequested(const QString &text);

private:
  Totp::Settings m_settings;
  QPushButtonWithClipboard *m_copyButton{nullptr};
  QLabel *m_codeLabel{nullptr};
  QProgressBar *m_progress{nullptr};
  QString m_code;
  quint64 m_counter{0};
  uint m_secondsRemaining{0};
  bool m_hasCode{false};
};

#endif // SRC_OTPCODEWIDGET_H_
