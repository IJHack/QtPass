// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief MenuBarPeek: shows the main window's hidden menu bar while Alt is
 *        held or tapped, and hides it again afterwards.
 */
#ifndef SRC_MENUBARPEEK_H_
#define SRC_MENUBARPEEK_H_

#include <QObject>

class QAction;
class QKeyEvent;
class QMainWindow;

/**
 * @class MenuBarPeek
 * @ingroup ui
 * @brief Shows a hidden menu bar while Alt is used, as browsers and file
 *        managers do.
 *
 * Pressing Alt in the window shows the bar, so Alt+F and the other mnemonics
 * open their menus. A lone tap leaves it up until Alt is tapped again, Escape
 * is pressed, the window is clicked or loses focus; after a menu closes, or
 * after an Alt shortcut that was not a menu, it goes again. The stored "Show
 * menu bar" choice is never touched: checking it during a peek keeps the bar.
 */
class MenuBarPeek : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Watch @p window for Alt while its menu bar is hidden.
   * @param window The main window whose menuBar() is shown; also the parent.
   * @param toggle The window's "Show menu bar" action. While it is checked
   *        the bar is simply there and Alt does nothing extra.
   */
  MenuBarPeek(QMainWindow *window, QAction *toggle);

  /**
   * @brief Whether the bar is up for a peek rather than by choice.
   * @return true between the Alt press that showed it and its hiding.
   */
  [[nodiscard]] auto isPeeking() const -> bool;

protected:
  /**
   * @brief Follows Alt, Escape, clicks and focus for the whole application.
   * @param watched The object the event is for.
   * @param event The event; never consumed.
   * @return Always false.
   */
  auto eventFilter(QObject *watched, QEvent *event) -> bool override;

private:
  /**
   * @brief Handle a key press seen by the application filter.
   *
   * A first, non-repeated Alt press inside the window shows the bar when it
   * is hidden; any other key marks the press as "Alt plus something", and
   * Escape during a peek hides the bar. A key event reaches the filter once
   * for every widget it propagates to and again as auto-repeat; m_altDown
   * makes the second sighting a no-op.
   * @param watched The object the event was sent to.
   * @param event The key press.
   */
  void onKeyPress(QObject *watched, const QKeyEvent *event);

  /**
   * @brief Handle the release of Alt: a second lone tap hides the bar, an
   *        Alt+key chord that did not open a menu hides it once no menu is
   *        open, and the tap that showed it leaves it up.
   * @param event The key release; anything but a tracked Alt is ignored.
   */
  void onKeyRelease(const QKeyEvent *event);

  /**
   * @brief Whether @p watched is a widget that belongs to the main window.
   * @param watched The object an event was sent to.
   * @return true if it is a widget whose top-level window is m_window.
   */
  [[nodiscard]] auto inWindow(QObject *watched) const -> bool;

  /**
   * @brief Whether @p watched is the menu bar itself or one of its children,
   *        so a click there must not end the peek.
   * @param watched The object a mouse press was sent to.
   * @return true for the menu bar or a descendant of it.
   */
  [[nodiscard]] auto onMenuBar(QObject *watched) const -> bool;

  /** @brief Show the menu bar and mark the peek as active. */
  void show();

  /**
   * @brief End the peek; hides the menu bar unless the "Show menu bar"
   *        toggle was checked in the meantime.
   */
  void hide();

  /**
   * @brief hide() on the next event-loop pass, and only if no popup is open
   *        and the bar has no active action. Deferred because a menu's
   *        aboutToHide also fires when the pointer moves on to the next menu,
   *        and Escape is seen before the bar leaves keyboard mode.
   */
  void hideWhenNoMenuIsOpen();

  /** @brief The main window whose menuBar() is shown and hidden; the parent. */
  QMainWindow *m_window;
  /**
   * @brief The window's "Show menu bar" action; while checked the bar is up
   *        by choice and the peek state is cleared.
   */
  QAction *m_toggle;
  /** @brief true while the bar is shown because of Alt, not the toggle. */
  bool m_peeking{false};
  /**
   * @brief true between an Alt press in the window and its release (or the
   *        window losing activation); filters repeats and propagated copies.
   */
  bool m_altDown{false};
  /**
   * @brief true while no other key has been pressed since Alt went down, i.e.
   *        the release will be a lone tap rather than a chord.
   */
  bool m_altAlone{false};
  /**
   * @brief true when the current Alt press is the one that showed the bar,
   *        so its release must not hide it again.
   */
  bool m_shownByThisPress{false};
};

#endif // SRC_MENUBARPEEK_H_
