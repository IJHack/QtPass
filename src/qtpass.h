#ifndef QTPASS_H
#define QTPASS_H

#include <QObject>

class MainWindow;
class Pass;
class QtPass : public QObject {
  Q_OBJECT
public:
  QtPass();

  void setMainWindow(MainWindow *mW) { m_mainWindow = mW; }

private:
  MainWindow *m_mainWindow;

  void connectPassSignalHandlers(Pass *pass);

signals:

public slots:

private slots:
  void processError(QProcess::ProcessError);

#endif // QTPASS_H
