// SPDX-FileCopyrightText: 2011 Morgan Leborgne
// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: MIT

#ifndef SRC_QPROGRESSINDICATOR_H_
#define SRC_QPROGRESSINDICATOR_H_

#include <QColor>
#include <QWidget>

/*!
 * QProgressIndicator provides a simple indeterminate progress indicator widget
 * that spins to show the application is busy.
 *
 * @sa QProgressBar
 */

/*!
 * Returns the delay between animation steps.
 *
 * @return The number of milliseconds between animation steps. The default value
 * is 40.
 * @sa setAnimationDelay
 */

/*!
 * Indicates whether the indicator is currently animating.
 *
 * @return `true` if the indicator is animating, `false` otherwise.
 * @sa startAnimation stopAnimation
 */

/*!
 * Indicates whether the indicator remains visible when not animating.
 *
 * @return `true` if the indicator shows itself when stopped, `false` otherwise.
 * The default is `false`.
 * @sa setDisplayedWhenStopped
 */

/*!
 * Returns the current drawing color used by the indicator.
 *
 * @return Reference to the indicator's QColor.
 * @sa setColor
 */

/*!
 * Starts the spin animation.
 *
 * @sa stopAnimation isAnimated
 */

/*!
 * Stops the spin animation.
 *
 * @sa startAnimation isAnimated
 */

/*!
 * Sets the delay between animation steps.
 *
 * @param delay Delay in milliseconds. Values larger than 40 slow the animation;
 * values smaller than 40 speed it up.
 * @sa animationDelay
 */

/*!
 * Sets whether the indicator remains visible when not animating.
 *
 * @param state Set to `true` to keep the indicator visible when stopped; set to
 * `false` to hide it when stopped.
 * @sa isDisplayedWhenStopped
 */

/*!
 * Sets the drawing color for the indicator.
 *
 * @param color The new color to use for rendering the indicator.
 * @sa color
 */
class QProgressIndicator : public QWidget {
  Q_OBJECT

public:
  explicit QProgressIndicator(QWidget *parent = nullptr);

  /*! Returns the delay between animation steps.
      \return The number of milliseconds between animation steps. By default,
     the animation delay is set to 40 milliseconds.
      \sa setAnimationDelay
   */
  [[nodiscard]] auto animationDelay() const -> int { return m_delay; }

  /*! Returns a Boolean value indicating whether the component is currently
     animated.
      \return Animation state.
      \sa startAnimation stopAnimation
   */
  [[nodiscard]] auto isAnimated() const -> bool;

  /*! Returns a Boolean value indicating whether the receiver shows itself even
     when it is not animating.
      \return Return true if the progress indicator shows itself even when it is
     not animating. By default, it returns false.
      \sa setDisplayedWhenStopped
   */
  [[nodiscard]] auto isDisplayedWhenStopped() const -> bool;

  /*! Returns the color of the component.
      \sa setColor
   */
  [[nodiscard]] auto color() const -> const QColor & { return m_color; }

  [[nodiscard]] virtual auto sizeHint() const -> QSize;
  [[nodiscard]] auto heightForWidth(int w) const -> int;

public slots:
  /*! Starts the spin animation.
      \sa stopAnimation isAnimated
   */
  void startAnimation();

  /*! Stops the spin animation.
      \sa startAnimation isAnimated
   */
  void stopAnimation();

  /*! Sets the delay between animation steps.
      Setting the \a delay to a value larger than 40 slows the animation, while
     setting the \a delay to a smaller value speeds it up.
      \param delay The delay, in milliseconds.
      \sa animationDelay
   */
  void setAnimationDelay(int delay);

  /*! Sets whether the component hides itself when it is not animating.
     \param state The animation state. Set false to hide the progress indicator
     when it is not animating; otherwise true.
     \sa isDisplayedWhenStopped
   */
  void setDisplayedWhenStopped(bool state);

  /*! Sets the color of the component to the given color.
         \param color The new color to use. Pass an invalid QColor to reset to
     the palette fallback color.
         \sa color
      */
  void setColor(const QColor &color);

protected:
  virtual void timerEvent(QTimerEvent *event);
  virtual void paintEvent(QPaintEvent *event);

private:
  int m_angle;
  int m_timerId;
  int m_delay;
  bool m_displayedWhenStopped;
  QColor m_color;
};

#endif // SRC_QPROGRESSINDICATOR_H_
