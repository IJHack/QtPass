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

  for (const auto &line : out.split('\n')) {
    if (line.startsWith("fpr:")) {
      const auto parts = line.split(':');
      if (parts.size() >= 10)
        return parts[9].trimmed();
    }
  }
  return {};
}

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class tst_integration : public QObject {
  Q_OBJECT

  QString m_gpgExe;
  QTemporaryDir m_gnupgHome;
  QString m_keyFingerprint;

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
  qRegisterMetaType<GrepResults>("GrepResults");
  qRegisterMetaType<GrepResults>(
      "QList<QPair<QString,QStringList>>"); // Qt5 fallback
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

void tst_integration::imitatePass_insertAndShow() {
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());

  QtPassSettings::setPassStore(storeDir.path());

  {
    QFile gpgId(storeDir.path() + "/.gpg-id");
    QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
    gpgId.write((m_keyFingerprint + "\n").toUtf8());
  }

  ImitatePass pass;
  setupPass(pass);

  const QString entryName = QStringLiteral("test/password");
  const QString entryContent = QStringLiteral("hunter2\nuser: testuser\n");

  // ImitatePass::Insert does not create parent directories; the UI does.
  QVERIFY(QDir(storeDir.path()).mkpath("test"));

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  pass.Insert(entryName, entryContent, false);
  QVERIFY2(waitForSignal(insertSpy), "finishedInsert not emitted");

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
  QVERIFY(storeDir.isValid());

  QtPassSettings::setPassStore(storeDir.path());

  {
    QFile gpgId(storeDir.path() + "/.gpg-id");
    QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
    gpgId.write((m_keyFingerprint + "\n").toUtf8());
  }

  ImitatePass pass;
  setupPass(pass);

  QVERIFY(QDir(storeDir.path()).mkpath("work"));

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  pass.Insert(QStringLiteral("work/github"),
              QStringLiteral("s3cr3t\ntoken: abc123\n"), false);
  QVERIFY2(waitForSignal(insertSpy), "finishedInsert not emitted");

  insertSpy.clear();
  pass.Insert(QStringLiteral("work/gitlab"),
              QStringLiteral("another\ntoken: xyz789\n"), false);
  QVERIFY2(waitForSignal(insertSpy), "finishedInsert not emitted (2nd)");

  QSignalSpy grepSpy(&pass, &Pass::finishedGrep);
  pass.Grep(QStringLiteral("token"));
  QVERIFY2(waitForSignal(grepSpy, 20000), "finishedGrep not emitted");

  const auto results = grepSpy[0][0].value<GrepResults>();
  QVERIFY2(results.size() == 2, "grep should find both entries");
}

void tst_integration::imitatePass_insertMoveAndShow() {
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());

  QtPassSettings::setPassStore(storeDir.path());

  {
    QFile gpgId(storeDir.path() + "/.gpg-id");
    QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
    gpgId.write((m_keyFingerprint + "\n").toUtf8());
  }

  ImitatePass pass;
  setupPass(pass);

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  pass.Insert(QStringLiteral("original"), QStringLiteral("moveme\n"), false);
  QVERIFY2(waitForSignal(insertSpy), "finishedInsert not emitted");

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
  QVERIFY(storeDir.isValid());

  QtPassSettings::setPassStore(storeDir.path());

  {
    QFile gpgId(storeDir.path() + "/.gpg-id");
    QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
    gpgId.write((m_keyFingerprint + "\n").toUtf8());
  }

  ImitatePass pass;
  setupPass(pass);

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  pass.Insert(QStringLiteral("original"), QStringLiteral("copyme\n"), false);
  QVERIFY2(waitForSignal(insertSpy), "finishedInsert not emitted");

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
  QVERIFY(storeDir.isValid());

  QtPassSettings::setPassStore(storeDir.path());

  {
    QFile gpgId(storeDir.path() + "/.gpg-id");
    QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
    gpgId.write((m_keyFingerprint + "\n").toUtf8());
  }

  ImitatePass pass;
  setupPass(pass);

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  pass.Insert(QStringLiteral("deleteme"), QStringLiteral("gone\n"), false);
  QVERIFY2(waitForSignal(insertSpy), "finishedInsert not emitted");

  const QString gpgFile = storeDir.path() + "/deleteme.gpg";
  QVERIFY2(QFile::exists(gpgFile), "file must exist before remove");

  // Without git, ImitatePass::Remove removes the file synchronously; no signal.
  pass.Remove(QStringLiteral("deleteme"), false);
  QVERIFY2(!QFile::exists(gpgFile), "file should be gone after remove");
}

void tst_integration::imitatePass_nestedDirectoryInsertAndShow() {
  QTemporaryDir storeDir;
  QVERIFY(storeDir.isValid());

  QtPassSettings::setPassStore(storeDir.path());

  {
    QFile gpgId(storeDir.path() + "/.gpg-id");
    QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
    gpgId.write((m_keyFingerprint + "\n").toUtf8());
  }

  ImitatePass pass;
  setupPass(pass);

  QVERIFY(QDir(storeDir.path()).mkpath("level1/level2/level3"));

  QSignalSpy insertSpy(&pass, &Pass::finishedInsert);
  pass.Insert(QStringLiteral("level1/level2/level3/deep"),
              QStringLiteral("deepvalue\n"), false);
  QVERIFY2(waitForSignal(insertSpy), "finishedInsert not emitted");

  const QString gpgFile = storeDir.path() + "/level1/level2/level3/deep.gpg";
  QVERIFY2(QFile::exists(gpgFile), "nested .gpg file should be created");

  QSignalSpy showSpy(&pass, &Pass::finishedShow);
  pass.Show(QStringLiteral("level1/level2/level3/deep"));
  QVERIFY2(waitForSignal(showSpy), "finishedShow not emitted for nested entry");
  QVERIFY2(showSpy[0][0].toString().contains("deepvalue"),
           "decrypted nested entry should contain the content");
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
  pass.Insert(QStringLiteral("realtest"),
              QStringLiteral("realpassword\nurl: example.com\n"), false);
  QVERIFY2(waitForSignal(insertSpy, 20000),
           "RealPass finishedInsert not emitted");

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
  pass.Insert(QStringLiteral("email/gmail"),
              QStringLiteral("gmailpass\nurl: mail.google.com\n"), false);
  QVERIFY2(waitForSignal(insertSpy, 20000), "insert 1 not emitted");

  insertSpy.clear();
  pass.Insert(QStringLiteral("email/outlook"),
              QStringLiteral("outlookpass\nurl: outlook.com\n"), false);
  QVERIFY2(waitForSignal(insertSpy, 20000), "insert 2 not emitted");

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
  // Check for pass-otp extension.
  const bool hasOtp =
      QFile::exists("/usr/lib/password-store/extensions/otp.bash");
  if (!hasOtp)
    QSKIP("pass-otp extension not found – skipping OTP integration test");

  const QString passExe = findPass();
  if (passExe.isEmpty())
    QSKIP("pass not installed – skipping OTP integration test");

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

QTEST_MAIN(tst_integration)
#include "tst_integration.moc"
