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
};

#endif // SRC_SINGLEAPPLICATION_H_
