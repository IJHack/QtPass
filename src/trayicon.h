#ifndef TRAYICON_H_
#define TRAYICON_H_

#include <QSystemTrayIcon>
#include <QWidget>

/*!
    \class TrayIcon
    \brief Handles the systemtray icon and menu.
 */
class QAction;
class QMainWindow;
class QMenu;
class TrayIcon : public QWidget {
  Q_OBJECT

public:
  explicit TrayIcon(QMainWindow *parent);
  void showMessage(const QString &title, const QString &msg, int time);
  void setVisible(bool visible);
  bool getIsAllocated();

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

#endif // TRAYICON_H_
