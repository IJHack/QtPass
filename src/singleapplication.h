// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_SINGLEAPPLICATION_H_
#define SRC_SINGLEAPPLICATION_H_

#include <QApplication>
#include <QLocalServer>
#include <QSharedMemory>

/**
 * @class SingleApplication
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
   * @brief Slot invoked when an incoming IPC message arrives.
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
   */
  void becomePrimary();
  /**
   * @brief Deliver a message over the local socket.
   * @return true if the peer accepted the whole payload.
   */
  auto forwardMessage(const QString &message) -> bool;

  bool _isRunning;
  QString _uniqueKey;
  QSharedMemory sharedMemory;
  QScopedPointer<QLocalServer> localServer;

  static const int timeout = 1000;
};

#endif // SRC_SINGLEAPPLICATION_H_
