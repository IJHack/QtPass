#include "trayicon.h"
#include "debughelper.h"
#include <QAction>
#include <QApplication>
#include <QMainWindow>
#include <QMenu>

/**
 * @brief TrayIcon::TrayIcon use a (system) tray icon with a nice QtPass logo on
 * it (currently) only Quits.
 * @param parent
 */
TrayIcon::TrayIcon(QMainWindow *parent) {
  parentwin = parent;

  if (QSystemTrayIcon::isSystemTrayAvailable() == true) {
    createActions();
    createTrayIcon();

    sysTrayIcon->setIcon(
        QIcon::fromTheme("qtpass-tray", QIcon(":/artwork/icon.png")));

    sysTrayIcon->show();

    QObject::connect(sysTrayIcon, &QSystemTrayIcon::activated, this,
                     &TrayIcon::iconActivated);

    isAllocated = true;
  } else {
    dbg() << "No tray icon for this OS possibly also not show options?";

    isAllocated = false;

    showAction = nullptr;
    hideAction = nullptr;
    minimizeAction = nullptr;
    maximizeAction = nullptr;
    restoreAction = nullptr;
    quitAction = nullptr;
    sysTrayIcon = nullptr;
    trayIconMenu = nullptr;
  }
}

/**
 * @brief TrayIcon::setVisible show or hide the icon.
 * @param visible
 */
void TrayIcon::setVisible(bool visible) {
  if (visible == true)
    parentwin->show();
  else
    parentwin->hide();
}

/**
 * @brief TrayIcon::getIsAllocated return if TrayIcon is allocated
 */
bool TrayIcon::getIsAllocated() { return isAllocated; }

/**
 * @brief TrayIcon::createActions setup the signals.
 */
void TrayIcon::createActions() {
  showAction = new QAction(tr("&Show"), this);
  connect(showAction, &QAction::triggered, parentwin, &QWidget::show);
  hideAction = new QAction(tr("&Hide"), this);
  connect(hideAction, &QAction::triggered, parentwin, &QWidget::hide);

  minimizeAction = new QAction(tr("Mi&nimize"), this);
  connect(minimizeAction, &QAction::triggered, parentwin,
          &QWidget::showMinimized);
  maximizeAction = new QAction(tr("Ma&ximize"), this);
  connect(maximizeAction, &QAction::triggered, parentwin,
          &QWidget::showMaximized);
  restoreAction = new QAction(tr("&Restore"), this);
  connect(restoreAction, &QAction::triggered, parentwin, &QWidget::showNormal);

  quitAction = new QAction(tr("&Quit"), this);
  connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
}

/**
 * @brief TrayIcon::createTrayIcon set up menu.
 */
void TrayIcon::createTrayIcon() {
  trayIconMenu = new QMenu(this);
  trayIconMenu->addAction(showAction);
  trayIconMenu->addAction(hideAction);
  trayIconMenu->addAction(minimizeAction);
  trayIconMenu->addAction(maximizeAction);
  trayIconMenu->addAction(restoreAction);
  trayIconMenu->addSeparator();
  trayIconMenu->addAction(quitAction);

  sysTrayIcon = new QSystemTrayIcon(this);
  sysTrayIcon->setContextMenu(trayIconMenu);
}

/**
 * @brief TrayIcon::showHideParent toggle app visibility.
 */
void TrayIcon::showHideParent() {
  if (parentwin->isVisible() == true)
    parentwin->hide();
  else
    parentwin->show();
}

/**
 * @brief TrayIcon::iconActivated you clicked on the trayicon.
 * @param reason
 */
void TrayIcon::iconActivated(QSystemTrayIcon::ActivationReason reason) {
  switch (reason) {
  case QSystemTrayIcon::Trigger:
  case QSystemTrayIcon::DoubleClick:
    showHideParent();
    break;
  case QSystemTrayIcon::MiddleClick:
    showMessage("test", "test msg", 1000);
    break;
  default: {};
  }
}

/**
 * @brief TrayIcon::showMessage show a systray message for notification.
 * @param title
 * @param msg
 * @param time
 */
void TrayIcon::showMessage(QString title, QString msg, int time) {
  sysTrayIcon->showMessage(title, msg, QSystemTrayIcon::Information, time);
}
