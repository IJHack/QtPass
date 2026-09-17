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
  void executeAsyncWorkDirDoesNotCarryOver();
  void executeAsyncNonUtf8OutputIsNotDropped();
  void bareNameRunsFromPathOnBothPaths();
  void bundledBinaryNextToTheApplicationWins();
  void nonExecutableFileNextToTheApplicationDoesNotMaskPath();
  void executeBlockingCancelFlagEndsChild();
  void executeBlockingCancelFlagKillsChildIgnoringTerminate();
  void executeBlockingCancelFlagAlreadySetSkipsStart();
  void wslPrefixBlockingUsesExec();
  void wslPrefixAsyncUsesExec();
#endif
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

void tst_executor::executeBlockingWithInput() {
  QString output;
  QString input = "test input";
  int result = Executor::executeBlocking("cat", {}, input, &output);
  QVERIFY2(result == 0, "cat should exit successfully");
  QVERIFY2(output.contains("test input"), "output should echo input");
}

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
    QStringList expectedArgs = {"-e", "gpgconf"};
    QVERIFY2(result.program == "wsl" && result.arguments == expectedArgs,
             "WSL with -e should keep the user's flag and not add --exec");
    result = Pass::resolveGpgconfCommand("wsl --exec gpg2");
    expectedArgs = {"--exec", "gpgconf"};
    QVERIFY2(result.program == "wsl" && result.arguments == expectedArgs,
             "WSL with --exec should not add a second --exec");
  }

  // WSL with distro
  {
    auto result = Pass::resolveGpgconfCommand("wsl --distro Debian gpg2");
    QVERIFY2(result.program == "wsl", "WSL distro preserves wsl");
    QStringList expectedArgs = {"--distro", "Debian", "--exec", "gpgconf"};
    QVERIFY2(result.arguments == expectedArgs,
             "WSL distro arguments should be preserved before --exec");
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
  const QString path =
      QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(name);
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
    QFile script(m_dir.filePath("wsl"));
    if (!script.open(QIODevice::WriteOnly)) {
      return;
    }
    script.write("#!/bin/sh\nprintf '%s\\n' \"$@\"\n");
    script.close();
    script.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                          QFile::ExeOwner);
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
  const QString path =
      QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(name);
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
  const QString path = QDir(QCoreApplication::applicationDirPath())
                           .absoluteFilePath(name + QStringLiteral(".exe"));
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
  QCOMPARE(Executor::resolveExecutable(name), QDir::cleanPath(path));
#endif
}

QTEST_MAIN(tst_executor)
#include "tst_executor.moc"
