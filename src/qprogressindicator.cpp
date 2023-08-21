/*
 * This code is based on https://github.com/mojocorp/QProgressIndicator
 * and published under
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2011 Morgan Leborgne
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "qprogressindicator.h"
#include <QPainter>

/**
 * @brief QProgressIndicator::QProgressIndicator constructor.
 * @param parent widget the indicator is placed in.
 */
QProgressIndicator::QProgressIndicator(QWidget *parent)
    : QWidget(parent), m_angle(0), m_timerId(-1), m_delay(40),
      m_displayedWhenStopped(false), m_color(Qt::black) {
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  setFocusPolicy(Qt::NoFocus);
}

bool QProgressIndicator::isAnimated() const { return m_timerId != -1; }

void QProgressIndicator::setDisplayedWhenStopped(bool state) {
  m_displayedWhenStopped = state;

  update();
}

bool QProgressIndicator::isDisplayedWhenStopped() const {
  return m_displayedWhenStopped;
}

void QProgressIndicator::startAnimation() {
  m_angle = 0;

  if (m_timerId == -1)
    m_timerId = startTimer(m_delay);
}

void QProgressIndicator::stopAnimation() {
  if (m_timerId != -1)
    killTimer(m_timerId);

  m_timerId = -1;

  update();
}

void QProgressIndicator::setAnimationDelay(int delay) {
  if (m_timerId != -1)
    killTimer(m_timerId);

  m_delay = delay;

  if (m_timerId != -1)
    m_timerId = startTimer(m_delay);
}

void QProgressIndicator::setColor(const QColor &color) {
  m_color = color;

  update();
}

/**
 * @brief QProgressIndicator::sizeHint default minimum size.
 * @return QSize(20, 20)
 */
QSize QProgressIndicator::sizeHint() const { return {20, 20}; }

/**
 * @brief QProgressIndicator::heightForWidth square ratio.
 * @param w requested width
 * @return w returned height
 */
int QProgressIndicator::heightForWidth(int w) const { return w; }

/**
 * @brief QProgressIndicator::timerEvent do the actual animation.
 */
void QProgressIndicator::timerEvent(QTimerEvent * /*event*/) {
  m_angle = (m_angle + 30) % 360;

  update();
}

/**
 * @brief QProgressIndicator::paintEvent draw the spinner.
 */
void QProgressIndicator::paintEvent(QPaintEvent * /*event*/) {
  if (!m_displayedWhenStopped && !isAnimated())
    return;

  int width = qMin(this->width(), this->height());

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  auto outerRadius = int((width - 1) * 0.5);
  auto innerRadius = int((width - 1) * 0.5 * 0.38);

  int capsuleHeight = outerRadius - innerRadius;
  int capsuleWidth =
      (width > 32) ? int(capsuleHeight * 0.23) : int(capsuleHeight * 0.35);
  int capsuleRadius = capsuleWidth / 2;

  for (int i = 0; i < 12; ++i) {
    QColor color = m_color;
    color.setAlphaF(int(1.0f - (i / 12.0f)));
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.save();
    p.translate(rect().center());
    p.rotate(int(m_angle - i * 30.0f));
    p.drawRoundedRect(int(-capsuleWidth * 0.5), -(innerRadius + capsuleHeight),
                      capsuleWidth, capsuleHeight, capsuleRadius,
                      capsuleRadius);
    p.restore();
  }
}
