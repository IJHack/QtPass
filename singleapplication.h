#ifndef SINGLEAPPLICATION_H_
#define SINGLEAPPLICATION_H_

#include <QApplication>
#include <QLocalServer>
#include <QSharedMemory>

class SingleApplication : public QApplication {
  Q_OBJECT
public:
  SingleApplication(int &argc, char *argv[], const QString uniqueKey);
  bool isRunning();
  bool sendMessage(const QString &message);

public slots:
  void receiveMessage();

signals:
  void messageAvailable(QString message);

private:
  bool _isRunning;
  QString _uniqueKey;
  QSharedMemory sharedMemory;
  QScopedPointer<QLocalServer> localServer;

  static const int timeout = 1000;
};

#endif // SINGLEAPPLICATION_H_
