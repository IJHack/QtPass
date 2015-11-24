#include "singleapplication.h"
#include <QLocalSocket>

/**
 * @brief SingleApplication::SingleApplication
 * @param argc
 * @param argv
 * @param uniqueKey
 */
SingleApplication::SingleApplication(int &argc, char *argv[],
                                     const QString uniqueKey)
    : QApplication(argc, argv), _uniqueKey(uniqueKey) {
  sharedMemory.setKey(_uniqueKey);
  if (sharedMemory.attach()) {
    _isRunning = true;
  } else {
    _isRunning = false;
    // create shared memory.
    if (!sharedMemory.create(1)) {
      qDebug("Unable to create single instance.");
      return;
    }
    // create local server and listen to incomming messages from other
    // instances.
    localServer.reset(new QLocalServer(this));
    connect(localServer.data(), SIGNAL(newConnection()), this,
            SLOT(receiveMessage()));
    localServer->listen(_uniqueKey);
  }
}

// public slots.

/**
 * @brief SingleApplication::receiveMessage
 */
void SingleApplication::receiveMessage() {
  QLocalSocket *localSocket = localServer->nextPendingConnection();
  if (!localSocket->waitForReadyRead(timeout)) {
    qDebug() << localSocket->errorString().toLatin1();
    return;
  }
  QByteArray byteArray = localSocket->readAll();
  QString message = QString::fromUtf8(byteArray.constData());
  emit messageAvailable(message);
  localSocket->disconnectFromServer();
}

// public functions.
/**
 * @brief SingleApplication::isRunning
 * @return
 */
bool SingleApplication::isRunning() { return _isRunning; }

/**
 * @brief SingleApplication::sendMessage
 * @param message
 * @return
 */
bool SingleApplication::sendMessage(const QString &message) {
  if (!_isRunning)
    return false;
  QLocalSocket localSocket(this);
  localSocket.connectToServer(_uniqueKey, QIODevice::WriteOnly);
  if (!localSocket.waitForConnected(timeout)) {
    qDebug() << localSocket.errorString().toLatin1();
    return false;
  }
  localSocket.write(message.toUtf8());
  if (!localSocket.waitForBytesWritten(timeout)) {
    qDebug() << localSocket.errorString().toLatin1();
    return false;
  }
  localSocket.disconnectFromServer();
  return true;
}
