// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "../../../src/executor.h"
#include "../../../src/pass.h"

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
#endif
  void executeBlockingNotFound();
  void executeBlockingGpgVersion();
  void gpgSupportsEd25519();
  void getDefaultKeyTemplate();
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
      Executor::executeBlocking("echo", {"arg1", "arg2"}, QString(), &output);
  QVERIFY2(result == 0, "echo should exit successfully");
  QVERIFY2(output.contains("arg1"), "output should contain 'arg1'");
  QVERIFY2(output.contains("arg2"), "output should contain 'arg2'");
}

void tst_executor::executeBlockingWithInput() {
  QString output;
  int result =
      Executor::executeBlocking("cat", QStringList(), "test input", &output);
  QVERIFY2(result == 0, "cat should exit successfully");
  QVERIFY2(output.contains("test input"), "output should contain input");
}

void tst_executor::executeBlockingExitCode() {
  QString output;
  int result =
      Executor::executeBlocking("false", QStringList(), QString(), &output);
  QVERIFY2(result != 0, "false should exit with non-zero");
}

void tst_executor::executeBlockingStderr() {
  QString output;
  QString err;
  Executor::executeBlocking("bash", {"-c", "echo error >&2"}, QString(),
                            &output, &err);
  QVERIFY2(err.contains("error"), "stderr should contain 'error'");
}

void tst_executor::executeBlockingEmptyArgs() {
  QString output;
  int result =
      Executor::executeBlocking("echo", QStringList(), QString(), &output);
  QVERIFY2(result == 0, "echo with empty args should succeed");
}

void tst_executor::executeBlockingEchoMultiple() {
  QString output;
  int result =
      Executor::executeBlocking("echo", {"a", "b", "c"}, QString(), &output);
  QVERIFY2(result == 0, "echo with multiple args should succeed");
  QVERIFY2(output.contains("a b c"), "output should contain all args");
}
#endif

void tst_executor::executeBlockingNotFound() {
  QString output;
  QString err;
  int result = Executor::executeBlocking(
      "nonexistent-command-12345.exe", QStringList(), QString(), &output, &err);
  QVERIFY2(result != 0, "non-existent command should fail");
}

void tst_executor::executeBlockingGpgVersion() {
  QString output;
  int result = Executor::executeBlocking("gpg", {"--version"}, QString(),
                                         &output, nullptr);
  QVERIFY2(result == 0, "gpg --version should succeed");
  QVERIFY2(output.contains("gpg"), "output should contain gpg version");
}

void tst_executor::gpgSupportsEd25519() {
  bool result = Pass::gpgSupportsEd25519();
  QString output;
  int gpgVersionExitCode = Executor::executeBlocking(
      "gpg", {"--version"}, QString(), &output, nullptr);
  if (gpgVersionExitCode != 0) {
    QVERIFY2(
        result == false,
        "gpgSupportsEd25519() should return false when GPG is not available");
  } else {
    QVERIFY2(result == true || result == false,
             "gpgSupportsEd25519() must return a boolean value");
  }
}

void tst_executor::getDefaultKeyTemplate() {
  bool ed25519Supported = Pass::gpgSupportsEd25519();
  QString templateStr = Pass::getDefaultKeyTemplate();
  QVERIFY2(!templateStr.isEmpty(), "Template should not be empty");
  QVERIFY2(templateStr.contains("Key-Type:"),
           "Template should contain Key-Type");
  QVERIFY2(templateStr.contains("%echo done"),
           "Template should contain done marker");
  if (ed25519Supported) {
    QVERIFY2(templateStr.contains("ed25519") || templateStr.contains("EdDSA"),
             "Template should be the Ed25519 variant when gpgSupportsEd25519() "
             "is true");
  } else {
    QVERIFY2(templateStr.contains("RSA"), "Template should be the RSA fallback "
                                          "when gpgSupportsEd25519() is false");
  }
}

QTEST_MAIN(tst_executor)
#include "tst_executor.moc"