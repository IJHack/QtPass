// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_SINGLEAPPLICATION_H_
#define SRC_SINGLEAPPLICATION_H_

#include <QApplication>
#include <QLocalServer>
#include <QSharedMemory>

/*!
    \class SingleApplication
    \brief The SingleApplication class is used for commandline intergration.

    This class needs a bit of work or possibly replacement.
 */
class SingleApplication : public QApplication {
  Q_OBJECT
public:
  SingleApplication(int &argc, char *argv[], QString uniqueKey);
  auto isRunning() -> bool;
  auto sendMessage(const QString &message) -> bool;

public Q_SLOTS:
  void receiveMessage();

Q_SIGNALS:
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
