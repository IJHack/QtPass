// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "../../../src/executor.h"

class tst_executor : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void executeBlockingEcho();
  void executeBlockingWithArgs();
  void executeBlockingWithInput();
#ifndef Q_OS_WIN
  void executeBlockingExitCode();
  void executeBlockingStderr();
#endif
  void executeBlockingNotFound();
  void executeBlockingEmptyArgs();
  void executeBlockingEchoMultiple();
};

void tst_executor::initTestCase() {}

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

#ifndef Q_OS_WIN
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
#endif

void tst_executor::executeBlockingNotFound() {
  QString output;
  QString err;
  int result = Executor::executeBlocking(
      "nonexistent-command-12345", QStringList(), QString(), &output, &err);
  QVERIFY2(result != 0, "non-existent command should fail");
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

QTEST_MAIN(tst_executor)
#include "tst_executor.moc"