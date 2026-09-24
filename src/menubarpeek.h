// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_MENUBARPEEK_H_
#define SRC_MENUBARPEEK_H_

#include <QObject>

class QAction;
class QKeyEvent;
class QMainWindow;

/**
 * @class MenuBarPeek
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
  void onKeyPress(QObject *watched, const QKeyEvent *event);
  void onKeyRelease(const QKeyEvent *event);
  [[nodiscard]] auto inWindow(QObject *watched) const -> bool;
  [[nodiscard]] auto onMenuBar(QObject *watched) const -> bool;
  void show();
  void hide();
  void hideWhenNoMenuIsOpen();

  QMainWindow *m_window;
  QAction *m_toggle;
  bool m_peeking{false};
  bool m_altDown{false};
  bool m_altAlone{false};
  bool m_shownByThisPress{false};
};

#endif // SRC_MENUBARPEEK_H_
