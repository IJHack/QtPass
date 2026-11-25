// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Implements SingleApplication; stale-lock and takeover-race
 *        recovery lives in claimOrFollow() and sendMessage().
 */
#include "singleapplication.h"
#include "qtpasslogging.h"
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLocalSocket>
#include <QRandomGenerator>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QThread>
#include <utility>

#ifdef Q_OS_WIN
#include <vector>
#include <windows.h>
#endif

SingleApplication::SingleApplication(
    int &argc, char *argv[], // NOLINT(modernize-avoid-c-arrays)
    QString uniqueKey)
    : QApplication(argc, argv), _isRunning(false),
      _uniqueKey(std::move(uniqueKey)), _lock(lockPathFor(_uniqueKey)) {
  // Stale only when its process is gone: a running instance holds the lock
  // for as long as it runs, far beyond any age limit.
  _lock.setStaleLockTime(0);
  claimOrFollow();
}

auto SingleApplication::privateKey(const QString &base) -> QString {
#ifdef Q_OS_WIN
  return base;
#else
  const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
  if (!dir.isEmpty()) {
    QDir().mkpath(dir);
  }
  return privateKeyIn(base, dir);
#endif
}

// Named after a hash of the base, so any login name makes a valid file name
// and the path stays well under the 104 bytes macOS allows a socket path.
auto SingleApplication::privateKeyIn(const QString &base, const QString &dir)
    -> QString {
  if (dir.isEmpty() || !QFileInfo(dir).isDir()) {
    qCWarning(lcQtPass) << "SingleApplication: no private folder for the"
                           " socket; running without single instance.";
    return base +
           QStringLiteral("%1").arg(QRandomGenerator::system()->generate64(),
                                    16, 16, QLatin1Char('0'));
  }
  const QByteArray hash =
      QCryptographicHash::hash(base.toUtf8(), QCryptographicHash::Sha256)
          .toHex()
          .left(16);
  return QDir(dir).filePath(QStringLiteral("qtpass-") +
                            QString::fromLatin1(hash));
}

#ifdef Q_OS_WIN
namespace {

/**
 * @brief The TOKEN_USER record of @p token, copied out.
 * @param token A token opened with TOKEN_QUERY.
 * @return The record's bytes; empty when it cannot be read.
 */
auto tokenUser(HANDLE token) -> std::vector<BYTE> {
  DWORD size = 0;
  GetTokenInformation(token, TokenUser, nullptr, 0, &size);
  if (size == 0) {
    return {};
  }
  std::vector<BYTE> record(size);
  if (!GetTokenInformation(token, TokenUser, record.data(), size, &size)) {
    return {};
  }
  return record;
}

/**
 * @brief The user of process @p process, as a TOKEN_USER record.
 * @param process A process handle with query access.
 * @return The record's bytes; empty when it cannot be read.
 */
auto processUser(HANDLE process) -> std::vector<BYTE> {
  HANDLE token = nullptr;
  if (!OpenProcessToken(process, TOKEN_QUERY, &token)) {
    return {};
  }
  std::vector<BYTE> record = tokenUser(token);
  CloseHandle(token);
  return record;
}

} // namespace
#endif

// Windows only: see the header. A server whose process or token this user
// may not open is not this user's.
auto SingleApplication::serverRunsAsThisUser(const QLocalSocket &socket)
    -> bool {
#ifdef Q_OS_WIN
  // NOLINTNEXTLINE(performance-no-int-to-ptr): QLocalSocket gives the HANDLE
  const auto pipe = reinterpret_cast<HANDLE>(socket.socketDescriptor());
  ULONG pid = 0;
  if (pipe == nullptr || pipe == INVALID_HANDLE_VALUE ||
      !GetNamedPipeServerProcessId(pipe, &pid)) {
    return false;
  }
  HANDLE server = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (server == nullptr) {
    return false;
  }
  const std::vector<BYTE> theirs = processUser(server);
  CloseHandle(server);
  const std::vector<BYTE> ours = processUser(GetCurrentProcess());
  if (theirs.empty() || ours.empty()) {
    return false;
  }
  return EqualSid(
             reinterpret_cast<const TOKEN_USER *>(theirs.data())->User.Sid,
             reinterpret_cast<const TOKEN_USER *>(ours.data())->User.Sid) !=
         FALSE;
#else
  Q_UNUSED(socket);
  return true;
#endif
}

auto SingleApplication::lockPathFor(const QString &key) -> QString {
  if (QDir::isAbsolutePath(key)) {
    return key + QStringLiteral(".lock");
  }
  QString name = key;
  name.replace(QLatin1Char('/'), QLatin1Char('_'));
  name.replace(QLatin1Char('\\'), QLatin1Char('_'));
  return QDir(QDir::tempPath()).filePath(name + QStringLiteral(".lock"));
}

// public slots.

void SingleApplication::receiveMessage() {
  while (QLocalSocket *localSocket = localServer->nextPendingConnection()) {
    auto buffer = QSharedPointer<QByteArray>::create();
    connect(localSocket, &QLocalSocket::readyRead, this,
            [localSocket, buffer]() {
              buffer->append(localSocket->readAll());
              if (buffer->size() > maxMessageBytes) {
                // Not a command line any more; drop the peer and its data.
                buffer->clear();
                localSocket->abort();
              }
            });
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
auto SingleApplication::isRunning() -> bool { return _isRunning; }

auto SingleApplication::sendMessage(const QString &message) -> bool {
  if (!_isRunning) {
    return false;
  }
  if (forwardMessage(message)) {
    return true;
  }
  // The peer vanished between the probe in the constructor and now. Take its
  // lock and its place, so the window main() opens next is the reachable
  // instance; if it still holds the lock, it may be restarting its server.
  _isRunning = false;
  claimOrFollow();
  return false;
}

// private functions.
void SingleApplication::claimOrFollow() {
  const bool leftOver = QFileInfo::exists(_lock.fileName());
  if (_lock.tryLock(0)) {
    if (leftOver) {
      // A crash leaves the lock file behind; its process is gone.
      qCDebug(lcQtPass) << "Stale single instance lock, taking over.";
    }
    becomePrimary();
    return;
  }
  if (_lock.error() != QLockFile::LockFailedError) {
    qCWarning(lcQtPass) << "SingleApplication: cannot take" << _lock.fileName()
                        << "; running without IPC.";
    return;
  }
  qCDebug(lcQtPass) << "Single instance lock held, probing its server.";
  waitForPeer();
}

void SingleApplication::becomePrimary() {
  // A socket file left behind by a crashed instance would make listen() fail
  // (address in use) and silently disable IPC for good, so clear it first:
  // this instance holds the lock, so no live instance listens on it.
  QLocalServer::removeServer(_uniqueKey);
  localServer.reset(new QLocalServer(this));
  localServer->setSocketOptions(QLocalServer::UserAccessOption);
  connect(localServer.data(), &QLocalServer::newConnection, this,
          &SingleApplication::receiveMessage);
  if (!localServer->listen(_uniqueKey)) {
    qCWarning(lcQtPass) << "SingleApplication: cannot listen on" << _uniqueKey
                        << ":" << localServer->errorString();
    // Holding the lock without a server would make every later launch wait
    // for a server that never comes: no instance could take over while this
    // one lives. Release it and run without IPC.
    localServer.reset();
    _lock.unlock();
  }
}

void SingleApplication::waitForPeer() {
  for (int attempt = 0; attempt < takeoverProbes; ++attempt) {
    if (peerIsListening()) {
      _isRunning = true;
      return;
    }
    QThread::msleep(takeoverProbeIntervalMs);
  }
  qCDebug(lcQtPass) << "Unable to create single instance.";
}

auto SingleApplication::forwardMessage(const QString &message) -> bool {
  QLocalSocket localSocket(this);
  localSocket.connectToServer(_uniqueKey, QIODevice::WriteOnly);
  if (!localSocket.waitForConnected(timeout)) {
    qCDebug(lcQtPass) << localSocket.errorString().toLatin1();
    return false;
  }
  if (!serverRunsAsThisUser(localSocket)) {
    qCWarning(lcQtPass) << "SingleApplication: the server on" << _uniqueKey
                        << "is not this user's; not forwarding.";
    localSocket.abort();
    return false;
  }
  QByteArray payload = message.toUtf8();
  if (payload.isEmpty()) {
    payload = QByteArray(1, '\0');
  }
  localSocket.write(payload);
  if (!localSocket.waitForBytesWritten(timeout)) {
    qCDebug(lcQtPass) << localSocket.errorString().toLatin1();
    return false;
  }
  localSocket.disconnectFromServer();
  return true;
}

auto SingleApplication::peerIsListening() -> bool {
  QLocalSocket probe;
  probe.connectToServer(_uniqueKey, QIODevice::WriteOnly);
  if (!probe.waitForConnected(timeout)) {
    return false;
  }
  if (!serverRunsAsThisUser(probe)) {
    qCWarning(lcQtPass) << "SingleApplication: the server on" << _uniqueKey
                        << "is not this user's.";
    probe.abort();
    return false;
  }
  probe.disconnectFromServer();
  return true;
}
