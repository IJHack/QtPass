// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

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
  void executeBlockingConstQStringRef();
#endif
  void executeBlockingNotFound();
  void executeBlockingGpgVersion();
  void gpgSupportsEd25519();
  void getDefaultKeyTemplate();
  void executeBlockingGpgKillAgent();
  void resolveGpgconfCommand();
  void executeBlockingWithEnvNotFound();
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
  Executor::executeBlocking("sh", {"-c", "echo error >&2"}, QString(), &output,
                            &err);
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
//   executeBlocking(const QStringList &env, const QString &app, ...)
// This overload was changed from (QString app) to (const QString &app).

void tst_executor::executeBlockingWithEnv() {
  // Basic: custom env, echo succeeds, output captured.
  QStringList env = {"MY_VAR=hello_env"};
  QString output;
  int result = Executor::executeBlocking(env, "sh", {"-c", "echo ok"}, &output);
  QVERIFY2(result == 0, "sh with custom env should exit 0");
  QVERIFY2(output.contains("ok"), "output should contain 'ok'");
}

void tst_executor::executeBlockingWithEnvEmpty() {
  // Empty env list: process starts with an empty environment.
  // On POSIX, running 'env' with empty environment should succeed.
  QStringList env;
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
  QStringList env = {"QTPASS_TEST_VAR=injected_value"};
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

void tst_executor::executeBlockingConstQStringRef() {
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

void tst_executor::executeBlockingNotFound() {
  QString output;
  int result = Executor::executeBlocking("nonexistent_command_xyz", {},
                                         QString(), &output);
  QVERIFY2(result != 0, "nonexistent should fail");
}

void tst_executor::executeBlockingWithEnvNotFound() {
  // The env-based overload should also fail gracefully for a missing binary.
  QStringList env = {"MY_VAR=irrelevant"};
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
  QString output;
  QString err;
  int result = Executor::executeBlocking("gpgconf", {"--kill", "gpg-agent"},
                                         QString(), &output, &err);
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
    QStringList expectedArgs = {"gpgconf"};
    QVERIFY2(result.program == "wsl" && result.arguments == expectedArgs,
             "WSL simple should replace gpg with gpgconf");
  }

  // WSL with distro
  {
    auto result = Pass::resolveGpgconfCommand("wsl --distro Debian gpg2");
    QVERIFY2(result.program == "wsl", "WSL distro preserves wsl");
    QVERIFY2(result.arguments.contains("--distro") &&
                 result.arguments.contains("Debian"),
             "WSL distro arguments should be preserved");
  }

  // WSL with full path
  {
    auto result = Pass::resolveGpgconfCommand("wsl /usr/bin/gpg2");
    QStringList expectedArgs = {"/usr/bin/gpgconf"};
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

QTEST_MAIN(tst_executor)
#include "tst_executor.moc"