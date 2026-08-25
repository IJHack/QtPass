// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @class OtpCodeWidget
 * @brief Live one-time password display implementation.
 *
 * @see otpcodewidget.h
 */

#include "otpcodewidget.h"
#include "qpushbuttonwithclipboard.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <utility>

namespace {

/// Refresh cadence: the countdown is shown in whole seconds.
constexpr int kRefreshIntervalMs = 1000;
/// Matches the action-button visual height used elsewhere in the field rows.
constexpr int kProgressHeight = 18;
/// Enough for a three-digit countdown without the bar dominating the row.
constexpr int kProgressWidth = 52;

} // namespace

OtpCodeWidget::OtpCodeWidget(Totp::Settings settings, bool withCopyButton,
                             bool monospace, QWidget *parent)
    : QWidget(parent), m_settings(std::move(settings)) {
  // Same borderless look as the action buttons PasswordDisplayPanel builds.
  const QString buttonStyle =
      "QPushButton { border-style: none; background: transparent; padding: 0; "
      "margin: 0; icon-size: 16px; color: inherit; }";

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  if (withCopyButton) {
    m_copyButton = new QPushButtonWithClipboard(QString(), this);
    m_copyButton->setStyleSheet(buttonStyle);
    connect(m_copyButton, &QPushButtonWithClipboard::clicked, this,
            &OtpCodeWidget::copyRequested);
    layout->addWidget(m_copyButton);
  }

  m_codeLabel = new QLabel(this);
  m_codeLabel->setObjectName(QStringLiteral("OTP Code"));
  m_codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  if (monospace) {
    m_codeLabel->setStyleSheet("QLabel { font-family: monospace; }");
  }
  layout->addWidget(m_codeLabel);
  layout->addStretch();

  m_progress = new QProgressBar(this);
  m_progress->setObjectName(QStringLiteral("otpCountdown"));
  m_progress->setRange(0, static_cast<int>(m_settings.step));
  // Number-only format: no translatable string, and no unit to mistranslate.
  m_progress->setFormat(QStringLiteral("%v"));
  m_progress->setFixedWidth(kProgressWidth);
  m_progress->setMaximumHeight(kProgressHeight);
  m_progress->setToolTip(tr("Seconds until the OTP code changes"));
  layout->addWidget(m_progress);

  // The timer is a child of this widget: deleting the row stops it, so a
  // queued timeout can never reach a destroyed label.
  auto *timer = new QTimer(this);
  timer->setInterval(kRefreshIntervalMs);
  connect(timer, &QTimer::timeout, this,
          [this]() { refresh(Totp::currentUnixTime()); });
  timer->start();

  refresh(Totp::currentUnixTime());
}

void OtpCodeWidget::refresh(quint64 unixTime) {
  const quint64 counter = Totp::counter(m_settings, unixTime);
  if (!m_hasCode || counter != m_counter) {
    m_counter = counter;
    m_hasCode = true;
    m_code = Totp::generate(m_settings, unixTime);
    m_codeLabel->setText(m_code);
    if (m_copyButton != nullptr) {
      m_copyButton->setTextToCopy(m_code);
    }
  }

  m_secondsRemaining = Totp::secondsRemaining(m_settings, unixTime);
  m_progress->setValue(static_cast<int>(m_secondsRemaining));
}

auto OtpCodeWidget::code() const -> QString { return m_code; }

auto OtpCodeWidget::secondsRemaining() const -> uint {
  return m_secondsRemaining;
}
