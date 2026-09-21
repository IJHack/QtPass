// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSharedMemory>
#include <QSignalSpy>
#include <QTextStream>
#include <QtTest>
#include <cstdio>
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
 * @brief A second launch of QtPass, played by this very test binary.
 *
 * SingleApplication is the QApplication of its process, so the paths a second
 * instance takes (a stale segment in the constructor, losing the takeover race,
 * a successful forward, listen() failing) can only run in another process.
 * main() re-executes the test binary with TST_SINGLEAPP_HELPER set and the
 * helper reports what its SingleApplication did over stdout, one line per
 * step, while the test drives it over stdin. Its qtpass log goes to stderr
 * with debug enabled so the test can pin the exact branch that was taken and,
 * through waitForLog(), act the moment the helper reaches it.
 *
 * Modes: "hold" claims the segment without listening and blocks until killed
 * (a crashed instance); "probe" constructs a SingleApplication, reports
 * "running=<0|1> server=<0|1>", then for every "send <text>" line on stdin
 * calls sendMessage() and reports "sent=<0|1> running=... server=...".
 */
class Helper {
public:
  Helper(const QByteArray &mode, const QString &key) {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("TST_SINGLEAPP_HELPER"),
               QString::fromLatin1(mode));
    env.insert(QStringLiteral("TST_SINGLEAPP_KEY"), key);
    env.insert(QStringLiteral("QT_LOGGING_RULES"),
               QStringLiteral("qtpass.debug=true"));
    env.insert(QStringLiteral("QT_MESSAGE_PATTERN"),
               QStringLiteral("%{message}"));
    // Under systemd (JOURNAL_STREAM set) Qt logs to the journal when stderr
    // is not a terminal; keep the helper's log on the pipe.
    env.insert(QStringLiteral("QT_LOGGING_TO_CONSOLE"), QStringLiteral("1"));
    env.insert(QStringLiteral("QT_FORCE_STDERR_LOGGING"), QStringLiteral("1"));
    if (!env.contains(QStringLiteral("QT_QPA_PLATFORM"))) {
      env.insert(QStringLiteral("QT_QPA_PLATFORM"),
                 QStringLiteral("offscreen"));
    }
    m_process.setProcessEnvironment(env);
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.start(QCoreApplication::applicationFilePath(), QStringList());
    m_process.waitForStarted(5000);
  }
  ~Helper() {
    if (m_process.state() != QProcess::NotRunning) {
      m_process.kill();
      m_process.waitForFinished(5000);
    }
  }
  Helper(const Helper &) = delete;
  auto operator=(const Helper &) -> Helper & = delete;

  /// Next stdout line, trimmed; empty when none arrives within @p ms.
  auto readLine(int ms = 10000) -> QString {
    QElapsedTimer timer;
    timer.start();
    while (!m_process.canReadLine()) {
      const int remaining = ms - static_cast<int>(timer.elapsed());
      if (remaining <= 0 || m_process.state() == QProcess::NotRunning) {
        break;
      }
      m_process.waitForReadyRead(remaining);
    }
    return QString::fromUtf8(m_process.readLine()).trimmed();
  }
  void send(const QString &line) {
    m_process.write((line + QLatin1Char('\n')).toUtf8());
    m_process.waitForBytesWritten(1000);
  }
  /// Close stdin so the helper exits, and check it did so cleanly.
  auto finish() -> bool {
    m_process.closeWriteChannel();
    return m_process.waitForFinished(10000) &&
           m_process.exitStatus() == QProcess::NormalExit &&
           m_process.exitCode() == 0;
  }
  void kill() {
    m_process.kill();
    m_process.waitForFinished(5000);
  }
  /// Everything the helper has logged so far (its qtpass.debug log, stderr).
  auto log() -> QString {
    m_log += QString::fromUtf8(m_process.readAllStandardError());
    return m_log;
  }
  /**
   * Blocks until @p needle shows up in the log, so a test can act at the exact
   * moment the helper's SingleApplication reaches a branch instead of
   * guessing with a fixed delay. False when the helper exits or @p ms pass
   * without it.
   */
  auto waitForLog(const QString &needle, int ms = 10000) -> bool {
    QElapsedTimer timer;
    timer.start();
    // waitForReadyRead() only returns early for the current read channel;
    // it still buffers stdout meanwhile, so no report line is lost.
    m_process.setReadChannel(QProcess::StandardError);
    bool found = false;
    while (!(found = log().contains(needle))) {
      const int remaining = ms - static_cast<int>(timer.elapsed());
      if (remaining <= 0 || m_process.state() == QProcess::NotRunning) {
        break;
      }
      m_process.waitForReadyRead(remaining);
    }
    m_process.setReadChannel(QProcess::StandardOutput);
    return found;
  }

private:
  QProcess m_process;
  QString m_log;
};

/**
 * @brief Removes a segment a killed helper may leave behind, whatever the
 * test's outcome, so a failing run does not litter System V shared memory.
 */
class SegmentReaper {
public:
  explicit SegmentReaper(const QString &key) : m_key(key) {}
  ~SegmentReaper() {
    QSharedMemory segment;
    segment.setKey(m_key);
    if (segment.attach()) {
      segment.detach(); // last one out removes the segment
    }
  }
  SegmentReaper(const SegmentReaper &) = delete;
  auto operator=(const SegmentReaper &) -> SegmentReaper & = delete;

private:
  QString m_key;
};

/**
 * @brief Entry point of the helper process, see Helper.
 */
auto runHelper(int argc, char *argv[], // NOLINT(modernize-avoid-c-arrays)
               const QByteArray &mode, const QString &key) -> int {
  QTextStream out(stdout);
  QTextStream in(stdin);
  if (mode == "hold") {
    QSharedMemory segment;
    segment.setKey(key);
    if (!segment.create(1)) {
      out << "error=" << segment.errorString() << Qt::endl;
      return 1;
    }
    out << "held" << Qt::endl;
    in.readLine(); // blocks until the test kills us
    return 0;
  }
  if (mode == "probe") {
    SingleApplication app(argc, argv, key);
    auto report = [&](const QString &prefix) {
      out << prefix << "running=" << (app.isRunning() ? 1 : 0)
          << " server=" << (app.findChild<QLocalServer *>() != nullptr ? 1 : 0)
          << Qt::endl;
    };
    report(QString());
    QString line;
    while (in.readLineInto(&line)) {
      if (line.startsWith(QStringLiteral("send "))) {
        const bool sent = app.sendMessage(line.mid(5));
        report(QStringLiteral("sent=%1 ").arg(sent ? 1 : 0));
      }
    }
    return 0;
  }
  out << "error=unknown mode " << mode << Qt::endl;
  return 2;
}

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
  void oversizedPayloadIsDroppedAndPeerDisconnected();
  void primaryIsNotRunning();
  void secondLaunchForwardsMessage();
  void staleSegmentIsTakenOver();
  void takeoverRaceLostToPeerThatStartsListening();
  void takeoverGivesUpWhenNobodyListens();
  void listenFailureReleasesSegment();

private:
  auto app() -> SingleApplication *;
  void ensurePrimary();
  auto connectClient(QLocalSocket &client) -> bool;
  auto flushClient(QLocalSocket &client) -> bool;
  auto acceptedSockets() -> int;

  QString m_key;
  FakePeer &m_peer;
};

auto tst_singleapplication::app() -> SingleApplication * {
  return qobject_cast<SingleApplication *>(QCoreApplication::instance());
}

/**
 * Every test but the takeover one needs this process to be the listening
 * instance. When the suite runs in order the first test gets it there; when a
 * single function is selected on the command line the fake peer is still up,
 * so perform the same takeover here without the assertions.
 */
void tst_singleapplication::ensurePrimary() {
  if (!app()->isRunning()) {
    return;
  }
  m_peer.vanish();
  app()->sendMessage(QStringLiteral("takeover"));
}

auto tst_singleapplication::connectClient(QLocalSocket &client) -> bool {
  client.connectToServer(m_key, QIODevice::WriteOnly);
  return client.waitForConnected(1000);
}

/**
 * The server lives in this very process, so it only accepts the connection
 * and posts its read while the event loop runs. Blocking in
 * waitForBytesWritten() would starve it: on Windows QLocalServer creates its
 * pipes with a zero-sized buffer, so the write cannot even complete until the
 * server reads, and the wait times out. Spin the loop until the payload is
 * out instead.
 */
auto tst_singleapplication::flushClient(QLocalSocket &client) -> bool {
  QElapsedTimer timer;
  timer.start();
  while (client.bytesToWrite() > 0) {
    if (timer.elapsed() > 2000) {
      return false;
    }
    QTest::qWait(10);
  }
  return true;
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
  ensurePrimary();
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
  ensurePrimary();
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
  ensurePrimary();
  QSignalSpy spy(app(), &SingleApplication::messageAvailable);
  QLocalSocket client;
  QVERIFY(connectClient(client));
  client.write("search me");
  QVERIFY(flushClient(client));
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
  ensurePrimary();
  QSignalSpy spy(app(), &SingleApplication::messageAvailable);
  QLocalSocket client;
  QVERIFY(connectClient(client));
  client.write(QByteArray(1, '\0'));
  QVERIFY(flushClient(client));
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
  ensurePrimary();
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

/**
 * A peer that streams more than a command line could ever be must not grow
 * the buffer without bound: the data is dropped, the peer disconnected, no
 * message is emitted and the accepted socket is released.
 */
void tst_singleapplication::oversizedPayloadIsDroppedAndPeerDisconnected() {
  ensurePrimary();
  QSignalSpy spy(app(), &SingleApplication::messageAvailable);
  QLocalSocket client;
  QVERIFY(connectClient(client));
  const QByteArray chunk(16 * 1024, 'x');
  QElapsedTimer timer;
  timer.start();
  // Keep writing until the server hangs up on us (or give up after 5 s).
  while (client.state() == QLocalSocket::ConnectedState &&
         timer.elapsed() < 5000) {
    client.write(chunk);
    QTest::qWait(10);
  }
  QVERIFY2(client.state() != QLocalSocket::ConnectedState,
           "server kept accepting an unbounded payload");
  QTRY_COMPARE(acceptedSockets(), 0);
  QTest::qWait(50);
  QCOMPARE(spy.count(), 0);
}

void tst_singleapplication::primaryIsNotRunning() {
  ensurePrimary();
  QVERIFY(!app()->isRunning());
  QVERIFY(!app()->sendMessage(QStringLiteral("nobody home")));
}

/**
 * A real second launch: the peer (this process) is listening, so the helper's
 * SingleApplication reports isRunning() and sendMessage() delivers the whole
 * payload over the socket, returns true and the message surfaces here through
 * messageAvailable; an empty command line is padded to a NUL so it arrives
 * too. Pins the success path of forwardMessage().
 */
void tst_singleapplication::secondLaunchForwardsMessage() {
#ifdef Q_OS_WIN
  QSKIP("the helper protocol blocks on pipes the server must drain");
#endif
  ensurePrimary();
  QSignalSpy spy(app(), &SingleApplication::messageAvailable);
  Helper second("probe", m_key);
  QCOMPARE(second.readLine(), QStringLiteral("running=1 server=0"));

  second.send(QStringLiteral("send --search launch two"));
  QCOMPARE(second.readLine(), QStringLiteral("sent=1 running=1 server=0"));
  // A launch without arguments: the empty text still has to reach the peer
  // (as a single NUL) so it can raise its window.
  second.send(QStringLiteral("send "));
  QCOMPARE(second.readLine(), QStringLiteral("sent=1 running=1 server=0"));
  QVERIFY2(second.finish(), "second launch did not exit cleanly");

  // Both connections sit in the backlog until this event loop runs; the two
  // sockets are independent, so the order they are drained in is not part of
  // the contract (nor stable across event dispatchers).
  QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 2000);
  QStringList received{spy.at(0).at(0).toString(), spy.at(1).at(0).toString()};
  received.sort();
  QCOMPARE(received,
           QStringList({QString(), QStringLiteral("--search launch two")}));
  QTRY_COMPARE(acceptedSockets(), 0);
  QVERIFY2(!app()->isRunning(), "primary must stay primary");
}

/**
 * A crashed instance leaves its System V segment behind with nobody on the
 * socket. The next launch must notice (the log names the stale segment),
 * release it and become the reachable, listening instance instead of
 * forwarding into the void. The segment goes away with that instance.
 */
void tst_singleapplication::staleSegmentIsTakenOver() {
#ifdef Q_OS_WIN
  QSKIP("a killed process releases its shared memory on Windows");
#endif
  ensurePrimary();
  const QString key = m_key + QStringLiteral("_stale");
  SegmentReaper reaper(key);
  {
    Helper crashed("hold", key);
    QCOMPARE(crashed.readLine(), QStringLiteral("held"));
    crashed.kill();
  }

  Helper next("probe", key);
  QCOMPARE(next.readLine(), QStringLiteral("running=0 server=1"));
  QLocalSocket client;
  client.connectToServer(key, QIODevice::WriteOnly);
  QVERIFY2(client.waitForConnected(1000),
           qPrintable("new instance not reachable: " + client.errorString()));
  client.disconnectFromServer();
  QVERIFY2(next.finish(), "new instance did not exit cleanly");
  QVERIFY2(next.log().contains(QStringLiteral("Stale single instance segment")),
           qPrintable("stale segment was not detected; log:\n" + next.log()));

  QSharedMemory gone;
  gone.setKey(key);
  QVERIFY2(!gone.attach(), "segment outlived the instance that took it over");
}

/**
 * Two launches recover from the same crash: this test plays the one that
 * claimed the segment. The helper sees it, probes it, and is told to forward
 * once the winner's socket is gone. create() fails for the helper, and it must
 * keep probing for the winner instead of giving up at once: the winner starts
 * listening again after the forward has failed and the helper becomes its
 * secondary (isRunning() true, no server of its own) without a second window.
 */
void tst_singleapplication::takeoverRaceLostToPeerThatStartsListening() {
#ifdef Q_OS_WIN
  QSKIP("the helper protocol blocks on pipes the server must drain");
#endif
  ensurePrimary();
  const QString key = m_key + QStringLiteral("_race");
  QSharedMemory winner;
  winner.setKey(key);
  QVERIFY2(winner.create(1), qPrintable(winner.errorString()));
  QLocalServer socket;
  QVERIFY2(socket.listen(key), qPrintable(socket.errorString()));

  Helper loser("probe", key);
  QCOMPARE(loser.readLine(), QStringLiteral("running=1 server=0"));

  socket.close(); // the winner is briefly unreachable
  loser.send(QStringLiteral("send raced"));
  // forwardMessage() logs the connect error the moment it fails; only then is
  // the helper in becomePrimary() probing for us, with takeoverProbes *
  // takeoverProbeIntervalMs (500 ms) to spare. Listening any earlier would
  // let the forward succeed instead.
  QVERIFY2(loser.waitForLog(QStringLiteral("QLocalSocket::connectToServer")),
           qPrintable("forward failure never logged; log:\n" + loser.log()));
  QVERIFY2(socket.listen(key), qPrintable(socket.errorString()));
  QCOMPARE(loser.readLine(), QStringLiteral("sent=0 running=1 server=0"));
  QVERIFY2(loser.finish(), "loser did not exit cleanly");
  QVERIFY2(!loser.log().contains(QStringLiteral("Unable to create")),
           qPrintable("loser gave up on IPC; log:\n" + loser.log()));
  socket.close();
  winner.detach();
}

/**
 * Same race, but the segment's holder never starts listening. After all the
 * probes fail the helper must let go of the segment and run without IPC:
 * neither running nor listening, and only after having actually waited for
 * the holder (the probe interval times the number of probes).
 */
void tst_singleapplication::takeoverGivesUpWhenNobodyListens() {
#ifdef Q_OS_WIN
  QSKIP("the helper process is only exercised on Unix");
#endif
  ensurePrimary();
  const QString key = m_key + QStringLiteral("_deaf");
  SegmentReaper reaper(key);
  QSharedMemory holder;
  holder.setKey(key);
  QVERIFY2(holder.create(1), qPrintable(holder.errorString()));

  Helper orphan("probe", key);
  // The constructor finds our segment with nobody listening and says so right
  // before becomePrimary(); everything from here to the report is the failed
  // create(), the probes and the give-up, so the clock excludes process and
  // QApplication start-up.
  QVERIFY2(orphan.waitForLog(QStringLiteral("Stale single instance segment")),
           qPrintable("stale segment not detected; log:\n" + orphan.log()));
  QElapsedTimer timer;
  timer.start();
  QCOMPARE(orphan.readLine(), QStringLiteral("running=0 server=0"));
  QVERIFY2(
      timer.elapsed() >= 400,
      qPrintable(
          QStringLiteral("gave up after only %1 ms").arg(timer.elapsed())));
  QVERIFY2(orphan.log().contains(QStringLiteral("Unable to create")),
           qPrintable("no give-up in log:\n" + orphan.log()));

  // The helper detached but still lives (its exit alone would release its
  // attachment, proving nothing). With this process gone too the segment has
  // no users left and is removed, so attaching to it must fail.
  holder.detach();
  QSharedMemory gone;
  gone.setKey(key);
  const bool stillHeld = gone.attach();
  if (stillHeld) {
    gone.detach();
  }
  QVERIFY2(!stillHeld, "orphan is still attached to the segment");
  QVERIFY2(orphan.finish(), "orphan did not exit cleanly");
}

/**
 * When listen() fails the instance must not keep the segment: holding it
 * without a server would make every later launch attach, fail the probe, fail
 * create() and give up, so no instance could ever take over. A key that
 * resolves to a socket path in a missing directory makes listen() fail; the
 * instance then reports neither running nor listening, logs the error, and
 * another process can claim the segment while it still lives.
 */
void tst_singleapplication::listenFailureReleasesSegment() {
#ifdef Q_OS_WIN
  QSKIP("named pipe names have no directories to be missing");
#endif
  ensurePrimary();
  const QString key = m_key + QStringLiteral("_missing_dir/socket");
  QVERIFY2(!QFileInfo::exists(FakePeer::socketPathFor(key)),
           "the socket path must not exist for listen() to fail");
  SegmentReaper reaper(key);

  Helper mute("probe", key);
  QCOMPARE(mute.readLine(), QStringLiteral("running=0 server=0"));

  QSharedMemory claim;
  claim.setKey(key);
  QVERIFY2(claim.create(1),
           qPrintable("segment still held after listen failed: " +
                      claim.errorString()));
  claim.detach();
  QVERIFY2(mute.finish(), "instance did not exit cleanly");
  QVERIFY2(mute.log().contains(QStringLiteral("cannot listen on")),
           qPrintable("listen failure not logged:\n" + mute.log()));
}

auto main(int argc, char *argv[]) -> int {
  const QByteArray helperMode = qgetenv("TST_SINGLEAPP_HELPER");
  if (!helperMode.isEmpty()) {
    return runHelper(argc, argv, helperMode,
                     qEnvironmentVariable("TST_SINGLEAPP_KEY"));
  }
  const QString key = QStringLiteral("tst_singleapplication_%1")
                          .arg(QCoreApplication::applicationPid());
  FakePeer peer(key);
  peer.start();
  SingleApplication app(argc, argv, key);
  tst_singleapplication tc(key, peer);
  QTEST_SET_MAIN_SOURCE_PATH
  return QTest::qExec(&tc, QCoreApplication::arguments());
}

#include "tst_singleapplication.moc"
