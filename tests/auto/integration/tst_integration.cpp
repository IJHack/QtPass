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

#include <algorithm>
#include <functional>
#include <utility>

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimer>
#include <QTreeView>
#include <QtTest>

#include "../../../src/enums.h"
#include "../../../src/executor.h"
#include "../../../src/gpgidsigner.h"
#include "../../../src/imitatepass.h"
#include "../../../src/mainwindow.h"
#include "../../../src/pass.h"
#include "../../../src/passbackendfactory.h"
#include "../../../src/passworddialog.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/realpass.h"
#include "../../../src/userinfo.h"
#include "../../../src/usersdialog.h"
#include "../testsettings.h"

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

// Read a whole file; empty when it cannot be opened.
static QByteArray readFileBytes(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return {};
  return f.readAll();
}

// Encrypt @p bytes as they are (not through QString) to @p recipients into
// @p gpgFile, as another tool (pass insert -m from a Latin-1 terminal, a
// key file) would have stored them. Returns gpg's exit code.
static int encryptBytesTo(const QString &gnupgHome, const QByteArray &bytes,
                          const QStringList &recipients,
                          const QString &gpgFile) {
  QStringList args = {"--batch", "--yes", "--trust-model", "always",
                      "-e",      "-o",    gpgFile};
  for (const QString &r : recipients) {
    args << "-r" << r;
  }
  QProcess p;
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("GNUPGHOME", gnupgHome);
  p.setProcessEnvironment(env);
  p.start(findGpg(), args);
  p.write(bytes);
  p.closeWriteChannel();
  p.waitForFinished(30000);
  return p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
}

// Decrypt @p gpgFile to its exact bytes; empty on failure.
static QByteArray decryptBytes(const QString &gnupgHome,
                               const QString &gpgFile) {
  QProcess p;
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("GNUPGHOME", gnupgHome);
  p.setProcessEnvironment(env);
  p.start(findGpg(), {"--batch", "--quiet", "-d", gpgFile});
  p.waitForFinished(30000);
  return p.exitCode() == 0 ? p.readAllStandardOutput() : QByteArray();
}

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

// Generate a test GPG key with the given identity in GNUPGHOME; returns its
// fingerprint.
static QString generateNamedTestKey(const QString &gnupgHome,
                                    const QString &name, const QString &email) {
  const QString batch = QStringLiteral("%no-protection\n"
                                       "Key-Type: RSA\n"
                                       "Key-Length: 2048\n"
                                       "Subkey-Type: RSA\n"
                                       "Subkey-Length: 2048\n"
                                       "Name-Real: %1\n"
                                       "Name-Email: %2\n"
                                       "Expire-Date: 0\n"
                                       "%commit\n")
                            .arg(name, email);

  int rc = runGpg(gnupgHome, {"--batch", "--gen-key"}, batch);
  if (rc != 0)
    return {};

  QString out;
  runGpg(gnupgHome, {"--with-colons", "--fingerprint", email}, QString(), &out);

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

// Generate the default integration test key.
static QString generateTestKey(const QString &gnupgHome) {
  return generateNamedTestKey(gnupgHome, "QtPass Integration Test",
                              "qtpass-test@localhost");
}

// Return the 16-char encryption-subkey key IDs a .gpg file is encrypted to,
// parsed from `gpg --list-packets` (works without the secret key).
static QStringList recipientKeyIds(const QString &gnupgHome,
                                   const QString &gpgFile) {
  QString out;
  QString err;
  runGpg(gnupgHome, {"--list-packets", gpgFile}, QString(), &out, &err);
  const QString combined = out + err;
  QStringList ids;
  static const QRegularExpression re(QStringLiteral("keyid ([0-9A-Fa-f]{16})"));
  auto it = re.globalMatch(combined);
  while (it.hasNext()) {
    ids << it.next().captured(1).toUpper();
  }
  return ids;
}

// Return the 16-char key ID of a primary key's encryption subkey.
static QString encryptionSubkeyId(const QString &gnupgHome,
                                  const QString &fingerprint) {
  QString out;
  runGpg(gnupgHome, {"--with-colons", "--list-keys", fingerprint}, QString(),
         &out);
  for (const auto &line : out.split('\n')) {
    if (line.startsWith("sub:")) {
      const auto f = line.split(':');
      // field 12 = capabilities (contains 'e' for an encryption key),
      // field 5 = key ID.
      if (f.size() >= 12 && f.at(11).contains('e')) {
        return f.at(4).trimmed().toUpper();
      }
    }
  }
  return {};
}

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

/// The body the revoked-key test signs; any recipient list would do.
static const QByteArray kSignedListBody = QByteArrayLiteral("ALICE\n");

class tst_integration : public QObject {
  Q_OBJECT

  QString m_gpgExe;
  QTemporaryDir m_gnupgHome;
  QString m_keyFingerprint;
  QString m_keyFingerprint2;

  // Wait for a signal spy to receive at least one signal (up to timeoutMs).
  static bool waitForSignal(QSignalSpy &spy, int timeoutMs = 15000) {
    if (spy.count() > 0)
      return true;
    return spy.wait(timeoutMs);
  }

  // Initialize a Pass object with the test keyring and store.
  static void setupPass(Pass &pass) {
    AppSettings s = QtPassSettings::load();
    pass.init(s);
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
    AppSettings s = QtPassSettings::load();
    pass.init(s);
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

  // Initialize a git repo with local identity and disabled commit signing so
  // ImitatePass git operations succeed in tests. Returns true on success.
  static auto initGitRepo(const QString &storePath, const QString &gitExe)
      -> bool {
    QProcess gitInit;
    gitInit.setWorkingDirectory(storePath);
    gitInit.start(gitExe, {"init"});
    if (!gitInit.waitForFinished() || gitInit.exitCode() != 0) {
      return false;
    }
    QProcess cfgName;
    cfgName.setWorkingDirectory(storePath);
    if (!runGitConfig(cfgName, gitExe, {"config", "user.name", "Test User"})) {
      return false;
    }
    QProcess cfgEmail;
    cfgEmail.setWorkingDirectory(storePath);
    if (!runGitConfig(cfgEmail, gitExe,
                      {"config", "user.email", "test@example.com"})) {
      return false;
    }
    QProcess cfgSign;
    cfgSign.setWorkingDirectory(storePath);
    return runGitConfig(cfgSign, gitExe, {"config", "commit.gpgsign", "false"});
  }

  // A store made by `pass init`, under Git with every file committed, and
  // the settings pointing at it with the pass backend and Git on. Empty on
  // success, else what failed.
  auto initRealPassGitStore(const QString &store, const QString &passExe,
                            const QString &gitExe) -> QString {
    QtPassSettings::setPassStore(store);
    {
      AppSettings s = QtPassSettings::load();
      s.passExecutable = passExe;
      s.gpgExecutable = m_gpgExe;
      s.gitExecutable = gitExe;
      s.useGit = true;
      QtPassSettings::save(s);
    }
    QtPassSettings::setUsePass(true);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("GNUPGHOME", m_gnupgHome.path());
    env.insert("PASSWORD_STORE_DIR", store);
    QProcess init;
    init.setProcessEnvironment(env);
    init.start(passExe, {"init", m_keyFingerprint});
    if (!init.waitForFinished(15000) || init.exitCode() != 0)
      return QStringLiteral("pass init failed");
    if (!initGitRepo(store, gitExe))
      return QStringLiteral("git init failed");
    if (!runGit(store, gitExe, {"add", "-A"}) ||
        !runGit(store, gitExe, {"commit", "-q", "-m", "store"}))
      return QStringLiteral("first commit failed");
    return {};
  }

  // Run git in @p dir; true when it exits 0. @p out receives stdout.
  static auto runGit(const QString &dir, const QString &gitExe,
                     const QStringList &args, QString *out = nullptr) -> bool {
    QProcess git;
    git.setWorkingDirectory(dir);
    git.start(gitExe, args);
    if (!git.waitForFinished(15000) || git.exitCode() != 0)
      return false;
    if (out != nullptr)
      *out = QString::fromUtf8(git.readAllStandardOutput());
    return true;
  }

private Q_SLOTS:
  void initTestCase();
  void cleanupTestCase();

  // ImitatePass backend
  void imitatePass_insertAndShow();
  void imitatePass_insertAndGrep();
  void imitatePass_grepSkipsUndecryptableFiles();
  void imitatePass_insertMoveAndShow();
  void imitatePass_colonNameIsAnEntryInTheStore();
  void imitatePass_insertCopyAndShow();
  void imitatePass_copyOntoFolderAndShow();
  void imitatePass_copyOntoExistingEntryClash();
  void imitatePass_copyIntoFolderReencryptsToFolderKey();
  void imitatePass_moveIntoFolderReencryptsToFolderKey();
  void imitatePass_gitMoveReencryptsOnceItRan();
  void imitatePass_noMessageBoxInsideATransaction();
  void imitatePass_insertAndRemove();
  void imitatePass_generateGpgKeysEmptyExecutable();
  void imitatePass_nestedDirectoryInsertAndShow();
  void imitatePass_editExistingEntry();
  void imitatePass_grepCaseInsensitive();
  void imitatePass_specialCharactersInPassword();
  void imitatePass_emptyPassword();
  void imitatePass_gitInitAndCommit();
  void imitatePass_gitCopyAndShow();
  void imitatePass_insertCompletesWhenGitNotConfigured();
  void imitatePass_removeFallsBackWhenGitNotConfigured();
  void imitatePass_moveFallsBackWhenGitNotConfigured();
  void imitatePass_usersDialogListsAndFilters();
  void imitatePass_multiRecipientReencryptChangesRecipients();
  void imitatePass_reencryptKeyListingWritesNoFile();
  void imitatePass_gitAttributesPinnedForStore();
  void imitatePass_gitInitPinsAttributes();
  void imitatePass_ignoredEntryIsStillCommitted();
  void imitatePass_insertIgnoresGpgConfEncryptTo();
  void imitatePass_reencryptIgnoresGpgConfEncryptTo();
  void imitatePass_reencryptPreservesPerFolderRecipients();
  void imitatePass_reencryptIncludesDotNamedEntries();
  void imitatePass_signedGpgIdInitAndReencrypt();
  void imitatePass_tamperedGpgIdSignatureAbortsReencrypt();
  void imitatePass_reencryptMixedFailuresReportedOnce();
  void mainWindow_selectingEntryShowsDecryptedContent();
  void mainWindow_editEntryThroughModalDialogUpdatesContent();

  // UTF-8 and unicode handling
  void imitatePass_utf8Characters();
  void imitatePass_showFlagsAnEntryThatIsNotUtf8();
  void imitatePass_reencryptKeepsBytesThatAreNotUtf8();

  // Edge cases
  void imitatePass_longPassword();
  void imitatePass_multilineContent();

  // RealPass backend (skipped if `pass` not installed)
  void realPass_insertAndShow();
  void realPass_initThatCannotReencryptIsReported();
  void realPass_insertAndGrep();
  void realPass_unrecordedEntryIsReported();
  void realPass_initRefusesUnrecordedFiles();
  void realPass_fileManagerFilesGetAHint();
  void realPass_refusesStagedAndUnignoredFiles();
  void realPass_checksTheRepositoryPassCommitsIn();
  void realPass_dashNameDoesNotEscapeStore();
  void realPass_pushedBareRepoDoesNotRunCommand();
  void gpgIdSigner_revokedExpiredKeyRefusedUnderTrustModelAlways();

  // pass-otp (skipped if extension not installed)
};

// ---------------------------------------------------------------------------

void tst_integration::initTestCase() {
  isolateTestSettings();
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

  // A second, independent key used by multi-recipient / re-encryption tests.
  m_keyFingerprint2 = generateNamedTestKey(
      m_gnupgHome.path(), "QtPass Second Key", "qtpass-test2@localhost");
  QVERIFY2(!m_keyFingerprint2.isEmpty(),
           "Failed to generate second GPG key for integration tests");

  // Redirect GNUPGHOME process-wide so all child processes inherit the test
  // keyring. Also set it in QtPassSettings so Pass::init() propagates it
  // to the Executor environment via updateEnv().
  qputenv("GNUPGHOME", m_gnupgHome.path().toLocal8Bit());
  QtPassSettings::getInstance()->setValue(SettingsConstants::gpgHome,
                                          m_gnupgHome.path());
  {
    AppSettings s = QtPassSettings::load();
    s.gpgExecutable = m_gpgExe;
    QtPassSettings::save(s);
  }
  {
    AppSettings s = QtPassSettings::load();
    s.passSigningKey = QString();
    QtPassSettings::save(s);
  }
}

void tst_integration::cleanupTestCase() {
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

void tst_integration::imitatePass_generateGpgKeysEmptyExecutable() {
  // Regression test for #1598: with no gpg executable configured,
  // GenerateGPGKeys must surface an error rather than silently dropping the
  // command (which would leave the keygen dialog spinning forever with no
  // feedback). No gpg is run.
  ImitatePass pass;
  AppSettings s = QtPassSettings::load();
  s.gpgExecutable = QString(); // unconfigured
  pass.init(s);

  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);
  QSignalSpy successSpy(&pass, &Pass::finishedGenerateGPGKeys);
  QVERIFY(errorSpy.isValid());
  QVERIFY(successSpy.isValid());

  pass.GenerateGPGKeys(QStringLiteral("Key-Type: RSA\n%commit\n"));

  QTRY_COMPARE_WITH_TIMEOUT(errorSpy.count(), 1, 5000);
  QCOMPARE(successSpy.count(), 0);
  QVERIFY2(errorSpy.first().at(0).toInt() != 0,
           "an unconfigured gpg executable must report a non-zero exit code");
}

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
  // The report names the entry it wrote (scan ANT-2026-1299KSWS).
  QString reported;
  connect(&pass, &Pass::finishedInsert, this,
          [&pass, &reported] { reported = pass.reportedInsert(); });
  pass.Insert(entryName, entryContent, false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  // Verify the .gpg file was created.
  const QString expectedFile = storeDir.path() + "/" + entryName + ".gpg";
  QVERIFY2(QFile::exists(expectedFile), "encrypted file should exist");
  QCOMPARE(reported, QDir::cleanPath(expectedFile));
  QVERIFY(pass.reportedInsert().isEmpty());

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
  pass.Grep(QStringLiteral("token"), false);
  QVERIFY2(waitForSignal(grepSpy, 20000), "finishedGrep not emitted");

  const auto results = grepSpy[0][0].value<GrepResults>();
  QVERIFY2(results.size() == 2, "grep should find both entries");
}

// Verifies the env-aware Executor failure path used by
// NativeGrep::matchFile(): when gpg returns non-zero on a corrupt /
// foreign .gpg file inside the store, NativeGrep::scanStore must drop that
// file's results and keep walking — not abort the whole scan or surface
// garbage.
//
// Plants two legitimate entries, then drops two non-gpg payloads with a
// .gpg extension into the store (one with the "token" substring and one
// without, both undecryptable). Grep for "token" must come back with
// exactly the two legitimate matches.
void tst_integration::imitatePass_grepSkipsUndecryptableFiles() {
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  QVERIFY2(QDir(storeDir.path()).mkpath("work"),
           ("failed to create work directory at " + storeDir.path()).toUtf8());

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

  // Plant two .gpg files that aren't valid gpg ciphertext at all. Their
  // file name contains "token" — but since gpg --decrypt will fail with
  // non-zero exit on the garbage payload, matchFile must throw away
  // the would-be plaintext (including the embedded "token" substring)
  // and NativeGrep::scanStore must continue past the failure.
  const QString corrupt1 = storeDir.path() + "/work/garbage-token.gpg";
  const QString corrupt2 = storeDir.path() + "/corrupt.gpg";
  QFile c1(corrupt1);
  QVERIFY2(c1.open(QIODevice::WriteOnly), "should create garbage-token.gpg");
  const char *payload1 = "not a real gpg payload but contains the token word\n";
  qint64 bytesWritten1 = c1.write(payload1);
  QVERIFY2(bytesWritten1 == static_cast<qint64>(strlen(payload1)),
           "failed to write test payload to c1");
  c1.close();
  QFile c2(corrupt2);
  QVERIFY2(c2.open(QIODevice::WriteOnly), "should create corrupt.gpg");
  QByteArray corruptData("\xFF\xFE\x00\x01 binary junk\n", 17);
  qint64 bytesWritten2 = c2.write(corruptData);
  QVERIFY2(bytesWritten2 == corruptData.size(),
           "failed to write test payload to c2");
  c2.close();

  QSignalSpy grepSpy(&pass, &Pass::finishedGrep);
  pass.Grep(QStringLiteral("token"), false);
  QVERIFY2(waitForSignal(grepSpy, 20000), "finishedGrep not emitted");

  const auto results = grepSpy[0][0].value<GrepResults>();
  // The two real entries must be present; neither of the corrupt files
  // may contribute a match even though one contains "token" in its raw
  // bytes — gpg failed to decrypt them, so matchFile returned empty.
  QVERIFY2(
      results.size() == 2,
      qPrintable(
          QStringLiteral("expected 2 matches, got %1").arg(results.size())));
  QStringList paths;
  for (const auto &pair : results)
    paths << pair.first;
  std::sort(paths.begin(), paths.end());
  QCOMPARE(paths, (QStringList{QStringLiteral("work/github"),
                               QStringLiteral("work/gitlab")}));
}

// A name starting with ':' is an entry in the store: Qt takes it for a
// resource path, and the save went to QtPass's working directory while the
// store kept the old entry (scan finding ANT-2026-70DZK0Q9).
void tst_integration::imitatePass_colonNameIsAnEntryInTheStore() {
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral(":top"), QStringLiteral("secret\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(errorSpy));
  QVERIFY(QFileInfo(storeDir.path() + "/:top.gpg").isFile());
  QVERIFY(!QFileInfo::exists(QDir::current().filePath(":top.gpg")));
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
  pass.Move(src, dst, false);
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
  pass.Copy(src, dst, false);
  QVERIFY2(QFile::exists(src), "source should still exist after copy");
  QVERIFY2(QFile::exists(dst), "destination should exist after copy");

  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("copy"));
  QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted after copy");
  QVERIFY2(showSpy[0][0].toString().contains("copyme"),
           "decrypted copy should contain original content");
}

void tst_integration::imitatePass_copyOntoFolderAndShow() {
  // Regression for #1682: a drag-and-drop copy hands the *folder* to the
  // backend (the pass CLI needs it that way). ImitatePass::Copy used to pass
  // that folder straight to QFile::copy, which cannot write a directory, so
  // every copy onto a folder failed. It must land as <folder>/<entry>.gpg.
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  QVERIFY(QDir(storeDir.path()).mkpath("folder"));

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("original"), QStringLiteral("copyme\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  const QString src = storeDir.path() + "/original.gpg";
  const QString folder = storeDir.path() + "/folder";
  const QString dst = folder + "/original.gpg";

  QSignalSpy criticalSpy(&pass, &Pass::critical);
  QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
  // Without git, Copy is synchronous — no finishedCopy signal emitted.
  pass.Copy(src, folder, false);
  QVERIFY2(criticalSpy.isEmpty(), "copy onto a folder must not fail");
  QVERIFY2(QFile::exists(src), "source should still exist after copy");
  QVERIFY2(QFile::exists(dst),
           "copy onto a folder should create <folder>/<entry>.gpg");
  QVERIFY2(!QFile::exists(dst + ".gpg"), "must not double the .gpg suffix");

  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("folder/original"));
  QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted after copy");
  QVERIFY2(showSpy[0][0].toString().contains("copyme"),
           "decrypted copy should contain original content");

  // Copying onto the folder again without force must refuse: the folder
  // itself is a valid destination, so only the resolved file reveals the
  // clash, and the existing entry must stay byte-for-byte as it was. The
  // first copy re-encrypts the folder on a worker thread; let that land
  // before taking the reference bytes.
  QVERIFY2(waitForSignal(endSpy, 60000), "endReencryptPath not emitted");
  const QByteArray before = readFileBytes(dst);
  pass.Copy(src, folder, false);
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(criticalSpy[0][0].toString(), QStringLiteral("Copy failed"));
  QCOMPARE(readFileBytes(dst), before);
}

void tst_integration::imitatePass_copyOntoExistingEntryClash() {
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("original"), QStringLiteral("copyme\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));
  insertSpy.clear();
  pass.Insert(QStringLiteral("taken"), QStringLiteral("keepme\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  const QString src = storeDir.path() + "/original.gpg";
  const QString dst = storeDir.path() + "/taken.gpg";
  const QByteArray srcBytes = readFileBytes(src);
  const QByteArray dstBytes = readFileBytes(dst);
  QVERIFY2(!srcBytes.isEmpty() && !dstBytes.isEmpty(),
           "both entries must exist before the clash");

  // Without force the clash must be reported, not silently ignored, and
  // neither file may change.
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  pass.Copy(src, dst, false);
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(criticalSpy[0][0].toString(), QStringLiteral("Copy failed"));
  QCOMPARE(readFileBytes(src), srcBytes);
  QCOMPARE(readFileBytes(dst), dstBytes);

  // With force the existing entry is overwritten by the copy.
  pass.Copy(src, dst, true);
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(readFileBytes(src), srcBytes);
  QVERIFY2(readFileBytes(dst) != dstBytes,
           "force copy should overwrite the target");

  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("taken"));
  QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted after copy");
  QVERIFY2(showSpy[0][0].toString().contains("copyme"),
           "overwritten entry should now hold the copied content");

  // Copying an entry onto its own folder resolves to the entry itself; with
  // force that must not delete the only copy.
  const QByteArray copied = readFileBytes(dst);
  pass.Copy(dst, storeDir.path(), true);
  QCOMPARE(criticalSpy.count(), 2);
  QCOMPARE(criticalSpy[1][0].toString(), QStringLiteral("Copy failed"));
  QVERIFY2(QFile::exists(dst),
           "source must survive a copy onto its own folder");
  QCOMPARE(readFileBytes(dst), copied);
}

void tst_integration::imitatePass_copyIntoFolderReencryptsToFolderKey() {
  // A copy into a folder with its own .gpg-id must be re-encrypted for that
  // folder's recipients. Copy() decides whether to re-encrypt from a QFileInfo
  // it created before the file existed; QFileInfo caches, so a stale "not a
  // file" answer would silently leave the copy readable only by the source
  // folder's key.
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QtPassSettings::setPassStore(storeDir.path());

  auto writeGpgId = [](const QString &dir, const QString &fpr) -> bool {
    QDir().mkpath(dir);
    QFile f(QDir::cleanPath(dir + "/.gpg-id"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
      return false;
    const QByteArray payload = (fpr + "\n").toUtf8();
    return f.write(payload) == payload.size();
  };
  QVERIFY(writeGpgId(storeDir.path(), m_keyFingerprint));
  const QString subDir = QDir::cleanPath(storeDir.path() + "/team");
  QVERIFY(writeGpgId(subDir, m_keyFingerprint2));

  ImitatePass pass;
  {
    AppSettings s = QtPassSettings::load();
    pass.init(s);
    pass.updateEnv();
  }

  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);
  {
    QSignalSpy spy(&pass, &Pass::finishedInsert);
    pass.Insert(QStringLiteral("rootentry"), QStringLiteral("r\n"), false);
    QVERIFY2(waitForSignal(spy), gpgInsertErrorMsg(errorSpy));
  }

  const QString src = QDir::cleanPath(storeDir.path() + "/rootentry.gpg");
  const QString dst = QDir::cleanPath(subDir + "/rootentry.gpg");
  QSignalSpy criticalSpy(&pass, &Pass::critical);
  {
    QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
    pass.Copy(src, subDir, false);
    QVERIFY2(criticalSpy.isEmpty(), "copy into a folder must not fail");
    QVERIFY2(QFile::exists(dst), "copy must land as <folder>/<entry>.gpg");
    QVERIFY2(waitForSignal(endSpy, 60000),
             "copy into a folder must re-encrypt it (endReencryptPath)");
  }

  const QString sub1 = encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint);
  const QString sub2 =
      encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint2);
  QVERIFY(!sub1.isEmpty() && !sub2.isEmpty());
  const QStringList ids = recipientKeyIds(m_gnupgHome.path(), dst);
  QVERIFY2(ids.contains(sub2) && !ids.contains(sub1),
           qPrintable(QStringLiteral("copied entry must target the folder's "
                                     "key only, got: %1")
                          .arg(ids.join(','))));
  // The source stays as it was.
  const QStringList srcIds = recipientKeyIds(m_gnupgHome.path(), src);
  QVERIFY2(srcIds.contains(sub1) && !srcIds.contains(sub2),
           qPrintable(QStringLiteral("source must still target key1 only, "
                                     "got: %1")
                          .arg(srcIds.join(','))));
}

void tst_integration::imitatePass_moveIntoFolderReencryptsToFolderKey() {
  // A move into a folder with another .gpg-id re-encrypts what moved for
  // that folder's recipients, as `pass mv` does: the source folder's
  // recipients must not keep reading it. A rename, or a folder that takes
  // its own list along, needs no re-encryption.
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QtPassSettings::setPassStore(storeDir.path());

  auto writeGpgId = [](const QString &dir, const QString &fpr) -> bool {
    QDir().mkpath(dir);
    QFile f(QDir::cleanPath(dir + "/.gpg-id"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
      return false;
    const QByteArray payload = (fpr + "\n").toUtf8();
    return f.write(payload) == payload.size();
  };
  const QDir store(storeDir.path());
  QVERIFY(writeGpgId(storeDir.path(), m_keyFingerprint));
  const QString team = QDir::cleanPath(store.filePath("team"));
  QVERIFY(writeGpgId(team, m_keyFingerprint2));
  QVERIFY(writeGpgId(store.filePath("own"), m_keyFingerprint));
  QVERIFY(store.mkpath("plain"));

  ImitatePass pass;
  {
    AppSettings s = QtPassSettings::load();
    pass.init(s);
    pass.updateEnv();
  }

  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);
  for (const QString &name :
       {QStringLiteral("rootentry"), QStringLiteral("other"),
        QStringLiteral("own/kept"), QStringLiteral("plain/carried")}) {
    QSignalSpy spy(&pass, &Pass::finishedInsert);
    pass.Insert(name, QStringLiteral("r\n"), false);
    QVERIFY2(waitForSignal(spy), gpgInsertErrorMsg(errorSpy));
  }

  const QString sub1 = encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint);
  const QString sub2 =
      encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint2);
  QVERIFY(!sub1.isEmpty() && !sub2.isEmpty());
  const auto targetsOnly = [&](const QString &file, const QString &key,
                               const QString &other) {
    const QStringList ids = recipientKeyIds(m_gnupgHome.path(), file);
    return ids.contains(key) && !ids.contains(other);
  };

  QSignalSpy criticalSpy(&pass, &Pass::critical);
  QSignalSpy startSpy(&pass, &ImitatePass::startReencryptPath);
  // A neighbour still encrypted to the root's key: the move re-encrypts
  // what moved, not the folder it lands in.
  const QString neighbour = team + "/neighbour.gpg";
  QVERIFY(QFile::copy(store.filePath("other.gpg"), neighbour));
  // An entry dropped onto a folder with another list.
  const QString moved = QDir::cleanPath(team + "/rootentry.gpg");
  {
    QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
    pass.Move(store.filePath("rootentry.gpg"), team, false);
    QVERIFY2(criticalSpy.isEmpty(), "the move must not fail");
    QVERIFY(QFile::exists(moved));
    QVERIFY2(waitForSignal(endSpy, 60000),
             "a move into a folder with another list must re-encrypt it");
  }
  QVERIFY2(targetsOnly(moved, sub2, sub1),
           qPrintable(recipientKeyIds(m_gnupgHome.path(), moved).join(',')));
  QVERIFY2(targetsOnly(neighbour, sub1, sub2), "the neighbour was left alone");

  // A rename in the same folder keeps the list: nothing to re-encrypt.
  startSpy.clear();
  pass.Move(moved, team + "/renamed", false);
  QVERIFY(QFile::exists(team + "/renamed.gpg"));
  QCOMPARE(startSpy.size(), 0);

  // A folder with its own list takes it along.
  pass.Move(store.filePath("own"), team, false);
  QVERIFY(QFile::exists(team + "/own/kept.gpg"));
  QCOMPARE(startSpy.size(), 0);
  QVERIFY(targetsOnly(team + "/own/kept.gpg", sub1, sub2));

  // A folder without one does not.
  {
    QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
    pass.Move(store.filePath("plain"), team, false);
    QVERIFY(QFile::exists(team + "/plain/carried.gpg"));
    QVERIFY2(waitForSignal(endSpy, 60000),
             "a folder moved under another list must be re-encrypted");
  }
  QVERIFY(targetsOnly(team + "/plain/carried.gpg", sub2, sub1));
  QVERIFY2(criticalSpy.isEmpty(),
           qPrintable(criticalSpy.isEmpty()
                          ? QString()
                          : criticalSpy.first().at(1).toString()));
}

void tst_integration::imitatePass_noMessageBoxInsideATransaction() {
  // A message box spins an event loop; whatever the user does meanwhile can
  // queue a process. Opened inside a move's, copy's or removal's
  // transaction, that process was booked as a step of it: an entry opened
  // then came back as the output of `pass mv`, which is not secret, and
  // never reached finishedShow. Here the box is a slot that opens the entry.
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);
  const QDir store(storeDir.path());
  QVERIFY(store.mkpath("locked"));

  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  for (const QString &name : {QStringLiteral("mail"), QStringLiteral("bank"),
                              QStringLiteral("locked/entry")}) {
    QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
    pass.Insert(name, name + QStringLiteral("-secret\n"), false);
    QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));
  }
  QVERIFY(QFile::link(store.filePath("bank.gpg"), store.filePath("work.gpg")));

  bool open = false;
  QStringList titles;
  connect(&pass, &Pass::critical, this, [&](const QString &title) {
    titles << title;
    if (open) {
      open = false;
      pass.Show(QStringLiteral("bank"));
    }
  });
  const auto showsWhileTheBoxIsOpen = [&](const std::function<void()> &op) {
    QSignalSpy showSpy(&pass, &Pass::finishedShow);
    titles.clear();
    open = true;
    op();
    return !open && waitForSignal(showSpy) &&
           showSpy.first().at(0).toString().contains("bank-secret");
  };

  // A rename onto a name whose .gpg is a link.
  QVERIFY2(showsWhileTheBoxIsOpen([&] {
             pass.Move(store.filePath("mail.gpg"), store.filePath("work"),
                       false);
           }),
           qPrintable(titles.join(',')));
  QVERIFY(QFile::exists(store.filePath("mail.gpg")));
  // A copy onto it.
  QVERIFY2(showsWhileTheBoxIsOpen([&] {
             pass.Copy(store.filePath("mail.gpg"), store.filePath("work"),
                       false);
           }),
           qPrintable(titles.join(',')));

  // A removal that fails.
  const QString locked = store.filePath("locked");
  QVERIFY(QFile::setPermissions(locked, QFile::ReadOwner | QFile::ExeOwner));
  QFile probe(locked + "/probe");
  if (probe.open(QIODevice::WriteOnly)) {
    probe.close();
    probe.remove();
    QFile::setPermissions(locked, QFile::ReadOwner | QFile::WriteOwner |
                                      QFile::ExeOwner);
    QSKIP("a read-only folder does not stop this user (root?)");
  }
  const bool shown = showsWhileTheBoxIsOpen(
      [&] { pass.Remove(QStringLiteral("locked/entry"), false); });
  QFile::setPermissions(locked,
                        QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
  QVERIFY2(shown, qPrintable(titles.join(',')));
  QVERIFY(QFile::exists(locked + "/entry.gpg"));
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
// RAII guard to ensure useGit is restored on any exit path
struct RestoreUseGit {
  bool orig;
  QString origGit;
  RestoreUseGit()
      : orig(QtPassSettings::isUseGit()),
        origGit(QtPassSettings::load().gitExecutable) {}
  ~RestoreUseGit() {
    AppSettings s = QtPassSettings::load();
    s.useGit = orig;
    s.gitExecutable = origGit;
    QtPassSettings::save(s);
  }
};

// RAII guard for the signing key setting; the rest of the suite expects the
// store to be unsigned (verifyGpgIdFile is fail-open without a signing key).
struct RestorePassSigningKey {
  QString orig;
  RestorePassSigningKey() : orig(QtPassSettings::load().passSigningKey) {}
  ~RestorePassSigningKey() {
    AppSettings s = QtPassSettings::load();
    s.passSigningKey = orig;
    QtPassSettings::save(s);
  }
};

// A UserInfo representing key @p keyId enabled for use; used to drive
// Pass::Init (what UsersDialog::accept() calls).
static UserInfo enabledUser(const QString &keyId) {
  UserInfo u;
  u.enabled = true;
  u.have_secret = true;
  u.key_id = keyId;
  return u;
}
} // namespace

void tst_integration::imitatePass_editExistingEntry() {
  RestoreUseGit restoreUseGit;
  {
    AppSettings s = QtPassSettings::load();
    s.useGit = false; // Ensure git is off for this test
    QtPassSettings::save(s);
  }

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

  {
    AppSettings s = QtPassSettings::load();
    s.gitExecutable = gitExe;
    s.useGit = true;
    QtPassSettings::save(s);
  }

  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  QVERIFY2(initGitRepo(storeDir.path(), gitExe),
           "git repo setup (init + identity/signing config) should succeed");

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

void tst_integration::imitatePass_gitMoveReencryptsOnceItRan() {
  // With Git the move is queued: whether it happened, and which list
  // applies where it lands, is known only once git mv has run. A folder
  // move queued before can create the destination folder, and a git mv
  // that fails leaves the destination as it was, not to be rewritten.
  const QString gitExe = QStandardPaths::findExecutable("git");
  if (gitExe.isEmpty())
    QSKIP("git not installed");
  RestoreUseGit restoreUseGit;
  {
    AppSettings s = QtPassSettings::load();
    s.gitExecutable = gitExe;
    s.useGit = true;
    QtPassSettings::save(s);
  }
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);
  QVERIFY(initGitRepo(storeDir.path(), gitExe));
  const QDir store(storeDir.path());
  QVERIFY(store.mkpath("own"));
  QFile list(store.filePath("own/.gpg-id"));
  QVERIFY(list.open(QIODevice::WriteOnly));
  list.write((m_keyFingerprint2 + "\n").toUtf8());
  list.close();
  QVERIFY(runGit(storeDir.path(), gitExe, {"add", "own/.gpg-id"}));
  QVERIFY(runGit(storeDir.path(), gitExe, {"commit", "-q", "-m", "own"}));

  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  for (const QString &name :
       {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"),
        QStringLiteral("d"), QStringLiteral("f"), QStringLiteral("g"),
        QStringLiteral("h"), QStringLiteral("i"), QStringLiteral("j"),
        QStringLiteral("k"), QStringLiteral("l"), QStringLiteral("own/kept")}) {
    QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
    pass.Insert(name, QStringLiteral("r\n"), false);
    QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));
  }
  const QString sub1 = encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint);
  const QString sub2 =
      encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint2);
  QVERIFY(!sub1.isEmpty() && !sub2.isEmpty());

  QSignalSpy critical(&pass, &Pass::critical);
  QSignalSpy started(&pass, &ImitatePass::startReencryptPath);
  QSignalSpy ended(&pass, &ImitatePass::endReencryptPath);
  QSignalSpy movedSpy(&pass, &Pass::finishedMove);
  // A move into what a queued move is still to create: refused, as
  // which list applies there is not known yet. Once that has run it goes.
  pass.Move(store.filePath("own"), store.filePath("team"), false);
  pass.Move(store.filePath("a.gpg"), store.filePath("team/a.gpg"), false);
  QCOMPARE(critical.size(), 1);
  QVERIFY2(critical.first().at(1).toString().contains("still being moved"),
           qPrintable(critical.first().at(1).toString()));
  QVERIFY(QFile::exists(store.filePath("a.gpg")));
  QVERIFY(waitForSignal(movedSpy, 30000));
  QCOMPARE(started.size(), 0);
  critical.clear();
  pass.Move(store.filePath("a.gpg"), store.filePath("team/a.gpg"), false);
  QVERIFY2(waitForSignal(ended, 60000), "the entry was re-encrypted");
  QCOMPARE(started.size(), 1);
  const QStringList ids =
      recipientKeyIds(m_gnupgHome.path(), store.filePath("team/a.gpg"));
  QVERIFY2(ids.contains(sub2) && !ids.contains(sub1),
           qPrintable(ids.join(',')));
  QVERIFY2(critical.isEmpty(),
           qPrintable(critical.isEmpty() ? QString()
                                         : critical.first().at(1).toString()));

  // Moving the folder an entry's run is still waiting for: refused, or the
  // run would look for the entry where it no longer is. While the entry's
  // move is queued, while its run is in flight, and while it waits behind
  // another run.
  const auto keyOf = [&](const QString &name) {
    return recipientKeyIds(m_gnupgHome.path(), store.filePath(name));
  };
  const auto moveTeamAway = [&] {
    pass.Move(store.filePath("team"), store.filePath("archive"), false);
  };
  started.clear();
  ended.clear();
  pass.Move(store.filePath("f.gpg"), store.filePath("team"), false);
  moveTeamAway();
  QCOMPARE(critical.size(), 1);
  QVERIFY2(waitForSignal(ended, 60000), "the entry was re-encrypted");

  bool armed = true;
  auto hook = connect(&pass, &ImitatePass::startReencryptPath, this, [&] {
    if (std::exchange(armed, false)) {
      moveTeamAway();
    }
  });
  ended.clear();
  pass.Move(store.filePath("h.gpg"), store.filePath("team"), false);
  QVERIFY2(waitForSignal(ended, 60000), "the entry was re-encrypted");
  disconnect(hook);
  QCOMPARE(critical.size(), 2);

  armed = true;
  hook = connect(&pass, &ImitatePass::endReencryptPath, this, [&] {
    if (std::exchange(armed, false)) {
      moveTeamAway();
    }
  });
  ended.clear();
  pass.Move(store.filePath("i.gpg"), store.filePath("team"), false);
  pass.Move(store.filePath("j.gpg"), store.filePath("team"), false);
  QTRY_COMPARE_WITH_TIMEOUT(ended.size(), 2, 60000);
  disconnect(hook);
  QCOMPARE(critical.size(), 3);
  QVERIFY(QFileInfo(store.filePath("team")).isDir());
  for (const QString &name :
       {QStringLiteral("team/f.gpg"), QStringLiteral("team/h.gpg"),
        QStringLiteral("team/i.gpg"), QStringLiteral("team/j.gpg")}) {
    const QStringList now = keyOf(name);
    QVERIFY2(now.contains(sub2) && !now.contains(sub1),
             qPrintable(name + ": " + now.join(',')));
  }
  critical.clear();

  // A git mv that fails moves nothing, even when an earlier step took its
  // source away: the entry under the destination name stays as it is. Git
  // through a wrapper that holds `rm` and `commit` back a second, so the
  // source is still there when the move is asked for.
  QTemporaryDir wrapDir;
  QVERIFY(wrapDir.isValid());
  const QString wrapper = wrapDir.filePath("git");
  {
    QFile script(wrapper);
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write("#!/bin/sh\ncase \" $* \" in *\" rm \"*|*\" commit \"*) sleep "
                 "1;; esac\n"
                 "exec \"" +
                 gitExe.toUtf8() + "\" \"$@\"\n");
    script.close();
    QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                                  QFile::ExeOwner));
  }
  {
    AppSettings s = QtPassSettings::load();
    s.gitExecutable = wrapper;
    pass.init(s);
    pass.updateEnv();
  }
  started.clear();
  QSignalSpy failedMove(&pass, &Pass::processErrorExit);
  pass.Remove(QStringLiteral("g"), false);
  QVERIFY(QFile::exists(store.filePath("g.gpg")));
  pass.Move(store.filePath("g.gpg"), store.filePath("team/kept.gpg"), true);
  QVERIFY2(waitForSignal(failedMove, 30000), "git mv failed");
  QTest::qWait(300);
  QCOMPARE(started.size(), 0);
  QVERIFY(!QFile::exists(store.filePath("g.gpg")));

  // Something written under the source name after git mv ran, before the
  // move settles (its commit is held back), does not stop the run.
  started.clear();
  ended.clear();
  pass.Move(store.filePath("k.gpg"), store.filePath("team"), false);
  QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(store.filePath("team/k.gpg")), 10000);
  QFile again(store.filePath("k.gpg"));
  QVERIFY(again.open(QIODevice::WriteOnly));
  again.write("written again\n");
  again.close();
  QVERIFY2(waitForSignal(ended, 60000), "the entry was re-encrypted");
  const QStringList kIds = keyOf("team/k.gpg");
  QVERIFY2(kIds.contains(sub2) && !kIds.contains(sub1),
           qPrintable(kIds.join(',')));
  QVERIFY(QFile::remove(store.filePath("k.gpg")));
  {
    AppSettings s = QtPassSettings::load();
    pass.init(s);
    pass.updateEnv();
  }

  // git mv refuses a file Git does not track: nothing moved, so the entry
  // under the destination name is not re-encrypted.
  QVERIFY(QFile::copy(store.filePath("team/a.gpg"),
                      store.filePath("untracked.gpg")));
  QSignalSpy failed(&pass, &Pass::processErrorExit);
  started.clear();
  pass.Move(store.filePath("untracked.gpg"), store.filePath("team/kept.gpg"),
            true);
  QVERIFY2(waitForSignal(failed, 30000), "git mv failed");
  QTest::qWait(300);
  QCOMPARE(started.size(), 0);
  QVERIFY(QFile::exists(store.filePath("untracked.gpg")));
  QVERIFY(QFile::remove(store.filePath("untracked.gpg")));

  // A remote: the push after a move waits for its re-encryption, so what
  // reaches it is the entry for team's recipients, not the one it moved as.
  QTemporaryDir remoteDir;
  QVERIFY(remoteDir.isValid());
  QVERIFY(runGit(remoteDir.path(), gitExe, {"init", "-q", "--bare"}));
  QVERIFY(runGit(storeDir.path(), gitExe,
                 {"remote", "add", "origin", remoteDir.path()}));
  QVERIFY(
      runGit(storeDir.path(), gitExe, {"push", "-q", "-u", "origin", "HEAD"}));
  QSignalSpy pushed(&pass, &Pass::finishedGitPush);

  // Two entries queued into team: the second settles while the first's
  // run is in flight, and is re-encrypted after it, not skipped. The push
  // waits for both runs.
  started.clear();
  ended.clear();
  pass.Move(store.filePath("b.gpg"), store.filePath("team"), false);
  pass.Move(store.filePath("c.gpg"), store.filePath("team"), false);
  pass.GitPushAutomatic();
  QVERIFY(waitForSignal(ended, 60000));
  QTRY_COMPARE_WITH_TIMEOUT(ended.size(), 2, 60000);
  QCOMPARE(started.size(), 2);
  for (const QString &name :
       {QStringLiteral("team/b.gpg"), QStringLiteral("team/c.gpg")}) {
    const QStringList now =
        recipientKeyIds(m_gnupgHome.path(), store.filePath(name));
    QVERIFY2(now.contains(sub2) && !now.contains(sub1),
             qPrintable(name + ": " + now.join(',')));
  }

  QVERIFY2(waitForSignal(pushed, 30000), "the push was made");
  QCOMPARE(pushed.size(), 1);
  const auto remoteIdsOf = [&](const QString &name) {
    QProcess show;
    show.setWorkingDirectory(remoteDir.path());
    show.setStandardOutputFile(store.filePath("remote.gpg"));
    show.start(gitExe, {"show", "HEAD:" + name});
    show.waitForFinished();
    const QStringList ids =
        recipientKeyIds(m_gnupgHome.path(), store.filePath("remote.gpg"));
    QFile::remove(store.filePath("remote.gpg"));
    return ids;
  };
  for (const QString &name :
       {QStringLiteral("team/b.gpg"), QStringLiteral("team/c.gpg")}) {
    const QStringList remote = remoteIdsOf(name);
    QVERIFY2(remote.contains(sub2) && !remote.contains(sub1),
             qPrintable("remote " + name + ": " + remote.join(',')));
  }

  // One entry, and the push asked for while its move is still queued.
  pushed.clear();
  pass.Move(store.filePath("d.gpg"), store.filePath("team"), false);
  pass.GitPushAutomatic();
  QVERIFY2(waitForSignal(pushed, 60000), "the push was made");
  const QStringList remoteIds = remoteIdsOf("team/d.gpg");
  QVERIFY2(remoteIds.contains(sub2) && !remoteIds.contains(sub1),
           qPrintable(remoteIds.join(',')));

  // A rename needs no re-encryption: its push goes once the move settled.
  pushed.clear();
  started.clear();
  pass.Move(store.filePath("team/d.gpg"), store.filePath("team/e"), false);
  pass.GitPushAutomatic();
  QVERIFY2(waitForSignal(pushed, 30000), "the push was made");
  QCOMPARE(started.size(), 0);
  QVERIFY(
      runGit(remoteDir.path(), gitExe, {"cat-file", "-e", "HEAD:team/e.gpg"}));

  // A run refused at its start (a rebase begun while the move was queued):
  // the entry is still for the recipients it came from, so automatic
  // pushes are held, as after a run that did not finish.
  pushed.clear();
  started.clear();
  critical.clear();
  QVERIFY(!pass.autoPushHeld());
  pass.Move(store.filePath("l.gpg"), store.filePath("team"), false);
  QVERIFY(store.mkpath(".git/rebase-merge"));
  pass.GitPushAutomatic();
  QTRY_VERIFY_WITH_TIMEOUT(!critical.isEmpty(), 30000);
  QCOMPARE(critical.first().at(0).toString(),
           QStringLiteral("Unfinished Git operation"));
  QTest::qWait(500);
  QCOMPARE(started.size(), 0);
  QCOMPARE(pushed.size(), 0);
  QVERIFY2(pass.autoPushHeld(), "the push waits for a clean run");
  QVERIFY(QDir(store.filePath(".git/rebase-merge")).removeRecursively());
}

void tst_integration::imitatePass_gitCopyAndShow() {
  const QString gitExe = QStandardPaths::findExecutable("git");
  if (gitExe.isEmpty())
    QSKIP("git not installed – skipping Git copy test");

  RestoreUseGit restoreUseGit;
  {
    AppSettings s = QtPassSettings::load();
    s.gitExecutable = gitExe;
    s.useGit = true;
    QtPassSettings::save(s);
  }

  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  QVERIFY2(initGitRepo(storeDir.path(), gitExe),
           "git repo setup should succeed");

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("original"), QStringLiteral("copyme\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  const QString src = QDir::cleanPath(storeDir.path() + "/original.gpg");
  const QString dst = QDir::cleanPath(storeDir.path() + "/copy.gpg");
  QVERIFY2(QFile::exists(src), "source must exist before copy");

  // Regression: git-mode Copy used a non-existent `git cp` subcommand, so the
  // destination was silently never created. It must now produce a decryptable
  // copy.
  pass.Copy(src, dst, false);
  QVERIFY2(QFile::exists(src), "source should still exist after copy");
  QVERIFY2(QFile::exists(dst), "destination should exist after git-mode copy");

  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("copy"));
  QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted after copy");
  QVERIFY2(showSpy[0][0].toString().contains("copyme"),
           "decrypted git-mode copy should contain the original content");

  // Confirm the copy was staged/committed into the repository, not just written
  // to disk: a git command run in the wrong directory would leave it
  // untracked.
  QProcess gitLs;
  gitLs.setWorkingDirectory(storeDir.path());
  gitLs.start(gitExe, {"ls-files"});
  QVERIFY2(gitLs.waitForFinished(), "git ls-files should complete");
  const QString tracked = QString::fromUtf8(gitLs.readAll());
  QVERIFY2(
      tracked.contains("copy.gpg"),
      qPrintable(
          QString("copied entry should be tracked in git: %1").arg(tracked)));

  // The Show above was queued after the copy's commit, so it has landed. Its
  // message names the entries relative to the store, not by local path.
  QProcess gitLog;
  gitLog.setWorkingDirectory(storeDir.path());
  gitLog.start(gitExe, {"log", "--format=%s"});
  QVERIFY2(gitLog.waitForFinished(), "git log should complete");
  const QString subjects = QString::fromUtf8(gitLog.readAll());
  QVERIFY2(subjects.contains(
               QStringLiteral("Copied from original to copy using QtPass.")),
           qPrintable(subjects));
}

void tst_integration::imitatePass_insertCompletesWhenGitNotConfigured() {
  // Regression test for #1682: with useGit enabled but no git executable
  // configured, an insert used to silently drop the empty git command. No
  // completion signal was ever emitted, leaving a stale transaction head that
  // swallowed every later operation. The insert must now complete cleanly via
  // gpg (git is simply skipped).
  RestoreUseGit restoreUseGit;
  {
    AppSettings s = QtPassSettings::load();
    s.useGit = true;
    s.gitExecutable = QString(); // unconfigured
    QtPassSettings::save(s);
  }

  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  const QString entryName = QStringLiteral("nogit");
  const QString entryContent = QStringLiteral("secret\nurl: example.com\n");

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(entryName, entryContent, false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));
  QCOMPARE(insertErrorSpy.count(), 0);

  const QString gpgFile = QDir::cleanPath(storeDir.path() + "/" + entryName +
                                          QStringLiteral(".gpg"));
  QVERIFY2(QFile::exists(gpgFile), "encrypted file should still be written");

  // The queue must not be wedged: a subsequent operation completes too.
  QSignalSpy insertSpy2(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy2(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("nogit2"), QStringLiteral("more\n"), false);
  QVERIFY2(waitForSignal(insertSpy2), gpgInsertErrorMsg(insertErrorSpy2));
  QCOMPARE(insertErrorSpy2.count(), 0);
}

void tst_integration::imitatePass_removeFallsBackWhenGitNotConfigured() {
  // Regression test for #1682: with useGit enabled but no git executable,
  // Remove() took the git branch, executeGit() bailed out silently and the
  // filesystem fallback never ran, so the .gpg file stayed on disk with no
  // error shown. It must now delete the file, say why git was skipped, and
  // leave the command queue usable.
  RestoreUseGit restoreUseGit;
  {
    AppSettings s = QtPassSettings::load();
    s.useGit = true;
    s.gitExecutable = QString(); // unconfigured
    QtPassSettings::save(s);
  }

  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("nogit-rm"), QStringLiteral("gone\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  const QString gpgFile = storeDir.path() + "/nogit-rm.gpg";
  QVERIFY2(QFile::exists(gpgFile), "file must exist before remove");

  QSignalSpy statusSpy(&pass, &Pass::statusMsg);
  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);
  pass.Remove(QStringLiteral("nogit-rm"), false);
  QVERIFY2(!QFile::exists(gpgFile),
           "file must be removed on the filesystem when git is unconfigured");
  QCOMPARE(errorSpy.count(), 0);
  QVERIFY2(!statusSpy.isEmpty(), "user must be told that git was skipped");
  QVERIFY2(statusSpy.first().at(0).toString().contains(
               QStringLiteral("not configured")),
           qPrintable(statusSpy.first().at(0).toString()));

  // The queue must not be wedged: a subsequent operation completes too.
  QSignalSpy insertSpy2(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy2(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("nogit-after"), QStringLiteral("more\n"), false);
  QVERIFY2(waitForSignal(insertSpy2), gpgInsertErrorMsg(insertErrorSpy2));
}

void tst_integration::imitatePass_moveFallsBackWhenGitNotConfigured() {
  // Same defect as Remove(): with useGit on and no git executable, Move()
  // reached executeMoveGit() and nothing was renamed. The filesystem rename
  // must run instead.
  RestoreUseGit restoreUseGit;
  {
    AppSettings s = QtPassSettings::load();
    s.useGit = true;
    s.gitExecutable = QString(); // unconfigured
    QtPassSettings::save(s);
  }

  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("nogit-src"), QStringLiteral("moveme\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  const QString src = storeDir.path() + "/nogit-src.gpg";
  const QString dst = storeDir.path() + "/nogit-dst.gpg";
  QVERIFY2(QFile::exists(src), "source .gpg must exist before move");

  QSignalSpy statusSpy(&pass, &Pass::statusMsg);
  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);
  pass.Move(src, dst, false);
  QVERIFY2(!QFile::exists(src), "source must be gone after move");
  QVERIFY2(QFile::exists(dst), "destination must exist after move");
  QCOMPARE(errorSpy.count(), 0);
  QVERIFY2(!statusSpy.isEmpty(), "user must be told that git was skipped");

  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("nogit-dst"));
  QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted after move");
  QVERIFY2(showSpy[0][0].toString().contains("moveme"),
           "decrypted moved entry should contain original content");
}

void tst_integration::imitatePass_usersDialogListsAndFilters() {
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  // UsersDialog loads the available GPG keys from the test keyring in its
  // constructor; with the generated test key present it must populate the
  // recipient list (no modal error dialog).
  const AppSettings s = QtPassSettings::load();
  UsersDialog dialog(&pass, s, storeDir.path());

  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY2(list != nullptr, "listWidget must exist");
  QVERIFY2(list->count() >= 1,
           "the generated test key should populate the recipient list");

  bool found = false;
  for (int i = 0; i < list->count(); ++i) {
    if (list->item(i)->text().contains(QStringLiteral("QtPass Integration"))) {
      found = true;
      break;
    }
  }
  QVERIFY2(found,
           "the test key's identity should appear in the recipient list");

  auto *filter = dialog.findChild<QLineEdit *>(QStringLiteral("lineEdit"));
  QVERIFY2(filter != nullptr, "lineEdit must exist");

  // populateList() rebuilds the list on each filter change, so a non-matching
  // filter empties it and a matching one brings the key back.
  filter->setText(QStringLiteral("zzz-definitely-no-such-recipient-zzz"));
  QCOMPARE(list->count(), 0);

  filter->setText(QStringLiteral("QtPass"));
  QVERIFY2(list->count() >= 1, "a matching filter should show the key again");
}

void tst_integration::imitatePass_multiRecipientReencryptChangesRecipients() {
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());

  // Store initially encrypts to BOTH keys.
  QtPassSettings::setPassStore(storeDir.path());
  {
    QFile gpgId(QDir::cleanPath(storeDir.path() + "/.gpg-id"));
    QVERIFY2(gpgId.open(QIODevice::WriteOnly | QIODevice::Text),
             "failed to open .gpg-id");
    const QByteArray payload =
        (m_keyFingerprint + "\n" + m_keyFingerprint2 + "\n").toUtf8();
    QVERIFY2(gpgId.write(payload) == payload.size(), "failed to write .gpg-id");
  }

  ImitatePass pass;
  {
    AppSettings s = QtPassSettings::load();
    pass.init(s);
    pass.updateEnv();
  }

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("multi"), QStringLiteral("secret\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  const QString gpgFile = QDir::cleanPath(storeDir.path() + "/multi.gpg");
  QVERIFY2(QFile::exists(gpgFile), "encrypted entry must exist after insert");

  const QString sub1 = encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint);
  const QString sub2 =
      encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint2);
  QVERIFY2(!sub1.isEmpty() && !sub2.isEmpty(),
           "both keys must have an encryption subkey");

  // Freshly inserted entry is encrypted to both recipients.
  const QStringList before = recipientKeyIds(m_gnupgHome.path(), gpgFile);
  QVERIFY2(before.contains(sub1) && before.contains(sub2),
           qPrintable(QStringLiteral("entry should target both keys, got: %1")
                          .arg(before.join(','))));

  // Drop the second recipient from the .gpg-id and re-encrypt the store. This
  // exercises reencryptPath()/verifyGpgIdForDir(): the entry must be rewritten
  // to the CURRENT recipient set, not the one it was originally encrypted to.
  {
    QFile gpgId(QDir::cleanPath(storeDir.path() + "/.gpg-id"));
    QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray payload = (m_keyFingerprint + "\n").toUtf8();
    QVERIFY(gpgId.write(payload) == payload.size());
  }

  // reencryptPath runs on a worker thread; wait for it to finish.
  {
    QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
    pass.reencryptPath(storeDir.path());
    QVERIFY2(waitForSignal(endSpy, 60000), "endReencryptPath not emitted");
  }

  const QStringList after = recipientKeyIds(m_gnupgHome.path(), gpgFile);
  QVERIFY2(after.contains(sub1),
           qPrintable(QStringLiteral("re-encrypted entry must still target "
                                     "the kept key, got: %1")
                          .arg(after.join(','))));
  QVERIFY2(!after.contains(sub2),
           qPrintable(QStringLiteral("re-encrypted entry must drop the removed "
                                     "recipient, got: %1")
                          .arg(after.join(','))));
}

// getKeysFromFile() asks gpg for an entry's recipients. Run with --list-only
// but no command, gpg guesses one and saves OpenPGP data that is not encrypted
// under a name in the store (through a link, over a file outside it). With
// --decrypt --max-output such data goes to a capped stdout instead and no file
// is written.
void tst_integration::imitatePass_reencryptKeyListingWritesNoFile() {
#ifdef Q_OS_WIN
  QSKIP("POSIX symlink semantics; Windows junction handling differs");
#else
  QTemporaryDir storeDir;
  QTemporaryDir outsideDir;
  QVERIFY(storeDir.isValid());
  QVERIFY(outsideDir.isValid());
  QtPassSettings::setPassStore(storeDir.path());
  const QString subkey =
      encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint);
  QVERIFY2(!subkey.isEmpty(), "the test key must have an encryption subkey");
  {
    QFile f(QDir::cleanPath(storeDir.path() + "/.gpg-id"));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write((subkey + "\n").toUtf8());
  }

  ImitatePass pass;
  {
    AppSettings s = QtPassSettings::load();
    pass.init(s);
    pass.updateEnv();
  }

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("keep"), QStringLiteral("secret\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));
  const QString keepFile = QDir::cleanPath(storeDir.path() + "/keep.gpg");
  const QByteArray keepBefore = readFileBytes(keepFile);
  QVERIFY(!keepBefore.isEmpty());

  // OpenPGP data that is not encrypted: a bare literal data packet.
  const QString plain = QDir::cleanPath(outsideDir.path() + "/plain");
  {
    QFile f(plain);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("not a secret\n");
  }
  const QString literal = QDir::cleanPath(storeDir.path() + "/odd.gpg");
  QCOMPARE(runGpg(m_gnupgHome.path(),
                  {"--batch", "--yes", "--store", "--output", literal, plain}),
           0);
  // gpg writes "odd" by way of "odd.part": a file outside the store that a
  // link under that name points at must keep its content.
  const QString outside = QDir::cleanPath(outsideDir.path() + "/outside");
  {
    QFile f(outside);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("precious\n");
  }
  QVERIFY(QFile::link(outside, QDir::cleanPath(storeDir.path() + "/odd.part")));

  {
    QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
    pass.reencryptPath(storeDir.path());
    QVERIFY2(waitForSignal(endSpy, 60000), "endReencryptPath not emitted");
  }

  const QFileInfo written(QDir::cleanPath(storeDir.path() + "/odd"));
  QVERIFY2(!written.exists() && !written.isSymLink(),
           "gpg saved the unencrypted data next to the entry");
  QCOMPARE(readFileBytes(outside), QByteArray("precious\n"));
  QCOMPARE(readFileBytes(keepFile), keepBefore);
#endif
}

/**
 * @brief Append @p line to the shared test gpg.conf for the lifetime of the
 * returned guard; the original file is restored when the guard goes out of
 * scope, so later tests see the plain configuration again.
 */
class GpgConfLine {
public:
  GpgConfLine(const QString &gnupgHome, const QString &line)
      : m_path(QDir::cleanPath(gnupgHome + "/gpg.conf")) {
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
      return;
    m_original = f.readAll();
    f.close();
    // Atomic replacement: a short write must not leave gpg with a truncated
    // configuration, in this test or the ones after it.
    QSaveFile replacement(m_path);
    if (!replacement.open(QIODevice::WriteOnly))
      return;
    const QByteArray payload = m_original + line.toUtf8() + "\n";
    m_ok = replacement.write(payload) == payload.size() && replacement.commit();
  }
  ~GpgConfLine() {
    QSaveFile replacement(m_path);
    if (replacement.open(QIODevice::WriteOnly) &&
        replacement.write(m_original) == m_original.size()) {
      replacement.commit();
    }
  }
  GpgConfLine(const GpgConfLine &) = delete;
  auto operator=(const GpgConfLine &) -> GpgConfLine & = delete;
  auto ok() const -> bool { return m_ok; }

private:
  QString m_path;
  QByteArray m_original;
  bool m_ok = false;
};

/**
 * @brief A user's gpg.conf `encrypt-to` must not add a hidden recipient.
 *
 * With `encrypt-to <second key>` in gpg.conf, Insert() must still produce a
 * file encrypted only to the key listed in .gpg-id (negative), and that file
 * must still decrypt to the inserted content (positive). Without
 * --no-encrypt-to on the encrypt argv gpg would silently add the second key.
 */
void tst_integration::imitatePass_insertIgnoresGpgConfEncryptTo() {
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  GpgConfLine encryptTo(m_gnupgHome.path(),
                        QStringLiteral("encrypt-to ") + m_keyFingerprint2);
  QVERIFY2(encryptTo.ok(), "failed to add encrypt-to to gpg.conf");

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("hidden-recipient"), QStringLiteral("secret\n"),
              false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  const QString gpgFile =
      QDir::cleanPath(storeDir.path() + "/hidden-recipient.gpg");
  QVERIFY2(QFile::exists(gpgFile), "encrypted entry must exist after insert");

  const QString sub1 = encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint);
  const QString sub2 =
      encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint2);
  QVERIFY2(!sub1.isEmpty() && !sub2.isEmpty(),
           "both keys must have an encryption subkey");

  const QStringList ids = recipientKeyIds(m_gnupgHome.path(), gpgFile);
  QVERIFY2(ids.contains(sub1),
           qPrintable(QStringLiteral("entry must target the .gpg-id key, "
                                     "got: %1")
                          .arg(ids.join(','))));
  QVERIFY2(!ids.contains(sub2),
           qPrintable(QStringLiteral("gpg.conf encrypt-to must not add a "
                                     "recipient, got: %1")
                          .arg(ids.join(','))));

  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("hidden-recipient"));
  QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted");
  QVERIFY2(showSpy[0][0].toString().contains(QStringLiteral("secret")),
           "entry must still decrypt to the inserted content");
}

/**
 * @brief Real `pass` exits 0 when gpg cannot re-encrypt an entry for the new
 *        recipients (here, one has no key in the keyring): QtPass reports a
 *        failure instead of finishedInit, which pushed the store as it was,
 *        also with gpg's messages in another language. A list it can encrypt
 *        to still finishes.
 */
void tst_integration::realPass_initThatCannotReencryptIsReported() {
  const QString passExe = findPass();
  if (passExe.isEmpty())
    QSKIP("pass not installed – skipping RealPass integration test");
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QtPassSettings::setPassStore(storeDir.path());
  {
    AppSettings s = QtPassSettings::load();
    s.passExecutable = passExe;
    s.gpgExecutable = m_gpgExe;
    QtPassSettings::save(s);
  }
  QtPassSettings::setUsePass(true);
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("GNUPGHOME", m_gnupgHome.path());
  env.insert("PASSWORD_STORE_DIR", storeDir.path());
  {
    QProcess init;
    init.setProcessEnvironment(env);
    init.start(passExe, {"init", m_keyFingerprint, m_keyFingerprint2});
    QVERIFY(init.waitForFinished(15000) && init.exitCode() == 0);
    QProcess insert;
    insert.setProcessEnvironment(env);
    insert.start(passExe, {"insert", "-m", "entry"});
    QVERIFY(insert.waitForStarted());
    insert.write("secret\n");
    insert.closeWriteChannel();
    QVERIFY(insert.waitForFinished(15000) && insert.exitCode() == 0);
  }
  const QString gpgFile = QDir(storeDir.path()).filePath("entry.gpg");
  const QString sub2 =
      encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint2);
  QVERIFY(recipientKeyIds(m_gnupgHome.path(), gpgFile).contains(sub2));

  RealPass pass;
  setupPass(pass);
  UserInfo kept;
  kept.key_id = m_keyFingerprint;
  kept.enabled = true;
  UserInfo absent;
  absent.key_id = QStringLiteral("0123456789ABCDEF0123456789ABCDEF01234567");
  absent.enabled = true;
  QSignalSpy inited(&pass, &Pass::finishedInit);
  QSignalSpy errors(&pass, &Pass::processErrorExit);
  QSignalSpy critical(&pass, &Pass::critical);
  pass.Init(storeDir.path(), {kept, absent});
  QVERIFY2(waitForSignal(errors, 30000), "the failure must be reported");
  QCOMPARE(inited.size(), 0);
  QCOMPARE(critical.size(), 1);
  // pass left the entry as it was: the removed key can still read it.
  QVERIFY(recipientKeyIds(m_gnupgHome.path(), gpgFile).contains(sub2));

  pass.Init(storeDir.path(), {kept});
  QVERIFY2(waitForSignal(inited, 30000), "a usable list still finishes");
  QCOMPARE(critical.size(), 1);
  QVERIFY(!recipientKeyIds(m_gnupgHome.path(), gpgFile).contains(sub2));

  // With a signing key pass verifies each .gpg-id with its own
  // --status-fd=1, after the --status-fd 2 QtPass adds: that must still
  // work, or every init would fail on a signed store.
  {
    AppSettings s = QtPassSettings::load();
    s.passSigningKey = m_keyFingerprint;
    QtPassSettings::save(s);
  }
  const auto unsign = qScopeGuard([] {
    AppSettings s = QtPassSettings::load();
    s.passSigningKey.clear();
    QtPassSettings::save(s);
  });
  RealPass signing;
  setupPass(signing);
  QSignalSpy signedInit(&signing, &Pass::finishedInit);
  QSignalSpy signedErrors(&signing, &Pass::processErrorExit);
  UserInfo both = kept;
  UserInfo second;
  second.key_id = m_keyFingerprint2;
  second.enabled = true;
  signing.Init(storeDir.path(), {both, second});
  QVERIFY2(waitForSignal(signedInit, 30000),
           qPrintable(signedErrors.isEmpty()
                          ? QStringLiteral("no finishedInit")
                          : signedErrors.first().at(1).toString()));
  QVERIFY(QFile::exists(QDir(storeDir.path()).filePath(".gpg-id.sig")));
  QVERIFY(recipientKeyIds(m_gnupgHome.path(), gpgFile).contains(sub2));
}

/**
 * @brief The same guarantee for the re-encryption path: rewriting a file for
 * the current .gpg-id must not pick up gpg.conf's `encrypt-to` recipient.
 */
void tst_integration::imitatePass_reencryptIgnoresGpgConfEncryptTo() {
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QtPassSettings::setPassStore(storeDir.path());

  // Start with both keys so the entry needs rewriting once key2 is dropped.
  auto writeGpgId = [&storeDir](const QStringList &fprs) -> bool {
    QFile f(QDir::cleanPath(storeDir.path() + "/.gpg-id"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
      return false;
    const QByteArray payload = (fprs.join('\n') + "\n").toUtf8();
    return f.write(payload) == payload.size();
  };
  QVERIFY(writeGpgId({m_keyFingerprint, m_keyFingerprint2}));

  ImitatePass pass;
  {
    AppSettings s = QtPassSettings::load();
    pass.init(s);
    pass.updateEnv();
  }

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("shrink"), QStringLiteral("secret\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));
  const QString gpgFile = QDir::cleanPath(storeDir.path() + "/shrink.gpg");
  QVERIFY(QFile::exists(gpgFile));

  const QString sub1 = encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint);
  const QString sub2 =
      encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint2);
  QVERIFY2(!sub1.isEmpty() && !sub2.isEmpty(),
           "both keys must have an encryption subkey");

  // Drop key2 from .gpg-id, but have gpg.conf try to sneak it back in.
  QVERIFY(writeGpgId({m_keyFingerprint}));
  GpgConfLine encryptTo(m_gnupgHome.path(),
                        QStringLiteral("encrypt-to ") + m_keyFingerprint2);
  QVERIFY2(encryptTo.ok(), "failed to add encrypt-to to gpg.conf");

  {
    QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
    pass.reencryptPath(storeDir.path());
    QVERIFY2(waitForSignal(endSpy, 60000), "endReencryptPath not emitted");
  }

  const QStringList after = recipientKeyIds(m_gnupgHome.path(), gpgFile);
  QVERIFY2(after.contains(sub1),
           qPrintable(QStringLiteral("re-encrypted entry must target the kept "
                                     "key, got: %1")
                          .arg(after.join(','))));
  QVERIFY2(!after.contains(sub2),
           qPrintable(QStringLiteral("gpg.conf encrypt-to must not re-add the "
                                     "removed recipient, got: %1")
                          .arg(after.join(','))));
}

void tst_integration::imitatePass_reencryptIncludesDotNamedEntries() {
  // An entry whose name starts with a dot (.env) is re-encrypted with the
  // rest, as `pass init` does; it was left encrypted to the previous
  // recipients. A hidden folder stays closed.
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QtPassSettings::setPassStore(storeDir.path());
  const QDir store(storeDir.path());
  const auto writeGpgId = [&](const QString &fpr) {
    QFile f(store.filePath(".gpg-id"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
      return false;
    const QByteArray payload = (fpr + "\n").toUtf8();
    return f.write(payload) == payload.size();
  };
  QVERIFY(writeGpgId(m_keyFingerprint));
  QVERIFY(store.mkpath("team"));
  QVERIFY(store.mkpath(".stash"));

  ImitatePass pass;
  {
    AppSettings s = QtPassSettings::load();
    pass.init(s);
    pass.updateEnv();
  }
  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);
  for (const QString &name : {QStringLiteral("plain"), QStringLiteral(".env"),
                              QStringLiteral("team/.secret")}) {
    QSignalSpy spy(&pass, &Pass::finishedInsert);
    pass.Insert(name, QStringLiteral("r\n"), false);
    QVERIFY2(waitForSignal(spy), gpgInsertErrorMsg(errorSpy));
  }
  QVERIFY(QFile::copy(store.filePath("plain.gpg"),
                      store.filePath(".stash/kept.gpg")));

  QVERIFY(writeGpgId(m_keyFingerprint2));
  {
    QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
    pass.reencryptPath(storeDir.path());
    QVERIFY2(waitForSignal(endSpy, 60000), "endReencryptPath not emitted");
  }

  const QString sub1 = encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint);
  const QString sub2 =
      encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint2);
  QVERIFY(!sub1.isEmpty() && !sub2.isEmpty());
  for (const QString &name :
       {QStringLiteral("plain.gpg"), QStringLiteral(".env.gpg"),
        QStringLiteral("team/.secret.gpg")}) {
    const QStringList ids =
        recipientKeyIds(m_gnupgHome.path(), store.filePath(name));
    QVERIFY2(ids.contains(sub2) && !ids.contains(sub1),
             qPrintable(name + ": " + ids.join(',')));
  }
  const QStringList stashed =
      recipientKeyIds(m_gnupgHome.path(), store.filePath(".stash/kept.gpg"));
  QVERIFY2(stashed.contains(sub1) && !stashed.contains(sub2),
           qPrintable(stashed.join(',')));
}

void tst_integration::imitatePass_reencryptPreservesPerFolderRecipients() {
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QtPassSettings::setPassStore(storeDir.path());

  // Root .gpg-id -> key1; a subfolder overrides it with key2.
  auto writeGpgId = [](const QString &dir, const QString &fpr) -> bool {
    QDir().mkpath(dir);
    QFile f(QDir::cleanPath(dir + "/.gpg-id"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
      return false;
    const QByteArray payload = (fpr + "\n").toUtf8();
    return f.write(payload) == payload.size();
  };
  QVERIFY(writeGpgId(storeDir.path(), m_keyFingerprint));
  const QString subDir = QDir::cleanPath(storeDir.path() + "/team");
  QVERIFY(writeGpgId(subDir, m_keyFingerprint2));

  ImitatePass pass;
  {
    AppSettings s = QtPassSettings::load();
    pass.init(s);
    pass.updateEnv();
  }

  // Insert one entry per folder; each is encrypted to its own .gpg-id.
  QSignalSpy errorSpy(&pass, &Pass::processErrorExit);
  {
    QSignalSpy spy(&pass, &Pass::finishedInsert);
    pass.Insert(QStringLiteral("rootentry"), QStringLiteral("r\n"), false);
    QVERIFY2(waitForSignal(spy), gpgInsertErrorMsg(errorSpy));
  }
  {
    QSignalSpy spy(&pass, &Pass::finishedInsert);
    pass.Insert(QStringLiteral("team/teamentry"), QStringLiteral("t\n"), false);
    QVERIFY2(waitForSignal(spy), gpgInsertErrorMsg(errorSpy));
  }

  // Re-encrypt the whole tree. reencryptPath walks both folders; each file must
  // end up encrypted to ITS folder's recipients — a stale cached recipient list
  // would cross-encrypt one folder's entry to the other folder's key.
  // reencryptPath runs on a worker thread; wait for it to finish.
  {
    QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
    pass.reencryptPath(storeDir.path());
    QVERIFY2(waitForSignal(endSpy, 60000), "endReencryptPath not emitted");
  }

  const QString sub1 = encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint);
  const QString sub2 =
      encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint2);
  QVERIFY(!sub1.isEmpty() && !sub2.isEmpty());

  const QStringList rootIds = recipientKeyIds(
      m_gnupgHome.path(), QDir::cleanPath(storeDir.path() + "/rootentry.gpg"));
  const QStringList teamIds = recipientKeyIds(
      m_gnupgHome.path(), QDir::cleanPath(subDir + "/teamentry.gpg"));

  QVERIFY2(
      rootIds.contains(sub1) && !rootIds.contains(sub2),
      qPrintable(QStringLiteral("root entry must target key1 only, got: %1")
                     .arg(rootIds.join(','))));
  QVERIFY2(
      teamIds.contains(sub2) && !teamIds.contains(sub1),
      qPrintable(QStringLiteral("team entry must target key2 only, got: %1")
                     .arg(teamIds.join(','))));
}

void tst_integration::imitatePass_signedGpgIdInitAndReencrypt() {
  // The `.gpg-id` signing feature has never run in CI because initTestCase()
  // clears passSigningKey. Exercise the happy path with a real signed store:
  // ImitatePass::Init writes and signs the recipients file, Insert() verifies
  // it, and re-encryption accepts the valid signature.
  RestorePassSigningKey restoreSigning;
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QtPassSettings::setPassStore(storeDir.path());

  ImitatePass pass;
  {
    AppSettings s = QtPassSettings::load();
    s.passSigningKey = m_keyFingerprint;
    s.useGit = false;
    pass.init(s);
    pass.updateEnv();
  }

  QSignalSpy criticalSpy(&pass, &Pass::critical);
  {
    // Init() writes and signs .gpg-id, then kicks off a re-encryption of the
    // (empty) store on the worker thread.
    QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
    pass.Init(storeDir.path() + QLatin1Char('/'),
              {enabledUser(m_keyFingerprint)});
    QVERIFY2(waitForSignal(endSpy, 60000), "Init re-encryption never finished");
  }
  QVERIFY2(QFile::exists(QDir::cleanPath(storeDir.path() + "/.gpg-id")),
           "Init must write .gpg-id");
  QVERIFY2(QFile::exists(QDir::cleanPath(storeDir.path() + "/.gpg-id.sig")),
           "Init must sign .gpg-id when a signing key is configured");
  QVERIFY2(criticalSpy.isEmpty(), "no critical expected on the signed path");

  // Insert verifies the signature before encrypting.
  {
    QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
    QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
    pass.Insert(QStringLiteral("signed"), QStringLiteral("secret\n"), false);
    QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));
  }
  QVERIFY2(criticalSpy.isEmpty(), "insert must not reject a valid signature");

  // Re-encrypt the entry; the still-valid signature must be accepted and the
  // content preserved.
  {
    QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
    pass.reencryptPath(storeDir.path());
    QVERIFY2(waitForSignal(endSpy, 60000), "re-encryption never finished");
  }
  QVERIFY2(criticalSpy.isEmpty(), "re-encryption must accept the signature");

  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("signed"));
  QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted");
  QVERIFY2(showSpy[0][0].toString().contains("secret"),
           "content must survive the re-encryption");
}

void tst_integration::imitatePass_tamperedGpgIdSignatureAbortsReencrypt() {
  // Security regression: a store legitimately signed by key1 whose .gpg-id is
  // later rewritten to key2 WITHOUT re-signing must abort the re-encryption;
  // otherwise a tampered recipients file silently re-encrypts the whole store
  // to an attacker-chosen key.
  RestorePassSigningKey restoreSigning;
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QtPassSettings::setPassStore(storeDir.path());

  ImitatePass pass;
  {
    AppSettings s = QtPassSettings::load();
    s.passSigningKey = m_keyFingerprint;
    s.useGit = false;
    pass.init(s);
    pass.updateEnv();
  }

  // Legitimate setup: signed .gpg-id for key1 and one encrypted entry.
  {
    QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
    QSignalSpy criticalSpy(&pass, &Pass::critical);
    pass.Init(storeDir.path() + QLatin1Char('/'),
              {enabledUser(m_keyFingerprint)});
    QVERIFY2(waitForSignal(endSpy, 60000), "Init re-encryption never finished");
    QVERIFY2(criticalSpy.isEmpty(), "clean setup must not complain");
  }
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("victim"), QStringLiteral("topsecret\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  const QString gpgFile = QDir::cleanPath(storeDir.path() + "/victim.gpg");
  const QString sub1 = encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint);
  const QString sub2 =
      encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint2);
  QVERIFY(!sub1.isEmpty() && !sub2.isEmpty());
  QVERIFY2(recipientKeyIds(m_gnupgHome.path(), gpgFile).contains(sub1),
           "entry must be encrypted to key1 before the attack");

  // Attacker swaps the recipients to their own key, leaving the stale
  // signature in place.
  {
    QFile gpgId(QDir::cleanPath(storeDir.path() + "/.gpg-id"));
    QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray payload = (m_keyFingerprint2 + "\n").toUtf8();
    QVERIFY(gpgId.write(payload) == payload.size());
  }

  QSignalSpy criticalSpy(&pass, &Pass::critical);
  {
    QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
    pass.reencryptPath(storeDir.path());
    QVERIFY2(waitForSignal(criticalSpy, 60000),
             "invalid signature must be surfaced");
    QVERIFY2(waitForSignal(endSpy, 60000), "run must still clean up");
  }
  // Any gpg hiccup emits critical and would also leave the entry on key1, so
  // pin the failure to the signature check itself.
  QCOMPARE(criticalSpy.count(), 1);
  QCOMPARE(criticalSpy.at(0).at(0).toString(),
           QStringLiteral("Check .gpg-id file signature!"));
  const QString sigMsg = criticalSpy.at(0).at(1).toString();
  QVERIFY2(sigMsg.startsWith(QStringLiteral("Signature for ")) &&
               sigMsg.endsWith(QStringLiteral("/.gpg-id is invalid.")),
           qPrintable(
               QStringLiteral("unexpected critical message: %1").arg(sigMsg)));

  const QStringList ids = recipientKeyIds(m_gnupgHome.path(), gpgFile);
  QVERIFY2(ids.contains(sub1),
           qPrintable(QStringLiteral("victim entry must still target key1, "
                                     "got: %1")
                          .arg(ids.join(','))));
  QVERIFY2(!ids.contains(sub2),
           qPrintable(QStringLiteral("victim entry must NOT target the "
                                     "attacker key2, got: %1")
                          .arg(ids.join(','))));
}

void tst_integration::imitatePass_reencryptMixedFailuresReportedOnce() {
  // Failure-path coverage with real gpg: undecryptable files among valid ones
  // must not abort the run — the valid entries are still re-encrypted and ALL
  // failures are aggregated into ONE critical dialog.
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QtPassSettings::setPassStore(storeDir.path());
  {
    QFile gpgId(QDir::cleanPath(storeDir.path() + "/.gpg-id"));
    QVERIFY2(gpgId.open(QIODevice::WriteOnly | QIODevice::Text),
             "failed to open .gpg-id");
    const QByteArray payload = (m_keyFingerprint + "\n").toUtf8();
    QVERIFY2(gpgId.write(payload) == payload.size(), "failed to write .gpg-id");
  }

  ImitatePass pass;
  {
    AppSettings s = QtPassSettings::load();
    pass.init(s);
    pass.updateEnv();
  }

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("okentry"), QStringLiteral("secret\n"), false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  // Two undecryptable files, the second in a subfolder whose .gpg-id is
  // inherited from the store root.
  {
    QFile bogus(QDir::cleanPath(storeDir.path() + "/broken1.gpg"));
    QVERIFY(bogus.open(QIODevice::WriteOnly | QIODevice::Text));
    bogus.write("not a gpg ciphertext\n");
  }
  {
    QFile bogus(QDir::cleanPath(storeDir.path() + "/sub/broken2.gpg"));
    QVERIFY(QDir().mkpath(storeDir.path() + "/sub"));
    QVERIFY(bogus.open(QIODevice::WriteOnly | QIODevice::Text));
    bogus.write("also not encrypted\n");
  }

  QSignalSpy criticalSpy(&pass, &Pass::critical);
  {
    QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
    pass.reencryptPath(storeDir.path());
    QVERIFY2(waitForSignal(criticalSpy, 60000), "failures must be surfaced");
    QVERIFY2(waitForSignal(endSpy, 60000), "run must still finish");
  }

  QVERIFY2(criticalSpy.count() == 1,
           qPrintable(QStringLiteral("expected exactly 1 aggregated critical, "
                                     "got %1")
                          .arg(criticalSpy.count())));
  const QString msg = criticalSpy.at(0).at(1).toString();
  QVERIFY2(
      msg.contains(QStringLiteral("broken1.gpg")),
      qPrintable(QStringLiteral("dialog must name broken1.gpg: %1").arg(msg)));
  QVERIFY2(
      msg.contains(QStringLiteral("broken2.gpg")),
      qPrintable(QStringLiteral("dialog must name broken2.gpg: %1").arg(msg)));

  const QString sub1 = encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint);
  QVERIFY(!sub1.isEmpty());
  const QStringList okIds = recipientKeyIds(
      m_gnupgHome.path(), QDir::cleanPath(storeDir.path() + "/okentry.gpg"));
  QVERIFY2(okIds.contains(sub1),
           qPrintable(QStringLiteral("valid entry must still be re-encrypted "
                                     "to key1, got: %1")
                          .arg(okIds.join(','))));
}

void tst_integration::mainWindow_selectingEntryShowsDecryptedContent() {
  // Full GUI end-to-end: build a store with one real entry, construct the
  // MainWindow over it, click the entry in the tree, and verify the panel shows
  // the decrypted content — driving the real UI through real gpg.
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);
  {
    QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
    QSignalSpy errSpy(&pass, &Pass::processErrorExit);
    pass.Insert(QStringLiteral("guitest"),
                QStringLiteral("topsecret\nurl: gui.example.com\n"), false);
    QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(errSpy));
  }

  // Restore every globally-mutated setting on any exit path (including early
  // QVERIFY returns): this test changes passStore/usePass/gpgExecutable/
  // hideContent, all of which live in AppSettings.
  const AppSettings savedSettings = QtPassSettings::load();
  struct SettingsRestorer {
    AppSettings s;
    ~SettingsRestorer() {
      QtPassSettings::save(s);
      PassBackendFactory::invalidate();
    }
  } restorer{savedSettings};

  // Point QtPassSettings at the store in gpg mode and pre-set the gpg path so
  // the constructor does not open the blocking config dialog. hideContent (not
  // hidePassword) is what passShowHandler() checks before rendering the panel.
  QtPassSettings::setPassStore(storeDir.path());
  QtPassSettings::setUsePass(false);
  {
    AppSettings s = QtPassSettings::load();
    s.gpgExecutable = m_gpgExe;
    s.hideContent = false;
    s.hidePassword = false;
    QtPassSettings::save(s);
  }
  PassBackendFactory::invalidate();

  MainWindow w;
  w.show();
  QVERIFY2(QTest::qWaitForWindowExposed(&w),
           "MainWindow should become exposed");

  auto *tree = w.findChild<QTreeView *>(QStringLiteral("treeView"));
  QVERIFY2(tree != nullptr, "treeView must exist");

  // QFileSystemModel loads asynchronously; wait until our entry appears, then
  // capture its index.
  QModelIndex entryIdx;
  auto findEntry = [&]() -> bool {
    QAbstractItemModel *m = tree->model();
    const QModelIndex root = tree->rootIndex();
    for (int r = 0; r < m->rowCount(root); ++r) {
      const QModelIndex idx = m->index(r, 0, root);
      if (m->data(idx).toString() == QStringLiteral("guitest")) {
        entryIdx = idx;
        return true;
      }
    }
    return false;
  };
  QTRY_VERIFY_WITH_TIMEOUT(findEntry(), 10000);
  QVERIFY(entryIdx.isValid());

  // Click the entry in the tree.
  tree->scrollTo(entryIdx);
  const QRect rect = tree->visualRect(entryIdx);
  QVERIFY2(rect.isValid() && !rect.isEmpty(), "entry must have a visible rect");
  QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                    rect.center());

  // The decrypted "url" field should render in the display panel (a
  // QTextBrowser); the show is asynchronous so poll for it.
  auto panelHasUrl = [&]() -> bool {
    const auto browsers = w.findChildren<QTextBrowser *>();
    for (auto *b : browsers) {
      if (b->toPlainText().contains(QStringLiteral("gui.example.com"))) {
        return true;
      }
    }
    return false;
  };
  QTRY_VERIFY_WITH_TIMEOUT(panelHasUrl(), 15000);
}

void tst_integration::mainWindow_editEntryThroughModalDialogUpdatesContent() {
  // GUI end-to-end for the modal edit flow: double-click an entry, let the
  // modal PasswordDialog load its decrypted content, change the password,
  // accept, and confirm the store entry was rewritten.
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);
  {
    QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
    QSignalSpy errSpy(&pass, &Pass::processErrorExit);
    pass.Insert(QStringLiteral("editme"),
                QStringLiteral("oldpass\nurl: keep.example.com\n"), false);
    QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(errSpy));
  }

  const AppSettings savedSettings = QtPassSettings::load();
  struct SettingsRestorer {
    AppSettings s;
    ~SettingsRestorer() {
      QtPassSettings::save(s);
      PassBackendFactory::invalidate();
    }
  } restorer{savedSettings};

  QtPassSettings::setPassStore(storeDir.path());
  QtPassSettings::setUsePass(false);
  {
    AppSettings s = QtPassSettings::load();
    s.gpgExecutable = m_gpgExe;
    s.hideContent = false;
    s.hidePassword = false;
    QtPassSettings::save(s);
  }
  PassBackendFactory::invalidate();

  MainWindow w;
  w.show();
  QVERIFY2(QTest::qWaitForWindowExposed(&w), "MainWindow should be exposed");

  auto *tree = w.findChild<QTreeView *>(QStringLiteral("treeView"));
  QVERIFY2(tree != nullptr, "treeView must exist");

  QModelIndex entryIdx;
  auto findEntry = [&]() -> bool {
    QAbstractItemModel *m = tree->model();
    const QModelIndex root = tree->rootIndex();
    for (int r = 0; r < m->rowCount(root); ++r) {
      const QModelIndex idx = m->index(r, 0, root);
      if (m->data(idx).toString() == QStringLiteral("editme")) {
        entryIdx = idx;
        return true;
      }
    }
    return false;
  };
  QTRY_VERIFY_WITH_TIMEOUT(findEntry(), 10000);
  QVERIFY(entryIdx.isValid());

  Pass *backend = QtPassSettings::getPass();
  QSignalSpy editInsertSpy(backend, &Pass::finishedInsert);
  QSignalSpy editErrSpy(backend, &Pass::processErrorExit);

  // Drive the modal PasswordDialog once its decrypted content has loaded: set a
  // new password and accept. The attempts guard rejects the dialog so the test
  // can never hang if the content never arrives.
  bool driven = false;
  int attempts = 0;
  QTimer poker;
  poker.setInterval(50);
  QObject::connect(&poker, &QTimer::timeout, [&]() {
    auto *dlg =
        qobject_cast<PasswordDialog *>(QApplication::activeModalWidget());
    if (dlg == nullptr)
      return;
    auto *pw = dlg->findChild<QLineEdit *>(QStringLiteral("lineEditPassword"));
    if (pw == nullptr)
      return;
    if (pw->text().isEmpty()) { // wait for the asynchronous Show to populate it
      if (++attempts > 200) {
        poker.stop();
        dlg->reject();
      }
      return;
    }
    pw->setText(QStringLiteral("newpass"));
    auto *box = dlg->findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
    if (box != nullptr) {
      box->button(QDialogButtonBox::Ok)->click();
    } else {
      dlg->accept();
    }
    driven = true;
    poker.stop();
  });
  poker.start();

  // on_treeView_doubleClicked() checks the tree's *current* index before
  // editing, so select the entry first.
  tree->setCurrentIndex(entryIdx);

  // Double-click -> editPassword() -> modal exec(); the poker drives it. Direct
  // connection so this blocks in the modal loop until the dialog closes.
  QVERIFY(QMetaObject::invokeMethod(&w, "on_treeView_doubleClicked",
                                    Qt::DirectConnection,
                                    Q_ARG(QModelIndex, entryIdx)));
  QVERIFY2(driven,
           "the modal edit dialog should have been driven and accepted");

  // The accept triggers an overwrite Insert (asynchronous).
  QVERIFY2(waitForSignal(editInsertSpy), gpgInsertErrorMsg(editErrSpy));

  // Re-show and confirm the change stuck.
  QSignalSpy showSpy(backend, &Pass::finishedShow);
  backend->Show(QStringLiteral("editme"));
  QVERIFY2(waitForSignal(showSpy), "finishedShow after edit");
  const QString shown = showSpy[0][0].toString();
  QVERIFY2(shown.contains("newpass"),
           "edited entry must contain the new password");
  QVERIFY2(!shown.contains("oldpass"),
           "edited entry must not contain the old password");
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
  {
    AppSettings s = QtPassSettings::load();
    s.passExecutable = passExe;
    s.gpgExecutable = m_gpgExe;
    QtPassSettings::save(s);
  }
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

/**
 * @brief pass exits 0 whatever its `git add` did: an entry a pushed
 *        .gitignore matches is saved but never committed, so it is not
 *        pushed and a pull can overwrite it. QtPass says so; an entry Git
 *        records gets no such message.
 */
void tst_integration::realPass_unrecordedEntryIsReported() {
  const QString passExe = findPass();
  const QString gitExe = QStandardPaths::findExecutable("git");
  if (passExe.isEmpty() || gitExe.isEmpty())
    QSKIP("pass or git not installed");
  RestoreUseGit restoreUseGit;
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  const QString setUp = initRealPassGitStore(storeDir.path(), passExe, gitExe);
  QVERIFY2(setUp.isEmpty(), qPrintable(setUp));
  QFile ignore(QDir(storeDir.path()).filePath(".gitignore"));
  QVERIFY(ignore.open(QIODevice::WriteOnly));
  ignore.write("ignored.gpg\n");
  ignore.close();
  QVERIFY(runGit(storeDir.path(), gitExe, {"add", ".gitignore"}));
  QVERIFY(runGit(storeDir.path(), gitExe, {"commit", "-q", "-m", "ignore"}));

  RealPass pass;
  setupPass(pass);
  QSignalSpy inserted(&pass, &Pass::finishedInsert);
  QSignalSpy errors(&pass, &Pass::processErrorExit);
  QSignalSpy critical(&pass, &Pass::critical);
  pass.Insert(QStringLiteral("recorded"), QStringLiteral("one\n"), false);
  QVERIFY2(waitForSignal(inserted, 20000), gpgInsertErrorMsg(errors));
  QCOMPARE(critical.size(), 0);

  inserted.clear();
  pass.Insert(QStringLiteral("ignored"), QStringLiteral("two\n"), false);
  QVERIFY2(waitForSignal(inserted, 20000), gpgInsertErrorMsg(errors));
  QCOMPARE(critical.size(), 1);
  QVERIFY2(critical.first().at(0).toString().contains("not committed"),
           qPrintable(critical.first().at(0).toString()));
  QVERIFY2(critical.first().at(1).toString().contains("ignored.gpg"),
           qPrintable(critical.first().at(1).toString()));
  QtPassSettings::setUsePass(false);
}

/**
 * @brief pass init commits its whole folder with `git add`: a plaintext
 *        file lying in the store, which Git does not track, was committed
 *        and pushed with the new recipients. Init is refused while it is
 *        there, and goes ahead once Git ignores it.
 */
void tst_integration::realPass_initRefusesUnrecordedFiles() {
  const QString passExe = findPass();
  const QString gitExe = QStandardPaths::findExecutable("git");
  if (passExe.isEmpty() || gitExe.isEmpty())
    QSKIP("pass or git not installed");
  RestoreUseGit restoreUseGit;
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  const QString setUp = initRealPassGitStore(storeDir.path(), passExe, gitExe);
  QVERIFY2(setUp.isEmpty(), qPrintable(setUp));
  QFile notes(QDir(storeDir.path()).filePath("notes.txt"));
  QVERIFY(notes.open(QIODevice::WriteOnly));
  notes.write("plaintext export\n");
  notes.close();

  RealPass pass;
  setupPass(pass);
  UserInfo user;
  user.key_id = m_keyFingerprint;
  user.enabled = true;
  QSignalSpy inited(&pass, &Pass::finishedInit);
  QSignalSpy errors(&pass, &Pass::processErrorExit);
  QSignalSpy critical(&pass, &Pass::critical);
  pass.Init(storeDir.path(), {user});
  QCOMPARE(errors.size(), 1);
  QCOMPARE(critical.size(), 1);
  QVERIFY2(critical.first().at(1).toString().contains("notes.txt"),
           qPrintable(critical.first().at(1).toString()));
  QVERIFY2(!critical.first().at(1).toString().contains("Finder"),
           "no file manager hint for a file of the user's own");
  QString tracked;
  QVERIFY(runGit(storeDir.path(), gitExe, {"ls-files"}, &tracked));
  QVERIFY2(!tracked.contains("notes.txt"), qPrintable(tracked));

  // A folder that moves brings what lies in it along: pass mv stages the
  // new path whole.
  const QDir store(storeDir.path());
  QVERIFY(store.mkpath("team"));
  QFile exported(store.filePath("team/export.csv"));
  QVERIFY(exported.open(QIODevice::WriteOnly));
  exported.write("plaintext\n");
  exported.close();
  critical.clear();
  pass.Move(store.filePath("team"), store.filePath("moved"), false);
  QCOMPARE(critical.size(), 1);
  QVERIFY2(critical.first().at(1).toString().contains("team/export.csv"),
           qPrintable(critical.first().at(1).toString()));
  QVERIFY2(store.exists("team/export.csv"), "nothing was moved");
  QVERIFY(QDir(store.filePath("team")).removeRecursively());

  // Ignored, it is not pass's to commit.
  QFile ignore(QDir(storeDir.path()).filePath(".gitignore"));
  QVERIFY(ignore.open(QIODevice::WriteOnly));
  ignore.write("notes.txt\n");
  ignore.close();
  QVERIFY(runGit(storeDir.path(), gitExe, {"add", ".gitignore"}));
  QVERIFY(runGit(storeDir.path(), gitExe, {"commit", "-q", "-m", "ignore"}));
  critical.clear();
  pass.Init(storeDir.path(), {user});
  QVERIFY2(waitForSignal(inited, 30000), "init goes ahead once ignored");
  QCOMPARE(critical.size(), 0);
  QVERIFY(runGit(storeDir.path(), gitExe, {"ls-files"}, &tracked));
  QVERIFY2(!tracked.contains("notes.txt"), qPrintable(tracked));
  QtPassSettings::setUsePass(false);
}

/**
 * @brief pass commits the whole index, not only the folder it staged: a
 *        file staged anywhere stops an init of another folder, and an
 *        insert. A file Git ignores only at its old place
 *        (`/team/export.csv`) would be committed where a move or copy puts
 *        it: refused too.
 */
/**
 * @brief pass commits an entry in the repository nearest its folder, and a
 *        folder may hold one of its own: what is staged there is checked,
 *        not the store's index. A file renamed in the work tree (an
 *        intent-to-add file) is not taken for a staged change.
 */
void tst_integration::realPass_checksTheRepositoryPassCommitsIn() {
  const QString passExe = findPass();
  const QString gitExe = QStandardPaths::findExecutable("git");
  if (passExe.isEmpty() || gitExe.isEmpty())
    QSKIP("pass or git not installed");
  RestoreUseGit restoreUseGit;
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  const QString setUp = initRealPassGitStore(storeDir.path(), passExe, gitExe);
  QVERIFY2(setUp.isEmpty(), qPrintable(setUp));
  QDir store(storeDir.path());

  // A tracked file whose name has a space third, renamed in the work tree:
  // status lists the old name after the new one.
  QFile notes(store.filePath("ab notes.txt"));
  QVERIFY(notes.open(QIODevice::WriteOnly));
  notes.write("tracked\n");
  notes.close();
  QVERIFY(runGit(storeDir.path(), gitExe, {"add", "ab notes.txt"}));
  QVERIFY(runGit(storeDir.path(), gitExe, {"commit", "-q", "-m", "notes"}));
  // Whatever the user's own Git configuration says.
  QVERIFY(
      runGit(storeDir.path(), gitExe, {"config", "status.renames", "true"}));
  QVERIFY(store.rename("ab notes.txt", "renamed.txt"));
  QVERIFY(runGit(storeDir.path(), gitExe,
                 {"add", "--intent-to-add", "renamed.txt"}));
  QString status;
  QVERIFY(runGit(storeDir.path(), gitExe, {"status", "--porcelain"}, &status));
  QVERIFY2(status.startsWith(" R "), qPrintable(status));

  RealPass pass;
  setupPass(pass);
  QSignalSpy inserted(&pass, &Pass::finishedInsert);
  QSignalSpy errors(&pass, &Pass::processErrorExit);
  QSignalSpy critical(&pass, &Pass::critical);
  pass.Insert(QStringLiteral("entry"), QStringLiteral("one\n"), false);
  QVERIFY2(waitForSignal(inserted, 20000), gpgInsertErrorMsg(errors));
  QCOMPARE(critical.size(), 0);

  // A folder with a repository of its own, an entry committed there, then
  // something staged.
  QVERIFY(store.mkpath("team"));
  QVERIFY(initGitRepo(store.filePath("team"), gitExe));
  inserted.clear();
  pass.Insert(QStringLiteral("team/kept"), QStringLiteral("kept\n"), false);
  QVERIFY2(waitForSignal(inserted, 20000), gpgInsertErrorMsg(errors));
  QCOMPARE(critical.size(), 0);
  QFile secret(store.filePath("team/secret.txt"));
  QVERIFY(secret.open(QIODevice::WriteOnly));
  secret.write("plaintext\n");
  secret.close();
  QVERIFY(runGit(store.filePath("team"), gitExe, {"add", "secret.txt"}));
  pass.Insert(QStringLiteral("team/entry"), QStringLiteral("two\n"), false);
  QCOMPARE(critical.size(), 1);
  QVERIFY2(critical.first().at(1).toString().contains("team/secret.txt"),
           qPrintable(critical.first().at(1).toString()));
  QVERIFY2(!store.exists("team/entry.gpg"), "the insert did not run");

  // pass copies and commits where the copy lands: what is staged where the
  // original lies does not go along.
  QSignalSpy copied(&pass, &Pass::finishedCopy);
  critical.clear();
  pass.Copy(store.filePath("team/kept.gpg"), store.filePath("copied.gpg"),
            false);
  QVERIFY2(waitForSignal(copied, 20000), gpgInsertErrorMsg(errors));
  QCOMPARE(critical.size(), 0);
  QString committed;
  QVERIFY(runGit(storeDir.path(), gitExe,
                 {"ls-tree", "--name-only", "HEAD", "copied.gpg"}, &committed));
  QCOMPARE(committed.trimmed(), QStringLiteral("copied.gpg"));
  QString stagedThere;
  QVERIFY(runGit(store.filePath("team"), gitExe,
                 {"diff", "--cached", "--name-only"}, &stagedThere));
  QCOMPARE(stagedThere.trimmed(), QStringLiteral("secret.txt"));
  // A move commits the removal where the original was too, with whatever
  // is staged there.
  pass.Move(store.filePath("team/kept.gpg"), store.filePath("moved.gpg"),
            false);
  QCOMPARE(critical.size(), 1);
  QVERIFY2(critical.first().at(1).toString().contains("team/secret.txt"),
           qPrintable(critical.first().at(1).toString()));
  QVERIFY2(store.exists("team/kept.gpg"), "nothing was moved");

  // A folder whose name starts with two dots is a folder, not a step up.
  QVERIFY(store.mkpath("..team"));
  QVERIFY(initGitRepo(store.filePath("..team"), gitExe));
  QFile dotted(store.filePath("..team/secret.txt"));
  QVERIFY(dotted.open(QIODevice::WriteOnly));
  dotted.write("plaintext\n");
  dotted.close();
  QVERIFY(runGit(store.filePath("..team"), gitExe, {"add", "secret.txt"}));
  critical.clear();
  pass.Insert(QStringLiteral("..team/entry"), QStringLiteral("two\n"), false);
  QCOMPARE(critical.size(), 1);
  QVERIFY2(critical.first().at(1).toString().contains("..team/secret.txt"),
           qPrintable(critical.first().at(1).toString()));

  // The store's own repository has nothing staged: an entry there goes in.
  inserted.clear();
  critical.clear();
  pass.Insert(QStringLiteral("other"), QStringLiteral("three\n"), false);
  QVERIFY2(waitForSignal(inserted, 20000), gpgInsertErrorMsg(errors));
  QCOMPARE(critical.size(), 0);
  QtPassSettings::setUsePass(false);
}

/**
 * @brief Finder and Explorer leave .DS_Store and Thumbs.db in folders they
 *        open, and those refuse a move as any file Git has not recorded.
 *        The message says which .gitignore lines keep them out, and with
 *        those lines committed the move goes ahead.
 */
void tst_integration::realPass_fileManagerFilesGetAHint() {
  const QString passExe = findPass();
  const QString gitExe = QStandardPaths::findExecutable("git");
  if (passExe.isEmpty() || gitExe.isEmpty())
    QSKIP("pass or git not installed");
  RestoreUseGit restoreUseGit;
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  const QString setUp = initRealPassGitStore(storeDir.path(), passExe, gitExe);
  QVERIFY2(setUp.isEmpty(), qPrintable(setUp));
  const QDir store(storeDir.path());
  // Whatever the user's own ignore rules say.
  QTemporaryDir noRules;
  QVERIFY(noRules.isValid());
  const QString noExcludes = noRules.filePath("none");
  QVERIFY(runGit(storeDir.path(), gitExe,
                 {"config", "core.excludesFile", noExcludes}));
  QVERIFY(store.mkpath("team/sub"));
  for (const QString &name :
       {QStringLiteral("team/sub/Thumbs.db"), QStringLiteral("team/.DS_Store"),
        QStringLiteral("team/sub/.DS_Store")}) {
    QFile file(store.filePath(name));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("file manager\n");
    file.close();
  }

  RealPass pass;
  setupPass(pass);
  QSignalSpy moved(&pass, &Pass::finishedMove);
  QSignalSpy errors(&pass, &Pass::processErrorExit);
  QSignalSpy critical(&pass, &Pass::critical);
  pass.Move(store.filePath("team"), store.filePath("moved"), false);
  QCOMPARE(critical.size(), 1);
  const QString why = critical.first().at(1).toString();
  QVERIFY2(why.contains("Finder"), qPrintable(why));
  // The lines to add, in a fixed order, and only the ones found.
  QVERIFY2(why.endsWith(":\n\n.gitignore:\n.DS_Store\nThumbs.db"),
           qPrintable(why));
  QVERIFY2(store.exists("team/.DS_Store"), "nothing was moved");

  QFile ignore(store.filePath(".gitignore"));
  QVERIFY(ignore.open(QIODevice::WriteOnly));
  ignore.write(".DS_Store\nThumbs.db\n");
  ignore.close();
  QVERIFY(runGit(storeDir.path(), gitExe, {"add", ".gitignore"}));
  QVERIFY(runGit(storeDir.path(), gitExe, {"commit", "-q", "-m", "ignore"}));
  critical.clear();
  pass.Move(store.filePath("team"), store.filePath("moved"), false);
  QVERIFY2(waitForSignal(moved, 20000), gpgInsertErrorMsg(errors));
  QCOMPARE(critical.size(), 0);
  QVERIFY(store.exists("moved/.DS_Store"));
  QString tracked;
  QVERIFY(runGit(storeDir.path(), gitExe, {"ls-files"}, &tracked));
  QVERIFY2(!tracked.contains(".DS_Store") && !tracked.contains("Thumbs.db"),
           qPrintable(tracked));

  // One Git already tracks: a rule does not untrack it, so no hint.
  QVERIFY(store.mkpath("tracked"));
  QFile kept(store.filePath("tracked/.DS_Store"));
  QVERIFY(kept.open(QIODevice::WriteOnly));
  kept.write("one\n");
  kept.close();
  QVERIFY(runGit(storeDir.path(), gitExe, {"add", "-f", "tracked/.DS_Store"}));
  QVERIFY(runGit(storeDir.path(), gitExe, {"commit", "-q", "-m", "tracked"}));
  QVERIFY(kept.open(QIODevice::Append));
  kept.write("two\n");
  kept.close();
  critical.clear();
  pass.Move(store.filePath("tracked"), store.filePath("moved2"), false);
  QCOMPARE(critical.size(), 1);
  QVERIFY2(critical.first().at(1).toString().contains("tracked/.DS_Store"),
           qPrintable(critical.first().at(1).toString()));
  QVERIFY2(!critical.first().at(1).toString().contains("Finder"),
           qPrintable(critical.first().at(1).toString()));

  // A folder with a repository of its own reads its own .gitignore: the
  // hint names that one.
  QVERIFY(store.mkpath("other"));
  QVERIFY(initGitRepo(store.filePath("other"), gitExe));
  QVERIFY(runGit(store.filePath("other"), gitExe,
                 {"config", "core.excludesFile", noExcludes}));
  QFile otherFile(store.filePath("other/.DS_Store"));
  QVERIFY(otherFile.open(QIODevice::WriteOnly));
  otherFile.write("file manager\n");
  otherFile.close();
  critical.clear();
  pass.Move(store.filePath("other"), store.filePath("moved3"), false);
  QCOMPARE(critical.size(), 1);
  QVERIFY2(critical.first().at(1).toString().endsWith(
               ":\n\nother/.gitignore:\n.DS_Store"),
           qPrintable(critical.first().at(1).toString()));

  // Names from both repositories: each .gitignore gets its own alone.
  QVERIFY(store.mkpath("other/in"));
  QFile dolphin(store.filePath("other/in/.directory"));
  QVERIFY(dolphin.open(QIODevice::WriteOnly));
  dolphin.write("file manager\n");
  dolphin.close();
  QVERIFY(store.mkpath("fresh"));
  QFile finder(store.filePath("fresh/desktop.ini"));
  QVERIFY(finder.open(QIODevice::WriteOnly));
  finder.write("file manager\n");
  finder.close();
  critical.clear();
  pass.Move(store.filePath("fresh"), store.filePath("other/in"), false);
  QCOMPARE(critical.size(), 1);
  QVERIFY2(
      critical.first().at(1).toString().endsWith(
          ":\n\n.gitignore:\ndesktop.ini\n\nother/.gitignore:\n.directory"),
      qPrintable(critical.first().at(1).toString()));
  QtPassSettings::setUsePass(false);
}

void tst_integration::realPass_refusesStagedAndUnignoredFiles() {
  const QString passExe = findPass();
  const QString gitExe = QStandardPaths::findExecutable("git");
  if (passExe.isEmpty() || gitExe.isEmpty())
    QSKIP("pass or git not installed");
  RestoreUseGit restoreUseGit;
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  const QString setUp = initRealPassGitStore(storeDir.path(), passExe, gitExe);
  QVERIFY2(setUp.isEmpty(), qPrintable(setUp));
  QDir store(storeDir.path());
  QVERIFY(store.mkpath("team"));
  QFile staged(store.filePath("staged.txt"));
  QVERIFY(staged.open(QIODevice::WriteOnly));
  staged.write("plaintext\n");
  staged.close();
  QVERIFY(runGit(storeDir.path(), gitExe, {"add", "staged.txt"}));

  RealPass pass;
  setupPass(pass);
  UserInfo user;
  user.key_id = m_keyFingerprint;
  user.enabled = true;
  QSignalSpy critical(&pass, &Pass::critical);
  pass.Init(store.filePath("team"), {user});
  QCOMPARE(critical.size(), 1);
  QVERIFY2(critical.first().at(1).toString().contains("staged.txt"),
           qPrintable(critical.first().at(1).toString()));
  critical.clear();
  pass.Insert(QStringLiteral("entry"), QStringLiteral("one\n"), false);
  QCOMPARE(critical.size(), 1);
  QVERIFY2(critical.first().at(1).toString().contains("staged.txt"),
           qPrintable(critical.first().at(1).toString()));
  QVERIFY2(!store.exists("entry.gpg"), "the insert did not run");
  QVERIFY(
      runGit(storeDir.path(), gitExe, {"rm", "-q", "--cached", "staged.txt"}));
  QVERIFY(store.remove("staged.txt"));

  QFile ignore(store.filePath(".gitignore"));
  QVERIFY(ignore.open(QIODevice::WriteOnly));
  ignore.write("/team/export.csv\n/other/export.csv\n");
  ignore.close();
  QVERIFY(runGit(storeDir.path(), gitExe, {"add", ".gitignore"}));
  QVERIFY(runGit(storeDir.path(), gitExe, {"commit", "-q", "-m", "ignore"}));
  QFile exported(store.filePath("team/export.csv"));
  QVERIFY(exported.open(QIODevice::WriteOnly));
  exported.write("plaintext\n");
  exported.close();
  for (const bool copy : {false, true}) {
    critical.clear();
    if (copy)
      pass.Copy(store.filePath("team"), store.filePath("renamed"), false);
    else
      pass.Move(store.filePath("team"), store.filePath("renamed"), false);
    QCOMPARE(critical.size(), 1);
    QVERIFY2(critical.first().at(1).toString().contains("renamed/export.csv"),
             qPrintable(critical.first().at(1).toString()));
    QVERIFY2(!store.exists("renamed"), "nothing was moved or copied");
  }
  // Where a rule ignores it at the new place too, the copy goes ahead and
  // the copied export stays out of Git.
  QVERIFY(ignore.open(QIODevice::Append));
  ignore.write("/kept/export.csv\n");
  ignore.close();
  QVERIFY(runGit(storeDir.path(), gitExe, {"add", ".gitignore"}));
  QVERIFY(runGit(storeDir.path(), gitExe, {"commit", "-q", "-m", "keep"}));
  QVERIFY(runGit(storeDir.path(), gitExe,
                 {"check-ignore", "-q", "kept/export.csv"}));
  QSignalSpy copied(&pass, &Pass::finishedCopy);
  QSignalSpy errors(&pass, &Pass::processErrorExit);
  critical.clear();
  pass.Copy(store.filePath("team"), store.filePath("kept"), false);
  QVERIFY2(waitForSignal(copied, 20000), gpgInsertErrorMsg(errors));
  QCOMPARE(critical.size(), 0);
  QVERIFY(store.exists("kept/export.csv"));
  QString tracked;
  QVERIFY(runGit(storeDir.path(), gitExe, {"ls-files"}, &tracked));
  QVERIFY2(!tracked.contains("export.csv"), qPrintable(tracked));
  // A name starting with ':' is a name: ":kept" is not "kept".
  critical.clear();
  // Joined by hand: QDir takes ":kept" for a resource path.
  pass.Copy(store.filePath("team"), storeDir.path() + "/:kept", false);
  QCOMPARE(critical.size(), 1);
  QVERIFY2(critical.first().at(1).toString().contains(":kept/export.csv"),
           qPrintable(critical.first().at(1).toString()));
  QVERIFY2(!QFileInfo::exists(storeDir.path() + "/:kept"),
           "nothing was copied");
  // So does one where the folder's own .gitignore ignores it: the rule
  // goes along.
  QVERIFY(store.mkpath("carry"));
  QFile carryIgnore(store.filePath("carry/.gitignore"));
  QVERIFY(carryIgnore.open(QIODevice::WriteOnly));
  carryIgnore.write("/export.csv\n");
  carryIgnore.close();
  QVERIFY(QFile::copy(store.filePath("team/export.csv"),
                      store.filePath("carry/export.csv")));
  QVERIFY(runGit(storeDir.path(), gitExe, {"add", "carry/.gitignore"}));
  QVERIFY(runGit(storeDir.path(), gitExe, {"commit", "-q", "-m", "carry"}));
  critical.clear();
  copied.clear();
  pass.Copy(store.filePath("carry"), store.filePath("carried"), false);
  QVERIFY2(waitForSignal(copied, 20000), gpgInsertErrorMsg(errors));
  QCOMPARE(critical.size(), 0);
  QVERIFY(store.exists("carried/export.csv"));
  QVERIFY(runGit(storeDir.path(), gitExe, {"ls-files"}, &tracked));
  QVERIFY2(!tracked.contains("export.csv"), qPrintable(tracked));
  // Into a folder that already holds one of its name: cp -r would merge
  // the two, and a rule waiting deeper in it could undo the one that comes
  // along. Refused before that, as anything already where pass writes is.
  QVERIFY(store.mkpath("into/carry"));
  QFile readme(store.filePath("into/carry/readme"));
  QVERIFY(readme.open(QIODevice::WriteOnly));
  readme.write("here\n");
  readme.close();
  QVERIFY(runGit(storeDir.path(), gitExe, {"add", "into"}));
  QVERIFY(runGit(storeDir.path(), gitExe, {"commit", "-q", "-m", "into"}));
  pass.Copy(store.filePath("carry"), store.filePath("into"), false);
  QCOMPARE(critical.size(), 1);
  QVERIFY2(critical.first().at(1).toString().contains(
               QDir::toNativeSeparators(store.filePath("into/carry")) +
               " already exists"),
           qPrintable(critical.first().at(1).toString()));
  QVERIFY2(!store.exists("into/carry/export.csv"), "nothing was copied");
  // Into an existing folder it lands as other/team/export.csv, which no
  // rule ignores (other/export.csv would be).
  QVERIFY(store.mkpath("other"));
  critical.clear();
  pass.Move(store.filePath("team"), store.filePath("other"), false);
  QCOMPARE(critical.size(), 1);
  QVERIFY2(critical.first().at(1).toString().contains("other/team/export.csv"),
           qPrintable(critical.first().at(1).toString()));
  QVERIFY2(!store.exists("other/team"), "nothing was moved");
  QtPassSettings::setUsePass(false);
}

void tst_integration::realPass_insertAndGrep() {
  const QString passExe = findPass();
  if (passExe.isEmpty())
    QSKIP("pass not installed – skipping RealPass grep integration test");

  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());

  QtPassSettings::setPassStore(storeDir.path());
  {
    AppSettings s = QtPassSettings::load();
    s.passExecutable = passExe;
    s.gpgExecutable = m_gpgExe;
    QtPassSettings::save(s);
  }
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
  pass.Grep(QStringLiteral("url"), false);
  QVERIFY2(waitForSignal(grepSpy, 30000), "RealPass finishedGrep not emitted");

  const auto results = grepSpy[0][0].value<GrepResults>();
  QVERIFY2(results.size() >= 2, "grep should find both entries with 'url'");

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

/**
 * @brief Showing an entry says whether its text saves back as its bytes: not
 *        for a Latin-1 one, which the edit dialog then refuses to load (an OK
 *        replaced each such byte with U+FFFD); yes for one that holds a
 *        literal U+FFFD, as older QtPass versions left behind, so that one
 *        stays editable.
 */
void tst_integration::imitatePass_showFlagsAnEntryThatIsNotUtf8() {
  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);

  const struct {
    const char *name;
    QByteArray bytes;
    bool lossless;
  } cases[] = {
      {"latin1",
       QByteArray("Gr\xfc\xdf"
                  "e-4711\nlogin: alice\n"),
       false},
      {"replaced",
       QByteArray("Gr\xef\xbf\xbd"
                  "e-4711\n"),
       true},
      {"utf8",
       QByteArray("Gr\xc3\xbc\xc3\x9f"
                  "e-4711\n"),
       true},
  };
  for (const auto &c : cases) {
    const QString gpgFile =
        storeDir.path() + "/" + QLatin1String(c.name) + ".gpg";
    QCOMPARE(encryptBytesTo(m_gnupgHome.path(), c.bytes, {m_keyFingerprint},
                            gpgFile),
             0);
    QSignalSpy showSpy(&pass, &Pass::finishedShow);
    pass.Show(QLatin1String(c.name));
    QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted");
    QCOMPARE(showSpy.size(), 1);
    QVERIFY2(showSpy[0][2].toBool() == c.lossless,
             qPrintable(QStringLiteral("%1: lossless should be %2")
                            .arg(QLatin1String(c.name))
                            .arg(c.lossless)));
  }
}

/**
 * @brief Re-encrypting a folder keeps every byte of an entry that is not
 *        UTF-8: the plaintext went through QString, so a Latin-1 password and
 *        a binary key came back with U+FFFD in place of those bytes, and the
 *        check after encrypting compared two strings damaged the same way.
 */
void tst_integration::imitatePass_reencryptKeepsBytesThatAreNotUtf8() {
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QtPassSettings::setPassStore(storeDir.path());
  const auto writeGpgId = [&storeDir](const QString &content) {
    QFile gpgId(QDir::cleanPath(storeDir.path() + "/.gpg-id"));
    QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(gpgId.write(content.toUtf8()) == content.toUtf8().size());
  };
  writeGpgId(m_keyFingerprint + "\n" + m_keyFingerprint2 + "\n");

  ImitatePass pass;
  {
    AppSettings s = QtPassSettings::load();
    pass.init(s);
    pass.updateEnv();
  }

  const QByteArray latin1("Gr\xfc\xdf"
                          "e-4711\nlogin: alice\n");
  // No final newline: re-encryption added one.
  const QByteArray keyFile("\x8a\x00\xfe\xc3k", 5);
  // gpg drops the CRs of an entry it stored as text (textmode, below).
  const QByteArray crlf("pw\r\nnote\r\n");
  const QByteArray binary("key\n\x00\x80\xff\xfe\xc3\n", 10);
  const QString latin1File = storeDir.path() + "/latin1.gpg";
  const QString binaryFile = storeDir.path() + "/binary.gpg";
  const QString keyFileFile = storeDir.path() + "/keyfile.gpg";
  const QString crlfFile = storeDir.path() + "/crlf.gpg";
  const QStringList both = {m_keyFingerprint, m_keyFingerprint2};
  QCOMPARE(encryptBytesTo(m_gnupgHome.path(), latin1, both, latin1File), 0);
  QCOMPARE(encryptBytesTo(m_gnupgHome.path(), binary, both, binaryFile), 0);
  QCOMPARE(encryptBytesTo(m_gnupgHome.path(), keyFile, both, keyFileFile), 0);
  QCOMPARE(encryptBytesTo(m_gnupgHome.path(), crlf, both, crlfFile), 0);

  const QString sub2 =
      encryptionSubkeyId(m_gnupgHome.path(), m_keyFingerprint2);
  QVERIFY(!sub2.isEmpty());
  QVERIFY(recipientKeyIds(m_gnupgHome.path(), latin1File).contains(sub2));

  writeGpgId(m_keyFingerprint + "\n");
  {
    // A user's gpg.conf asking for text mode must not cost the CRs.
    GpgConfLine textmode(m_gnupgHome.path(), QStringLiteral("textmode"));
    QVERIFY(textmode.ok());
    QSignalSpy endSpy(&pass, &ImitatePass::endReencryptPath);
    pass.reencryptPath(storeDir.path());
    QVERIFY2(waitForSignal(endSpy, 60000), "endReencryptPath not emitted");
  }

  // Re-encrypted (the removed key is gone), and byte for byte the same.
  for (const QString &file : {latin1File, binaryFile, keyFileFile, crlfFile}) {
    QVERIFY2(!recipientKeyIds(m_gnupgHome.path(), file).contains(sub2),
             qPrintable(file + " was not re-encrypted"));
  }
  QCOMPARE(decryptBytes(m_gnupgHome.path(), latin1File), latin1);
  QCOMPARE(decryptBytes(m_gnupgHome.path(), binaryFile), binary);
  QCOMPARE(decryptBytes(m_gnupgHome.path(), keyFileFile), keyFile);
  QCOMPARE(decryptBytes(m_gnupgHome.path(), crlfFile), crlf);
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

// A co-writer can push a .gitignore (for example "*.gpg") into a shared store.
// Plain `git add` would then skip a new entry, leaving it untracked; a later
// pull that needs its path deletes it. QtPass stages its own entries with
// `git add -f`, so the ignore rule cannot keep them out of the commit.
void tst_integration::imitatePass_ignoredEntryIsStillCommitted() {
  const QString gitExe = QStandardPaths::findExecutable("git");
  if (gitExe.isEmpty())
    QSKIP("git not installed – skipping Git integration test");

  RestoreUseGit restoreUseGit;
  {
    AppSettings s = QtPassSettings::load();
    s.gitExecutable = gitExe;
    s.useGit = true;
    QtPassSettings::save(s);
  }

  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);
  QVERIFY2(initGitRepo(storeDir.path(), gitExe), "git repo setup should work");

  // The pushed ignore rule: every encrypted entry is ignored.
  {
    QFile ign(storeDir.path() + "/.gitignore");
    QVERIFY(ign.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(ign.write("*.gpg\n") > 0);
  }
  {
    QProcess add;
    add.setWorkingDirectory(storeDir.path());
    add.start(gitExe, {"add", ".gitignore"});
    QVERIFY(add.waitForFinished() && add.exitCode() == 0);
    QProcess commit;
    commit.setWorkingDirectory(storeDir.path());
    commit.start(gitExe, {"commit", "-m", "add gitignore"});
    QVERIFY(commit.waitForFinished() && commit.exitCode() == 0);
  }

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("newsite"), QStringLiteral("typed-secret\n"),
              false);
  QVERIFY2(waitForSignal(insertSpy), gpgInsertErrorMsg(insertErrorSpy));

  // The entry must be a tracked file in git, despite the "*.gpg" ignore rule.
  QProcess ls;
  ls.setWorkingDirectory(storeDir.path());
  ls.start(gitExe, {"ls-files", "--", "newsite.gpg"});
  QVERIFY(ls.waitForFinished() && ls.exitCode() == 0);
  QVERIFY2(!QString::fromUtf8(ls.readAll()).trimmed().isEmpty(),
           "the new entry must be tracked in git, not left ignored");

  // An entry already on disk but ignored and untracked (written before this
  // fix) must be staged when it is edited too, or the commit names an
  // untracked path and the edit stays outside git.
  {
    QFile old(storeDir.path() + "/oldsite.gpg");
    QVERIFY(old.open(QIODevice::WriteOnly));
    QVERIFY(old.write("left untracked") > 0);
  }
  QSignalSpy editSpy(&pass, &Pass::finishedInsert);
  pass.Insert(QStringLiteral("oldsite"), QStringLiteral("edited-secret\n"),
              true);
  QVERIFY2(waitForSignal(editSpy), gpgInsertErrorMsg(insertErrorSpy));
  QProcess lsOld;
  lsOld.setWorkingDirectory(storeDir.path());
  lsOld.start(gitExe, {"ls-files", "--", "oldsite.gpg"});
  QVERIFY(lsOld.waitForFinished() && lsOld.exitCode() == 0);
  QVERIFY2(!QString::fromUtf8(lsOld.readAll()).trimmed().isEmpty(),
           "an edited ignored entry must be tracked in git too");
}

void tst_integration::realPass_pushedBareRepoDoesNotRunCommand() {
#ifdef Q_OS_WIN
  QSKIP("POSIX / core.fsmonitor shell semantics; Windows differs");
#else
  const QString passExe = findPass();
  if (passExe.isEmpty())
    QSKIP("pass not installed – skipping RealPass integration test");
  const QString gitExe = QStandardPaths::findExecutable("git");
  if (gitExe.isEmpty())
    QSKIP("git not installed – skipping bare-repo test");

  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QtPassSettings::setPassStore(storeDir.path());
  {
    AppSettings s = QtPassSettings::load();
    s.passExecutable = passExe;
    s.gpgExecutable = m_gpgExe;
    QtPassSettings::save(s);
  }
  QtPassSettings::setUsePass(true);

  QProcess initProc;
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("GNUPGHOME", m_gnupgHome.path());
  env.insert("PASSWORD_STORE_DIR", storeDir.path());
  initProc.setProcessEnvironment(env);
  initProc.start(passExe, {"init", m_keyFingerprint});
  QVERIFY2(initProc.waitForFinished(15000), "pass init timed out");
  QVERIFY2(initProc.exitCode() == 0, "pass init should succeed");

  // The marker the pushed core.fsmonitor command would create if it ran.
  QTemporaryDir markerDir;
  QVERIFY(markerDir.isValid());
  const QString marker = markerDir.path() + "/ran";

  // Plant the bare-repo-shaped folder "work" inside the store.
  const QString work = storeDir.path() + "/work";
  QVERIFY(QDir().mkpath(work + "/objects"));
  QVERIFY(QDir().mkpath(work + "/refs"));
  auto writeFile = [](const QString &path, const QByteArray &data) -> bool {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
      return false;
    return f.write(data) == data.size();
  };
  QVERIFY(writeFile(work + "/HEAD", "ref: refs/heads/master\n"));
  QVERIFY(writeFile(work + "/objects/keep", ""));
  QVERIFY(writeFile(work + "/refs/keep", ""));
  QVERIFY(writeFile(work + "/config",
                    QStringLiteral("[core]\n\trepositoryformatversion = 0\n\t"
                                   "worktree = .\n\tfsmonitor = \"id > %1; "
                                   "false\"\n")
                        .arg(marker)
                        .toUtf8()));

  // Control: without the guard, git in "work" reads the pushed config and
  // runs the command, so the case below tests the guard and nothing else.
  {
    QProcess control;
    QProcessEnvironment plain = QProcessEnvironment::systemEnvironment();
    plain.remove("GIT_CONFIG_COUNT");
    control.setProcessEnvironment(plain);
    control.setWorkingDirectory(work);
    // A global safe.bareRepository=explicit would refuse it too.
    control.start(gitExe, {"-c", "safe.bareRepository=all", "status"});
    QVERIFY(control.waitForFinished(15000));
    QVERIFY2(QFile::exists(marker),
             "git without the guard must run the pushed command");
    QVERIFY(QFile::remove(marker));
  }

  RealPass pass;
  setupPass(pass);

  // Saving an entry in the planted folder is what would let git in "work" run
  // the command; with the guard the pushed repo is refused instead.
  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  QSignalSpy insertErrorSpy(&pass, &Pass::processErrorExit);
  pass.Insert(QStringLiteral("work/vpn"), QStringLiteral("secret\n"), false);
  // Either outcome is fine for the entry; the point is the command must not
  // run.
  const bool done =
      waitForSignal(insertSpy, 20000) || insertErrorSpy.count() > 0;
  QVERIFY2(done, "RealPass insert neither finished nor errored");

  QVERIFY2(!QFile::exists(marker),
           "a git command from the pushed bare repo must not run");

  QtPassSettings::setUsePass(false);
#endif
}

void tst_integration::imitatePass_gitAttributesPinnedForStore() {
  const QString gitExe = QStandardPaths::findExecutable("git");
  if (gitExe.isEmpty())
    QSKIP("git not installed – skipping Git integration test");

  RestoreUseGit restoreUseGit;
  {
    AppSettings s = QtPassSettings::load();
    s.gitExecutable = gitExe;
    s.useGit = true;
    QtPassSettings::save(s);
  }

  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);
  QVERIFY2(initGitRepo(storeDir.path(), gitExe), "git repo setup should work");

  // The pushed hostile attribute: force text conversion on every entry.
  {
    QFile ga(storeDir.path() + "/.gitattributes");
    QVERIFY(ga.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(ga.write("*.gpg text\n") > 0);
    // What `pass git init` writes for decrypted diffs; the pin keeps it.
    QVERIFY(ga.write("*.gpg diff=gpg\n") > 0);
  }

  // updateEnv() runs before every operation and installs the pin once .git
  // exists (INIT ran it before initGitRepo, so run it again here).
  pass.updateEnv();

  // The pin landed in the repo-private info/attributes.
  QFile attr(storeDir.path() + "/.git/info/attributes");
  QVERIFY2(attr.open(QIODevice::ReadOnly | QIODevice::Text),
           "git info/attributes should have been written");
  const QString attrText = QString::fromUtf8(attr.readAll());
  QVERIFY2(attrText.contains("*.gpg -text"),
           qPrintable("info/attributes should pin *.gpg: " + attrText));

  // And Git honours it over the pushed .gitattributes: text is unset for a .gpg
  // path, so no CR/LF or encoding rewrite happens on commit or checkout.
  QProcess chk;
  chk.setWorkingDirectory(storeDir.path());
  chk.start(gitExe, {"check-attr", "text", "--", "anything.gpg"});
  QVERIFY(chk.waitForFinished() && chk.exitCode() == 0);
  const QString chkOut = QString::fromUtf8(chk.readAll());
  QVERIFY2(chkOut.contains("text: unset"),
           qPrintable("the pushed .gitattributes must not win: " + chkOut));

  // diff only changes what Git shows, so the pin leaves pass's diff=gpg.
  QProcess diffChk;
  diffChk.setWorkingDirectory(storeDir.path());
  diffChk.start(gitExe, {"check-attr", "diff", "--", "anything.gpg"});
  QVERIFY(diffChk.waitForFinished() && diffChk.exitCode() == 0);
  const QString diffOut = QString::fromUtf8(diffChk.readAll());
  QVERIFY2(diffOut.contains("diff: gpg"),
           qPrintable("pass's decrypted diffs must keep working: " + diffOut));
}

// A store that becomes a repository after updateEnv() ran (Git ▸ Init) gets
// the pin when git init finishes, before anything is staged in it.
void tst_integration::imitatePass_gitInitPinsAttributes() {
  const QString gitExe = QStandardPaths::findExecutable("git");
  if (gitExe.isEmpty())
    QSKIP("git not installed – skipping Git integration test");

  RestoreUseGit restoreUseGit;
  {
    AppSettings s = QtPassSettings::load();
    s.gitExecutable = gitExe;
    s.useGit = true;
    QtPassSettings::save(s);
  }

  QTemporaryDir storeDir;
  ImitatePass pass;
  INIT_IMITATE_STORE_OR_FAIL(storeDir, pass);
  QVERIFY(!QFileInfo::exists(storeDir.path() + "/.git"));

  QSignalSpy initSpy(&pass, &Pass::finishedGitInit);
  pass.GitInit();
  QVERIFY2(waitForSignal(initSpy, 20000), "git init did not finish");

  QFile attr(storeDir.path() + "/.git/info/attributes");
  QVERIFY2(attr.open(QIODevice::ReadOnly | QIODevice::Text),
           "git init should leave the pin in info/attributes");
  QVERIFY2(QString::fromUtf8(attr.readAll()).contains("*.gpg -text"),
           "info/attributes should pin *.gpg");
}

void tst_integration::realPass_dashNameDoesNotEscapeStore() {
#ifdef Q_OS_WIN
  QSKIP("POSIX symlink semantics; Windows junction handling differs");
#else
  const QString passExe = findPass();
  if (passExe.isEmpty())
    QSKIP("pass not installed – skipping RealPass integration test");

  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());
  QtPassSettings::setPassStore(storeDir.path());
  {
    AppSettings s = QtPassSettings::load();
    s.passExecutable = passExe;
    s.gpgExecutable = m_gpgExe;
    QtPassSettings::save(s);
  }
  QtPassSettings::setUsePass(true);

  // .gpg-id with the test recipient; no `pass init` run (not needed to show).
  {
    QFile gpgId(storeDir.path() + "/.gpg-id");
    QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(gpgId.write((m_keyFingerprint + "\n").toUtf8()) > 0);
  }

  // Encrypt two files to the test key: a decoy inside the store named "--.gpg",
  // and a secret outside the store that a ".gpg" symlink will point at.
  auto encryptTo = [&](const QString &plain, const QString &outFile) -> bool {
    QProcess gpg;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("GNUPGHOME", m_gnupgHome.path());
    gpg.setProcessEnvironment(env);
    gpg.start(m_gpgExe, {"--batch", "--yes", "--trust-model", "always", "-e",
                         "-r", m_keyFingerprint, "-o", outFile, "--"});
    if (!gpg.waitForStarted(5000))
      return false;
    gpg.write(plain.toUtf8());
    gpg.closeWriteChannel();
    return gpg.waitForFinished(15000) && gpg.exitCode() == 0;
  };

  QTemporaryDir outsideDir;
  QVERIFY(outsideDir.isValid());
  const QString outsideSecret = outsideDir.path() + "/bank.gpg";
  QVERIFY2(encryptTo("BANK-SECRET\n", outsideSecret), "encrypt outside secret");
  QVERIFY2(encryptTo("DECOY\n", storeDir.path() + "/--.gpg"), "encrypt decoy");
  QVERIFY2(QFile::link(outsideSecret, storeDir.path() + "/.gpg"),
           "failed to create the .gpg symlink for the test");

  RealPass pass;
  setupPass(pass);

  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("--"));
  QVERIFY2(waitForSignal(showSpy, 20000), "RealPass finishedShow not emitted");
  const QString shown = showSpy[0][0].toString();
  QVERIFY2(!shown.contains("BANK-SECRET"),
           "`pass show --` must not decrypt the file the .gpg link points to");
  QVERIFY2(shown.contains("DECOY"),
           "the name \"--\" should resolve to the store's own --.gpg entry");

  QtPassSettings::setUsePass(false);
#endif
}

// A signing key that is revoked and has also expired: under `trust-model
// always` gpg skips the trust check that reports KEYREVOKED and calls the
// signature merely expired (EXPKEYSIG), which counts. The verify pins the
// trust model, so the revocation still shows (scan finding
// ANT-2026-762RKXO4). Real gpg, in a keyring of its own, keys made at a
// faked time so they have expired by now.
void tst_integration::
    gpgIdSigner_revokedExpiredKeyRefusedUnderTrustModelAlways() {
#ifdef Q_OS_WIN
  QSKIP("gpg home and faked time handled differently on Windows");
#else
  if (m_gpgExe.isEmpty())
    QSKIP("gpg not installed");
  QTemporaryDir home;
  QVERIFY(home.isValid());
  QVERIFY(QFile::setPermissions(home.path(), QFileDevice::ReadOwner |
                                                 QFileDevice::WriteOwner |
                                                 QFileDevice::ExeOwner));
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert(QStringLiteral("GNUPGHOME"), home.path());
  const auto cleanup = qScopeGuard([&] {
    QProcess kill;
    kill.setProcessEnvironment(env);
    kill.start(QStringLiteral("gpgconf"),
               {QStringLiteral("--kill"), QStringLiteral("all")});
    kill.waitForFinished(5000);
  });
  const auto gpg = [&](const QStringList &args, QString *out = nullptr) {
    QProcess p;
    p.setProcessEnvironment(env);
    p.start(m_gpgExe, QStringList{QStringLiteral("--batch")} + args);
    if (!p.waitForFinished(60000)) {
      return -1;
    }
    if (out != nullptr) {
      *out = QString::fromUtf8(p.readAllStandardOutput());
    }
    return p.exitCode();
  };
  const QString start = QStringLiteral("--faked-system-time=20260101T000000");
  const QString later = QStringLiteral("--faked-system-time=20260101T010000");
  const auto makeKey = [&](const QString &name) {
    if (gpg({start, QStringLiteral("--passphrase="),
             QStringLiteral("--quick-gen-key"), name, QStringLiteral("ed25519"),
             QStringLiteral("sign"), QStringLiteral("1d")}) != 0) {
      return QString();
    }
    QString out;
    gpg({QStringLiteral("--with-colons"), QStringLiteral("--list-keys"),
         QStringLiteral("--"), name},
        &out);
    static const QRegularExpression fpr(QStringLiteral("^fpr:+([0-9A-F]+):"),
                                        QRegularExpression::MultilineOption);
    return fpr.match(out).captured(1);
  };
  const QString list = home.filePath(QStringLiteral("list"));
  {
    QFile f(list);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QVERIFY(f.write(kSignedListBody) > 0);
  }
  const auto signAs = [&](const QString &fpr, const QString &sig) {
    return gpg({later, QStringLiteral("--yes"), QStringLiteral("--local-user"),
                fpr, QStringLiteral("--detach-sign"), QStringLiteral("-o"), sig,
                QStringLiteral("--"), list}) == 0;
  };
  const QString kept = makeKey(QStringLiteral("Kept <kept@example.invalid>"));
  const QString lost = makeKey(QStringLiteral("Lost <lost@example.invalid>"));
  QVERIFY2(!kept.isEmpty() && !lost.isEmpty(), "gpg could not make the keys");
  const QString keptSig = home.filePath(QStringLiteral("kept.sig"));
  const QString lostSig = home.filePath(QStringLiteral("lost.sig"));
  QVERIFY(signAs(kept, keptSig));
  QVERIFY(signAs(lost, lostSig));
  // Revoke the lost key with the certificate gpg made for it.
  {
    QFile rev(
        home.filePath(QStringLiteral("openpgp-revocs.d/%1.rev").arg(lost)));
    QVERIFY(rev.open(QIODevice::ReadOnly));
    QByteArray cert = rev.readAll();
    cert.replace("\n:-----BEGIN", "\n-----BEGIN");
    QFile out(home.filePath(QStringLiteral("rev.asc")));
    QVERIFY(out.open(QIODevice::WriteOnly) && out.write(cert) == cert.size());
  }
  QCOMPARE(gpg({QStringLiteral("--import"),
                home.filePath(QStringLiteral("rev.asc"))}),
           0);
  {
    QFile conf(home.filePath(QStringLiteral("gpg.conf")));
    QVERIFY(conf.open(QIODevice::WriteOnly));
    QVERIFY(conf.write("trust-model always\n") > 0);
  }

  const GpgIdSigner::Exec exec =
      [&env](const QString &app, const QStringList &args, const QString &input,
             QString *out, QString *err) {
        QProcess p;
        p.setProcessEnvironment(env);
        return Executor::executeBlocking(p, app, args, input, out, err);
      };
  // Control: an expired key that is not revoked still vouches, as gpg calls
  // its signature good.
  QVERIFY2(GpgIdSigner(m_gpgExe, {kept}, exec).verify(kSignedListBody, keptSig),
           "an expired, unrevoked signing key still counts");
  QVERIFY2(
      !GpgIdSigner(m_gpgExe, {lost}, exec).verify(kSignedListBody, lostSig),
      "a revoked key that has also expired must not vouch for a list");
#endif
}

QTEST_MAIN(tst_integration)
#include "tst_integration.moc"
