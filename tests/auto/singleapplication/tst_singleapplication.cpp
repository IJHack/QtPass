// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSharedMemory>
#include <QSignalSpy>
#include <QtTest>
#include <cstring>

#ifndef Q_OS_WIN
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#include "../../../src/singleapplication.h"

/**
 * @brief Stand-in for a QtPass instance that dies between the probe in the
 * SingleApplication constructor and the forward in sendMessage().
 *
 * It holds the shared-memory segment and listens on the socket like a live
 * instance would. vanish() then closes the listening end without unlinking
 * the socket file, which is exactly what a crash leaves behind, and lets go
 * of the segment. Raw POSIX calls because the peer has to be up before the
 * application object exists and QLocalServer needs one to listen.
 */
class FakePeer {
public:
  explicit FakePeer(const QString &key) : m_path(socketPathFor(key)) {
    m_segment.setKey(key);
  }
  ~FakePeer() { vanish(); }
  FakePeer(const FakePeer &) = delete;
  auto operator=(const FakePeer &) -> FakePeer & = delete;

  static auto socketPathFor(const QString &key) -> QString {
    // Same location QLocalServer resolves a bare name to.
    return QDir::cleanPath(QDir::tempPath()) + "/" + key;
  }

  auto start() -> bool {
#ifdef Q_OS_WIN
    return false;
#else
    if (!m_segment.create(1)) {
      return false;
    }
    const QByteArray path = QFile::encodeName(m_path);
    struct sockaddr_un addr{};
    if (path.size() >= static_cast<int>(sizeof(addr.sun_path))) {
      return false;
    }
    addr.sun_family = AF_UNIX;
    ::memcpy(addr.sun_path, path.constData(), path.size() + 1);
    m_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_fd < 0) {
      return false;
    }
    ::unlink(path.constData());
    if (::bind(m_fd, reinterpret_cast<struct sockaddr *>(&addr),
               sizeof(addr)) != 0 ||
        ::listen(m_fd, 5) != 0) {
      vanish();
      return false;
    }
    return true;
#endif
  }

  auto isUp() const -> bool { return m_fd >= 0; }

  void vanish() {
#ifndef Q_OS_WIN
    if (m_fd >= 0) {
      ::close(m_fd);
      m_fd = -1;
    }
#endif
    if (m_segment.isAttached()) {
      m_segment.detach();
    }
  }

private:
  QString m_path;
  QSharedMemory m_segment;
  int m_fd = -1;
};

/**
 * @class tst_singleapplication
 * @brief Tests for the single-instance IPC in SingleApplication.
 *
 * SingleApplication is the QApplication of the process, so the suite ships
 * its own main(). On Unix it first brings up a FakePeer, so the application
 * is constructed as a second instance; the first test makes the peer vanish
 * and verifies the failed forward turns this process into the listening
 * instance, over the socket file the peer left behind. Every later test talks
 * to that instance through plain QLocalSocket clients, exactly like a second
 * QtPass launch would. On Windows there is no fake peer and the application
 * is the first instance from the start.
 */
class tst_singleapplication : public QObject {
  Q_OBJECT

public:
  tst_singleapplication(QString key, FakePeer &peer)
      : m_key(std::move(key)), m_peer(peer) {}

private Q_SLOTS:
  void takesOverWhenPeerVanishesBeforeForward();
  void listensDespiteStaleSocket();
  void socketIsUserAccessOnly();
  void messageArrives();
  void emptyPayloadArrivesAsEmptyMessage();
  void silentPeerIsIgnoredAndReleased();
  void primaryIsNotRunning();

private:
  auto app() -> SingleApplication *;
  auto connectClient(QLocalSocket &client) -> bool;
  auto acceptedSockets() -> int;

  QString m_key;
  FakePeer &m_peer;
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
 * The peer answered the probe in the constructor but is gone by the time the
 * arguments are forwarded. Before the fix this instance fell through to a
 * normal start while still attached to the segment and without a server, so
 * every later launch failed create() and opened yet another window without
 * IPC. Now it releases the segment and becomes the listening instance.
 */
void tst_singleapplication::takesOverWhenPeerVanishesBeforeForward() {
#ifdef Q_OS_WIN
  QSKIP("the fake peer needs a Unix socket");
#endif
  QVERIFY2(m_peer.isUp(), "test setup failed to bring up the fake peer");
  QVERIFY(app()->isRunning());
  QVERIFY(!app()->findChild<QLocalServer *>());

  m_peer.vanish();
  QVERIFY2(QFileInfo::exists(FakePeer::socketPathFor(m_key)),
           "the vanished peer should leave its socket file behind");

  QVERIFY(!app()->sendMessage(QStringLiteral("into the void")));
  QVERIFY(!app()->isRunning());
  QVERIFY2(app()->findChild<QLocalServer *>(),
           "no server after the peer vanished");
  QLocalSocket client;
  QVERIFY2(connectClient(client),
           qPrintable("not reachable after takeover: " + client.errorString()));
  client.disconnectFromServer();
}

/**
 * Before the fix a leftover socket file made listen() fail (address in use)
 * and the failure was ignored, permanently disabling IPC. On Unix the fake
 * peer left such a file behind before this instance started listening.
 */
void tst_singleapplication::listensDespiteStaleSocket() {
  QVERIFY(app());
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
  const QString path = FakePeer::socketPathFor(m_key);
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
 *
 * The connection is only accepted once the event loop runs, so wait for the
 * server's newConnection before asserting anything: checking the socket
 * count straight after connecting would pass before the server ever saw the
 * client.
 */
void tst_singleapplication::silentPeerIsIgnoredAndReleased() {
  auto *server = app()->findChild<QLocalServer *>();
  QVERIFY(server);
  QSignalSpy accepted(server, &QLocalServer::newConnection);
  QSignalSpy spy(app(), &SingleApplication::messageAvailable);
  QLocalSocket client;
  QVERIFY(connectClient(client));
  client.disconnectFromServer();

  QElapsedTimer timer;
  timer.start();
  QVERIFY(accepted.wait(2000));
  QCOMPARE(accepted.count(), 1);
  QTRY_COMPARE(acceptedSockets(), 0);
  QVERIFY2(
      timer.elapsed() < 900,
      qPrintable(
          QStringLiteral("event loop stalled for %1 ms").arg(timer.elapsed())));
  QCOMPARE(spy.count(), 0);
}

void tst_singleapplication::primaryIsNotRunning() {
  QVERIFY(!app()->isRunning());
  QVERIFY(!app()->sendMessage(QStringLiteral("nobody home")));
}

auto main(int argc, char *argv[]) -> int {
  const QString key = QStringLiteral("tst_singleapplication_%1")
                          .arg(QCoreApplication::applicationPid());
  FakePeer peer(key);
  peer.start();
  SingleApplication app(argc, argv, key);
  tst_singleapplication tc(key, peer);
  QTEST_SET_MAIN_SOURCE_PATH
  return QTest::qExec(&tc, argc, argv);
}

#include "tst_singleapplication.moc"
