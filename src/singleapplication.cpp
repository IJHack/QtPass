// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "singleapplication.h"
#include <QLocalSocket>
#include <utility>
#ifdef QT_DEBUG
#include "debughelper.h"
#endif

/**
 * @brief SingleApplication::SingleApplication this replaces the QApplication
 * allowing for local socket based communications.
 * @param argc
 * @param argv
 * @param uniqueKey
 */
SingleApplication::SingleApplication(int &argc, char *argv[], QString uniqueKey)
    : QApplication(argc, argv), _uniqueKey(std::move(uniqueKey)) {
  sharedMemory.setKey(_uniqueKey);
  if (sharedMemory.attach()) {
    _isRunning = true;
  } else {
    _isRunning = false;
    // create shared memory.
    if (!sharedMemory.create(1)) {
#ifdef QT_DEBUG
      dbg() << "Unable to create single instance.";
#endif
      return;
    }
    // create local server and listen to incoming messages from other
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
#ifdef QT_DEBUG
    dbg() << localSocket->errorString().toLatin1();
#endif
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
 * running, to check whether to be server or client.
 * @return
 */
auto SingleApplication::isRunning() -> bool { return _isRunning; }

/**
 * @brief SingleApplication::sendMessage send a message (from commandline) to an
 * already running QtPass instance.
 * @param message
 * @return
 */
auto SingleApplication::sendMessage(const QString &message) -> bool {
  if (!_isRunning) {
    return false;
  }
  QLocalSocket localSocket(this);
  localSocket.connectToServer(_uniqueKey, QIODevice::WriteOnly);
  if (!localSocket.waitForConnected(timeout)) {
#ifdef QT_DEBUG
    dbg() << localSocket.errorString().toLatin1();
#endif
    return false;
  }
  QByteArray payload = message.toUtf8();
  if (payload.isEmpty()) {
    payload = QByteArray(1, '\0');
  }
  localSocket.write(payload);
  if (!localSocket.waitForBytesWritten(timeout)) {
#ifdef QT_DEBUG
    dbg() << localSocket.errorString().toLatin1();
#endif
    return false;
  }
  localSocket.disconnectFromServer();
  return true;
}
