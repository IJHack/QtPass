// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QScopedPointer>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>
#include <atomic>
#include <optional>
#include <utility>
#ifndef Q_OS_WIN
#include <unistd.h>
#endif

#include "../../../src/executor.h"
#include "../../../src/pass.h"
#include "../../../src/qtpasssettings.h"

class tst_executor : public QObject {
  Q_OBJECT

private Q_SLOTS:
#ifndef Q_OS_WIN
  void executeBlockingEcho();
  void executeBlockingWithArgs();
  void executeBlockingWithInput();
  void executeBlockingLargeInputRoundTrips();
  void executeBlockingExitCode();
  void executeBlockingStderr();
  void executeBlockingEmptyArgs();
  void executeBlockingEchoMultiple();
  void executeBlockingSpecialChars();
  void executeBlockingWithEnv();
  void executeBlockingWithEnvEmpty();
  void executeBlockingWithEnvSetsVariable();
  void executeBlockingTwoArgOverload();
  void executeBlockingConstQString();
  void executeAsyncFinishedSignal();
  void executeAsyncCapturesStdout();
  void executeAsyncNonZeroExitCode();
  void executeAsyncStartingSignal();
  void executeAsyncMultipleSequential();
  void executeAsyncWithWorkDir();
  void executeAsyncFailedToStartEmitsError();
  void executeAsyncFailedToStartNoInputDoesNotStall();
  void executeAsyncEmptyExecutableEmitsErrorAndContinues();
  void executeAsyncCrashExitReportsNonZeroCode();
  void cancelNextWhileRunningReturnsMinusOne();
  void cancelNextFromErrorHandlerDropsQueuedItem();
  void dequeuingNamesEveryItemThatLeavesTheQueue();
  void drainedComesOnceTheLastItemIsDone();
  void dequeuingSlotMayCallBackIntoTheExecutor();
  void dequeuingOfADroppedItemMayCallBackIntoTheExecutor();
  void executeAsyncWorkDirDoesNotCarryOver();
  void executeAsyncNonUtf8OutputIsNotDropped();
  void bareNameRunsFromPathOnBothPaths();
  void bundledBinaryNextToTheApplicationWins();
  void nonExecutableFileNextToTheApplicationDoesNotMaskPath();
  void executeBlockingCancelFlagEndsChild();
  void executeBlockingCancelFlagKillsChildIgnoringTerminate();
  void executeBlockingCancelFlagAlreadySetSkipsStart();
  void executeBlockingTimeoutEndsChild();
  void executeBlockingTimeoutKillsChildIgnoringTerminate();
  void executeBlockingTimeoutWithCancelFlag();
  void executeBlockingWithinTimeoutReturnsOutput();
  void wslPrefixBlockingUsesExec();
  void wslPrefixAsyncUsesExec();
  void everyWslSpellingReachesTheSameArgv();
  void translatePathForWslUsesTheConfiguredDistribution();
  void childStartsInNewSessionBlocking();
  void childStartsInNewSessionBlockingOwnProcess();
  void childStartsInNewSessionAsync();
#endif
  void parseWslCommandAcceptsEveryWslSpelling();
  void parseWslCommandRejectsShellCommandLines();
  void wslExecArgsPrependsExec();
  void resolveExecutableRules();
  void resolveExecutableFindsBundledExeOnWindows();
  void executeBlockingNotFound();
  void executeBlockingGpgVersion();
  void gpgSupportsEd25519();
  void getDefaultKeyTemplate();
  void executeBlockingGpgKillAgent();
  void resolveGpgconfCommand();
  void executeBlockingWithEnvNotFound();
  void environmentDefaultsEmpty();
  void setAndGetEnvironment();
  void cancelNextEmptyReturnsMinusOne();
};

#ifndef Q_OS_WIN
void tst_executor::executeBlockingEcho() {
  QString output;
  int result = Executor::executeBlocking("echo", {"hello"}, QString(), &output);
  QVERIFY2(result == 0, "echo should exit successfully");
  QVERIFY2(output.contains("hello"), "output should contain 'hello'");
}

void tst_executor::executeBlockingWithArgs() {
  QString output;
  int result =
      Executor::executeBlocking("echo", {"hello", "world"}, QString(), &output);
  QVERIFY2(result == 0, "echo should exit successfully");
  QVERIFY2(output.contains("hello world"), "output should contain both args");
}

/**
 * @brief Verifies that executeBlocking() sends input to the child process and
 *        captures its standard output.
 */
void tst_executor::executeBlockingWithInput() {
  QString output;
  QString input = "test input";
  int result = Executor::executeBlocking("cat", {}, input, &output);
  QVERIFY2(result == 0, "cat should exit successfully");
  QVERIFY2(output.contains("test input"), "output should echo input");
}

/**
 * @brief Plaintext for gpg goes in on stdin, and a re-encrypted entry can be
 *        large. QProcess::write() only queues; the whole buffer must reach
 *        the child before the write channel closes, or an entry would be
 *        encrypted truncated with exit status 0.
 */
void tst_executor::executeBlockingLargeInputRoundTrips() {
#ifdef Q_OS_WIN
  QSKIP("relies on cat");
#else
  QString input;
  input.reserve(4 * 1024 * 1024 + 64);
  const QString line = QStringLiteral("0123456789abcdefghijklmnopqrstuvwxyz\n");
  while (input.size() < 4 * 1024 * 1024) {
    input += line;
  }
  QString output;
  const int rc = Executor::executeBlocking("cat", {}, input, &output);
  QCOMPARE(rc, 0);
  QCOMPARE(output.size(), input.size());
  QVERIFY2(output == input, "every byte must come back, in order");
#endif
}

/**
 * @brief Verifies that executeBlocking() returns a child's non-zero exit code.
 */
void tst_executor::executeBlockingExitCode() {
  QString output;
  int result = Executor::executeBlocking("false", {}, QString(), &output);
  QVERIFY2(result != 0, "false should exit with non-zero");
}

void tst_executor::executeBlockingStderr() {
  QString output;
  QString err;
  const int rc = Executor::executeBlocking("sh", {"-c", "echo error >&2"},
                                           QString(), &output, &err);
  QCOMPARE(rc, 0);
  QVERIFY2(err.contains("error"), "stderr should contain error");
}

void tst_executor::executeBlockingEmptyArgs() {
  QString output;
  int result = Executor::executeBlocking("echo", {}, QString(), &output);
  QVERIFY2(result == 0, "echo with empty args should succeed");
}

void tst_executor::executeBlockingEchoMultiple() {
  QString output;
  int result = Executor::executeBlocking("sh", {"-c", "echo a && echo b"},
                                         QString(), &output);
  QVERIFY2(result == 0, "shell should exit successfully");
  QVERIFY(output.contains("a"));
  QVERIFY(output.contains("b"));
}

void tst_executor::executeBlockingSpecialChars() {
  QString output;
  int result = Executor::executeBlocking("echo", {"$PATH"}, QString(), &output);
  QVERIFY2(result == 0, "echo should succeed");
  QVERIFY2(output.trimmed() == "$PATH", "literal $PATH should be preserved");
}

// Tests for the third executeBlocking overload:
//   executeBlocking(const QProcessEnvironment &env, const QString &app, ...)

void tst_executor::executeBlockingWithEnv() {
  // Basic: custom env, echo succeeds, output captured.
  QProcessEnvironment env;
  env.insert(QStringLiteral("MY_VAR"), QStringLiteral("hello_env"));
  QString output;
  int result = Executor::executeBlocking(env, "sh", {"-c", "echo ok"}, &output);
  QVERIFY2(result == 0, "sh with custom env should exit 0");
  QVERIFY2(output.contains("ok"), "output should contain 'ok'");
}

void tst_executor::executeBlockingWithEnvEmpty() {
  // Empty env: process starts with an empty environment.
  // On POSIX, running 'env' with empty environment should succeed.
  const QProcessEnvironment env;
  QString output;
  QString err;
  int result =
      Executor::executeBlocking(env, "sh", {"-c", "echo empty"}, &output, &err);
  QVERIFY2(result == 0, "sh with empty env should start");
  QVERIFY2(output.contains("empty"), "output should contain 'empty'");
}

void tst_executor::executeBlockingWithEnvSetsVariable() {
  // Verify that a variable injected via the env parameter is visible inside
  // the process.
  QProcessEnvironment env;
  env.insert(QStringLiteral("QTPASS_TEST_VAR"),
             QStringLiteral("injected_value"));
  QString output;
  int result = Executor::executeBlocking(
      env, "sh", {"-c", "echo $QTPASS_TEST_VAR"}, &output);
  QVERIFY2(result == 0, "sh should exit 0");
  QVERIFY2(output.contains("injected_value"),
           "env variable should be visible in child process");
}

void tst_executor::executeBlockingTwoArgOverload() {
  // The two-argument overload (app, args, out, err) delegates to the
  // three-argument overload with empty input. Verify it captures stdout.
  QString output;
  QString err;
  int result =
      Executor::executeBlocking("echo", {"two-arg-overload"}, &output, &err);
  QVERIFY2(result == 0, "echo two-arg-overload should succeed");
  QVERIFY2(output.contains("two-arg-overload"),
           "output should contain 'two-arg-overload'");
}

void tst_executor::executeBlockingConstQString() {
  // Explicitly verify that the refactored const QString & parameter
  // accepts a const-qualified variable without copies or issues.
  const QString app = QStringLiteral("echo");
  const QStringList args = {QStringLiteral("const-ref-ok")};
  QString output;
  const int result = Executor::executeBlocking(app, args, QString(), &output);
  QVERIFY2(result == 0, "const QString& app should be accepted");
  QVERIFY2(output.contains("const-ref-ok"),
           "output should contain 'const-ref-ok'");
}

#endif

/**
 * @brief Every way of writing the launcher is one WSL command: bare or as a
 *        path, any case, with wsl.exe options before the program.
 */
void tst_executor::parseWslCommandAcceptsEveryWslSpelling() {
  const struct {
    const char *app;
    const char *launcher;
    QStringList options;
    const char *command;
  } cases[] = {
      {"wsl gpg", "wsl", {}, "gpg"},
      {"wsl.exe gpg", "wsl.exe", {}, "gpg"},
      {"WSL /usr/bin/gpg", "WSL", {}, "/usr/bin/gpg"},
      {"WSL.EXE gpg2", "WSL.EXE", {}, "gpg2"},
      {"wsl -e gpg", "wsl", {}, "gpg"},
      {"wsl --exec gpg", "wsl", {}, "gpg"},
      {"wsl -d Debian gpg", "wsl", {"-d", "Debian"}, "gpg"},
      {"wsl --distribution Debian -u me gpg",
       "wsl",
       {"--distribution", "Debian", "-u", "me"},
       "gpg"},
      {"wsl -d Debian --exec gpg", "wsl", {"-d", "Debian"}, "gpg"},
      {"wsl \"/usr/local bin/gpg\"", "wsl", {}, "/usr/local bin/gpg"},
      {"C:\\Windows\\System32\\wsl.exe gpg",
       "C:\\Windows\\System32\\wsl.exe",
       {},
       "gpg"},
      {"/mnt/c/Windows/System32/WSL.EXE gpg",
       "/mnt/c/Windows/System32/WSL.EXE",
       {},
       "gpg"},
  };
  for (const auto &c : cases) {
    const auto wsl = Executor::parseWslCommand(QString::fromLatin1(c.app));
    QVERIFY2(wsl.has_value(), c.app);
    // A bare spelling starts as written outside Windows; on Windows it is
    // `wsl`. A path stays a path anywhere.
    QString launcher = QString::fromLatin1(c.launcher);
#ifdef Q_OS_WIN
    if (!launcher.contains(QLatin1Char('/')) &&
        !launcher.contains(QLatin1Char('\\'))) {
      launcher = QStringLiteral("wsl");
    }
#endif
    QCOMPARE(wsl->launcher, launcher);
    QCOMPARE(wsl->options, c.options);
    QCOMPARE(wsl->command, QString::fromLatin1(c.command));
    const QStringList expectedArgv =
        c.options + QStringList{QStringLiteral("--exec"),
                                QString::fromLatin1(c.command),
                                QStringLiteral("--version")};
    QCOMPARE(wsl->argv({QStringLiteral("--version")}), expectedArgv);
  }
}

/**
 * @brief What is not one program for wsl.exe to start is not a WSL command:
 *        it is started as written, and fails to start, rather than being
 *        handed to a shell.
 */
void tst_executor::parseWslCommandRejectsShellCommandLines() {
  const char *rejected[] = {
      "",
      "wsl",
      "wsl.exe",
      "wsl sh -c \"gpg --version\"",
      "wsl gpg --homedir /x",
      "wsl -d",
      "wsl -d Debian",
      "wslx gpg",
      "gpg",
      "C:\\Program Files\\GnuPG\\bin\\gpg.exe",
      "/usr/bin/gpg",
      "wsl-wrapper gpg",
  };
  for (const char *app : rejected) {
    QVERIFY2(!Executor::parseWslCommand(QString::fromLatin1(app)), app);
  }
}

void tst_executor::wslExecArgsPrependsExec() {
  const QStringList args = Executor::wslExecArgs(
      QStringLiteral("wslpath"), {QStringLiteral("C:\\store\\$(id).gpg")});
  const QStringList expected = {QStringLiteral("--exec"),
                                QStringLiteral("wslpath"),
                                QStringLiteral("C:\\store\\$(id).gpg")};
  QCOMPARE(args, expected);
  QCOMPARE(Executor::wslExecArgs(QStringLiteral("gpg2"), {}),
           QStringList({QStringLiteral("--exec"), QStringLiteral("gpg2")}));
}

void tst_executor::executeBlockingNotFound() {
  QString output;
  int result = Executor::executeBlocking("nonexistent_command_xyz", {},
                                         QString(), &output);
  QVERIFY2(result != 0, "nonexistent should fail");
}

void tst_executor::executeBlockingWithEnvNotFound() {
  // The env-based overload should also fail gracefully for a missing binary.
  QProcessEnvironment env;
  env.insert(QStringLiteral("MY_VAR"), QStringLiteral("irrelevant"));
  QString output;
  int result = Executor::executeBlocking(env, "nonexistent_command_env_xyz", {},
                                         &output);
  QVERIFY2(result != 0,
           "env-overload with nonexistent command should return non-zero");
}

void tst_executor::executeBlockingGpgVersion() {
  QString output;
  QString err;
  int result =
      Executor::executeBlocking("gpg", {"--version"}, QString(), &output, &err);
  if (result != 0) {
    QSKIP("gpg not available");
  }
  QVERIFY2(output.contains("gpg"), "output should contain gpg");
}

void tst_executor::gpgSupportsEd25519() {
  QString output;
  QString err;
  int result =
      Executor::executeBlocking("gpg", {"--version"}, QString(), &output, &err);
  if (result != 0) {
    QSKIP("gpg not available");
  }
  bool supported = Pass::gpgSupportsEd25519();
  QRegularExpression versionRegex(R"(gpg \(GnuPG\) (\d+)\.(\d+))");
  QRegularExpressionMatch match = versionRegex.match(output);
  QVERIFY2(match.hasMatch(), "Could not parse gpg version output");
  int major = match.captured(1).toInt();
  int minor = match.captured(2).toInt();
  bool expectedSupported = major > 2 || (major == 2 && minor >= 1);
  if (supported != expectedSupported) {
    QSKIP("GPG version mismatch between test and Pass::gpgSupportsEd25519");
  }
}

void tst_executor::getDefaultKeyTemplate() {
  QString templateStr = Pass::getDefaultKeyTemplate();
  QVERIFY2(!templateStr.isEmpty(), "Default key template should not be empty");
  QVERIFY2(templateStr.contains("Key-Type"),
           "Template should contain Key-Type");
}

void tst_executor::executeBlockingGpgKillAgent() {
#ifndef Q_OS_WIN
  // Point gpgconf at a throwaway GNUPGHOME: with the developer's real home
  // this test killed their session gpg-agent, dropping the passphrase cache
  // and leaving the respawned agent without the desktop's pinentry setup.
  QTemporaryDir gnupgHome;
  QVERIFY2(gnupgHome.isValid(), "temporary GNUPGHOME should be creatable");
  QVERIFY2(QFile::setPermissions(gnupgHome.path(), QFile::ReadOwner |
                                                       QFile::WriteOwner |
                                                       QFile::ExeOwner),
           "temporary GNUPGHOME must permit only owner access");
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert(QStringLiteral("GNUPGHOME"), gnupgHome.path());
  QString output;
  QString err;
  int result = Executor::executeBlocking(
      env, "gpgconf", {"--kill", "gpg-agent"}, &output, &err);
  if (result != 0) {
    QSKIP("gpgconf not available in PATH");
  }
  QVERIFY2(result == 0, "gpgconf --kill gpg-agent should succeed");
#else
  QSKIP("gpgconf not available on Windows");
#endif
}

void tst_executor::resolveGpgconfCommand() {
  // Empty input
  {
    auto result = Pass::resolveGpgconfCommand("");
    QVERIFY2(result.program == "gpgconf",
             "Empty input should fallback to gpgconf");
  }

  // WSL simple
  {
    auto result = Pass::resolveGpgconfCommand("wsl gpg2");
    QStringList expectedArgs = {"--exec", "gpgconf"};
    QVERIFY2(result.program == "wsl" && result.arguments == expectedArgs,
             "WSL simple should replace gpg with gpgconf and run it directly");
  }

  // WSL with an explicit --exec / -e is not doubled
  {
    auto result = Pass::resolveGpgconfCommand("wsl -e gpg2");
    QStringList expectedArgs = {"--exec", "gpgconf"};
    QVERIFY2(result.program == "wsl" && result.arguments == expectedArgs,
             "WSL with -e should run gpgconf through a single --exec");
    result = Pass::resolveGpgconfCommand("wsl --exec gpg2");
    QVERIFY2(result.program == "wsl" && result.arguments == expectedArgs,
             "WSL with --exec should not add a second --exec");
  }

  // WSL with distro
  {
    auto result = Pass::resolveGpgconfCommand("wsl -d Debian gpg2");
    QVERIFY2(result.program == "wsl", "WSL distro preserves wsl");
    QStringList expectedArgs = {"-d", "Debian", "--exec", "gpgconf"};
    QVERIFY2(result.arguments == expectedArgs,
             "WSL distro arguments should be preserved before --exec");
  }

  // wsl.exe, in any case, is the same launcher
  {
    auto result = Pass::resolveGpgconfCommand("WSL.EXE /usr/bin/gpg");
    QStringList expectedArgs = {"--exec", "/usr/bin/gpgconf"};
#ifdef Q_OS_WIN
    const QString launcher = QStringLiteral("wsl");
#else
    const QString launcher = QStringLiteral("WSL.EXE");
#endif
    QVERIFY2(result.program == launcher && result.arguments == expectedArgs,
             "wsl.exe must be recognised like wsl");
  }

  // WSL with full path
  {
    auto result = Pass::resolveGpgconfCommand("wsl /usr/bin/gpg2");
    QStringList expectedArgs = {"--exec", "/usr/bin/gpgconf"};
    QVERIFY2(result.program == "wsl" && result.arguments == expectedArgs,
             "WSL with full path should preserve directory");
  }

  // WSL complex (should fallback)
  {
    auto result = Pass::resolveGpgconfCommand("wsl sh -c \"gpg2 --version\"");
    QVERIFY2(result.program == "gpgconf", "Complex WSL shell should fallback");
  }

  // WSL malformed (only "wsl")
  {
    auto result = Pass::resolveGpgconfCommand("wsl");
    QVERIFY2(result.program == "gpgconf", "Malformed WSL should fallback");
  }

  // PATH-only
  {
    auto result = Pass::resolveGpgconfCommand("gpg2");
    QVERIFY2(result.program == "gpgconf" && result.arguments.isEmpty(),
             "PATH-only should fallback with no extra arguments");
  }

  // Unix absolute - use temp directory for filesystem-independent test
  {
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temp directory should be valid");
    QString absPath = tempDir.path() + "/gpg2";
    QFile gpg2File(absPath);
    QVERIFY2(gpg2File.open(QIODevice::WriteOnly),
             "Should be able to create temporary gpg2 file");
    gpg2File.close();
    auto result = Pass::resolveGpgconfCommand(absPath);
    QVERIFY2(result.program == "gpgconf", "Absolute path should fallback");
  }
}

void tst_executor::environmentDefaultsEmpty() {
  Executor exec;
  QVERIFY2(exec.environment().isEmpty(),
           "default executor environment must be empty");
}

void tst_executor::setAndGetEnvironment() {
  Executor exec;
  QProcessEnvironment env;
  env.insert(QStringLiteral("QTPASS_FOO"), QStringLiteral("bar"));
  env.insert(QStringLiteral("QTPASS_BAZ"), QStringLiteral("qux"));
  exec.setEnvironment(env);
  const QProcessEnvironment actual = exec.environment();
  QCOMPARE(actual.value(QStringLiteral("QTPASS_FOO")), QStringLiteral("bar"));
  QCOMPARE(actual.value(QStringLiteral("QTPASS_BAZ")), QStringLiteral("qux"));
}

void tst_executor::cancelNextEmptyReturnsMinusOne() {
  Executor exec;
  QCOMPARE(exec.cancelNext(), -1);
}

#ifndef Q_OS_WIN
void tst_executor::executeAsyncFinishedSignal() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  Executor exec;
  QSignalSpy spy(&exec, &Executor::finished);
  QVERIFY2(spy.isValid(),
           "spy must connect to Executor::finished(int,int,...) signal");
  exec.execute(42, sh, {"-c", "echo async-hello"}, true, false);
  QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
  const QList<QVariant> args = spy.first();
  QCOMPARE(args.at(0).toInt(), 42);
  QCOMPARE(args.at(1).toInt(), 0);
  QVERIFY2(args.at(2).toString().contains("async-hello"),
           "stdout must contain 'async-hello'");
}

void tst_executor::executeAsyncCapturesStdout() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  Executor exec;
  QSignalSpy spy(&exec, &Executor::finished);
  QVERIFY2(spy.isValid(), "spy must connect to Executor::finished signal");
  exec.execute(1, sh, {"-c", "echo captured-output"}, true, false);
  QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
  const QString output = spy.first().at(2).toString();
  QVERIFY2(output.contains("captured-output"),
           "stdout must contain 'captured-output'");
}

void tst_executor::executeAsyncNonZeroExitCode() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  Executor exec;
  QSignalSpy spy(&exec, &Executor::finished);
  QVERIFY2(spy.isValid(), "spy must connect to Executor::finished signal");
  exec.execute(7, sh, {"-c", "exit 1"}, false, false);
  QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
  const int exitCode = spy.first().at(1).toInt();
  QVERIFY2(exitCode != 0, "sh -c 'exit 1' must exit with non-zero code");
}

void tst_executor::executeAsyncFailedToStartEmitsError() {
  // Regression test for #1598: a command that carries stdin but fails to start
  // must surface an error rather than being silently dropped. Previously
  // executeNext() dequeued it without emitting finished() or error(), leaving
  // callers (e.g. the GPG keygen dialog) hanging with no feedback.
  Executor exec;
  QSignalSpy errorSpy(&exec, &Executor::error);
  QSignalSpy finishedSpy(&exec, &Executor::finished);
  QVERIFY2(errorSpy.isValid(), "spy must connect to Executor::error signal");
  QVERIFY2(finishedSpy.isValid(),
           "spy must connect to Executor::finished signal");
  // Non-empty input forces the waitForStarted() branch in executeNext().
  exec.execute(1, "/nonexistent/definitely-not-a-real-binary",
               {"--gen-key", "--no-tty", "--batch"},
               QStringLiteral("Key-Type: RSA\n%commit\n"), true, true);
  QTRY_COMPARE_WITH_TIMEOUT(errorSpy.count(), 1, 5000);
  QCOMPARE(finishedSpy.count(), 0);
  QCOMPARE(errorSpy.first().at(0).toInt(), 1);
  QVERIFY2(errorSpy.first().at(1).toInt() != 0,
           "a failed-to-start process must report a non-zero exit code");
}

void tst_executor::executeAsyncFailedToStartNoInputDoesNotStall() {
  // A command WITHOUT stdin that fails to start used to leave `running` true
  // forever: QProcess emits errorOccurred(FailedToStart) but never finished(),
  // and the no-input path skipped waitForStarted(), so the whole queue stalled.
  // It must now surface an error and let queued commands proceed.
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  Executor exec;
  QSignalSpy errorSpy(&exec, &Executor::error);
  QSignalSpy finishedSpy(&exec, &Executor::finished);
  QVERIFY2(errorSpy.isValid(), "spy must connect to Executor::error signal");
  QVERIFY2(finishedSpy.isValid(),
           "spy must connect to Executor::finished signal");

  // No input -> the path that previously skipped the start check and stalled.
  exec.execute(1, "/nonexistent/definitely-not-a-real-binary", {"status"},
               false, false);
  // A valid command queued behind the failure must still run.
  exec.execute(2, sh, {"-c", "echo after-failure"}, true, false);

  QTRY_COMPARE_WITH_TIMEOUT(errorSpy.count(), 1, 5000);
  QCOMPARE(errorSpy.first().at(0).toInt(), 1);
  QVERIFY2(errorSpy.first().at(1).toInt() != 0,
           "failed-to-start must report a non-zero exit code");

  // The queue did not stall: the second command completed.
  QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
  QCOMPARE(finishedSpy.first().at(0).toInt(), 2);
}

void tst_executor::executeAsyncEmptyExecutableEmitsErrorAndContinues() {
  // Regression test for #1682: an empty executable (e.g. git not configured
  // yet) used to be dropped silently. The command stayed at the head of the
  // queue forever, swallowing every later completion signal. It must now
  // surface an error through the normal path and let queued commands proceed.
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  Executor exec;
  QSignalSpy errorSpy(&exec, &Executor::error);
  QSignalSpy finishedSpy(&exec, &Executor::finished);
  QVERIFY2(errorSpy.isValid(), "spy must connect to Executor::error signal");
  QVERIFY2(finishedSpy.isValid(),
           "spy must connect to Executor::finished signal");

  exec.execute(1, "", {}, true, false);
  // A valid command queued behind the empty executable must still run.
  exec.execute(2, sh, {"-c", "echo after-empty"}, true, false);

  QTRY_COMPARE_WITH_TIMEOUT(errorSpy.count(), 1, 5000);
  QCOMPARE(errorSpy.first().at(0).toInt(), 1);
  QVERIFY2(errorSpy.first().at(1).toInt() != 0,
           "an empty executable must report a non-zero exit code");

  // The queue did not wedge: the following command completed.
  QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 5000);
  QCOMPARE(finishedSpy.first().at(0).toInt(), 2);
}

void tst_executor::executeAsyncCrashExitReportsNonZeroCode() {
  // A signal-killed process is reported by QProcess as CrashExit with an
  // undefined exit code (the signal number on Unix); Executor reports it as
  // -1 so Pass::finished never mistakes it for success or for grep's
  // exit-code-1 "no matches".
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  Executor exec;
  QSignalSpy errorSpy(&exec, &Executor::error);
  QVERIFY2(errorSpy.isValid(), "spy must connect to Executor::error signal");
  exec.execute(2, sh, {"-c", "kill -SEGV $$"}, true, true);
  QTRY_COMPARE_WITH_TIMEOUT(errorSpy.count(), 1, 5000);
  QCOMPARE(errorSpy.first().at(1).toInt(), -1);
  // sh dies without writing anything, so the message has to come from us.
  const QString err = errorSpy.first().at(3).toString();
  QVERIFY2(
      err.contains(sh),
      qPrintable("crash without stderr must still name the program: " + err));
}

void tst_executor::executeAsyncStartingSignal() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  Executor exec;
  QSignalSpy startSpy(&exec, &Executor::starting);
  QSignalSpy doneSpy(&exec, &Executor::finished);
  QVERIFY2(doneSpy.isValid(),
           "doneSpy must connect to Executor::finished signal");
  exec.execute(3, sh, {"-c", "echo starting-test"}, false, false);
  QTRY_COMPARE_WITH_TIMEOUT(doneSpy.count(), 1, 5000);
  QVERIFY2(startSpy.count() >= 1, "starting signal must have been emitted");
}

void tst_executor::executeAsyncMultipleSequential() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  Executor exec;
  QSignalSpy spy(&exec, &Executor::finished);
  QVERIFY2(spy.isValid(), "spy must connect to Executor::finished signal");
  exec.execute(10, sh, {"-c", "echo first"}, true, false);
  exec.execute(11, sh, {"-c", "echo second"}, true, false);
  QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 5000);
  QCOMPARE(spy.at(0).at(0).toInt(), 10);
  QCOMPARE(spy.at(1).at(0).toInt(), 11);
}

void tst_executor::executeAsyncWithWorkDir() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  QTemporaryDir tmp;
  QVERIFY2(tmp.isValid(), "temp dir must be valid");
  Executor exec;
  QSignalSpy spy(&exec, &Executor::finished);
  QVERIFY2(spy.isValid(), "spy must connect to Executor::finished signal");
  exec.execute(5, tmp.path(), sh, {"-c", "pwd"}, true, false);
  QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
  const QString output = spy.first().at(2).toString().trimmed();
  QVERIFY2(!output.isEmpty(), "pwd must produce output");
  // Compare canonical paths: on macOS the temp dir lives under /var (a symlink
  // to /private/var) and the child's pwd reports the resolved physical path, so
  // a plain string compare fails. canonicalFilePath() resolves symlinks on both
  // sides so equivalent locations compare equal on every platform.
  QVERIFY2(QFileInfo(output).canonicalFilePath() ==
               QFileInfo(tmp.path()).canonicalFilePath(),
           "working directory must match the tmp path");
}

void tst_executor::cancelNextWhileRunningReturnsMinusOne() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  Executor exec;
  exec.execute(1, sh, {"-c", "sleep 2"}, false, false);
  exec.execute(2, sh, {"-c", "echo queued"}, false, false);
  QCOMPARE(exec.cancelNext(), -1);
}

/**
 * @brief The cancel cascade ImitatePass relies on: while the error signal for
 *        a failed command is being handled the queue is not running, so
 *        cancelNext() removes the next item and it never starts. A command
 *        queued afterwards still runs.
 */
void tst_executor::cancelNextFromErrorHandlerDropsQueuedItem() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  Executor exec;
  QSignalSpy finished(&exec, &Executor::finished);
  QSignalSpy errors(&exec, &Executor::error);
  // A non-zero exit is a normal exit, so it arrives on finished(); Pass
  // routes both signals to the same slot.
  int cancelled = -2;
  connect(&exec, &Executor::finished, &exec,
          [&exec, &cancelled](int, int exitCode) {
            if (exitCode != 0) {
              cancelled = exec.cancelNext();
            }
          });

  exec.execute(1, sh, {"-c", "exit 3"}, false, true);
  exec.execute(2, sh, {"-c", "echo must-not-run"}, true, false);
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
  QCOMPARE(finished.first().at(1).toInt(), 3);
  QCOMPARE(cancelled, 2);

  exec.execute(3, sh, {"-c", "echo after"}, true, false);
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 5000);
  QCOMPARE(finished.at(1).at(0).toInt(), 3);
  QVERIFY2(finished.at(1).at(2).toString().contains("after"),
           "the command queued after the cancel must run");
  QTest::qWait(200);
  QCOMPARE(finished.count(), 2); // item 2 never ran
  QCOMPARE(errors.count(), 0);
}

/**
 * @brief dequeuing() names every item as it leaves the queue, in order: one
 *        that cannot start (no executable), one that runs, one that
 *        cancelNext() drops (the only one no completion follows), and one
 *        that runs after it. It comes before
 *        the item runs, so a caller can look at the world as it is then.
 */
void tst_executor::dequeuingNamesEveryItemThatLeavesTheQueue() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  Executor exec;
  QSignalSpy finished(&exec, &Executor::finished);
  QList<int> order;
  QList<int> finishedSoFar;
  QList<bool> completes;
  connect(&exec, &Executor::dequeuing, &exec,
          [&order, &finishedSoFar, &finished, &completes](int id, bool c) {
            order << id;
            finishedSoFar << static_cast<int>(finished.count());
            completes << c;
          });
  connect(&exec, &Executor::finished, &exec, [&exec](int id) {
    if (id == 2) {
      exec.cancelNext();
    }
  });

  exec.execute(1, "", {}, false, false);
  exec.execute(2, sh, {"-c", "exit 0"}, false, false);
  exec.execute(3, sh, {"-c", "echo must-not-run"}, true, false);
  exec.execute(4, sh, {"-c", "exit 0"}, false, false);
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 5000);
  QCOMPARE(order, (QList<int>{1, 2, 3, 4}));
  // All but the dropped one complete.
  QCOMPARE(completes, (QList<bool>{true, true, false, true}));
  // Item 2 left the queue before anything had finished, item 4 after 2.
  QCOMPARE(finishedSoFar.at(1), 0);
  QCOMPARE(finishedSoFar.at(3), 1);
  QCOMPARE(finished.at(1).at(0).toInt(), 4);
}

/**
 * @brief A slot on dequeuing() that calls back into the executor: queuing
 *        more waits behind the item that is leaving, and cancelNext() drops
 *        nothing while that item counts as running (-1, as for any running
 *        head). Every item runs once, in order.
 */
void tst_executor::dequeuingSlotMayCallBackIntoTheExecutor() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  Executor exec;
  QSignalSpy finished(&exec, &Executor::finished);
  QList<int> dequeued;
  int cancelled = -2;
  connect(&exec, &Executor::dequeuing, &exec,
          [&exec, &dequeued, &cancelled, &sh](int id, bool) {
            dequeued << id;
            if (id == 1) {
              // Enough to make the queue's storage grow under the head.
              for (int more = 2; more <= 40; ++more) {
                exec.execute(more, sh, {"-c", "exit 0"}, false, false);
              }
              cancelled = exec.cancelNext();
            }
          });

  exec.execute(1, sh, {"-c", "echo first"}, true, false);
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 40, 10000);
  QTest::qWait(100);
  QCOMPARE(finished.count(), 40);
  QCOMPARE(cancelled, -1);
  QList<int> ran;
  for (const QList<QVariant> &call : std::as_const(finished)) {
    ran << call.at(0).toInt();
  }
  QList<int> expected;
  for (int id = 1; id <= 40; ++id) {
    expected << id;
  }
  QCOMPARE(ran, expected);
  QCOMPARE(dequeued, expected);
  QVERIFY(finished.first().at(2).toString().contains(QStringLiteral("first")));
}

/**
 * @brief The same for an item cancelNext() drops (completes == false), as
 *        ImitatePass does from a completion handler: a slot's cancelNext()
 *        drops nothing more, and what it queues starts only after the
 *        signal has returned, once the handler is done.
 */
void tst_executor::dequeuingOfADroppedItemMayCallBackIntoTheExecutor() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  Executor exec;
  QSignalSpy finished(&exec, &Executor::finished);
  bool inDroppedSignal = false;
  bool startedInside = false;
  int nestedCancel = -2;
  int cancelled = -2;
  connect(&exec, &Executor::dequeuing, &exec, [&](int id, bool completes) {
    if (inDroppedSignal) {
      startedInside = true; // something left the queue meanwhile
      return;
    }
    if (!completes && id == 2) {
      inDroppedSignal = true;
      nestedCancel = exec.cancelNext();
      exec.execute(4, sh, {"-c", "exit 0"}, false, false);
      inDroppedSignal = false;
    }
  });
  connect(&exec, &Executor::finished, &exec,
          [&exec, &cancelled](int id, int exitCode) {
            if (id == 1 && exitCode != 0) {
              cancelled = exec.cancelNext();
            }
          });

  exec.execute(1, sh, {"-c", "exit 3"}, false, false);
  exec.execute(2, sh, {"-c", "echo must-not-run"}, true, false);
  exec.execute(3, sh, {"-c", "exit 0"}, false, false);
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 3, 10000);
  QTest::qWait(100);
  QCOMPARE(cancelled, 2);
  QCOMPARE(nestedCancel, -1);
  QVERIFY2(!startedInside, "nothing starts while the signal is out");
  QList<int> ran;
  for (const QList<QVariant> &call : std::as_const(finished)) {
    ran << call.at(0).toInt();
  }
  QCOMPARE(ran, (QList<int>{1, 3, 4}));
}

/**
 * @brief drained() comes once the queue is empty and nothing runs, after
 *        the last item's finished() (or a failed start's error()), not
 *        between items; isIdle() says the same.
 */
void tst_executor::drainedComesOnceTheLastItemIsDone() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  Executor exec;
  QList<QString> events;
  connect(&exec, &Executor::finished, &exec, [&events](int id) {
    events << QStringLiteral("finished %1").arg(id);
  });
  connect(&exec, &Executor::error, &exec,
          [&events](int id) { events << QStringLiteral("error %1").arg(id); });
  connect(&exec, &Executor::drained, &exec, [&events, &exec]() {
    events << QStringLiteral("drained");
    QVERIFY(exec.isIdle());
  });
  QVERIFY(exec.isIdle());
  exec.execute(1, sh, {"-c", "exit 0"}, false, false);
  exec.execute(2, sh, {"-c", "exit 0"}, false, false);
  QVERIFY(!exec.isIdle());
  QTRY_VERIFY_WITH_TIMEOUT(events.contains(QStringLiteral("drained")), 5000);
  QTest::qWait(100);
  QCOMPARE(events, (QList<QString>{QStringLiteral("finished 1"),
                                   QStringLiteral("finished 2"),
                                   QStringLiteral("drained")}));

  // A failed start: its posted error first, then drained.
  events.clear();
  exec.execute(3, "", {}, false, false);
  QTRY_VERIFY_WITH_TIMEOUT(events.contains(QStringLiteral("drained")), 5000);
  QTest::qWait(100);
  QCOMPARE(events, (QList<QString>{QStringLiteral("error 3"),
                                   QStringLiteral("drained")}));

  // Two failed starts queued before the event loop runs: both errors, then
  // one drained, not one between them.
  events.clear();
  exec.execute(4, "", {}, false, false);
  exec.execute(5, "", {}, false, false);
  QTRY_VERIFY_WITH_TIMEOUT(events.contains(QStringLiteral("drained")), 5000);
  QTest::qWait(100);
  QCOMPARE(events,
           (QList<QString>{QStringLiteral("error 4"), QStringLiteral("error 5"),
                           QStringLiteral("drained")}));
}

/**
 * @brief A queued item without a working directory runs in the application's
 *        cwd even when the previous item set one on the shared QProcess.
 */
void tst_executor::executeAsyncWorkDirDoesNotCarryOver() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  Executor exec;
  QSignalSpy spy(&exec, &Executor::finished);
  exec.execute(1, tmp.path(), sh, {"-c", "pwd"}, true, false);
  exec.execute(2, sh, {"-c", "pwd"}, true, false);
  QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 5000);
  const QString first = spy.at(0).at(2).toString().trimmed();
  const QString second = spy.at(1).at(2).toString().trimmed();
  QCOMPARE(QFileInfo(first).canonicalFilePath(),
           QFileInfo(tmp.path()).canonicalFilePath());
  QCOMPARE(QFileInfo(second).canonicalFilePath(),
           QFileInfo(QDir::currentPath()).canonicalFilePath());
}

/**
 * @brief Output that is not valid UTF-8 (a Latin-1 byte from an old gpg or a
 *        user's shell) still reaches the caller instead of being dropped. A
 *        stray byte at the very end used to vanish: the stateful decoder kept
 *        it back as the start of a multi-byte sequence.
 */
void tst_executor::executeAsyncNonUtf8OutputIsNotDropped() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  Executor exec;
  QSignalSpy spy(&exec, &Executor::finished);
  // \351 is 'é' in Latin-1 and an invalid lead byte in UTF-8.
  exec.execute(1, sh, {"-c", "printf 'abc\\351'"}, true, false);
  QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
  const QString out = spy.first().at(2).toString();
  QVERIFY2(out.startsWith(QStringLiteral("abc")),
           qPrintable("the valid prefix must survive: " + out));
  QCOMPARE(out.size(), 4); // one character for the odd byte, not nothing
}

/**
 * @brief A bare executable name ("gpg" typed into the settings) used to run
 *        through PATH on the blocking path but as <appDir>/gpg on the queued
 *        path, which does not exist unless bundled — the same setting ran two
 *        different binaries. Both go through resolveExecutable() now.
 */
void tst_executor::bareNameRunsFromPathOnBothPaths() {
  if (QStandardPaths::findExecutable("sh").isEmpty())
    QSKIP("sh not found in PATH");
  QString out;
  QCOMPARE(Executor::executeBlocking(QStringLiteral("sh"),
                                     {"-c", "echo blocking"}, &out),
           0);
  QVERIFY(out.contains(QStringLiteral("blocking")));

  Executor exec;
  QSignalSpy finished(&exec, &Executor::finished);
  QSignalSpy errors(&exec, &Executor::error);
  exec.execute(1, QStringLiteral("sh"), {"-c", "echo queued"}, true, false);
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
  QCOMPARE(errors.count(), 0);
  QVERIFY2(finished.first().at(2).toString().contains(QStringLiteral("queued")),
           "a bare name must be found on PATH by the queued path too");
}

/**
 * @brief A binary shipped next to the application (the Windows/macOS bundle
 *        case) takes precedence over PATH, on both paths.
 */
void tst_executor::bundledBinaryNextToTheApplicationWins() {
  if (QStandardPaths::findExecutable("sh").isEmpty())
    QSKIP("sh not found in PATH");
  const QString name = QStringLiteral("qtpass-tst-bundled-%1")
                           .arg(QCoreApplication::applicationPid());
  const QString path = QDir::cleanPath(
      QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(name));
  {
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly),
             "the test needs to write next to its own binary");
    f.write("#!/bin/sh\necho bundled\n");
    f.close();
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                             QFile::ExeOwner));
  }
  struct Cleanup {
    QString path;
    ~Cleanup() { QFile::remove(path); }
  } cleanup{path};

  QCOMPARE(Executor::resolveExecutable(name), path);
  QString out;
  QCOMPARE(Executor::executeBlocking(name, {}, &out), 0);
  QVERIFY(out.contains(QStringLiteral("bundled")));

  Executor exec;
  QSignalSpy finished(&exec, &Executor::finished);
  exec.execute(1, name, {}, true, false);
  QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
  QVERIFY(
      finished.first().at(2).toString().contains(QStringLiteral("bundled")));
}

namespace {
/**
 * Run @p script through `sh -c` on the cancellable executeBlocking() overload
 * while a second thread sets the cancel flag after @p delayMs. The second
 * thread only touches the atomic, never the QProcess; the calling thread is
 * blocked inside executeBlocking(), so it cannot arm a timer of its own.
 * Returns the exit code and stores the wall time in @p elapsedMs.
 */
int runCancelled(const QString &sh, const QString &script, int delayMs,
                 qint64 *elapsedMs, QProcess *process) {
  std::atomic_bool cancel{false};
  QScopedPointer<QThread> setter(QThread::create([&cancel, delayMs]() {
    QThread::msleep(delayMs);
    cancel.store(true);
  }));
  setter->start();
  QElapsedTimer elapsed;
  elapsed.start();
  const int rc = Executor::executeBlocking(
      *process, sh, {"-c", script}, QString(), nullptr, nullptr, &cancel);
  *elapsedMs = elapsed.elapsed();
  setter->wait();
  return rc;
}
} // namespace

// Setting the flag from another thread must make the calling thread end its
// child and return non-zero within seconds, not after the 60 s sleep.
void tst_executor::executeBlockingCancelFlagEndsChild() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  QProcess process;
  qint64 elapsedMs = 0;
  const int rc = runCancelled(sh, QStringLiteral("exec sleep 60"), 300,
                              &elapsedMs, &process);
  QVERIFY2(rc != 0, "a cancelled run must not report success");
  QVERIFY2(elapsedMs < 3000,
           qPrintable(QStringLiteral("cancel took %1 ms").arg(elapsedMs)));
  QCOMPARE(process.state(), QProcess::NotRunning);
}

// A child that ignores SIGTERM is kill()ed by the same thread after the grace
// period; the run still ends well before the sleep would.
void tst_executor::executeBlockingCancelFlagKillsChildIgnoringTerminate() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  QProcess process;
  qint64 elapsedMs = 0;
  // No exec: the trap must apply to the process that receives the SIGTERM.
  const int rc = runCancelled(sh, QStringLiteral("trap '' TERM; sleep 60"), 300,
                              &elapsedMs, &process);
  QVERIFY2(rc != 0, "a killed run must not report success");
  QVERIFY2(elapsedMs < 5000,
           qPrintable(QStringLiteral("cancel took %1 ms").arg(elapsedMs)));
  QCOMPARE(process.state(), QProcess::NotRunning);
}

// A run given a deadline ends once it has passed, as a cancelled one does,
// and does not report success.
void tst_executor::executeBlockingTimeoutEndsChild() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  QProcess process;
  QElapsedTimer timer;
  timer.start();
  const int rc = Executor::executeBlocking(
      process, sh, {QStringLiteral("-c"), QStringLiteral("exec sleep 60")},
      QString(), nullptr, nullptr, nullptr, 300);
  const qint64 elapsedMs = timer.elapsed();
  QCOMPARE(rc, -1);
  QVERIFY2(elapsedMs >= 250 && elapsedMs < 3000,
           qPrintable(QStringLiteral("timeout took %1 ms").arg(elapsedMs)));
  QCOMPARE(process.state(), QProcess::NotRunning);
}

// A child that ignores SIGTERM is kill()ed after the grace period.
void tst_executor::executeBlockingTimeoutKillsChildIgnoringTerminate() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  QProcess process;
  QElapsedTimer timer;
  timer.start();
  const int rc = Executor::executeBlocking(
      process, sh,
      {QStringLiteral("-c"), QStringLiteral("trap '' TERM; sleep 60")},
      QString(), nullptr, nullptr, nullptr, 300);
  const qint64 elapsedMs = timer.elapsed();
  QCOMPARE(rc, -1);
  QVERIFY2(elapsedMs < 5000,
           qPrintable(QStringLiteral("timeout took %1 ms").arg(elapsedMs)));
  QCOMPARE(process.state(), QProcess::NotRunning);
}

// With a cancel flag that is never set, the deadline still ends the run.
void tst_executor::executeBlockingTimeoutWithCancelFlag() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  const std::atomic_bool cancel{false};
  QProcess process;
  QElapsedTimer timer;
  timer.start();
  const int rc = Executor::executeBlocking(
      process, sh, {QStringLiteral("-c"), QStringLiteral("exec sleep 60")},
      QString(), nullptr, nullptr, &cancel, 300);
  const qint64 elapsedMs = timer.elapsed();
  QCOMPARE(rc, -1);
  QVERIFY2(elapsedMs < 3000,
           qPrintable(QStringLiteral("timeout took %1 ms").arg(elapsedMs)));
  QCOMPARE(process.state(), QProcess::NotRunning);
}

// A run that finishes in time is not affected by its deadline.
void tst_executor::executeBlockingWithinTimeoutReturnsOutput() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  QProcess process;
  QString out;
  const int rc = Executor::executeBlocking(
      process, sh, {QStringLiteral("-c"), QStringLiteral("echo hi; exit 3")},
      QString(), &out, nullptr, nullptr, 10000);
  QCOMPARE(rc, 3);
  QCOMPARE(out.trimmed(), QStringLiteral("hi"));
}

// A flag that is already set must refuse the run without starting anything.
void tst_executor::executeBlockingCancelFlagAlreadySetSkipsStart() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  const QString marker = tmp.filePath(QStringLiteral("started"));
  const std::atomic_bool cancel{true};
  QProcess process;
  const int rc = Executor::executeBlocking(
      process, sh, {"-c", QStringLiteral(": > '%1'").arg(marker)}, QString(),
      nullptr, nullptr, &cancel);
  QCOMPARE(rc, -1);
  QVERIFY2(!QFile::exists(marker), "the child must not have been started");
  QCOMPARE(process.state(), QProcess::NotRunning);
}

namespace {
/**
 * Install a fake `wsl` shell script at the front of PATH that prints each
 * argv entry on its own line, so tests can see exactly what argv the
 * Executor hands to wsl.exe. Restores PATH when destroyed.
 */
class FakeWsl {
public:
  FakeWsl() : m_oldPath(qgetenv("PATH")) {
    if (!m_dir.isValid()) {
      return;
    }
    // Every spelling a configuration may use; outside Windows the launcher
    // is started exactly as written.
    for (const char *name : {"wsl", "wsl.exe", "WSL", "WSL.EXE"}) {
      QFile script(m_dir.filePath(QLatin1String(name)));
      if (!script.open(QIODevice::WriteOnly)) {
        return;
      }
      script.write("#!/bin/sh\nprintf '%s\\n' \"$@\"\n");
      script.close();
      script.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                            QFile::ExeOwner);
    }
    qputenv(
        "PATH",
        (m_dir.path() + ':' + QString::fromLocal8Bit(m_oldPath)).toLocal8Bit());
    m_ok = true;
  }
  ~FakeWsl() { qputenv("PATH", m_oldPath); }
  bool ok() const { return m_ok; }

private:
  QTemporaryDir m_dir;
  QByteArray m_oldPath;
  bool m_ok = false;
};
} // namespace

/**
 * @brief The launcher spelling changes nothing about what wsl.exe receives:
 *        `--exec`, the program, then the arguments verbatim, so the store's
 *        file names never meet a shell whichever way WSL was configured.
 */
void tst_executor::everyWslSpellingReachesTheSameArgv() {
  FakeWsl fake;
  QVERIFY2(fake.ok(), "fake wsl script should be installed on PATH");
  const QString hostile = QStringLiteral("foo; touch /tmp/pwned $(id)");
  const QStringList expected = {QStringLiteral("--exec"),
                                QStringLiteral("gpg2"),
                                QStringLiteral("--decrypt"), hostile};
  for (const char *app : {"wsl gpg2", "wsl.exe gpg2", "WSL gpg2",
                          "WSL.EXE gpg2", "wsl -e gpg2", "wsl --exec gpg2"}) {
    QString output;
    const int rc = Executor::executeBlocking(
        QString::fromLatin1(app), {QStringLiteral("--decrypt"), hostile},
        QString(), &output);
    QVERIFY2(rc == 0, app);
    QCOMPARE(output.split('\n', Qt::SkipEmptyParts), expected);
  }
  QString output;
  QCOMPARE(Executor::executeBlocking(QStringLiteral("wsl -d Debian -u me gpg2"),
                                     {hostile}, QString(), &output),
           0);
  QCOMPARE(
      output.split('\n', Qt::SkipEmptyParts),
      (QStringList{QStringLiteral("-d"), QStringLiteral("Debian"),
                   QStringLiteral("-u"), QStringLiteral("me"),
                   QStringLiteral("--exec"), QStringLiteral("gpg2"), hostile}));
}

/**
 * @brief wslpath runs in the distribution the gpg was configured for, and a
 *        non-WSL executable gets the path back untranslated.
 */
void tst_executor::translatePathForWslUsesTheConfiguredDistribution() {
  FakeWsl fake;
  QVERIFY2(fake.ok(), "fake wsl script should be installed on PATH");
  // The fake prints its argv one per line; that is the "translated" path.
  const QString translated =
      Executor::translatePathForWsl(QStringLiteral("C:/store/x.gpg"),
                                    QStringLiteral("wsl.exe -d Debian gpg"));
  QCOMPARE(translated.split('\n', Qt::SkipEmptyParts),
           (QStringList{QStringLiteral("-d"), QStringLiteral("Debian"),
                        QStringLiteral("--exec"), QStringLiteral("wslpath"),
                        QStringLiteral("C:/store/x.gpg")}));
  QCOMPARE(Executor::translatePathForWsl(QStringLiteral("C:/store//x.gpg"),
                                         QStringLiteral("gpg")),
           QStringLiteral("C:/store/x.gpg"));
  QCOMPARE(Executor::translatePathForWsl(QStringLiteral("C:/store/x.gpg"),
                                         QStringLiteral("wsl sh -c gpg")),
           QStringLiteral("C:/store/x.gpg"));
}

// A "wsl <binary>" executable must reach wsl.exe as `--exec <binary> args...`
// so the arguments are not word-split or $()-expanded by the default shell.
void tst_executor::wslPrefixBlockingUsesExec() {
  FakeWsl fake;
  QVERIFY2(fake.ok(), "fake wsl script should be installed on PATH");
  QString output;
  const QString hostile = QStringLiteral("$(touch /tmp/pwned) two words");
  int result = Executor::executeBlocking(QStringLiteral("wsl gpg2"),
                                         {QStringLiteral("--version"), hostile},
                                         QString(), &output);
  QCOMPARE(result, 0);
  const QStringList argv = output.split('\n', Qt::SkipEmptyParts);
  const QStringList expected = {QStringLiteral("--exec"),
                                QStringLiteral("gpg2"),
                                QStringLiteral("--version"), hostile};
  QCOMPARE(argv, expected);
}

void tst_executor::wslPrefixAsyncUsesExec() {
  FakeWsl fake;
  QVERIFY2(fake.ok(), "fake wsl script should be installed on PATH");
  Executor exec;
  QSignalSpy spy(&exec, &Executor::finished);
  const QString hostile = QStringLiteral("$(id) -r");
  exec.execute(1, QStringLiteral("wsl git"), {QStringLiteral("rm"), hostile},
               true, true);
  QVERIFY2(spy.wait(5000), "finished signal should be emitted");
  QCOMPARE(spy.count(), 1);
  const QList<QVariant> args = spy.takeFirst();
  QCOMPARE(args.at(1).toInt(), 0);
  const QStringList argv =
      args.at(2).toString().split('\n', Qt::SkipEmptyParts);
  const QStringList expected = {QStringLiteral("--exec"), QStringLiteral("git"),
                                QStringLiteral("rm"), hostile};
  QCOMPARE(argv, expected);
}

/**
 * @brief A plain data file that happens to carry the tool's name next to the
 *        application must not be picked over the real tool on PATH.
 */
void tst_executor::nonExecutableFileNextToTheApplicationDoesNotMaskPath() {
  if (QStandardPaths::findExecutable("sh").isEmpty())
    QSKIP("sh not found in PATH");
  const QString name = QStringLiteral("qtpass-tst-plain-%1")
                           .arg(QCoreApplication::applicationPid());
  const QString path = QDir::cleanPath(
      QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(name));
  {
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("not a program");
    f.close();
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner));
  }
  struct Cleanup {
    QString path;
    ~Cleanup() { QFile::remove(path); }
  } cleanup{path};
  QCOMPARE(Executor::resolveExecutable(name), name);
}

namespace {
/// Has the shell print its own pid and session id: field 6 of
/// /proc/$$/stat where there is one, `ps -o sid=` elsewhere.
const QString kPrintPidAndSid = QStringLiteral(
    "if [ -r /proc/$$/stat ]; then sid=$(cut -d' ' -f6 /proc/$$/stat); "
    "else sid=$(ps -o sid= -p $$ 2>/dev/null); fi; echo $$ $sid");

/// The "pid sid" a child printed; nullopt where the session id is unreadable.
auto parsePidAndSid(const QString &output)
    -> std::optional<std::pair<qint64, qint64>> {
  const QStringList fields = output.simplified().split(QLatin1Char(' '));
  if (fields.size() != 2)
    return std::nullopt;
  bool pidOk = false;
  bool sidOk = false;
  const qint64 pid = fields.at(0).toLongLong(&pidOk);
  const qint64 sid = fields.at(1).toLongLong(&sidOk);
  if (!pidOk || !sidOk)
    return std::nullopt;
  return std::make_pair(pid, sid);
}

/**
 * @brief Whether the child that printed @p output led a session of its own,
 * not the test's, so it had no controlling terminal to prompt on.
 */
auto ranInOwnSession(const QString &output) -> bool {
  const auto ids = parsePidAndSid(output);
  return ids && ids->second == ids->first &&
         ids->second != static_cast<qint64>(::getsid(0));
}
} // namespace

// A child must not inherit QtPass's controlling terminal: git and ssh would
// prompt there, unseen, and stall the queue behind them.
void tst_executor::childStartsInNewSessionBlocking() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  QString output;
  QCOMPARE(Executor::executeBlocking(sh, {"-c", kPrintPidAndSid}, &output), 0);
  if (!parsePidAndSid(output))
    QSKIP("no way to read a process's session id here");
  QVERIFY2(ranInOwnSession(output), qPrintable(output));
}

void tst_executor::childStartsInNewSessionBlockingOwnProcess() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  QProcess process;
  QString output;
  QCOMPARE(Executor::executeBlocking(process, sh, {"-c", kPrintPidAndSid},
                                     QString(), &output),
           0);
  if (!parsePidAndSid(output))
    QSKIP("no way to read a process's session id here");
  QVERIFY2(ranInOwnSession(output), qPrintable(output));
}

void tst_executor::childStartsInNewSessionAsync() {
  const QString sh = QStandardPaths::findExecutable("sh");
  if (sh.isEmpty())
    QSKIP("sh not found in PATH");
  Executor exec;
  QSignalSpy spy(&exec, &Executor::finished);
  QVERIFY(spy.isValid());
  // Twice: the queue reuses one QProcess, and the second run must not lose
  // the setting.
  exec.execute(1, sh, {"-c", kPrintPidAndSid}, true, false);
  exec.execute(2, sh, {"-c", kPrintPidAndSid}, true, false);
  QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 5000);
  for (const QList<QVariant> &run : std::as_const(spy)) {
    const QString output = run.at(2).toString();
    if (!parsePidAndSid(output))
      QSKIP("no way to read a process's session id here");
    QVERIFY2(ranInOwnSession(output), qPrintable(output));
  }
}

#endif

void tst_executor::resolveExecutableRules() {
  QCOMPARE(Executor::resolveExecutable(QString()), QString());
  QCOMPARE(Executor::resolveExecutable(QStringLiteral("wsl gpg")),
           QStringLiteral("wsl gpg"));
  QCOMPARE(Executor::resolveExecutable(QStringLiteral("/usr/bin/gpg")),
           QStringLiteral("/usr/bin/gpg"));
  // Not bundled, so left alone for PATH.
  QCOMPARE(Executor::resolveExecutable(QStringLiteral("no-such-qtpass-tool")),
           QStringLiteral("no-such-qtpass-tool"));
}

/**
 * @brief On Windows a bundled tool is <appDir>/<name>.exe; the resolver must
 *        add the extension itself because settings and callers use the bare
 *        name.
 */
void tst_executor::resolveExecutableFindsBundledExeOnWindows() {
#ifndef Q_OS_WIN
  QSKIP("the .exe candidate is Windows-only");
#else
  const QString name = QStringLiteral("qtpass-tst-bundled-%1")
                           .arg(QCoreApplication::applicationPid());
  const QString path =
      QDir::cleanPath(QDir(QCoreApplication::applicationDirPath())
                          .absoluteFilePath(name + QStringLiteral(".exe")));
  {
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("MZ");
    f.close();
  }
  struct Cleanup {
    QString path;
    ~Cleanup() { QFile::remove(path); }
  } cleanup{path};
  QCOMPARE(Executor::resolveExecutable(name), path);
#endif
}

QTEST_MAIN(tst_executor)
#include "tst_executor.moc"
