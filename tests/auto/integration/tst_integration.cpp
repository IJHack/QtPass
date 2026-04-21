// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @brief Integration tests for ImitatePass and RealPass backends.
 *
 * These tests spin up a real GPG homedir, generate a temporary key, and drive
 * the password-store operations end-to-end without mocking gpg.
 *
 * Prerequisites (all available on standard CI):
 *   - gpg (GnuPG 2.x)
 *   - pass (optional – RealPass tests are skipped when absent)
 *   - pass otp extension (optional – OTP tests skipped when absent)
 */

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "../../../src/enums.h"
#include "../../../src/imitatepass.h"
#include "../../../src/pass.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/realpass.h"
#include "../../../src/userinfo.h"

using GrepResults = QList<QPair<QString, QStringList>>;
Q_DECLARE_METATYPE(GrepResults)

#define INIT_IMITATE_STORE_OR_FAIL(storeDir, pass)                             \
  do {                                                                         \
    /* Note: storeDir and pass must be lvalues (not parenthesized) because     \
       initImitateStore takes non-const references. */                         \
    QString _initImitateStoreErr = initImitateStore(storeDir, pass);           \
    if (!_initImitateStoreErr.isEmpty())                                       \
      QFAIL(qPrintable(_initImitateStoreErr));                                 \
  } while (0)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static QString findGpg() {
  for (const auto &c : {"/usr/bin/gpg2", "/usr/bin/gpg", "/usr/local/bin/gpg2",
                        "/usr/local/bin/gpg"}) {
    if (QFile::exists(c))
      return c;
  }
  return QStandardPaths::findExecutable("gpg");
}

static QString findGpgconf() {
  for (const auto &c : {"/usr/bin/gpgconf", "/usr/local/bin/gpgconf"}) {
    if (QFile::exists(c))
      return c;
  }
  return QStandardPaths::findExecutable("gpgconf");
}

static QString findPass() { return QStandardPaths::findExecutable("pass"); }

// Run gpg synchronously with the given GNUPGHOME, return exit code.
static int runGpg(const QString &gnupgHome, const QStringList &args,
                  const QString &input = QString(), QString *out = nullptr,
                  QString *err = nullptr) {
  QProcess p;
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("GNUPGHOME", gnupgHome);
  p.setProcessEnvironment(env);
  p.start(findGpg(), args);
  if (!input.isEmpty()) {
    p.write(input.toUtf8());
    p.closeWriteChannel();
  }
  p.waitForFinished(30000);
  if (out)
    *out = QString::fromUtf8(p.readAllStandardOutput());
  if (err)
    *err = QString::fromUtf8(p.readAllStandardError());
  return p.exitCode();
}

// Generate a test GPG key in the given GNUPGHOME; returns the key fingerprint.
static QString generateTestKey(const QString &gnupgHome) {
  const QString batch = QStringLiteral("%no-protection\n"
                                       "Key-Type: RSA\n"
                                       "Key-Length: 2048\n"
                                       "Subkey-Type: RSA\n"
                                       "Subkey-Length: 2048\n"
                                       "Name-Real: QtPass Integration Test\n"
                                       "Name-Email: qtpass-test@localhost\n"
                                       "Expire-Date: 0\n"
                                       "%commit\n");

  int rc = runGpg(gnupgHome, {"--batch", "--gen-key"}, batch);
  if (rc != 0)
    return {};

  QString out;
  runGpg(gnupgHome, {"--with-colons", "--fingerprint", "qtpass-test@localhost"},
         QString(), &out);

  QString fingerprint;
  for (const auto &line : out.split('\n')) {
    if (line.startsWith("fpr:")) {
      const auto parts = line.split(':');
      if (parts.size() >= 10) {
        fingerprint = parts[9].trimmed();
        break;
      }
    }
  }
  if (fingerprint.isEmpty())
    return {};

  // Explicitly set ultimate trust — some GPG versions don't auto-assign it
  // from --gen-key --batch, which causes encryption to refuse the key.
  if (runGpg(gnupgHome, {"--batch", "--import-ownertrust"},
             fingerprint + ":6:\n") != 0) {
    qWarning() << "Failed to import ownertrust for key:" << fingerprint;
    return {};
  }

  return fingerprint;
}

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class tst_integration : public QObject {
  Q_OBJECT

  QString m_gpgExe;
  QTemporaryDir m_gnupgHome;
  QString m_keyFingerprint;
  QString m_originalPassSigningKey;

  // Wait for a signal spy to receive at least one signal (up to timeoutMs).
  static bool waitForSignal(QSignalSpy &spy, int timeoutMs = 15000) {
    if (spy.count() > 0)
      return true;
    return spy.wait(timeoutMs);
  }

  // Initialize a Pass object with the test keyring and store.
  static void setupPass(Pass &pass) {
    pass.init();
    pass.updateEnv();
  }

  // Initialize a temporary pass store with an .gpg-id file and ImitatePass.
  // Returns empty QString on success, or an error message on failure.
  auto initImitateStore(QTemporaryDir &storeDir, ImitatePass &pass) -> QString {
    if (!storeDir.isValid())
      return QStringLiteral("temp dir invalid");
    QtPassSettings::setPassStore(storeDir.path());
    {
      QFile gpgId(QDir::cleanPath(storeDir.path() + "/.gpg-id"));
      if (!gpgId.open(QIODevice::WriteOnly | QIODevice::Text))
        return QStringLiteral("failed to open .gpg-id");
      QByteArray payload = (m_keyFingerprint + "\n").toUtf8();
      qint64 bytesWritten = gpgId.write(payload);
      if (bytesWritten != payload.size())
        return QStringLiteral("failed to write .gpg-id");
    }
    pass.init();
    pass.updateEnv();
    return QString();
  }

  static auto gpgInsertErrorMsg(const QSignalSpy &errorSpy) -> QByteArray {
    if (errorSpy.count() > 0)
      return QString("GPG Insert error (rc=%1): %2")
          .arg(errorSpy[0][0].toInt())
          .arg(errorSpy[0][1].toString())
          .toUtf8();
    return "finishedInsert not emitted (GPG may have hung or failed to start)";
  }

  // Run a git config command and verify it succeeds.
  static auto runGitConfig(QProcess &proc, const QString &gitExe,
                           const QStringList &args) -> bool {
    proc.start(gitExe, args);
    if (!proc.waitForStarted()) {
      return false;
    }
    if (!proc.waitForFinished()) {
      return false;
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
      return false;
    }
    return true;
  }

private Q_SLOTS:
  void initTestCase();
  void cleanupTestCase();

  // ImitatePass backend
  void imitatePass_insertAndShow();
  void imitatePass_insertAndGrep();
  void imitatePass_insertMoveAndShow();
  void imitatePass_insertCopyAndShow();
  void imitatePass_insertAndRemove();
  void imitatePass_nestedDirectoryInsertAndShow();
  void imitatePass_editExistingEntry();
  void imitatePass_grepCaseInsensitive();
  void imitatePass_specialCharactersInPassword();
  void imitatePass_emptyPassword();
  void imitatePass_gitInitAndCommit();

  // UTF-8 and unicode handling
  void imitatePass_utf8Characters();

  // Edge cases
  void imitatePass_longPassword();
  void imitatePass_multilineContent();

  // RealPass backend (skipped if `pass` not installed)
  void realPass_insertAndShow();
  void realPass_insertAndGrep();

  // pass-otp (skipped if extension not installed)
  void imitatePass_otpGenerate();
};

// ---------------------------------------------------------------------------

void tst_integration::initTestCase() {
  m_gpgExe = findGpg();
  if (m_gpgExe.isEmpty())
    QSKIP("gpg not found – skipping integration tests");

  QVERIFY2(m_gnupgHome.isValid(), "Failed to create temp GNUPGHOME");
  // Restrict permissions so gpg2 doesn't complain about unsafe homedir.
  QFile::setPermissions(m_gnupgHome.path(),
                        QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

  // Configure gpg-agent for headless/CI: allow loopback pinentry so GPG
  // never blocks waiting for a PIN dialog on systems without a pinentry GUI.
  {
    QByteArray agentPayload =
        "allow-loopback-pinentry\ndefault-cache-ttl 300\n";
#ifdef Q_OS_LINUX
    agentPayload += "pinentry-program /usr/bin/pinentry-tty\n";
#endif
    QFile agentConf(QDir::cleanPath(m_gnupgHome.path() + "/gpg-agent.conf"));
    QVERIFY2(
        agentConf.open(QIODevice::WriteOnly | QIODevice::Text),
        qPrintable("Cannot open gpg-agent.conf: " + agentConf.errorString()));
    QVERIFY2(
        agentConf.write(agentPayload) == agentPayload.size(),
        qPrintable("Cannot write gpg-agent.conf: " + agentConf.errorString()));
    const QByteArray gpgPayload = "pinentry-mode loopback\nbatch\nno-tty\n";
    QFile gpgConf(QDir::cleanPath(m_gnupgHome.path() + "/gpg.conf"));
    QVERIFY2(gpgConf.open(QIODevice::WriteOnly | QIODevice::Text),
             qPrintable("Cannot open gpg.conf: " + gpgConf.errorString()));
    QVERIFY2(gpgConf.write(gpgPayload) == gpgPayload.size(),
             qPrintable("Cannot write gpg.conf: " + gpgConf.errorString()));
  }

  // Pre-start the agent so GPG subprocesses don't hang waiting for it.
  {
    QString gpgconfExe = findGpgconf();
    if (gpgconfExe.isEmpty()) {
      QSKIP("gpgconf not found - skipping GPG integration tests");
    }
    QProcess agentLaunch;
    QProcessEnvironment agentEnv = QProcessEnvironment::systemEnvironment();
    agentEnv.insert("GNUPGHOME", m_gnupgHome.path());
    agentLaunch.setProcessEnvironment(agentEnv);
    agentLaunch.start(
        gpgconfExe, {"--homedir", m_gnupgHome.path(), "--launch", "gpg-agent"});
    QVERIFY2(agentLaunch.waitForStarted(5000), "gpgconf failed to start");
    QVERIFY2(agentLaunch.waitForFinished(10000),
             "gpgconf --launch gpg-agent timed out");
    QVERIFY2(agentLaunch.exitStatus() == QProcess::NormalExit &&
                 agentLaunch.exitCode() == 0,
             qPrintable("gpgconf --launch gpg-agent failed (rc=" +
                        QString::number(agentLaunch.exitCode()) +
                        "): " + agentLaunch.readAllStandardError()));
  }

  m_keyFingerprint = generateTestKey(m_gnupgHome.path());
  QVERIFY2(!m_keyFingerprint.isEmpty(),
           "Failed to generate GPG key for integration tests");

  // Redirect GNUPGHOME process-wide so all child processes inherit the test
  // keyring. Also set it in QtPassSettings so Pass::init() propagates it
  // to the Executor environment via updateEnv().
  qputenv("GNUPGHOME", m_gnupgHome.path().toLocal8Bit());
  QtPassSettings::getInstance()->setValue(SettingsConstants::gpgHome,
                                          m_gnupgHome.path());
  QtPassSettings::setGpgExecutable(m_gpgExe);
  m_originalPassSigningKey = QtPassSettings::getPassSigningKey();
  QtPassSettings::setPassSigningKey(QString());
  qRegisterMetaType<GrepResults>("GrepResults");
  qRegisterMetaType<GrepResults>(
      "QList<QPair<QString,QStringList>>"); // Qt5 fallback
}

void tst_integration::cleanupTestCase() {
  // Restore original pass signing key
  QtPassSettings::setPassSigningKey(m_originalPassSigningKey);

  // Kill any gpg-agent started in our temporary homedir so it doesn't linger.
  QProcess killer;
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("GNUPGHOME", m_gnupgHome.path());
  killer.setProcessEnvironment(env);
  killer.start("gpgconf", {"--kill", "gpg-agent"});
  killer.waitForFinished(5000);
}

// ---------------------------------------------------------------------------
// ImitatePass tests
// ---------------------------------------------------------------------------

void tst_integration::imitatePass_insertAndShow() {
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  const QString entryName = QStringLiteral("test/password");
  const QString entryContent = QStringLiteral("hunter2\nuser: testuser\n");

  // ImitatePass::Insert does not create parent directories; the UI does.
  QVERIFY(QDir(storeDir.path()).mkpath("test"));

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(entryName, entryContent, false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  // Verify the .gpg file was created.
  const QString expectedFile = storeDir.path() + "/" + entryName + ".gpg";
  QVERIFY2(QFile::exists(expectedFile), "encrypted file should exist");

  // Now show it.
  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(entryName);
  QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted");

  const QString decrypted = showSpy[0][0].toString();
  QVERIFY2(decrypted.contains("hunter2"),
           "decrypted output should contain password");
  QVERIFY2(decrypted.contains("testuser"),
           "decrypted output should contain user");
}

void tst_integration::imitatePass_insertAndGrep() {
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  QVERIFY(QDir(storeDir.path()).mkpath("work"));

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("work/github"),
              QStringLiteral("s3cr3t\ntoken: abc123\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  insertSpy.clear();
  insertErrorSpy.clear();
  pass.Insert(QStringLiteral("work/gitlab"),
              QStringLiteral("another\ntoken: xyz789\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  QSignalSpy grepSpy(&pass, &Pass::finishedGrep);
  pass.Grep(QStringLiteral("token"));
  QVERIFY2(waitForSignal(grepSpy, 20000), "finishedGrep not emitted");

  const auto results = grepSpy[0][0].value<GrepResults>();
  QVERIFY2(results.size() == 2, "grep should find both entries");
}

void tst_integration::imitatePass_insertMoveAndShow() {
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("original"), QStringLiteral("moveme\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  const QString src = storeDir.path() + "/original.gpg";
  const QString dst = storeDir.path() + "/moved.gpg";
  QVERIFY2(QFile::exists(src), "source .gpg must exist before move");

  // Without git, Move is synchronous — no signal emitted.
  pass.Move(src, dst);
  QVERIFY2(!QFile::exists(src), "source should be gone after move");
  QVERIFY2(QFile::exists(dst), "destination should exist after move");

  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("moved"));
  QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted after move");
  QVERIFY2(showSpy[0][0].toString().contains("moveme"),
           "decrypted moved entry should contain original content");
}

void tst_integration::imitatePass_insertCopyAndShow() {
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("original"), QStringLiteral("copyme\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  const QString src = storeDir.path() + "/original.gpg";
  const QString dst = storeDir.path() + "/copy.gpg";

  // Without git, Copy is synchronous — no finishedCopy signal emitted.
  pass.Copy(src, dst);
  QVERIFY2(QFile::exists(src), "source should still exist after copy");
  QVERIFY2(QFile::exists(dst), "destination should exist after copy");

  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("copy"));
  QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted after copy");
  QVERIFY2(showSpy[0][0].toString().contains("copyme"),
           "decrypted copy should contain original content");
}

void tst_integration::imitatePass_insertAndRemove() {
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("deleteme"), QStringLiteral("gone\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  const QString gpgFile = storeDir.path() + "/deleteme.gpg";
  QVERIFY2(QFile::exists(gpgFile), "file must exist before remove");

  // Without git, ImitatePass::Remove removes the file synchronously; no signal.
  pass.Remove(QStringLiteral("deleteme"), false);
  QVERIFY2(!QFile::exists(gpgFile), "file should be gone after remove");
}

void tst_integration::imitatePass_nestedDirectoryInsertAndShow() {
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  QVERIFY(QDir(storeDir.path()).mkpath("level1/level2/level3"));

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("level1/level2/level3/deep"),
              QStringLiteral("deepvalue\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  const QString gpgFile = storeDir.path() + "/level1/level2/level3/deep.gpg";
  QVERIFY2(QFile::exists(gpgFile), "nested .gpg file should be created");

  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("level1/level2/level3/deep"));
  QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted for nested entry");
  QVERIFY2(showSpy[0][0].toString().contains("deepvalue"),
           "decrypted nested entry should contain the content");
}

namespace {
// RAII guard to ensure QtPassSettings::setUseGit is restored on any exit path
struct RestoreUseGit {
  bool orig;
  RestoreUseGit() : orig(QtPassSettings::isUseGit()) {}
  ~RestoreUseGit() { QtPassSettings::setUseGit(orig); }
};
} // namespace

void tst_integration::imitatePass_editExistingEntry() {
  RestoreUseGit restoreUseGit;
  QtPassSettings::setUseGit(false); // Ensure git is off for this test

  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  // Insert initial entry
  const QString entryName = QStringLiteral("editme");
  const QString originalContent = QStringLiteral("original\n");
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(entryName, originalContent, false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  // Verify file exists
  const QString gpgFile =
      QDir::cleanPath(storeDir.path() + "/" + entryName + ".gpg");
  QVERIFY2(QFile::exists(gpgFile), "encrypted file should exist");

  // Edit (overwrite) the entry
  const QString newContent = QStringLiteral("updated\npassword: newpass\n");
  QSignalSpy editSpy(&pass, &Pass::finishedInsert);
  pass.Insert(entryName, newContent, true);
  QVERIFY2(waitForSignal(editSpy), gpgInsertErrorMsg(insertErrorSpy));

  // Show the edited entry and verify content changed
  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(entryName);
  QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted");

  const QString decrypted = showSpy[0][0].toString();
  QVERIFY2(decrypted.contains("updated"),
           "decrypted should contain new content");
  QVERIFY2(decrypted.contains("newpass"),
           "decrypted should contain new password");
  QVERIFY2(!decrypted.contains("original"),
           "decrypted should NOT contain original content");
}

void tst_integration::imitatePass_gitInitAndCommit() {
  const QString gitExe = QStandardPaths::findExecutable("git");
  if (gitExe.isEmpty())
    QSKIP("git not installed – skipping Git integration test");

  RestoreUseGit restoreUseGit;

  QtPassSettings::setGitExecutable(gitExe);
  QtPassSettings::setUseGit(true);

  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  QProcess gitInit;
  gitInit.setWorkingDirectory(storeDir.path());
  gitInit.start(gitExe, {"init"});
  QVERIFY2(gitInit.waitForFinished(), "git init should complete");
  QVERIFY2(gitInit.exitCode() == 0, "git init should succeed");

  // Configure local git identity so ImitatePass commits succeed
  QProcess gitConfig;
  gitConfig.setWorkingDirectory(storeDir.path());
  QVERIFY2(
      runGitConfig(gitConfig, gitExe, {"config", "user.name", "Test User"}),
      qPrintable(
          QString("git config user.name failed: %1")
              .arg(QString::fromUtf8(gitConfig.readAllStandardError()))));

  QProcess gitConfigEmail;
  gitConfigEmail.setWorkingDirectory(storeDir.path());
  QVERIFY2(runGitConfig(gitConfigEmail, gitExe,
                        {"config", "user.email", "test@example.com"}),
           qPrintable(QString("git config user.email failed: %1")
                          .arg(QString::fromUtf8(
                              gitConfigEmail.readAllStandardError()))));

  QProcess gitConfigSign;
  gitConfigSign.setWorkingDirectory(storeDir.path());
  QVERIFY2(runGitConfig(gitConfigSign, gitExe,
                        {"config", "commit.gpgsign", "false"}),
           qPrintable(QString("git config commit.gpgsign failed: %1")
                          .arg(QString::fromUtf8(
                              gitConfigSign.readAllStandardError()))));

  const QString entryName = QStringLiteral("gitentry");
  const QString entryContent = QStringLiteral("secret\nurl: example.com\n");

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(entryName, entryContent, false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  QProcess gitLog;
  gitLog.setWorkingDirectory(storeDir.path());
  gitLog.start(gitExe, {"log", "--format=%s", "-1"});
  QVERIFY2(gitLog.waitForFinished(), "git log should complete");
  QVERIFY2(gitLog.exitCode() == 0, "git log should succeed");

  const QString commitMsg = QString::fromUtf8(gitLog.readAll()).trimmed();
  QVERIFY2(commitMsg.contains("gitentry"),
           qPrintable(QString("commit message should mention gitentry: %1")
                          .arg(commitMsg)));
}

// ---------------------------------------------------------------------------
// RealPass tests
// ---------------------------------------------------------------------------

void tst_integration::realPass_insertAndShow() {
  const QString passExe = findPass();
  if (passExe.isEmpty())
    QSKIP("pass not installed – skipping RealPass integration test");

  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());

  QtPassSettings::setPassStore(storeDir.path());
  QtPassSettings::setPassExecutable(passExe);
  QtPassSettings::setGpgExecutable(m_gpgExe);
  QtPassSettings::setUsePass(true);

  // Initialise the store via `pass init`.
  QProcess initProc;
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("GNUPGHOME", m_gnupgHome.path());
  env.insert("PASSWORD_STORE_DIR", storeDir.path());
  initProc.setProcessEnvironment(env);
  initProc.start(passExe, {"init", m_keyFingerprint});
  QVERIFY2(initProc.waitForFinished(15000), "pass init timed out");
  QVERIFY2(initProc.exitCode() == 0, "pass init should succeed");

  RealPass pass;
  setupPass(pass);

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("realtest"),
              QStringLiteral("realpassword\nurl: example.com\n"), false);
  QVERIFY2(waitForSignal(insertSpy, 20000), gpgInsertErrorMsg(insertErrorSpy));

  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("realtest"));
  QVERIFY2(waitForSignal(showSpy, 20000), "RealPass finishedShow not emitted");
  QVERIFY2(showSpy[0][0].toString().contains("realpassword"),
           "RealPass show should return inserted content");

  QtPassSettings::setUsePass(false);
}

void tst_integration::realPass_insertAndGrep() {
  const QString passExe = findPass();
  if (passExe.isEmpty())
    QSKIP("pass not installed – skipping RealPass grep integration test");

  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());

  QtPassSettings::setPassStore(storeDir.path());
  QtPassSettings::setPassExecutable(passExe);
  QtPassSettings::setGpgExecutable(m_gpgExe);
  QtPassSettings::setUsePass(true);

  QProcess initProc;
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("GNUPGHOME", m_gnupgHome.path());
  env.insert("PASSWORD_STORE_DIR", storeDir.path());
  initProc.setProcessEnvironment(env);
  initProc.start(passExe, {"init", m_keyFingerprint});
  QVERIFY2(initProc.waitForFinished(15000), "pass init timed out");
  QVERIFY2(initProc.exitCode() == 0, "pass init should succeed");

  RealPass pass;
  setupPass(pass);

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("email/gmail"),
              QStringLiteral("gmailpass\nurl: mail.google.com\n"), false);
  QVERIFY2(waitForSignal(insertSpy, 20000), gpgInsertErrorMsg(insertErrorSpy));

  insertSpy.clear();
  insertErrorSpy.clear();
  pass.Insert(QStringLiteral("email/outlook"),
              QStringLiteral("outlookpass\nurl: outlook.com\n"), false);
  QVERIFY2(waitForSignal(insertSpy, 20000), gpgInsertErrorMsg(insertErrorSpy));

  QSignalSpy grepSpy(&pass, &Pass::finishedGrep);
  pass.Grep(QStringLiteral("url"));
  QVERIFY2(waitForSignal(grepSpy, 30000), "RealPass finishedGrep not emitted");

  const auto results = grepSpy[0][0].value<GrepResults>();
  QVERIFY2(results.size() >= 2, "grep should find both entries with 'url'");

  QtPassSettings::setUsePass(false);
}

// ---------------------------------------------------------------------------
// OTP test
// ---------------------------------------------------------------------------

void tst_integration::imitatePass_otpGenerate() {
  const QString passExe = findPass();
  if (passExe.isEmpty())
    QSKIP("pass not installed – skipping OTP integration test");

  const bool hasOtp =
      QFile::exists("/usr/lib/password-store/extensions/otp.bash");
  if (!hasOtp)
    QSKIP("pass-otp extension not found – skipping OTP integration test");

  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());

  QtPassSettings::setPassStore(storeDir.path());
  QtPassSettings::setPassExecutable(passExe);
  QtPassSettings::setGpgExecutable(m_gpgExe);
  QtPassSettings::setUsePass(true);

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("GNUPGHOME", m_gnupgHome.path());
  env.insert("PASSWORD_STORE_DIR", storeDir.path());

  QProcess initProc;
  initProc.setProcessEnvironment(env);
  initProc.start(passExe, {"init", m_keyFingerprint});
  QVERIFY2(initProc.waitForFinished(15000), "pass init timed out");
  QVERIFY2(initProc.exitCode() == 0, "pass init should succeed");

  // A known TOTP secret (RFC 6238 test vector, base32 encoded).
  const QString totpUri = QStringLiteral(
      "otpauth://totp/test@example.com?secret=JBSWY3DPEHPK3PXP"
      "&issuer=IntegrationTest&algorithm=SHA1&digits=6&period=30");

  // Insert OTP entry via `pass insert` (non-interactive: pipe URI via stdin).
  // `pass otp insert` reads the URI from stdin, so we write it there.
  QProcess otpInsert;
  otpInsert.setProcessEnvironment(env);
  otpInsert.start(passExe, {"insert", "--force", "otp/testaccount"});
  QVERIFY2(otpInsert.waitForStarted(10000), "pass insert failed to start");
  otpInsert.write((totpUri + "\n" + totpUri + "\n").toUtf8());
  otpInsert.closeWriteChannel();
  QVERIFY2(otpInsert.waitForFinished(20000), "pass insert timed out");
  if (otpInsert.exitCode() != 0)
    QSKIP("pass insert for OTP failed – skipping OTP generation test");

  // Use RealPass::OtpGenerate to generate the TOTP token.
  RealPass pass;
  setupPass(pass);

  QSignalSpy otpSpy(&pass, &Pass::finishedOtpGenerate);
  pass.OtpGenerate(QStringLiteral("otp/testaccount"));
  QVERIFY2(waitForSignal(otpSpy, 20000), "finishedOtpGenerate not emitted");

  const QString token = otpSpy[0][0].toString().trimmed();
  // TOTP token is a 6-digit number.
  QVERIFY2(QRegularExpression("^\\d{6}$").match(token).hasMatch(),
           qPrintable("OTP token should be 6 digits, got: " + token));

  QtPassSettings::setUsePass(false);
}

void tst_integration::imitatePass_grepCaseInsensitive() {
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  QVERIFY(QDir(storeDir.path()).mkpath("accounts"));

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("accounts/Google"),
              QStringLiteral("pass123\nemail: user@google.com\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  insertSpy.clear();
  insertErrorSpy.clear();
  pass.Insert(QStringLiteral("accounts/Amazon"),
              QStringLiteral("pass456\nemail: user@amazon.com\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  QSignalSpy grepSpy(&pass, &Pass::finishedGrep);
  pass.Grep(QStringLiteral("USER"), true);
  QVERIFY2(waitForSignal(grepSpy, 20000), "finishedGrep not emitted");

  const auto results = grepSpy[0][0].value<GrepResults>();
  QVERIFY2(results.size() == 2,
           qPrintable(QStringLiteral("case-insensitive grep should find 2 "
                                     "entries, got: %1")
                          .arg(results.size())));

  for (const auto &result : results) {
    const QStringList lines = result.second;
    bool hasEmail = false;
    bool hasUser = false;
    for (const QString &line : lines) {
      if (line.toLower().contains("email:")) {
        hasEmail = true;
      }
      if (line.toLower().contains("user")) {
        hasUser = true;
      }
    }
    QVERIFY2(
        hasEmail,
        qPrintable(QStringLiteral(
                       "Result should contain 'email:' (case-insensitive): %1")
                       .arg(result.first)));
    QVERIFY2(
        hasUser,
        qPrintable(QStringLiteral(
                       "Result should contain 'user' (case-insensitive): %1")
                       .arg(result.first)));
  }
}

void tst_integration::imitatePass_specialCharactersInPassword() {
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  const QString specialPw = QStringLiteral(
      "p@ssw0rd!#$%^&*()\nurl: https://example.com\nuser: admin");
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("special"), specialPw, false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("special"));
  QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted");

  const QString decrypted = showSpy[0][0].toString();
  QVERIFY2(decrypted.contains("p@ssw0rd!#$%^&*()"),
           "decrypted should contain special characters");
  QVERIFY2(decrypted.contains("https://example.com"),
           "decrypted should contain URL field");
  QVERIFY2(decrypted.contains("user: admin"),
           "decrypted should contain user field");
}

void tst_integration::imitatePass_emptyPassword() {
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("empty"), QStringLiteral("\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("empty"));
  QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted");

  const QString decrypted = showSpy[0][0].toString();
  QVERIFY2(
      decrypted == "\n",
      qPrintable(QString("empty password should be preserved as '\\n', got: %1")
                     .arg(decrypted)));
}

void tst_integration::imitatePass_utf8Characters() {
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  const QString utf8Pw =
      QString::fromUtf8("pässwörd\nurl: https://exämplë.com\nuser: ädmin");
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("utf8"), utf8Pw, false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("utf8"));
  QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted");

  const QString decrypted = showSpy[0][0].toString();
  QVERIFY2(
      decrypted == utf8Pw,
      qPrintable(
          QString("UTF-8 content should match exactly, expected:\n%1\ngot:\n%2")
              .arg(utf8Pw)
              .arg(decrypted)));
}

void tst_integration::imitatePass_longPassword() {
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  const QString longPw = QString(500, 'x');
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("long"), longPw, false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("long"));
  QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted");

  const QString decrypted = showSpy[0][0].toString();
  QVERIFY2(
      decrypted.length() == 500,
      qPrintable(
          QString(
              "long password should be preserved, expected 500 chars, got: %1")
              .arg(decrypted.length())));
  QVERIFY2(decrypted == longPw,
           "decrypted should match original long password");
}

void tst_integration::imitatePass_multilineContent() {
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  const QString multilinePw =
      QStringLiteral("secret\nnote: line 1\nnote: line 2\nnote: line 3");
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("multiline"), multilinePw, false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("multiline"));
  QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted");

  const QString decrypted = showSpy[0][0].toString();
  QVERIFY2(
      decrypted == multilinePw,
      qPrintable(QString("decrypted should match original multiline content, "
                         "expected:\n%1\ngot:\n%2")
                     .arg(multilinePw)
                     .arg(decrypted)));
}

QTEST_MAIN(tst_integration)
#include "tst_integration.moc"