#include "singleapplication.h"
#include "debughelper.h"
#include <QLocalSocket>

/**
 * @brief SingleApplication::SingleApplication this replaces the QApplication
 * allowing for local socket based communications.
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
      dbg() << "Unable to create single instance.";
      return;
    }
    // create local server and listen to incomming messages from other
    // instances.
    localServer.reset(new QLocalServer(this));
    connect(localServer.data(), &QLocalServer::newConnection, this,
            &SingleApplication::receiveMessage);
    localServer->listen(_uniqueKey);
  }
}

// public slots.

/**
 * @brief SingleApplication::receiveMessage we have received (a command line)
 * message.
 */
void SingleApplication::receiveMessage() {
  QLocalSocket *localSocket = localServer->nextPendingConnection();
  if (!localSocket->waitForReadyRead(timeout)) {
    dbg() << localSocket->errorString().toLatin1();
    return;
  }
  QByteArray byteArray = localSocket->readAll();
  QString message = QString::fromUtf8(byteArray.constData());
  emit messageAvailable(message);
  localSocket->disconnectFromServer();
}

// public functions.
/**
 * @brief SingleApplication::isRunning is there already a QtPass instance
 * running, to check wether to be server or client.
 * @return
 */
bool SingleApplication::isRunning() { return _isRunning; }

/**
 * @brief SingleApplication::sendMessage send a message (from commandline) to an
 * already running QtPass instance.
 * @param message
 * @return
 */
bool SingleApplication::sendMessage(const QString &message) {
  if (!_isRunning)
    return false;
  QLocalSocket localSocket(this);
  localSocket.connectToServer(_uniqueKey, QIODevice::WriteOnly);
  if (!localSocket.waitForConnected(timeout)) {
    dbg() << localSocket.errorString().toLatin1();
    return false;
  }
  localSocket.write(message.toUtf8());
  if (!localSocket.waitForBytesWritten(timeout)) {
    dbg() << localSocket.errorString().toLatin1();
    return false;
  }
  localSocket.disconnectFromServer();
  return true;
}
