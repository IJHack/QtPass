// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief SingleApplication: a QApplication that keeps one QtPass instance per
 *        user via a lock file and forwards later command lines over a local
 *        socket.
 */
#ifndef SRC_SINGLEAPPLICATION_H_
#define SRC_SINGLEAPPLICATION_H_

#include <QApplication>
#include <QLocalServer>
#include <QLockFile>

/**
 * @class SingleApplication
 * @ingroup support
 * @brief QApplication subclass enforcing single-instance behavior via IPC.
 * Detects whether another instance with the same unique key is running and
 * enables forwarding messages (e.g., command-line arguments) to it.
 */
class SingleApplication : public QApplication {
  Q_OBJECT

public:
  /**
   * @brief Construct a SingleApplication with a unique instance key.
   *
   * Takes the lock file for the key (lockPathFor()). Whoever holds it is the
   * instance that listens; a lock left by a crashed instance is stale and
   * taken over. When another live instance holds it this one probes the
   * socket and, if it answers, becomes a secondary: isRunning() turns true.
   * @param argc Reference to program argc forwarded to QApplication.
   * @param argv Program argv forwarded to QApplication.
   * @param uniqueKey Name of the local socket: a full path (see privateKey())
   *        or a bare name, which Qt puts in the temporary folder.
   */
  SingleApplication(int &argc, char *argv[], // NOLINT(modernize-avoid-c-arrays)
                    QString uniqueKey);
  /**
   * @brief The key to construct with: a socket path in a folder only this
   *        user can write to (see privateKeyIn()).
   *
   * The socket used to be named after the login name in the shared
   * temporary folder, next to a shared-memory segment and its semaphore.
   * Another local user could create those first and keep QtPass from
   * starting (scan ANT-2026-1JUBRKLZ). The folder is
   * QStandardPaths::RuntimeLocation, which Qt refuses when another user owns
   * it. On Windows the key stays @p base: a pipe name cannot hold a path.
   * Call it before the application object exists.
   * @param base Per-user name, such as the login name plus "QtPass".
   * @return The key for the constructor.
   */
  static auto privateKey(const QString &base) -> QString;
  /**
   * @brief privateKey() with the folder given.
   *
   * Without a folder the key is @p base plus a random suffix: the launch
   * then runs as its own instance, without IPC, rather than on a name in
   * the shared temporary folder that someone else can take.
   * @param base Per-user name, such as the login name plus "QtPass".
   * @param dir Folder only this user can write to; empty when there is none.
   * @return A path in @p dir named after a hash of @p base, or @p base plus
   *         a random suffix.
   */
  static auto privateKeyIn(const QString &base, const QString &dir) -> QString;
  /**
   * @brief The lock file that marks the instance listening on @p key.
   *
   * Next to the socket for a full path; for a bare name in the temporary
   * folder, where Qt puts that socket too, with any slash replaced.
   * @param key The constructor's key.
   * @return The lock file's path.
   */
  static auto lockPathFor(const QString &key) -> QString;
  /**
   * @brief Query whether another instance is already running.
   * @return true if another instance with the same key is running.
   */
  auto isRunning() -> bool;
  /**
   * @brief Send a message to the running instance.
   *
   * When delivery fails the peer is taken to have exited since the
   * constructor probed it: this instance takes its lock and becomes the
   * listening instance, so isRunning() turns false and the window the caller
   * opens next is reachable by later launches. If the peer still holds the
   * lock it is probed again, as the constructor does.
   * @param message Text to deliver (typically forwarded command-line args).
   * @return true if the message was successfully delivered.
   */
  auto sendMessage(const QString &message) -> bool;

public slots:
  /**
   * @brief Slot for QLocalServer::newConnection: accept every pending
   *        connection and emit messageAvailable() with what it sent.
   *
   * Each accepted socket is read asynchronously so a peer that connects
   * without sending anything cannot stall the GUI thread; the message is
   * delivered once the peer disconnects and the socket is then deleted.
   */
  void receiveMessage();

signals:
  /**
   * @brief messageAvailable notification from commandline
   * @param message args sent to qtpass executable
   */
  void messageAvailable(const QString &message);

private:
  /**
   * @brief Probe whether an instance is actually accepting connections.
   * @return true if a connection to the unique key succeeded.
   */
  auto peerIsListening() -> bool;
  /**
   * @brief Take the lock and listen, or become the lock holder's secondary.
   *
   * A free lock, or one left by a crashed instance, is taken and this
   * instance listens (becomePrimary()). A lock held by a live instance makes
   * this one wait for that instance's socket (waitForPeer()). A lock that
   * cannot be taken at all leaves this instance without IPC.
   */
  void claimOrFollow();
  /**
   * @brief Listen on the key; this instance holds the lock.
   *
   * A socket file left by a crashed instance is removed first. When listen()
   * fails the lock is released and this instance runs without IPC.
   */
  void becomePrimary();
  /**
   * @brief Become the secondary of the live instance that holds the lock.
   *
   * It may still be starting, so it is probed a few times (see
   * takeoverProbes); if it never answers, isRunning() stays false and the
   * caller runs without IPC rather than not at all.
   */
  void waitForPeer();
  /**
   * @brief Deliver a message over the local socket.
   * @param message Text to send; an empty string is sent as a single NUL so
   *        the peer still sees a connection with data.
   * @return true if the peer accepted the whole payload.
   */
  auto forwardMessage(const QString &message) -> bool;

  /**
   * @brief true when another instance holds the lock and answers on the
   *        socket; this instance should forward and exit.
   */
  bool _isRunning;
  /** @brief Name of the local socket; also names the lock file. */
  QString _uniqueKey;
  /** @brief Held by the listening instance for as long as it runs. */
  QLockFile _lock;
  /**
   * @brief Server accepting forwarded command lines; only set in the primary
   *        instance, null in a secondary or when listening failed.
   */
  QScopedPointer<QLocalServer> localServer;

  /** @brief Milliseconds to wait for a socket connect or write. */
  static const int timeout = 1000;
  /**
   * @brief Upper bound for one forwarded message (a command line).
   *
   * The socket is user-only, but a peer that keeps a connection open and
   * streams data would otherwise grow the per-connection buffer without
   * limit; anything past this is dropped and the peer disconnected.
   */
  static const int maxMessageBytes = 64 * 1024;
  /// Probes for the lock's holder to start listening before giving up on IPC.
  static const int takeoverProbes = 5;
  /** @brief Pause between two takeover probes, in milliseconds. */
  static const int takeoverProbeIntervalMs = 100;
};

#endif // SRC_SINGLEAPPLICATION_H_
