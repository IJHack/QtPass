// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_TRAYICON_H_
#define SRC_TRAYICON_H_

#include <QSystemTrayIcon>
#include <QWidget>

/*!
    \class TrayIcon
    \brief Handles the systemtray icon and menu.
 */
class QAction;
class QMainWindow;
class QMenu;
/**
 * Construct a TrayIcon associated with the given main window.
 * @param parent Pointer to the QMainWindow this tray icon controls and mirrors.
 */
/**
 * Display a transient notification message via the tray icon.
 * @param title Title text of the notification.
 * @param msg Body text of the notification.
 * @param time Duration to display the notification in milliseconds.
 */
/**
 * Show or hide the tray-related UI and behavior.
 * @param visible `true` to make tray UI active/visible, `false` to deactivate/hide it.
 */
/**
 * Report whether tray resources have been allocated and initialized.
 * @returns `true` if resources are allocated, `false` otherwise.
 */
/**
 * Toggle the visibility/state of the associated main window in response to user interaction.
 */
/**
 * Handle activation events from the system tray icon.
 * @param reason The activation reason provided by QSystemTrayIcon.
 */
/**
 * Create and configure the tray menu actions (show, hide, minimize, maximize, restore, quit).
 */
/**
 * Initialize the QSystemTrayIcon and its context menu, wiring actions and event handling.
 */
class TrayIcon : public QWidget {
  Q_OBJECT

public:
  explicit TrayIcon(QMainWindow *parent);
  void showMessage(const QString &title, const QString &msg, int time);
  void setVisible(bool visible);
  auto getIsAllocated() -> bool;

signals:

public slots:
  void showHideParent();
  void iconActivated(QSystemTrayIcon::ActivationReason reason);

private:
  void createActions();
  void createTrayIcon();

  QAction *showAction;
  QAction *hideAction;
  QAction *minimizeAction;
  QAction *maximizeAction;
  QAction *restoreAction;
  QAction *quitAction;

  QSystemTrayIcon *sysTrayIcon;
  QMenu *trayIconMenu;
  QMainWindow *parentwin;

  bool isAllocated;
};

#endif // SRC_TRAYICON_H_
