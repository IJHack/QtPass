// SPDX-FileCopyrightText: 2015 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "trayicon.h"
#include <QAction>
#include <QApplication>
#include <QMainWindow>
#include <QMenu>

#include "qtpasslogging.h"

TrayIcon::TrayIcon(QMainWindow *parent) : QObject(parent) {
  parentwin = parent;

  if (QSystemTrayIcon::isSystemTrayAvailable()) {
    createActions();
    createTrayIcon();

    sysTrayIcon->setIcon(
        QIcon::fromTheme("qtpass-tray", QIcon(":/artwork/icon.png")));

    sysTrayIcon->show();

    QObject::connect(sysTrayIcon, &QSystemTrayIcon::activated, this,
                     &TrayIcon::iconActivated);

    isAllocated = true;
  } else {
    qCDebug(lcQtPass)
        << "No tray icon for this OS possibly also not show options?";
  }
}

auto TrayIcon::getIsAllocated() -> bool { return isAllocated; }

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
  connect(quitAction, &QAction::triggered, QApplication::instance(),
          &QApplication::quit);
}

void TrayIcon::createTrayIcon() {
  // QMenu needs a widget parent; the main window owns the menu, this object
  // owns the tray icon.
  trayIconMenu = new QMenu(parentwin);
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

void TrayIcon::showHideParent() {
  if (parentwin->isVisible()) {
    parentwin->hide();
  } else {
    parentwin->show();
  }
}

void TrayIcon::iconActivated(QSystemTrayIcon::ActivationReason reason) {
  switch (reason) {
  case QSystemTrayIcon::Trigger:
  case QSystemTrayIcon::DoubleClick:
    showHideParent();
    break;
  case QSystemTrayIcon::MiddleClick:
    break;
  default: {
  }
  }
}
