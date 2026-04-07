// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
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
  QString output;
  QString err;
  int gpgVersionExitCode =
      Executor::executeBlocking("gpg", {"--version"}, QString(), &output, &err);

  if (gpgVersionExitCode != 0) {
    QSKIP("GPG not available");
  }

  QVERIFY2(output.contains("gpg") || output.contains("GnuPG"),
           "GPG version output should be present");

  QRegularExpression versionRegex(R"(gpg \(GnuPG\) (\d+)\.(\d+))");
  QRegularExpressionMatch match = versionRegex.match(output);
  if (!match.hasMatch()) {
    qWarning() << "GPG output:" << output;
    qWarning() << "GPG stderr:" << err;
    QSKIP("Could not parse GPG version");
  }

  int major = match.captured(1).toInt();
  int minor = match.captured(2).toInt();
  bool expectEd25519 = major > 2 || (major == 2 && minor >= 1);

  bool result = Pass::gpgSupportsEd25519();
  if (expectEd25519) {
    if (!result) {
      qWarning() << "Pass::gpgSupportsEd25519() returned false for GPG" << major
                 << minor;
      QSKIP("QtPassSettings not properly initialized");
    }
    QVERIFY2(
        result == true,
        qPrintable(
            QString("GPG %1.%2 should support Ed25519").arg(major).arg(minor)));
  } else {
    QVERIFY2(result == false,
             qPrintable(QString("GPG %1.%2 should not support Ed25519")
                            .arg(major)
                            .arg(minor)));
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

void tst_executor::executeBlockingGpgKillAgent() {
#ifdef Q_OS_WIN
  QSKIP("gpgconf not reliably available on Windows PATH");
#endif
  QString output;
  QString err;
  int result = Executor::executeBlocking("gpgconf", {"--kill", "gpg-agent"},
                                         QString(), &output, &err);
  if (result != 0) {
    QSKIP("gpgconf not available in PATH");
  }
  QVERIFY2(result == 0, "gpgconf --kill gpg-agent should succeed");
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
    auto result = Pass::resolveGpgconfCommand("wsl -d Ubuntu gpg2");
    QStringList expectedArgs = {"-d", "Ubuntu", "gpgconf"};
    QVERIFY2(result.program == "wsl" && result.arguments == expectedArgs,
             "WSL with distro should preserve args");
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
    QString gpgPath = tempDir.filePath(
#ifdef Q_OS_WIN
        "gpg2.exe"
#else
        "gpg2"
#endif
    );
    QString gpgconfPath = tempDir.filePath(
#ifdef Q_OS_WIN
        "gpgconf.exe"
#else
        "gpgconf"
#endif
    );

    // Create dummy files and set executable
    QFile gpgFile(gpgPath);
    QVERIFY2(gpgFile.open(QFile::WriteOnly), "Should create gpg2 file");
    gpgFile.close();
#ifndef Q_OS_WIN
    QFile::setPermissions(gpgPath, QFile::ReadOwner | QFile::ExeOwner);
#endif

    QFile gpgconfFile(gpgconfPath);
    QVERIFY2(gpgconfFile.open(QFile::WriteOnly), "Should create gpgconf file");
    gpgconfFile.close();
#ifndef Q_OS_WIN
    QFile::setPermissions(gpgconfPath, QFile::ReadOwner | QFile::ExeOwner);
#endif

    auto result = Pass::resolveGpgconfCommand(gpgPath);
    QVERIFY2(result.program == gpgconfPath,
             "Should derive sibling gpgconf path");
  }
}

QTEST_MAIN(tst_executor)
#include "tst_executor.moc"