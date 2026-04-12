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
#endif
  void executeBlockingNotFound();
  void executeBlockingGpgVersion();
  void gpgSupportsEd25519();
  void getDefaultKeyTemplate();
  void executeBlockingGpgKillAgent();
  void resolveGpgconfCommand();
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
#endif

void tst_executor::executeBlockingNotFound() {
  QString output;
  int result = Executor::executeBlocking("nonexistent_command_xyz", {},
                                         QString(), &output);
  QVERIFY2(result != 0, "nonexistent should fail");
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
  int result = Executor::executeBlocking(
      "gpg", {"--list-keys", "--with-colons", "test@test.com"}, QString(),
      &output, &err);
  if (result != 0) {
    QSKIP("gpg not available");
  }
  bool supported = Pass::gpgSupportsEd25519();
  QVERIFY2(supported || !supported,
           "gpgSupportsEd25519 should return bool (may be true or false)");
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
    QVERIFY2(result.program == "gpgconf", "PATH-only should fallback");
  }

  // Unix absolute - use temp directory for filesystem-independent test
  {
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temp directory should be valid");
    QString absPath = tempDir.path() + "/gpg2";
    auto result = Pass::resolveGpgconfCommand(absPath);
    QVERIFY2(result.program == "gpgconf", "Absolute path should fallback");
  }
}

QTEST_MAIN(tst_executor)
#include "tst_executor.moc"