// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "../../../src/gpgidsigner.h"

/**
 * @brief GpgIdSigner against a scripted gpg: which arguments it passes and
 *        how it reads the status output. No real gpg is involved.
 */
class tst_gpgidsigner : public QObject {
  Q_OBJECT

  /// One recorded gpg invocation.
  struct Call {
    QString app;
    QStringList args;
  };
  /// A fake runner that records calls and answers with canned output.
  struct FakeGpg {
    QList<Call> calls;
    int rc = 0;
    QString out;
    QString err;
    auto exec() -> GpgIdSigner::Exec {
      return [this](const QString &app, const QStringList &args,
                    const QString &, QString *o, QString *e) {
        calls.append({app, args});
        if (o)
          *o = out;
        if (e)
          *e = err;
        return rc;
      };
    }
  };

  static const inline QString kFpr =
      QStringLiteral("0123456789ABCDEF0123456789ABCDEF01234567");
  static const inline QString kPrimary =
      QStringLiteral("FEDCBA9876543210FEDCBA9876543210FEDCBA98");
  static auto validSig(const QString &fpr, const QString &primary) -> QString {
    return QStringLiteral("[GNUPG:] NEWSIG\n[GNUPG:] SIG_ID abc 2026-09-17 1\n"
                          "[GNUPG:] VALIDSIG %1 2026-09-17 1758100000 0 4 0 1 "
                          "10 00 %2\n[GNUPG:] TRUST_ULTIMATE 0 pgp\n")
        .arg(fpr, primary);
  }

private slots:
  void keysFromSettingSplitsOnSpaces();
  void noKeysMeansNothingToSignAndVerifyPasses();
  void haveSecretKeyReadsKeyConsidered();
  void haveSecretKeyFailsOnGpgError();
  void signUsesTheFirstKeyOnlyAndReportsStderr();
  void signPassesTheFilePathThroughTheWslTranslation();
  void verifyPassesArgsAndAcceptsEitherFingerprint();
  void verifyRejectsUnknownSignerAndGpgFailure();
  void validSigFingerprintsParsesStatusLines();
  void defaultRunnerIsTheExecutor();
};

void tst_gpgidsigner::keysFromSettingSplitsOnSpaces() {
  QCOMPARE(GpgIdSigner::keysFromSetting(QString()), QStringList());
  QCOMPARE(GpgIdSigner::keysFromSetting(QStringLiteral("  ")), QStringList());
  QCOMPARE(GpgIdSigner::keysFromSetting(QStringLiteral("A  B ")),
           (QStringList{QStringLiteral("A"), QStringLiteral("B")}));
}

void tst_gpgidsigner::noKeysMeansNothingToSignAndVerifyPasses() {
  FakeGpg gpg;
  gpg.rc = 2; // would fail if it were ever called
  const GpgIdSigner signer(QStringLiteral("gpg"), {}, gpg.exec());
  QVERIFY(!signer.enabled());
  QVERIFY(signer.sign(QStringLiteral("/store/.gpg-id")));
  QVERIFY(signer.verify(QStringLiteral("/store/.gpg-id")));
  QVERIFY2(gpg.calls.isEmpty(), "no key configured: gpg must not run");
}

void tst_gpgidsigner::haveSecretKeyReadsKeyConsidered() {
  FakeGpg gpg;
  gpg.out = QStringLiteral("[GNUPG:] KEY_CONSIDERED %1 0\nsec   rsa4096\n")
                .arg(kPrimary);
  const GpgIdSigner signer(QStringLiteral("gpg"), {kFpr, kPrimary}, gpg.exec());
  QVERIFY(signer.haveSecretKey());
  QCOMPARE(gpg.calls.size(), 1);
  QCOMPARE(gpg.calls.first().app, QStringLiteral("gpg"));
  QCOMPARE(gpg.calls.first().args,
           (QStringList{QStringLiteral("--status-fd=1"),
                        QStringLiteral("--list-secret-keys"), kFpr, kPrimary}));

  gpg.out = QStringLiteral("[GNUPG:] KEY_CONSIDERED SOMEOTHERKEY 0\n");
  QVERIFY2(!signer.haveSecretKey(), "a different key does not count");
}

void tst_gpgidsigner::haveSecretKeyFailsOnGpgError() {
  FakeGpg gpg;
  gpg.rc = 2;
  gpg.out = QStringLiteral("[GNUPG:] KEY_CONSIDERED %1 0\n").arg(kFpr);
  const GpgIdSigner signer(QStringLiteral("gpg"), {kFpr}, gpg.exec());
  QVERIFY(!signer.haveSecretKey());
}

void tst_gpgidsigner::signUsesTheFirstKeyOnlyAndReportsStderr() {
  FakeGpg gpg;
  const GpgIdSigner signer(QStringLiteral("/usr/bin/gpg"),
                           {QStringLiteral("FIRST"), QStringLiteral("SECOND")},
                           gpg.exec());
  QVERIFY(signer.sign(QStringLiteral("/store/.gpg-id")));
  QCOMPARE(gpg.calls.size(), 1);
  QCOMPARE(
      gpg.calls.first().args,
      (QStringList{QStringLiteral("--default-key"), QStringLiteral("FIRST"),
                   QStringLiteral("--yes"), QStringLiteral("--detach-sign"),
                   QStringLiteral("/store/.gpg-id")}));

  gpg.rc = 2;
  gpg.err = QStringLiteral("gpg: signing failed: No secret key\n");
  QString error;
  QVERIFY(!signer.sign(QStringLiteral("/store/.gpg-id"), &error));
  QCOMPARE(error, gpg.err);
}

void tst_gpgidsigner::signPassesTheFilePathThroughTheWslTranslation() {
  // Executor::translatePathForWsl() only calls wslpath for a "wsl " gpg; for
  // any other gpg it hands back the cleaned path, which is what shows here.
  FakeGpg gpg;
  const GpgIdSigner signer(QStringLiteral("gpg"), {kPrimary}, gpg.exec());
  QVERIFY(signer.sign(QStringLiteral("/store//sub/../.gpg-id")));
  QCOMPARE(gpg.calls.first().args.last(), QStringLiteral("/store/.gpg-id"));
  gpg.out = validSig(kFpr, kPrimary);
  QVERIFY(signer.verify(QStringLiteral("/store//.gpg-id")));
  QCOMPARE(gpg.calls.last().args.mid(2),
           (QStringList{QStringLiteral("/store/.gpg-id.sig"),
                        QStringLiteral("/store/.gpg-id")}));
}

void tst_gpgidsigner::verifyPassesArgsAndAcceptsEitherFingerprint() {
  FakeGpg gpg;
  gpg.out = validSig(kFpr, kPrimary);
  {
    const GpgIdSigner bySubkey(QStringLiteral("gpg"), {kFpr}, gpg.exec());
    QVERIFY(bySubkey.verify(QStringLiteral("/store/.gpg-id")));
    QCOMPARE(gpg.calls.first().args,
             (QStringList{QStringLiteral("--verify"),
                          QStringLiteral("--status-fd=1"),
                          QStringLiteral("/store/.gpg-id.sig"),
                          QStringLiteral("/store/.gpg-id")}));
  }
  {
    const GpgIdSigner byPrimary(
        QStringLiteral("gpg"), {QStringLiteral("OTHER"), kPrimary}, gpg.exec());
    QVERIFY2(byPrimary.verify(QStringLiteral("/store/.gpg-id")),
             "the primary key's fingerprint is accepted too");
  }
}

void tst_gpgidsigner::verifyRejectsUnknownSignerAndGpgFailure() {
  FakeGpg gpg;
  gpg.out = validSig(kFpr, kPrimary);
  const GpgIdSigner signer(
      QStringLiteral("gpg"),
      {QStringLiteral("1111111111111111111111111111111111111111")}, gpg.exec());
  QVERIFY2(!signer.verify(QStringLiteral("/store/.gpg-id")),
           "a valid signature by someone else is not good enough");

  const GpgIdSigner mine(QStringLiteral("gpg"), {kFpr}, gpg.exec());
  gpg.rc = 1; // BADSIG: gpg exits non-zero, whatever it prints
  QVERIFY(!mine.verify(QStringLiteral("/store/.gpg-id")));
  gpg.rc = 0;
  gpg.out = QStringLiteral("[GNUPG:] NEWSIG\n[GNUPG:] ERRSIG 1 2 3\n");
  QVERIFY2(!mine.verify(QStringLiteral("/store/.gpg-id")),
           "exit 0 without VALIDSIG is not a verification");
}

void tst_gpgidsigner::validSigFingerprintsParsesStatusLines() {
  QCOMPARE(GpgIdSigner::validSigFingerprints(validSig(kFpr, kPrimary)),
           (QStringList{kFpr, kPrimary}));
  QCOMPARE(GpgIdSigner::validSigFingerprints(
               validSig(kFpr, kPrimary).replace('\n', "\r\n")),
           (QStringList{kFpr, kPrimary}));
  QVERIFY(GpgIdSigner::validSigFingerprints(QString()).isEmpty());
  QVERIFY(GpgIdSigner::validSigFingerprints(
              QStringLiteral("VALIDSIG %1 x %2").arg(kFpr, kPrimary))
              .isEmpty()); // not a status line
}

void tst_gpgidsigner::defaultRunnerIsTheExecutor() {
  // No runner given: the real Executor runs the command. A gpg that does not
  // exist cannot have a secret key, and must not be reported as having one.
  const GpgIdSigner signer(QStringLiteral("/nonexistent/qtpass-gpg"), {kFpr});
  QVERIFY(!signer.haveSecretKey());
  QVERIFY(!signer.verify(QStringLiteral("/nonexistent/.gpg-id")));
}

QTEST_MAIN(tst_gpgidsigner)
#include "tst_gpgidsigner.moc"
