// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief SingleApplication: a QApplication that keeps one QtPass instance per
 *        user via a shared-memory segment and forwards later command lines
 *        over a local socket.
 */
#ifndef SRC_SINGLEAPPLICATION_H_
#define SRC_SINGLEAPPLICATION_H_

#include <QApplication>
#include <QLocalServer>
#include <QSharedMemory>

/**
 * @class SingleApplication
 * @ingroup support
 * @brief QApplication subclass enforcing single-instance behavior via IPC.
 *
 * Detects whether another instance with the same unique key is running and
 * enables forwarding messages (e.g., command-line arguments) to it.
 */
class SingleApplication : public QApplication {
  Q_OBJECT

public:
  /**
   * @brief Construct a SingleApplication with a unique instance key.
   *
   * Attaches to the shared-memory segment named @p uniqueKey. If an instance
   * answers on the local socket this becomes a secondary and isRunning()
   * turns true; if the segment is stale (left behind by a crash) or absent,
   * this instance claims it and starts listening.
   * @param argc Reference to program argc forwarded to QApplication.
   * @param argv Program argv forwarded to QApplication.
   * @param uniqueKey Key used to identify and scope the single instance.
   */
  SingleApplication(int &argc, char *argv[], // NOLINT(modernize-avoid-c-arrays)
                    QString uniqueKey);
  /**
   * @brief The key to construct with: @p base plus a random suffix kept in
   *        a folder only this user can write to (see privateKeyIn()).
   *
   * The segment, its semaphore and the socket are named after the key, and
   * on Linux and BSD those names sit in the shared temporary folder. Built
   * from the login name alone, another local user could create them first
   * and keep QtPass from starting (scan ANT-2026-1JUBRKLZ). The suffix is
   * kept in QStandardPaths::RuntimeLocation (Qt refuses that folder when
   * another user owns it); on Windows in the per-user temporary folder.
   * Call it before the application object exists.
   * @param base Per-user name, such as the login name plus "QtPass".
   * @return The key for the constructor.
   */
  static auto privateKey(const QString &base) -> QString;
  /**
   * @brief privateKey() with the folder for the suffix given.
   *
   * The first call writes a random suffix to a file in @p dir, readable by
   * this user only; later calls read it back, so every launch of this user
   * computes the same key. Without a usable folder the suffix is random and
   * not kept: each launch then runs as its own instance, without IPC, rather
   * than on a name someone else can take.
   * @param base Per-user name, such as the login name plus "QtPass".
   * @param dir Folder only this user can write to; empty when there is none.
   * @return @p base followed by privateSuffixLength lowercase hex digits.
   */
  static auto privateKeyIn(const QString &base, const QString &dir) -> QString;
  /**
   * @brief Query whether another instance is already running.
   * @return true if another instance with the same key is running.
   */
  auto isRunning() -> bool;
  /**
   * @brief Send a message to the running instance.
   *
   * When delivery fails the peer is taken to have exited since the
   * constructor probed it: this instance releases its segment and, if it can,
   * becomes the listening instance itself, so isRunning() turns false and the
   * window the caller opens next is reachable by later launches.
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
   * @brief Claim the segment and listen; no-op when another instance holds it.
   *
   * When the segment is already taken the other launcher is probed a few
   * times (see takeoverProbes) and, if it answers, this becomes a secondary.
   * Otherwise isRunning() stays false and the caller runs without IPC rather
   * than not at all.
   */
  void becomePrimary();
  /**
   * @brief Deliver a message over the local socket.
   * @param message Text to send; an empty string is sent as a single NUL so
   *        the peer still sees a connection with data.
   * @return true if the peer accepted the whole payload.
   */
  auto forwardMessage(const QString &message) -> bool;

  /**
   * @brief true when another instance holds the segment and answers on the
   *        socket; this instance should forward and exit.
   */
  bool _isRunning;
  /** @brief Name of both the shared-memory segment and the local socket. */
  QString _uniqueKey;
  /** @brief The one-byte segment whose existence marks a running instance. */
  QSharedMemory sharedMemory;
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
  /// Probes for the winner of a takeover race before giving up on IPC.
  static const int takeoverProbes = 5;
  /** @brief Pause between two takeover probes, in milliseconds. */
  static const int takeoverProbeIntervalMs = 100;

public:
  /**
   * @brief Hex digits privateKey() appends: 64 random bits, short enough to
   *        keep a socket path under the 104 bytes macOS allows.
   */
  static const int privateSuffixLength = 16;
};

#endif // SRC_SINGLEAPPLICATION_H_
