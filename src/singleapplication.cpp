// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Implements SingleApplication; stale-segment and takeover-race
 *        recovery lives in the constructor and becomePrimary().
 */
#include "singleapplication.h"
#include "qtpasslogging.h"
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QLocalSocket>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QThread>
#include <utility>

namespace {

/**
 * @brief A fresh random suffix for privateKeyIn().
 * @return SingleApplication::privateSuffixLength lowercase hex digits.
 */
auto randomSuffix() -> QString {
  static_assert(SingleApplication::privateSuffixLength == 16,
                "one 64-bit number in hex");
  return QStringLiteral("%1").arg(QRandomGenerator::system()->generate64(),
                                  SingleApplication::privateSuffixLength, 16,
                                  QLatin1Char('0'));
}

/**
 * @brief Read a suffix privateKeyIn() wrote earlier.
 * @param path The suffix file.
 * @return The suffix, or an empty string when the file is missing or does
 *         not hold exactly one.
 */
auto readSuffix(const QString &path) -> QString {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  const QString text =
      QString::fromLatin1(file.read(SingleApplication::privateSuffixLength + 2))
          .trimmed();
  static const QRegularExpression hex(QRegularExpression::anchoredPattern(
      QStringLiteral("[0-9a-f]{%1}")
          .arg(SingleApplication::privateSuffixLength)));
  return hex.match(text).hasMatch() ? text : QString();
}

/**
 * @brief Write @p suffix to @p path unless a file is there already.
 *
 * Written to a temporary file next to it (mode 0600) and renamed into place
 * without replacing: of two launches writing at once, one file wins and the
 * other launch reads it.
 * @param path The suffix file.
 * @param suffix The suffix to keep.
 * @return true when this call put the file in place.
 */
auto writeSuffix(const QString &path, const QString &suffix) -> bool {
  QTemporaryFile file(path + QStringLiteral(".XXXXXX"));
  if (!file.open() || file.write(suffix.toLatin1()) != suffix.size() ||
      !file.flush()) {
    return false;
  }
  if (!file.rename(path)) {
    return false;
  }
  file.setAutoRemove(false);
  return true;
}

} // namespace

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
    qCDebug(lcQtPass) << "Stale single instance segment, taking over.";
    sharedMemory.detach();
  }
  becomePrimary();
}

auto SingleApplication::privateKey(const QString &base) -> QString {
#ifdef Q_OS_WIN
  const QString dir = QDir::tempPath();
#else
  const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
#endif
  return privateKeyIn(base, dir);
}

// The suffix file is named after a hash of the base, so any login name
// makes a valid file name.
auto SingleApplication::privateKeyIn(const QString &base, const QString &dir)
    -> QString {
  if (dir.isEmpty() || !QDir(dir).exists()) {
    qCWarning(lcQtPass) << "SingleApplication: no private folder for the"
                           " instance key; running without single instance.";
    return base + randomSuffix();
  }
  const QString path = QDir(dir).filePath(
      QStringLiteral("qtpass-instance-") +
      QString::fromLatin1(
          QCryptographicHash::hash(base.toUtf8(), QCryptographicHash::Sha256)
              .toHex()
              .left(16)));
  QString suffix = readSuffix(path);
  if (suffix.isEmpty()) {
    // Not written yet, or not one we wrote: put a new one in place. A file
    // that holds anything else is replaced, as nothing can use it.
    if (QFile::exists(path)) {
      QFile::remove(path);
    }
    const QString fresh = randomSuffix();
    suffix = writeSuffix(path, fresh) ? fresh : readSuffix(path);
  }
  if (suffix.isEmpty()) {
    qCWarning(lcQtPass) << "SingleApplication: cannot keep the instance key in"
                        << dir << "; running without single instance.";
    return base + randomSuffix();
  }
  return base + suffix;
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
  // The peer vanished between the probe in the constructor and now. Let go of
  // its segment and take its place, so the window main() opens next is the
  // reachable instance. Staying attached without listening would make every
  // later launch fail create() and open yet another window without IPC.
  _isRunning = false;
  sharedMemory.detach();
  becomePrimary();
  return false;
}

// private functions.
void SingleApplication::becomePrimary() {
  // create shared memory.
  if (!sharedMemory.create(1)) {
    // Another launch claimed the segment first (two launchers recovering
    // from the same crash). Give it a moment to start listening and become
    // its secondary, so main() forwards instead of opening a second window.
    if (sharedMemory.attach()) {
      for (int attempt = 0; attempt < takeoverProbes; ++attempt) {
        if (peerIsListening()) {
          _isRunning = true;
          return;
        }
        QThread::msleep(takeoverProbeIntervalMs);
      }
      sharedMemory.detach();
    }
    qCDebug(lcQtPass) << "Unable to create single instance.";
    return;
  }
  // create local server and listen to incoming messages from other
  // instances. A socket file left behind by a crashed instance would make
  // listen() fail (address in use) and silently disable IPC for good, so
  // clear it first: no live peer answered on it, or create() would have
  // failed.
  QLocalServer::removeServer(_uniqueKey);
  localServer.reset(new QLocalServer(this));
  localServer->setSocketOptions(QLocalServer::UserAccessOption);
  connect(localServer.data(), &QLocalServer::newConnection, this,
          &SingleApplication::receiveMessage);
  if (!localServer->listen(_uniqueKey)) {
    qCWarning(lcQtPass) << "SingleApplication: cannot listen on" << _uniqueKey
                        << ":" << localServer->errorString();
    // Holding the segment without a server would make every later launch
    // attach, fail the probe and then fail create(): no instance could ever
    // take over while this one lives. Release it and run without IPC.
    localServer.reset();
    sharedMemory.detach();
  }
}

auto SingleApplication::forwardMessage(const QString &message) -> bool {
  QLocalSocket localSocket(this);
  localSocket.connectToServer(_uniqueKey, QIODevice::WriteOnly);
  if (!localSocket.waitForConnected(timeout)) {
    qCDebug(lcQtPass) << localSocket.errorString().toLatin1();
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
  probe.disconnectFromServer();
  return true;
}
