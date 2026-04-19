// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_TRAYICON_H_
#define SRC_TRAYICON_H_

#include <QSystemTrayIcon>
#include <QWidget>

class QAction;
class QMainWindow;
class QMenu;

/**
 * @class TrayIcon
 * @brief Manages a system tray icon, its context menu, and related actions for a main window.
 */

/**
 * @brief Create a TrayIcon bound to a specific main window.
 * @param parent Pointer to the QMainWindow that the tray icon will control.
 */

/**
 * @brief Display a transient notification via the system tray icon.
 * @param title Notification title text.
 * @param msg Notification body text.
 * @param time Display duration in milliseconds.
 */

/**
 * @brief Show or hide the tray icon and its associated menu/actions.
 * @param visible `true` to show the tray icon, `false` to hide it.
 */

/**
 * @brief Indicates whether tray resources have been allocated and initialized.
 * @returns `true` if tray resources are allocated and initialized, `false` otherwise.
 */

/**
 * @brief Toggle the visibility of the associated main window.
 */

/**
 * @brief Handle activation events from the system tray icon.
 * @param reason Activation reason provided by QSystemTrayIcon.
 */
class TrayIcon : public QWidget {
  Q_OBJECT

public:
  /**
   * @brief Construct a TrayIcon associated with the given main window.
   * @param parent Pointer to the QMainWindow this tray icon controls.
   */
  explicit TrayIcon(QMainWindow *parent);

  /**
   * @brief Display a transient notification message via the tray icon.
   * @param title Title text of the notification.
   * @param msg Body text of the notification.
   * @param time Duration to display the notification in milliseconds.
   */
  void showMessage(const QString &title, const QString &msg, int time);

  /**
   * @brief Show or hide the tray icon and its associated UI.
   * @param visible true to make the tray icon visible, false to hide it.
   */
  void setVisible(bool visible);

  /**
   * @brief Returns true if tray resources have been allocated and initialized.
   */
  auto getIsAllocated() -> bool;

signals:

public slots:
  /**
   * @brief Toggle the visibility of the associated main window.
   */
  void showHideParent();

  /**
   * @brief Handle activation events from the system tray icon.
   * @param reason The activation reason provided by QSystemTrayIcon.
   */
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
