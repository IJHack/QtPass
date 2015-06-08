#include "trayicon.h"

trayIcon::trayIcon(QMainWindow *parent)
{
  parentwin = parent;

  createActions();
  createTrayIcon();

  sysTrayIcon->setIcon(QIcon(":/artwork/logo.png"));

  sysTrayIcon->show();

  QObject::connect(sysTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
          this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

}

void trayIcon::setVisible(bool visible)
{
  if(visible == true) {
    parentwin->show();
  } else {
    parentwin->hide();
  }
}

void trayIcon::createActions()
{
  minimizeAction = new QAction(tr("Mi&nimize"), this);
  connect(minimizeAction, SIGNAL(triggered()), this, SLOT(hide()));

  maximizeAction = new QAction(tr("Ma&ximize"), this);
  connect(maximizeAction, SIGNAL(triggered()), this, SLOT(showMaximized()));

  restoreAction = new QAction(tr("&Restore"), this);
  connect(restoreAction, SIGNAL(triggered()), this, SLOT(showNormal()));

  quitAction = new QAction(tr("&Quit"), this);
  connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
}


void trayIcon::createTrayIcon()
{
  trayIconMenu = new QMenu(this);
  trayIconMenu->addAction(minimizeAction);
  trayIconMenu->addAction(maximizeAction);
  trayIconMenu->addAction(restoreAction);
  trayIconMenu->addSeparator();
  trayIconMenu->addAction(quitAction);

  sysTrayIcon = new QSystemTrayIcon(this);
  sysTrayIcon->setContextMenu(trayIconMenu);
}

void trayIcon::showHideParent()
{
  if(parentwin->isVisible() == true) {
    parentwin->hide();
  } else {
    parentwin->show();
  }
}

void trayIcon::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
         showHideParent();
        break;
    case QSystemTrayIcon::MiddleClick:
        showMessage("test", "test msg", 1000);
        break;
    default:
        ;
    }
}

void trayIcon::showMessage(QString title, QString msg, int time)
{
    sysTrayIcon->showMessage(title, msg, QSystemTrayIcon::Information, time);
}
