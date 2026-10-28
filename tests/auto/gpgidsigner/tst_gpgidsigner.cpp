// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <optional>

#include "../../../src/gpgidsigner.h"
#include "../../../src/util.h"

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

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
    QString input;
  };
  /// A fake runner that records calls and answers with canned output.
  struct FakeGpg {
    QList<Call> calls;
    int rc = 0;
    QString out;
    QString err;
    /// What a signing call writes to its --output; empty writes nothing.
    QByteArray signature = QByteArrayLiteral("SIGNATURE");
    /// When set, what a --verify call prints instead of @c out: sign()
    /// checks the signature it just made, and that check is answered here.
    std::optional<QString> verifyOut;
    auto exec() -> GpgIdSigner::Exec {
      return [this](const QString &app, const QStringList &args,
                    const QString &input, QString *o, QString *e) {
        calls.append({app, args, input});
        if (verifyOut && args.contains(QStringLiteral("--verify"))) {
          if (o)
            *o = *verifyOut;
          return 0;
        }
        const int at = args.indexOf(QStringLiteral("--output"));
        if (rc == 0 && at >= 0 && at + 1 < args.size()) {
          // As gpg does: the output exists once it ran; empty when the
          // fake is told to write nothing.
          QFile f(args.at(at + 1));
          if (f.open(QIODevice::WriteOnly) && !signature.isEmpty())
            f.write(signature);
        }
        if (o)
          *o = out;
        if (e)
          *e = err;
        return rc;
      };
    }
  };

  static const inline QByteArray kGpgId = QByteArrayLiteral("ALICE\nBOB\n");
  static const inline QString kFpr =
      QStringLiteral("0123456789ABCDEF0123456789ABCDEF01234567");
  static const inline QString kPrimary =
      QStringLiteral("FEDCBA9876543210FEDCBA9876543210FEDCBA98");
  /// A real .gpg-id.sig to hand to verify(), which reads only a regular
  /// file; gpg itself is the fake, so the content does not matter.
  static auto sigPath() -> QString {
    static QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral(".gpg-id.sig"));
    if (!QFileInfo::exists(path)) {
      QFile f(path);
      if (f.open(QIODevice::WriteOnly)) {
        f.write("SIGNATURE");
      }
    }
    return path;
  }

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
  void signDoesNotWriteASignatureByAnotherKey();
  void signFailureLeavesNoSignature();
  void signPassesTheFilePathThroughTheWslTranslation();
  void signReplacesALinkUnderTheSignatureNameAsAnEntry();
  void verifyPassesArgsAndAcceptsEitherFingerprint();
  void verifyFileHandsBackTheBytesItVerified();
  void verifyFileReadsOnlyARegularFileOfBoundedSize();
  void linkedGpgIdOrSignatureIsNotVerified();
  void verifyRefusesBytesThatAreNotUtf8();
  void verifyRefusesASignatureThatIsNoRegularFile();
  void verifyRejectsUnknownSignerAndGpgFailure();
  void validSigFingerprintsParsesStatusLines();
  void defaultRunnerIsTheExecutor();
};

void tst_gpgidsigner::keysFromSettingSplitsOnSpaces() {
  QCOMPARE(GpgIdSigner::keysFromSetting(QString()), QStringList());
  QCOMPARE(GpgIdSigner::keysFromSetting(QStringLiteral("  ")), QStringList());
  QCOMPARE(GpgIdSigner::keysFromSetting(QStringLiteral("A  B ")),
           (QStringList{QStringLiteral("A"), QStringLiteral("B")}));
  // gpg's status lines carry fingerprints in upper case; a lower-case
  // fingerprint in the settings must still match them.
  QCOMPARE(GpgIdSigner::keysFromSetting(kFpr.toLower()), QStringList{kFpr});
}

void tst_gpgidsigner::noKeysMeansNothingToSignAndVerifyPasses() {
  FakeGpg gpg;
  gpg.rc = 2; // would fail if it were ever called
  const GpgIdSigner signer(QStringLiteral("gpg"), {}, gpg.exec());
  QVERIFY(!signer.enabled());
  QVERIFY(signer.sign(QStringLiteral("/store/.gpg-id"), kGpgId));
  QVERIFY(signer.verify(kGpgId, sigPath()));
  QVERIFY2(gpg.calls.isEmpty(), "no key configured: gpg must not run");
}

void tst_gpgidsigner::haveSecretKeyReadsKeyConsidered() {
  FakeGpg gpg;
  gpg.out =
      QStringLiteral("[GNUPG:] KEY_CONSIDERED %1 0\nsec   rsa4096\n").arg(kFpr);
  const GpgIdSigner signer(QStringLiteral("gpg"), {kFpr, kPrimary}, gpg.exec());
  QVERIFY(signer.haveSecretKey());
  QCOMPARE(gpg.calls.size(), 1);
  QCOMPARE(gpg.calls.first().app, QStringLiteral("gpg"));
  // Only the first key is asked about: it is the one sign() will use.
  QCOMPARE(gpg.calls.first().args,
           (QStringList{QStringLiteral("--status-fd=1"),
                        QStringLiteral("--list-secret-keys"), kFpr}));

  gpg.out = QStringLiteral("[GNUPG:] KEY_CONSIDERED %1 0\n").arg(kPrimary);
  QVERIFY2(!signer.haveSecretKey(),
           "a secret key for the second entry does not make the first sign");

  const GpgIdSigner none(QStringLiteral("gpg"), {}, gpg.exec());
  QVERIFY2(!none.haveSecretKey(), "nothing configured, nothing to sign with");
}

void tst_gpgidsigner::haveSecretKeyFailsOnGpgError() {
  FakeGpg gpg;
  gpg.rc = 2;
  gpg.out = QStringLiteral("[GNUPG:] KEY_CONSIDERED %1 0\n").arg(kFpr);
  const GpgIdSigner signer(QStringLiteral("gpg"), {kFpr}, gpg.exec());
  QVERIFY(!signer.haveSecretKey());
}

void tst_gpgidsigner::signUsesTheFirstKeyOnlyAndReportsStderr() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QString gpgId = QDir(store.path()).filePath(QStringLiteral(".gpg-id"));
  FakeGpg gpg;
  gpg.verifyOut = validSig(kFpr, kFpr);
  const GpgIdSigner signer(QStringLiteral("/usr/bin/gpg"), {kFpr, kPrimary},
                           gpg.exec());
  QVERIFY(signer.sign(gpgId, kGpgId));
  QCOMPARE(gpg.calls.size(), 2);
  // The bytes to sign go in on stdin, the signature comes out in a
  // directory of QtPass's own: no store path is opened by gpg. --local-user
  // makes gpg fail without that secret key instead of signing with another.
  const QStringList args = gpg.calls.first().args;
  QCOMPARE(args.mid(0, 4), (QStringList{QStringLiteral("--local-user"), kFpr,
                                        QStringLiteral("--yes"),
                                        QStringLiteral("--detach-sign")}));
  QCOMPARE(args.at(4), QStringLiteral("--output"));
  QVERIFY2(!args.at(5).startsWith(store.path()), qPrintable(args.at(5)));
  QCOMPARE(args.last(), QStringLiteral("-"));
  QCOMPARE(gpg.calls.first().input, QString::fromUtf8(kGpgId));
  // Then the new signature is checked, in the scratch, against the same
  // bytes, before it goes next to the list.
  QCOMPARE(
      gpg.calls.at(1).args,
      (QStringList{QStringLiteral("--verify"), QStringLiteral("--status-fd=1"),
                   QStringLiteral("--"), args.at(5), QStringLiteral("-")}));
  QCOMPARE(gpg.calls.at(1).input, QString::fromUtf8(kGpgId));
  QFile sig(gpgId + QStringLiteral(".sig"));
  QVERIFY(sig.open(QIODevice::ReadOnly));
  QCOMPARE(sig.readAll(), gpg.signature);
  QVERIFY2(!QFileInfo::exists(args.at(5)), "the scratch is gone");

  gpg.rc = 2;
  gpg.err = QStringLiteral("gpg: signing failed: No secret key\n");
  QString error;
  QVERIFY(!signer.sign(gpgId, kGpgId, &error));
  QCOMPARE(error, gpg.err);

  // gpg says it succeeded but wrote an empty signature: none is placed,
  // and the one there stays.
  gpg.rc = 0;
  gpg.signature.clear();
  QVERIFY(!signer.sign(gpgId, kGpgId, &error));
  QVERIFY2(error.contains(QStringLiteral("no signature")), qPrintable(error));
  QFile still(gpgId + QStringLiteral(".sig"));
  QVERIFY(still.open(QIODevice::ReadOnly));
  QCOMPARE(still.readAll(), QByteArrayLiteral("SIGNATURE"));
  still.close();

  // Bytes that do not survive the executor's UTF-8 round trip are not
  // signed as something else: gpg is not run, the signature stays.
  gpg.signature = QByteArrayLiteral("OTHER");
  const int calls = gpg.calls.size();
  QVERIFY(!signer.sign(gpgId, QByteArrayLiteral("ALICE\xff\n"), &error));
  QVERIFY2(error.contains(QStringLiteral("UTF-8")), qPrintable(error));
  QCOMPARE(gpg.calls.size(), calls);
  QFile kept(gpgId + QStringLiteral(".sig"));
  QVERIFY(kept.open(QIODevice::ReadOnly));
  QCOMPARE(kept.readAll(), QByteArrayLiteral("SIGNATURE"));
}

/**
 * @brief gpg made a signature, but not by a configured key (a gpg that fell
 *        back to another secret key, as `--default-key` does): it is not
 *        written, and a signature already there stays.
 */
void tst_gpgidsigner::signDoesNotWriteASignatureByAnotherKey() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QString gpgId = QDir(store.path()).filePath(QStringLiteral(".gpg-id"));
  const QString other =
      QStringLiteral("1111111111111111111111111111111111111111");
  FakeGpg gpg;
  gpg.verifyOut = validSig(other, other);
  const GpgIdSigner signer(QStringLiteral("gpg"), {kFpr}, gpg.exec());
  QString error;
  QVERIFY(!signer.sign(gpgId, kGpgId, &error));
  QVERIFY2(error.contains(QStringLiteral("does not verify")),
           qPrintable(error));
  QVERIFY2(!QFileInfo::exists(gpgId + QStringLiteral(".sig")),
           "a signature by another key is not placed");

  {
    QFile old(gpgId + QStringLiteral(".sig"));
    QVERIFY(old.open(QIODevice::WriteOnly));
    old.write("OLD");
  }
  // No VALIDSIG at all, or gpg failing the check, is no better.
  gpg.verifyOut = QStringLiteral("[GNUPG:] NEWSIG\n[GNUPG:] ERRSIG 1 2 3\n");
  QVERIFY(!signer.sign(gpgId, kGpgId, &error));
  QFile kept(gpgId + QStringLiteral(".sig"));
  QVERIFY(kept.open(QIODevice::ReadOnly));
  QCOMPARE(kept.readAll(), QByteArrayLiteral("OLD"));
}

/**
 * @brief gpg failing to sign (no secret key for --local-user) leaves no
 *        signature behind and checks nothing.
 */
void tst_gpgidsigner::signFailureLeavesNoSignature() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QString gpgId = QDir(store.path()).filePath(QStringLiteral(".gpg-id"));
  FakeGpg gpg;
  gpg.rc = 2;
  gpg.err = QStringLiteral("gpg: skipped \"%1\": No secret key\n"
                           "gpg: signing failed: No secret key\n")
                .arg(kFpr);
  gpg.verifyOut = validSig(kFpr, kFpr);
  const GpgIdSigner signer(QStringLiteral("gpg"), {kFpr}, gpg.exec());
  QString error;
  QVERIFY(!signer.sign(gpgId, kGpgId, &error));
  QCOMPARE(error, gpg.err);
  QVERIFY(!QFileInfo::exists(gpgId + QStringLiteral(".sig")));
  QCOMPARE(gpg.calls.size(), 1);
}

void tst_gpgidsigner::signPassesTheFilePathThroughTheWslTranslation() {
  // Executor::translatePathForWsl() only calls wslpath for a "wsl " gpg; for
  // any other gpg it hands back the cleaned path, which is what shows here.
  FakeGpg gpg;
  const GpgIdSigner signer(QStringLiteral("gpg"), {kPrimary}, gpg.exec());
  gpg.out = validSig(kFpr, kPrimary);
  const QString dir = QFileInfo(sigPath()).absolutePath();
  QVERIFY(signer.verify(kGpgId, dir + QStringLiteral("//.gpg-id.sig")));
  QCOMPARE(gpg.calls.last().args.mid(2),
           (QStringList{QStringLiteral("--"), sigPath(), QStringLiteral("-")}));
}

/**
 * @brief The signature goes next to the list through a staged file and a
 *        rename: a link a co-writer planted under the `.sig` name after the
 *        caller's check is replaced as an entry, and what it pointed at is
 *        untouched (gpg --output on that name would have written through).
 */
void tst_gpgidsigner::signReplacesALinkUnderTheSignatureNameAsAnEntry() {
#ifdef Q_OS_WIN
  QSKIP("creating a symlink needs a privilege a CI runner may lack");
#else
  QTemporaryDir store;
  QTemporaryDir outside;
  QVERIFY(store.isValid() && outside.isValid());
  const QString gpgId = QDir(store.path()).filePath(QStringLiteral(".gpg-id"));
  const QString victim = QDir(outside.path()).filePath(QStringLiteral("v"));
  {
    QFile f(victim);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("precious");
  }
  QVERIFY(QFile::link(victim, gpgId + QStringLiteral(".sig")));
  FakeGpg gpg;
  gpg.verifyOut = validSig(kFpr, kPrimary);
  const GpgIdSigner signer(QStringLiteral("gpg"), {kPrimary}, gpg.exec());
  QVERIFY(signer.sign(gpgId, kGpgId));
  QVERIFY(!QFileInfo(gpgId + QStringLiteral(".sig")).isSymLink());
  QFile sig(gpgId + QStringLiteral(".sig"));
  QVERIFY(sig.open(QIODevice::ReadOnly));
  QCOMPARE(sig.readAll(), gpg.signature);
  QFile v(victim);
  QVERIFY(v.open(QIODevice::ReadOnly));
  QCOMPARE(v.readAll(), QByteArrayLiteral("precious"));
#endif
}

void tst_gpgidsigner::verifyPassesArgsAndAcceptsEitherFingerprint() {
  FakeGpg gpg;
  gpg.out = validSig(kFpr, kPrimary);
  {
    const GpgIdSigner bySubkey(QStringLiteral("gpg"), {kFpr}, gpg.exec());
    QVERIFY(bySubkey.verify(kGpgId, sigPath()));
    // The signed data goes to gpg on stdin: the very bytes the caller holds,
    // not whatever is in the file by the time gpg opens it.
    QCOMPARE(gpg.calls.first().args,
             (QStringList{QStringLiteral("--verify"),
                          QStringLiteral("--status-fd=1"), QStringLiteral("--"),
                          sigPath(), QStringLiteral("-")}));
    QCOMPARE(gpg.calls.first().input, QString::fromUtf8(kGpgId));
  }
  {
    const GpgIdSigner byPrimary(
        QStringLiteral("gpg"), {QStringLiteral("OTHER"), kPrimary}, gpg.exec());
    QVERIFY2(byPrimary.verify(kGpgId, sigPath()),
             "the primary key's fingerprint is accepted too");
  }
}

void tst_gpgidsigner::verifyRejectsUnknownSignerAndGpgFailure() {
  FakeGpg gpg;
  gpg.out = validSig(kFpr, kPrimary);
  const GpgIdSigner signer(
      QStringLiteral("gpg"),
      {QStringLiteral("1111111111111111111111111111111111111111")}, gpg.exec());
  QVERIFY2(!signer.verify(kGpgId, sigPath()),
           "a valid signature by someone else is not good enough");

  const GpgIdSigner mine(QStringLiteral("gpg"), {kFpr}, gpg.exec());
  gpg.rc = 1; // BADSIG: gpg exits non-zero, whatever it prints
  QVERIFY(!mine.verify(kGpgId, sigPath()));
  gpg.rc = 0;
  gpg.out = QStringLiteral("[GNUPG:] NEWSIG\n[GNUPG:] ERRSIG 1 2 3\n");
  QVERIFY2(!mine.verify(kGpgId, sigPath()),
           "exit 0 without VALIDSIG is not a verification");
  // What gpg 2.5 prints, exit 0, for the configured key once revoked.
  gpg.out =
      QStringLiteral("[GNUPG:] NEWSIG\n"
                     "[GNUPG:] REVKEYSIG 89ABCDEF01234567 Signer <s@x>\n"
                     "[GNUPG:] VALIDSIG %1 2026-10-27 1793095590 0 4 0 22 10 "
                     "00 %2\n[GNUPG:] KEYREVOKED\n")
          .arg(kFpr, kPrimary);
  QVERIFY2(!mine.verify(kGpgId, sigPath()),
           "a revoked signing key vouches for nothing");
}

/**
 * @brief A .gpg-id or .gpg-id.sig that is a link is refused before gpg is
 *        asked: a validly signed pair replayed from elsewhere through two
 *        links is not this folder's list.
 */
void tst_gpgidsigner::linkedGpgIdOrSignatureIsNotVerified() {
#ifdef Q_OS_WIN
  QSKIP("uses symlinks");
#else
  QTemporaryDir dir;
  QTemporaryDir outside;
  QVERIFY(dir.isValid() && outside.isValid());
  const QString realId = outside.filePath(QStringLiteral(".gpg-id"));
  const QString realSig = outside.filePath(QStringLiteral(".gpg-id.sig"));
  for (const QString &path : {realId, realSig}) {
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(kGpgId);
  }
  FakeGpg gpg;
  gpg.out = validSig(kFpr, kPrimary);
  const GpgIdSigner signer(QStringLiteral("gpg"), {kFpr}, gpg.exec());
  QByteArray contents;

  // Both linked.
  const QString gpgId = dir.filePath(QStringLiteral(".gpg-id"));
  QVERIFY(QFile::link(realId, gpgId));
  QVERIFY(QFile::link(realSig, gpgId + QStringLiteral(".sig")));
  QVERIFY(!signer.verifyFile(gpgId, &contents));
  QVERIFY(contents.isEmpty());

  // Real list, linked signature.
  QVERIFY(QFile::remove(gpgId));
  {
    QFile f(gpgId);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(kGpgId);
  }
  QVERIFY(!signer.verifyFile(gpgId, &contents));
  QVERIFY2(gpg.calls.isEmpty(), "gpg is never asked about a linked pair");

  // Real pair: verified as before.
  QVERIFY(QFile::remove(gpgId + QStringLiteral(".sig")));
  {
    QFile f(gpgId + QStringLiteral(".sig"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("sig");
  }
  QVERIFY(signer.verifyFile(gpgId, &contents));
  QCOMPARE(gpg.calls.size(), 1);
#endif
}

/**
 * @brief A .gpg-id.sig that is no regular file of sensible size is refused
 *        before gpg sees its path: a FIFO would block gpg and the GUI
 *        thread waiting for it, a sparse file of gigabytes be read whole.
 */
void tst_gpgidsigner::verifyRefusesASignatureThatIsNoRegularFile() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  FakeGpg gpg;
  gpg.out = validSig(kFpr, kPrimary);
  const GpgIdSigner signer(QStringLiteral("gpg"), {kFpr}, gpg.exec());
  const QString big = dir.filePath(QStringLiteral("big.sig"));
  {
    QFile f(big);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QVERIFY(f.resize(GpgIdSigner::kMaxSignatureBytes + 1));
  }
  QVERIFY(!signer.verify(kGpgId, big));
#ifdef Q_OS_UNIX
  const QString fifo = dir.filePath(QStringLiteral("fifo.sig"));
  QCOMPARE(::mkfifo(QFile::encodeName(fifo).constData(), 0600), 0);
  QVERIFY(!signer.verify(kGpgId, fifo));
#endif
  QVERIFY(!signer.verify(kGpgId, dir.filePath(QStringLiteral("missing.sig"))));
  QVERIFY2(gpg.calls.isEmpty(), "gpg must not be given such a path");
}

void tst_gpgidsigner::verifyRefusesBytesThatAreNotUtf8() {
  // stdin reaches gpg as UTF-8 text; bytes that would change on the way
  // are not verified as something else, they are not verified.
  FakeGpg gpg;
  gpg.out = validSig(kFpr, kPrimary);
  const GpgIdSigner signer(QStringLiteral("gpg"), {kFpr}, gpg.exec());
  QVERIFY(!signer.verify(QByteArrayLiteral("ALICE\xff\n"), sigPath()));
  QVERIFY2(gpg.calls.isEmpty(), "gpg must not be asked about altered bytes");
  QVERIFY(signer.verify(QByteArrayLiteral("ALICE \xc3\xa9\n"), sigPath()));
}

/**
 * @brief A .gpg-id larger than any real list (a sparse file of gigabytes,
 * say) is refused before it is read whole, and so is one that is no
 * regular file: a FIFO would block the GUI. gpg is not asked.
 */
void tst_gpgidsigner::verifyFileReadsOnlyARegularFileOfBoundedSize() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  FakeGpg gpg;
  gpg.out = validSig(kFpr, kPrimary);
  const GpgIdSigner signer(QStringLiteral("gpg"), {kFpr}, gpg.exec());

  const QString big = dir.filePath(QStringLiteral("big.gpg-id"));
  {
    QFile f(big);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QVERIFY(f.resize(Util::kMaxGpgIdBytes + 1)); // sparse where it can be
  }
  QByteArray contents = "stale";
  QVERIFY(!signer.verifyFile(big, &contents));
  QVERIFY(contents.isEmpty());

#ifdef Q_OS_UNIX
  const QString fifo = dir.filePath(QStringLiteral("fifo.gpg-id"));
  QCOMPARE(::mkfifo(QFile::encodeName(fifo).constData(), 0600), 0);
  QVERIFY2(!signer.verifyFile(fifo, &contents), "a FIFO is no list");
#endif
  QVERIFY2(gpg.calls.isEmpty(), "nothing to verify was read");
}

void tst_gpgidsigner::verifyFileHandsBackTheBytesItVerified() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString gpgId = dir.filePath(QStringLiteral(".gpg-id"));
  for (const QString &name : {gpgId, gpgId + QStringLiteral(".sig")}) {
    QFile f(name);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(kGpgId);
  }
  FakeGpg gpg;
  gpg.out = validSig(kFpr, kPrimary);
  const GpgIdSigner signer(QStringLiteral("gpg"), {kFpr}, gpg.exec());
  QByteArray contents;
  QVERIFY(signer.verifyFile(gpgId, &contents));
  QCOMPARE(contents, kGpgId);
  QCOMPARE(gpg.calls.first().input, QString::fromUtf8(kGpgId));
  QCOMPARE(gpg.calls.first().args.mid(2),
           (QStringList{QStringLiteral("--"), gpgId + QStringLiteral(".sig"),
                        QStringLiteral("-")}));

  QByteArray none;
  QVERIFY2(!signer.verifyFile(dir.filePath(QStringLiteral("missing")), &none),
           "an unreadable .gpg-id cannot be verified");
  QVERIFY(none.isEmpty());
  QCOMPARE(gpg.calls.size(), 1);
}

void tst_gpgidsigner::validSigFingerprintsParsesStatusLines() {
  QCOMPARE(GpgIdSigner::validSigFingerprints(validSig(kFpr, kPrimary)),
           (QStringList{kFpr, kPrimary}));
  QCOMPARE(GpgIdSigner::validSigFingerprints(
               validSig(kFpr, kPrimary).replace('\n', "\r\n")),
           (QStringList{kFpr, kPrimary}));
  // A v5/v6 key signs with a 64-hex fingerprint; the primary may be either.
  const QString v5 = QString(64, QLatin1Char('A'));
  QCOMPARE(GpgIdSigner::validSigFingerprints(validSig(v5, kPrimary)),
           (QStringList{v5, kPrimary}));
  QCOMPARE(GpgIdSigner::validSigFingerprints(validSig(kFpr, v5)),
           (QStringList{kFpr, v5}));
  QVERIFY(GpgIdSigner::validSigFingerprints(QString()).isEmpty());
  QVERIFY(GpgIdSigner::validSigFingerprints(
              QStringLiteral("VALIDSIG %1 x %2").arg(kFpr, kPrimary))
              .isEmpty()); // not a status line

  // gpg prints VALIDSIG and exits 0 for a revoked signing key too; that
  // signature does not count. An expired one does, as gpg's "good
  // signature from expired key". The real status lines of gpg 2.5.
  const QString revoked =
      QStringLiteral("[GNUPG:] NEWSIG\n[GNUPG:] SIG_ID abc 2026-10-27 1\n"
                     "[GNUPG:] REVKEYSIG 89ABCDEF01234567 Signer <s@x>\n"
                     "[GNUPG:] VALIDSIG %1 2026-10-27 1793095590 0 4 0 22 10 "
                     "00 %2\n[GNUPG:] KEYREVOKED\n")
          .arg(kFpr, kPrimary);
  QVERIFY(GpgIdSigner::validSigFingerprints(revoked).isEmpty());
  QVERIFY(GpgIdSigner::validSigFingerprints(
              QString(revoked).remove(
                  QRegularExpression(QStringLiteral("^.*REVKEYSIG.*\n"),
                                     QRegularExpression::MultilineOption)))
              .isEmpty()); // KEYREVOKED alone says it as well
  const QString expired =
      QString(revoked)
          .replace(QStringLiteral("REVKEYSIG"), QStringLiteral("EXPKEYSIG"))
          .remove(QStringLiteral("[GNUPG:] KEYREVOKED\n"));
  QCOMPARE(GpgIdSigner::validSigFingerprints(expired),
           (QStringList{kFpr, kPrimary}));
  // A revoked key's signature after the one that counts does not spoil it.
  QCOMPARE(
      GpgIdSigner::validSigFingerprints(validSig(kFpr, kPrimary) + revoked),
      (QStringList{kFpr, kPrimary}));
}

void tst_gpgidsigner::defaultRunnerIsTheExecutor() {
  // No runner given: the real Executor runs the command. A gpg that does not
  // exist cannot have a secret key, and must not be reported as having one.
  const GpgIdSigner signer(QStringLiteral("/nonexistent/qtpass-gpg"), {kFpr});
  QVERIFY(!signer.haveSecretKey());
  QVERIFY(!signer.verify(kGpgId, QStringLiteral("/nonexistent/.gpg-id.sig")));
}

QTEST_MAIN(tst_gpgidsigner)
#include "tst_gpgidsigner.moc"
