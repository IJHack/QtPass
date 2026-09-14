// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "singleapplication.h"
#include <QDebug>
#include <QLocalSocket>
#include <QSharedPointer>
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
SingleApplication::SingleApplication(
    int &argc, char *argv[], // NOLINT(modernize-avoid-c-arrays)
    QString uniqueKey)
    : QApplication(argc, argv), _isRunning(false),
      _uniqueKey(std::move(uniqueKey)) {
  sharedMemory.setKey(_uniqueKey);
  if (sharedMemory.attach()) {
    if (peerIsListening()) {
      _isRunning = true;
      return;
    }
    // The segment outlived the instance that created it (a crash leaves
    // System V shared memory behind on Unix). Nobody answers on the socket,
    // so take over instead of forwarding into the void forever.
#ifdef QT_DEBUG
    dbg() << "Stale single instance segment, taking over.";
#endif
    sharedMemory.detach();
  }
  // create shared memory.
  if (!sharedMemory.create(1)) {
#ifdef QT_DEBUG
    dbg() << "Unable to create single instance.";
#endif
    return;
  }
  // create local server and listen to incoming messages from other
  // instances. A socket file left behind by a crashed instance would make
  // listen() fail (address in use) and silently disable IPC for good, so
  // clear it first: no live peer answered above.
  QLocalServer::removeServer(_uniqueKey);
  localServer.reset(new QLocalServer(this));
  localServer->setSocketOptions(QLocalServer::UserAccessOption);
  connect(localServer.data(), &QLocalServer::newConnection, this,
          &SingleApplication::receiveMessage);
  if (!localServer->listen(_uniqueKey)) {
    qWarning() << "SingleApplication: cannot listen on" << _uniqueKey << ":"
               << localServer->errorString();
  }
}

// public slots.

/**
 * @brief SingleApplication::receiveMessage we have received (a command line)
 * message.
 *
 * Each accepted socket is read asynchronously so a peer that connects without
 * sending anything cannot stall the GUI thread; the message is delivered once
 * the peer disconnects and the socket is then deleted.
 */
void SingleApplication::receiveMessage() {
  while (QLocalSocket *localSocket = localServer->nextPendingConnection()) {
    auto buffer = QSharedPointer<QByteArray>::create();
    connect(
        localSocket, &QLocalSocket::readyRead, this,
        [localSocket, buffer]() { buffer->append(localSocket->readAll()); });
    connect(localSocket, &QLocalSocket::disconnected, this,
            [this, localSocket, buffer]() {
              buffer->append(localSocket->readAll());
              if (!buffer->isEmpty()) {
                emit messageAvailable(QString::fromUtf8(buffer->constData()));
              }
              localSocket->deleteLater();
            });
  }
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

// private functions.
/**
 * @brief SingleApplication::peerIsListening probe whether an instance is
 * actually accepting connections on the local socket.
 * @return true if a connection to the unique key succeeded.
 */
auto SingleApplication::peerIsListening() -> bool {
  QLocalSocket probe;
  probe.connectToServer(_uniqueKey, QIODevice::WriteOnly);
  if (!probe.waitForConnected(timeout)) {
    return false;
  }
  probe.disconnectFromServer();
  return true;
}
