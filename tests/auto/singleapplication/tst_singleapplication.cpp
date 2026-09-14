// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QtTest>

#include "../../../src/singleapplication.h"

/**
 * @class tst_singleapplication
 * @brief Tests for the single-instance IPC in SingleApplication.
 *
 * SingleApplication is the QApplication of the process, so the suite ships
 * its own main(): it plants a stale socket file where QLocalServer will want
 * to listen, then constructs the application. Every test talks to that one
 * instance through plain QLocalSocket clients, exactly like a second QtPass
 * launch would.
 */
class tst_singleapplication : public QObject {
  Q_OBJECT

public:
  tst_singleapplication(QString key, bool stalePlanted)
      : m_key(std::move(key)), m_stalePlanted(stalePlanted) {}

private Q_SLOTS:
  void listensDespiteStaleSocket();
  void socketIsUserAccessOnly();
  void messageArrives();
  void emptyPayloadArrivesAsEmptyMessage();
  void silentPeerIsIgnoredAndReleased();
  void firstInstanceIsNotRunning();

private:
  auto app() -> SingleApplication *;
  auto connectClient(QLocalSocket &client) -> bool;
  auto acceptedSockets() -> int;

  QString m_key;
  bool m_stalePlanted;
};

auto tst_singleapplication::app() -> SingleApplication * {
  return qobject_cast<SingleApplication *>(QCoreApplication::instance());
}

auto tst_singleapplication::connectClient(QLocalSocket &client) -> bool {
  client.connectToServer(m_key, QIODevice::WriteOnly);
  return client.waitForConnected(1000);
}

auto tst_singleapplication::acceptedSockets() -> int {
  // QLocalServer parents every accepted socket to itself and the server is
  // parented to the application, so this finds all of them.
  return app()->findChildren<QLocalSocket *>().size();
}

/**
 * Before the fix a leftover socket file made listen() fail (address in use)
 * and the failure was ignored, permanently disabling IPC. main() planted such
 * a file (Unix only) before constructing the application.
 */
void tst_singleapplication::listensDespiteStaleSocket() {
  QVERIFY(app());
#ifndef Q_OS_WIN
  QVERIFY2(m_stalePlanted, "test setup failed to plant a stale socket file");
#endif
  QLocalSocket client;
  QVERIFY2(connectClient(client),
           qPrintable("no server listening: " + client.errorString()));
  client.disconnectFromServer();
}

/**
 * Other local users must not be able to talk to a QtPass instance.
 */
void tst_singleapplication::socketIsUserAccessOnly() {
#ifdef Q_OS_WIN
  QSKIP("named pipe ACLs are not inspectable through QFile");
#else
  const QString path = QDir::cleanPath(QDir::tempPath()) + "/" + m_key;
  const QFileInfo info(path);
  QVERIFY2(info.exists(), qPrintable("socket missing at " + path));
  const QFile::Permissions others = QFile::ReadGroup | QFile::WriteGroup |
                                    QFile::ExeGroup | QFile::ReadOther |
                                    QFile::WriteOther | QFile::ExeOther;
  QVERIFY2((info.permissions() & others) == 0,
           "socket is reachable by group/other users");
#endif
}

void tst_singleapplication::messageArrives() {
  QSignalSpy spy(app(), &SingleApplication::messageAvailable);
  QLocalSocket client;
  QVERIFY(connectClient(client));
  client.write("search me");
  QVERIFY(client.waitForBytesWritten(1000));
  client.disconnectFromServer();

  QVERIFY(spy.wait(2000));
  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("search me"));
  QTRY_COMPARE(acceptedSockets(), 0);
}

/**
 * A launch without arguments sends a single NUL so the peer sees data; it
 * must surface as an empty message (MainWindow then just raises itself).
 */
void tst_singleapplication::emptyPayloadArrivesAsEmptyMessage() {
  QSignalSpy spy(app(), &SingleApplication::messageAvailable);
  QLocalSocket client;
  QVERIFY(connectClient(client));
  client.write(QByteArray(1, '\0'));
  QVERIFY(client.waitForBytesWritten(1000));
  client.disconnectFromServer();

  QVERIFY(spy.wait(2000));
  QCOMPARE(spy.count(), 1);
  QVERIFY(spy.at(0).at(0).toString().isEmpty());
  QTRY_COMPARE(acceptedSockets(), 0);
}

/**
 * A peer that connects and hangs up without sending anything (a probe, or a
 * misbehaving client) must not raise the window, must not stall the event
 * loop and must not leak the accepted socket.
 */
void tst_singleapplication::silentPeerIsIgnoredAndReleased() {
  QSignalSpy spy(app(), &SingleApplication::messageAvailable);
  QLocalSocket client;
  QVERIFY(connectClient(client));
  client.disconnectFromServer();

  QElapsedTimer timer;
  timer.start();
  QTRY_COMPARE(acceptedSockets(), 0);
  QVERIFY2(
      timer.elapsed() < 900,
      qPrintable(
          QStringLiteral("event loop stalled for %1 ms").arg(timer.elapsed())));
  QCoreApplication::processEvents();
  QCOMPARE(spy.count(), 0);
}

void tst_singleapplication::firstInstanceIsNotRunning() {
  QVERIFY(!app()->isRunning());
  QVERIFY(!app()->sendMessage(QStringLiteral("nobody home")));
}

auto main(int argc, char *argv[]) -> int {
  const QString key = QStringLiteral("tst_singleapplication_%1")
                          .arg(QCoreApplication::applicationPid());
  bool stalePlanted = false;
#ifndef Q_OS_WIN
  // Same location QLocalServer resolves a bare name to. bind() refuses any
  // existing path with EADDRINUSE, so a plain file stands in for the socket
  // a crashed instance leaves behind.
  QFile stale(QDir::cleanPath(QDir::tempPath()) + "/" + key);
  stalePlanted = stale.open(QIODevice::WriteOnly);
  stale.close();
#endif
  SingleApplication app(argc, argv, key);
  tst_singleapplication tc(key, stalePlanted);
  QTEST_SET_MAIN_SOURCE_PATH
  return QTest::qExec(&tc, argc, argv);
}

#include "tst_singleapplication.moc"
