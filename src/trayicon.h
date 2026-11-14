// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief TrayIcon: the system tray icon and its context menu that show,
 *        hide, minimise, maximise, restore or quit the main window.
 */
#ifndef SRC_TRAYICON_H_
#define SRC_TRAYICON_H_

#include <QObject>
#include <QSystemTrayIcon>

class QAction;
class QMainWindow;
class QMenu;

/**
 * @class TrayIcon
 * @ingroup ui
 * @brief Handles the system tray icon and menu.
 *
 * A QObject parented to the main window: nothing here is a widget (the menu
 * and the QSystemTrayIcon are children of this object), and the parent makes
 * sure ~QSystemTrayIcon runs on quit, which is what removes the icon from the
 * notification area on Windows.
 */
class TrayIcon : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Construct a TrayIcon associated with the given main window.
   * @param parent Pointer to the QMainWindow this tray icon controls.
   */
  explicit TrayIcon(QMainWindow *parent);

  /**
   * @brief Check whether tray resources have been allocated and initialized.
   * @return true if tray resources are allocated and initialized.
   */
  auto getIsAllocated() -> bool;

  /**
   * @brief Put one of the window's toggles in the tray menu, above Quit.
   * @param action The window's own action, so its checked state is the same
   *        in both places. Ignored when there is no tray.
   */
  void addToggle(QAction *action);

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
  /**
   * @brief Create the Show/Hide/Minimize/Maximize/Restore/Quit actions and
   *        wire each one to the matching QWidget slot of the main window
   *        (Quit goes to QApplication::quit()).
   */
  void createActions();

  /**
   * @brief Build the context menu from the actions and create the
   *        QSystemTrayIcon that pops it up. Called only when a system tray
   *        is available.
   */
  void createTrayIcon();

  /** @brief Shows the main window (QWidget::show()). */
  QAction *showAction{nullptr};
  /** @brief Hides the main window (QWidget::hide()). */
  QAction *hideAction{nullptr};
  /** @brief Minimises the main window. */
  QAction *minimizeAction{nullptr};
  /** @brief Maximises the main window. */
  QAction *maximizeAction{nullptr};
  /** @brief Restores the main window to its normal size. */
  QAction *restoreAction{nullptr};
  /** @brief Quits the application. */
  QAction *quitAction{nullptr};
  /**
   * @brief Separator directly above Quit; addToggle() inserts the window's
   *        toggles (each with its own separator) in front of it.
   */
  QAction *quitSeparator{nullptr};

  /**
   * @brief The icon in the notification area; owned by this object so it is
   *        destroyed, and the icon removed, when the main window goes away.
   *        Null when no system tray is available.
   */
  QSystemTrayIcon *sysTrayIcon{nullptr};
  /**
   * @brief Context menu of the tray icon; a child of the main window because
   *        QMenu needs a widget parent. Null when no system tray is
   *        available.
   */
  QMenu *trayIconMenu{nullptr};
  /** @brief The main window the actions and showHideParent() operate on. */
  QMainWindow *parentwin;

  /**
   * @brief true once the actions, menu and icon exist; stays false when
   *        QSystemTrayIcon::isSystemTrayAvailable() was false at construction.
   */
  bool isAllocated{false};
};

#endif // SRC_TRAYICON_H_
