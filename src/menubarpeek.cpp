// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "menubarpeek.h"
#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QTimer>

MenuBarPeek::MenuBarPeek(QMainWindow *window, QAction *toggle)
    : QObject(window), m_window(window), m_toggle(toggle) {
  // Key events go to the focus widget, and an open menu is a window of its
  // own, so the filter has to sit on the application.
  qApp->installEventFilter(this);
  connect(m_toggle, &QAction::toggled, this, [this] { m_peeking = false; });
  for (QAction *entry : m_window->menuBar()->actions()) {
    if (QMenu *menu = entry->menu()) {
      connect(menu, &QMenu::aboutToHide, this,
              &MenuBarPeek::hideWhenNoMenuIsOpen);
    }
  }
}

auto MenuBarPeek::isPeeking() const -> bool { return m_peeking; }

auto MenuBarPeek::eventFilter(QObject *watched, QEvent *event) -> bool {
  switch (event->type()) {
  case QEvent::KeyPress:
    onKeyPress(watched, static_cast<QKeyEvent *>(event));
    break;
  case QEvent::KeyRelease:
    onKeyRelease(static_cast<QKeyEvent *>(event));
    break;
  case QEvent::MouseButtonPress:
    if (m_peeking && inWindow(watched) && !onMenuBar(watched)) {
      hideWhenNoMenuIsOpen();
    }
    break;
  case QEvent::WindowDeactivate:
    if (watched == m_window) {
      // Alt+Tab: the release goes to another application.
      m_altDown = false;
      hideWhenNoMenuIsOpen();
    }
    break;
  default:
    break;
  }
  return false;
}

// A key event reaches the filter once for every widget it propagates to, and
// again as auto-repeat; m_altDown makes the second sighting a no-op.
void MenuBarPeek::onKeyPress(QObject *watched, const QKeyEvent *event) {
  if (event->key() != Qt::Key_Alt) {
    m_altAlone = false;
    if (event->key() == Qt::Key_Escape && m_peeking) {
      hideWhenNoMenuIsOpen();
    }
    return;
  }
  if (m_altDown || event->isAutoRepeat() || !inWindow(watched)) {
    return;
  }
  m_altDown = true;
  m_altAlone = true;
  m_shownByThisPress =
      !m_peeking && !m_toggle->isChecked() && !m_window->menuBar()->isVisible();
  if (m_shownByThisPress) {
    show();
  }
}

void MenuBarPeek::onKeyRelease(const QKeyEvent *event) {
  if (event->key() != Qt::Key_Alt || !m_altDown || event->isAutoRepeat()) {
    return;
  }
  m_altDown = false;
  if (!m_peeking) {
    return;
  }
  if (m_altAlone && !m_shownByThisPress) {
    hide(); // the second tap
  } else if (!m_altAlone) {
    hideWhenNoMenuIsOpen(); // Alt+something that was not a menu
  }
}

auto MenuBarPeek::inWindow(QObject *watched) const -> bool {
  const auto *widget = qobject_cast<QWidget *>(watched);
  return widget != nullptr && widget->window() == m_window;
}

auto MenuBarPeek::onMenuBar(QObject *watched) const -> bool {
  const auto *widget = qobject_cast<QWidget *>(watched);
  const QMenuBar *bar = m_window->menuBar();
  return widget == bar || bar->isAncestorOf(widget);
}

void MenuBarPeek::show() {
  m_peeking = true;
  m_window->menuBar()->show();
}

void MenuBarPeek::hide() {
  m_peeking = false;
  if (!m_toggle->isChecked()) {
    m_window->menuBar()->hide();
  }
}

// Deferred: a menu's aboutToHide also fires when the pointer moves on to the
// next menu, and Escape is seen here before the bar leaves keyboard mode.
void MenuBarPeek::hideWhenNoMenuIsOpen() {
  QTimer::singleShot(0, this, [this] {
    if (m_peeking && QApplication::activePopupWidget() == nullptr &&
        m_window->menuBar()->activeAction() == nullptr) {
      hide();
    }
  });
}
