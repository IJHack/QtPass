// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_SINGLEAPPLICATION_H_
#define SRC_SINGLEAPPLICATION_H_

#include <QApplication>
#include <QLocalServer>
#include <QSharedMemory>

/**
 * @brief Provide single-instance application behavior and inter-process messaging using a unique key.
 *
 * This QApplication subclass detects whether another instance with the same unique key is already running
 * and enables sending and receiving textual messages between instances (typically to forward command-line
 * arguments to the already-running instance).
 *
 * @param argc Reference to program argc forwarded to QApplication.
 * @param argv Program argv forwarded to QApplication.
 * @param uniqueKey Identifier used to scope the single-instance mechanism and the local IPC resources.
 */

/**
 * @brief Query whether another instance of the application is already running.
 *
 * @returns `true` if an instance with the same unique key is already running, `false` otherwise.
 */

/**
 * @brief Send a textual message to the running instance identified by the unique key.
 *
 * The message is delivered to the existing instance; intended use is to forward command-line arguments.
 *
 * @param message Text to send to the running instance.
 * @returns `true` if the message was successfully delivered, `false` otherwise.
 */

/**
 * @brief Slot invoked when an incoming message is received on the local server.
 *
 * Emits messageAvailable() with the received payload.
 */

/**
 * @brief Notification emitted when a textual message from another process is received.
 *
 * @param message The message payload (for example, forwarded command-line arguments).
 */
class SingleApplication : public QApplication {
  Q_OBJECT
public:
  SingleApplication(int &argc, char *argv[], QString uniqueKey);
  auto isRunning() -> bool;
  auto sendMessage(const QString &message) -> bool;

public slots:
  void receiveMessage();

signals:
  /**
   * @brief messageAvailable notification from commandline
   * @param message args sent to qtpass executable
   */
  void messageAvailable(const QString &message);

private:
  bool _isRunning;
  QString _uniqueKey;
  QSharedMemory sharedMemory;
  QScopedPointer<QLocalServer> localServer;

  static const int timeout = 1000;
};

#endif // SRC_SINGLEAPPLICATION_H_
