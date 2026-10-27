// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "../../../src/rnpcrypto.h"

#include <type_traits>

// RNP keeps a pointer to the passphrase callback's context inside the
// object, so it must not move.
static_assert(!std::is_copy_constructible_v<RnpCrypto>);
static_assert(!std::is_move_constructible_v<RnpCrypto>);
static_assert(!std::is_move_assignable_v<RnpCrypto>);

/**
 * RnpCrypto against the real GnuPG on the system: what gpg encrypts, RNP must
 * decrypt (GnuPG 2.4 and later write OCB packets for its own default keys),
 * and what RNP encrypts, gpg must decrypt, for every recipient.
 */
class tst_rnpcrypto : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void cleanupTestCase();
  void decryptsWhatGpgEncrypted();
  void decryptsGpgOcbPackets();
  void gpgDecryptsWhatRnpEncryptedForEveryRecipient();
  void recipientsAsFingerprintKeyIdOrEmail();
  void passphraseProtectedKey();
  void unknownRecipientIsAnError();
  void emptyRecipientIsAnError();
  void shortKeyIdAndSpacedFingerprint();
  void ambiguousPartialUserIdIsAnError();
  void expiredKeyIsSkippedInAPartialMatch();
  void exactUserIdOnTwoKeysIsAnError();
  void keyWithNoUsableEncryptionSubkeyIsNoCandidate();
  void recipientFormsAsGpgReadsThem();
  void refusesAMessageWithoutIntegrityProtection();
  void plaintextLimitStopsADecompressionBomb();
  void blanksOnlyInAFingerprint();
  void anExactlyNamedUnusableKeySaysWhy();
  void wrongKeyCannotDecrypt();
  void gpgVerifiesWhatRnpSigned();
  void verifiesWhatGpgSignedAndNotATamperedCopy();
  void signNeedsTheNamedKeysSecret();
  void signUnlocksAProtectedKeyThroughTheProvider();
  void aRevokedSignerCountsForNothing();
  void anExpiredSignersOlderSignatureStillCounts();
  void listKeysSaysWhatEachKeyCanDo();
  void verifiesAnArmoredSignature();
  void onlyASecretSubkeyCanDecryptButNotSign();
  void aRevokedUserIdNamesNothing();
  void anAddressNamesThatAddressNotASubstring();
  void keyringRoundTripKeepsSecretKeysProtected();
  void saveRefusesAnUnprotectedSecretKey();
  void protectKeyThenSaveAndUseIt();
  void loadKeyringOfNothingIsEmpty();
  void loadKeyringReadsOnlyARegularFileOfBoundedSize();

private:
  struct Result {
    int exitCode = -1;
    QByteArray out;
    QByteArray err;
  };
  auto gpg(const QStringList &args, const QByteArray &input = {}) -> Result;
  auto generateKey(const QString &uid, const QString &passphrase) -> QString;
  auto exportKey(const QString &fpr, const QString &passphrase, bool secret)
      -> QByteArray;
  /// gpg's own detached signature by @p fpr over @p data, binary.
  auto gpgSign(const QString &fpr, const QByteArray &data,
               const QStringList &extra = {}) -> QByteArray;
  /// The fingerprints in gpg's VALIDSIG for @p signature over @p data.
  auto gpgValidSig(const QByteArray &data, const QByteArray &signature)
      -> QStringList;

  QString m_gpg;
  QTemporaryDir m_home;
  QString m_alice;
  QString m_bob;
  QString m_carol; // passphrase-protected
};

namespace {
auto isOcb(const QByteArray &packetList) -> bool {
  return packetList.contains(":ocb encrypted packet:") ||
         packetList.contains(":aead encrypted packet:");
}
} // namespace

auto tst_rnpcrypto::gpg(const QStringList &args, const QByteArray &input)
    -> Result {
  QProcess p;
  p.start(m_gpg,
          QStringList{QStringLiteral("--homedir"), m_home.path(),
                      QStringLiteral("--batch"), QStringLiteral("--no-tty")} +
              args);
  if (!p.waitForStarted(10000)) {
    return {};
  }
  if (!input.isNull()) {
    p.write(input);
  }
  p.closeWriteChannel();
  p.waitForFinished(60000);
  return {p.exitCode(), p.readAllStandardOutput(), p.readAllStandardError()};
}

auto tst_rnpcrypto::generateKey(const QString &uid, const QString &passphrase)
    -> QString {
  const Result gen =
      // --yes: a second key for a user ID that already has one (a rotation)
      // is otherwise refused in batch mode.
      gpg({QStringLiteral("--yes"), QStringLiteral("--status-fd"),
           QStringLiteral("1"), QStringLiteral("--pinentry-mode"),
           QStringLiteral("loopback"), QStringLiteral("--passphrase"),
           passphrase, QStringLiteral("--quick-gen-key"), uid,
           QStringLiteral("default"), QStringLiteral("default"),
           QStringLiteral("never")});
  if (gen.exitCode != 0) {
    qWarning() << gen.err;
    return {};
  }
  // "[GNUPG:] KEY_CREATED B <fingerprint>" names the key just made, also
  // when another key already has the same user ID.
  for (const QByteArray &line : gen.out.split('\n')) {
    if (line.startsWith("[GNUPG:] KEY_CREATED ")) {
      return QString::fromLatin1(line.split(' ').value(3)).trimmed();
    }
  }
  return {};
}

auto tst_rnpcrypto::exportKey(const QString &fpr, const QString &passphrase,
                              bool secret) -> QByteArray {
  QStringList args{QStringLiteral("--armor")};
  if (secret) {
    args << QStringLiteral("--pinentry-mode") << QStringLiteral("loopback")
         << QStringLiteral("--passphrase") << passphrase
         << QStringLiteral("--export-secret-keys");
  } else {
    args << QStringLiteral("--export");
  }
  args << fpr;
  return gpg(args).out;
}

void tst_rnpcrypto::initTestCase() {
  m_gpg = QStandardPaths::findExecutable(QStringLiteral("gpg"));
  if (m_gpg.isEmpty()) {
    m_gpg = QStandardPaths::findExecutable(QStringLiteral("gpg2"));
  }
  if (m_gpg.isEmpty()) {
    QSKIP("gpg not found; these tests compare RNP with GnuPG");
  }
  QVERIFY(m_home.isValid());
  QFile::setPermissions(m_home.path(),
                        QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
  QFile agentConf(m_home.filePath(QStringLiteral("gpg-agent.conf")));
  QVERIFY(agentConf.open(QIODevice::WriteOnly));
  agentConf.write("allow-loopback-pinentry\n");
  agentConf.close();
  qInfo().noquote()
      << gpg({QStringLiteral("--version")}).out.split('\n').first();

  m_alice = generateKey(QStringLiteral("Alice <alice@example.org>"), {});
  m_bob = generateKey(QStringLiteral("Bob <bob@example.org>"), {});
  m_carol = generateKey(QStringLiteral("Carol <carol@example.org>"),
                        QStringLiteral("correct horse"));
  QVERIFY2(!m_alice.isEmpty() && !m_bob.isEmpty() && !m_carol.isEmpty(),
           "gpg could not generate the test keys");
}

void tst_rnpcrypto::cleanupTestCase() {
  const QString gpgconf =
      QStandardPaths::findExecutable(QStringLiteral("gpgconf"));
  if (!gpgconf.isEmpty() && m_home.isValid()) {
    QProcess::execute(gpgconf,
                      {QStringLiteral("--homedir"), m_home.path(),
                       QStringLiteral("--kill"), QStringLiteral("all")});
  }
}

void tst_rnpcrypto::decryptsWhatGpgEncrypted() {
  const QByteArray entry = "hunter2\nlogin: alice\nurl: https://example.org\n";
  const Result enc =
      gpg({QStringLiteral("--trust-model"), QStringLiteral("always"),
           QStringLiteral("-e"), QStringLiteral("-r"), m_alice},
          entry);
  QCOMPARE(enc.exitCode, 0);
  const Result packets = gpg({QStringLiteral("--list-packets")}, enc.out);
  qInfo() << "gpg wrote"
          << (isOcb(packets.out) ? "an OCB (AEAD) packet" : "a SEIPD packet");

  RnpCrypto rnp;
  QVERIFY(rnp.isValid());
  QString error;
  QVERIFY2(!rnp.importKeys(exportKey(m_alice, {}, true), &error).isEmpty(),
           qPrintable(error));
  bool ok = false;
  const QByteArray plain = rnp.decrypt(enc.out, &error, &ok);
  QVERIFY2(ok, qPrintable(error));
  QCOMPARE(plain, entry);
}

// GnuPG 2.4 and later write OCB (AEAD) packets for keys that advertise it,
// so real stores hold them; force one so the test does not depend on the
// key defaults of the gpg at hand. gpg --list-packets calls it "ocb
// encrypted packet" (2.5) or "aead encrypted packet" (older).
void tst_rnpcrypto::decryptsGpgOcbPackets() {
  const QByteArray entry = "ocb entry\n";
  const Result enc =
      gpg({QStringLiteral("--trust-model"), QStringLiteral("always"),
           QStringLiteral("--force-ocb"), QStringLiteral("-e"),
           QStringLiteral("-r"), m_alice},
          entry);
  if (enc.exitCode != 0) {
    QSKIP("this gpg cannot write OCB packets (--force-ocb)");
  }
  const Result packets = gpg({QStringLiteral("--list-packets")}, enc.out);
  QVERIFY2(isOcb(packets.out), packets.out);

  RnpCrypto rnp;
  QString error;
  QVERIFY2(!rnp.importKeys(exportKey(m_alice, {}, true), &error).isEmpty(),
           qPrintable(error));
  bool ok = false;
  QCOMPARE(rnp.decrypt(enc.out, &error, &ok), entry);
  QVERIFY2(ok, qPrintable(error));
}

void tst_rnpcrypto::gpgDecryptsWhatRnpEncryptedForEveryRecipient() {
  RnpCrypto rnp;
  QString error;
  QVERIFY2(!rnp.importKeys(exportKey(m_alice, {}, false) +
                               exportKey(m_bob, {}, false),
                           &error)
                .isEmpty(),
           qPrintable(error));
  const QByteArray entry = "s3cr3t\n";
  const QByteArray cipher = rnp.encrypt(entry, {m_alice, m_bob}, &error);
  QVERIFY2(!cipher.isEmpty(), qPrintable(error));
  QVERIFY2(!cipher.startsWith("-----BEGIN"), "pass stores binary OpenPGP");

  // One public-key packet per recipient, and gpg (holding both secret
  // keys) reads it.
  const Result packets = gpg({QStringLiteral("--list-packets")}, cipher);
  QCOMPARE(packets.out.count(":pubkey enc packet:"), 2);
  const Result dec = gpg({QStringLiteral("-d")}, cipher);
  QCOMPARE(dec.exitCode, 0);
  QCOMPARE(dec.out, entry);

  // And each recipient on their own: a keyring holding only that one
  // secret key decrypts it, so both packets really are for both keys.
  for (const QString &recipient : {m_alice, m_bob}) {
    RnpCrypto own;
    QVERIFY(!own.importKeys(exportKey(recipient, {}, true)).isEmpty());
    bool ok = false;
    QCOMPARE(own.decrypt(cipher, &error, &ok), entry);
    QVERIFY2(ok, qPrintable(recipient + QStringLiteral(": ") + error));
  }
}

void tst_rnpcrypto::recipientsAsFingerprintKeyIdOrEmail() {
  RnpCrypto rnp;
  QVERIFY(!rnp.importKeys(exportKey(m_alice, {}, true)).isEmpty());
  const QStringList forms{m_alice, QStringLiteral("0x") + m_alice.right(16),
                          m_alice.right(16).toLower(),
                          QStringLiteral("alice@example.org")};
  for (const QString &recipient : forms) {
    QString error;
    const QByteArray cipher = rnp.encrypt("x", {recipient}, &error);
    QVERIFY2(!cipher.isEmpty(),
             qPrintable(recipient + QStringLiteral(": ") + error));
    bool ok = false;
    QCOMPARE(rnp.decrypt(cipher, &error, &ok), QByteArray("x"));
    QVERIFY2(ok, qPrintable(error));
  }
}

void tst_rnpcrypto::passphraseProtectedKey() {
  const Result enc =
      gpg({QStringLiteral("--trust-model"), QStringLiteral("always"),
           QStringLiteral("-e"), QStringLiteral("-r"), m_carol},
          QByteArray("carol's entry\n"));
  QCOMPARE(enc.exitCode, 0);

  RnpCrypto rnp;
  QString error;
  QVERIFY2(
      !rnp.importKeys(exportKey(m_carol, QStringLiteral("correct horse"), true),
                      &error)
           .isEmpty(),
      qPrintable(error));

  QStringList askedFor;
  rnp.setPassphraseProvider([&askedFor](const QString &keyId) {
    askedFor << keyId;
    return QStringLiteral("wrong");
  });
  bool ok = true;
  QVERIFY(rnp.decrypt(enc.out, &error, &ok).isEmpty());
  QVERIFY(!ok);
  QVERIFY(!askedFor.isEmpty());
  QCOMPARE(askedFor.first().size(), 16);

  rnp.setPassphraseProvider(
      [](const QString &) { return QStringLiteral("correct horse"); });
  QCOMPARE(rnp.decrypt(enc.out, &error, &ok), QByteArray("carol's entry\n"));
  QVERIFY2(ok, qPrintable(error));
}

void tst_rnpcrypto::unknownRecipientIsAnError() {
  RnpCrypto rnp;
  QVERIFY(!rnp.importKeys(exportKey(m_alice, {}, false)).isEmpty());
  QString error;
  QVERIFY(
      rnp.encrypt("x", {m_alice, QStringLiteral("nobody@example.org")}, &error)
          .isEmpty());
  QVERIFY2(error.contains(QLatin1String("nobody@example.org")),
           qPrintable(error));
  // The name comes through unmangled, also outside ASCII.
  const QString zoe = QStringLiteral("Zoë <zoe@example.org>");
  QVERIFY(rnp.encrypt("x", {zoe}, &error).isEmpty());
  QVERIFY2(error.contains(zoe), qPrintable(error));
}

// An empty .gpg-id line must not become "the first key in the keyring".
void tst_rnpcrypto::emptyRecipientIsAnError() {
  RnpCrypto rnp;
  QVERIFY(!rnp.importKeys(exportKey(m_alice, {}, false)).isEmpty());
  for (const QString &recipient :
       {QString(), QStringLiteral("   "), QStringLiteral("0x")}) {
    QString error;
    QVERIFY2(rnp.encrypt("x", {recipient}, &error).isEmpty(),
             qPrintable(QStringLiteral("encrypted to '%1'").arg(recipient)));
    QVERIFY(!error.isEmpty());
  }
}

void tst_rnpcrypto::shortKeyIdAndSpacedFingerprint() {
  RnpCrypto rnp;
  QVERIFY(!rnp.importKeys(exportKey(m_alice, {}, true)).isEmpty());
  QString spaced;
  for (int i = 0; i < m_alice.size(); i += 4) {
    spaced += m_alice.mid(i, 4) + QLatin1Char(' ');
  }
  for (const QString &recipient :
       {m_alice.right(8), QStringLiteral("0x") + m_alice.right(8).toLower(),
        spaced.trimmed()}) {
    QString error;
    const QByteArray cipher = rnp.encrypt("x", {recipient}, &error);
    QVERIFY2(!cipher.isEmpty(),
             qPrintable(recipient + QStringLiteral(": ") + error));
    bool ok = false;
    QCOMPARE(rnp.decrypt(cipher, &error, &ok), QByteArray("x"));
  }
}

// Part of a user ID that fits two people must not pick one of them.
void tst_rnpcrypto::ambiguousPartialUserIdIsAnError() {
  RnpCrypto rnp;
  QVERIFY(!rnp.importKeys(exportKey(m_alice, {}, false) +
                          exportKey(m_bob, {}, false))
               .isEmpty());
  QString error;
  QVERIFY(rnp.encrypt("x", {QStringLiteral("example.org")}, &error).isEmpty());
  QVERIFY2(error.contains(QLatin1String("more than one key")),
           qPrintable(error));
}

// gpg -r skips an expired key; so does a partial user ID here.
void tst_rnpcrypto::expiredKeyIsSkippedInAPartialMatch() {
  const Result gen = gpg(
      {QStringLiteral("--faked-system-time"),
       QStringLiteral("20200101T000000!"), QStringLiteral("--pinentry-mode"),
       QStringLiteral("loopback"), QStringLiteral("--passphrase"), QString(),
       QStringLiteral("--quick-gen-key"),
       QStringLiteral("Old Alice <alice@example.org>"),
       QStringLiteral("default"), QStringLiteral("default"),
       QStringLiteral("1d")});
  if (gen.exitCode != 0) {
    QSKIP("this gpg cannot make an expired key (--faked-system-time)");
  }
  // The expired key must really be in the keyring, or this would pass on
  // Alice's key alone.
  const QByteArray expired = exportKey(QStringLiteral("Old Alice"), {}, false);
  QVERIFY(!expired.isEmpty());
  RnpCrypto rnp;
  const QStringList imported =
      rnp.importKeys(exportKey(m_alice, {}, true) + expired);
  QCOMPARE(imported.size(), 2);
  QVERIFY(imported.at(0) != imported.at(1));
  QString error;
  const QByteArray cipher =
      rnp.encrypt("x", {QStringLiteral("alice@example.org")}, &error);
  QVERIFY2(!cipher.isEmpty(), qPrintable(error));
  bool ok = false;
  QCOMPARE(rnp.decrypt(cipher, &error, &ok), QByteArray("x"));
  QVERIFY2(ok, qPrintable(error));
}

// A key rotation can leave two keys with the very same user ID; naming it
// in full must not pick one of them either.
void tst_rnpcrypto::exactUserIdOnTwoKeysIsAnError() {
  const QString uid = QStringLiteral("Alice <alice@example.org>");
  const QString second = generateKey(uid, {});
  QVERIFY(!second.isEmpty());
  RnpCrypto rnp;
  QCOMPARE(rnp.importKeys(exportKey(m_alice, {}, false) +
                          exportKey(second, {}, false))
               .size(),
           2);
  QString error;
  QVERIFY(rnp.encrypt("x", {uid}, &error).isEmpty());
  QVERIFY2(error.contains(QLatin1String("more than one key")),
           qPrintable(error));
  // The fingerprint still names exactly one.
  QVERIFY2(!rnp.encrypt("x", {m_alice}, &error).isEmpty(), qPrintable(error));
}

// gpg -r skips a key whose encryption subkeys have all expired, even though
// its primary is valid; with a partial user ID that decides between two.
void tst_rnpcrypto::keyWithNoUsableEncryptionSubkeyIsNoCandidate() {
  // A valid primary whose only encryption subkey is revoked.
  const QString stale =
      generateKey(QStringLiteral("Rot Old <rot@example.net>"), {});
  QVERIFY(!stale.isEmpty());
  const Result revoke =
      gpg({QStringLiteral("--pinentry-mode"), QStringLiteral("loopback"),
           QStringLiteral("--passphrase"), QString(),
           QStringLiteral("--command-fd"), QStringLiteral("0"),
           QStringLiteral("--edit-key"), stale},
          QByteArray("key 1\nrevkey\ny\n0\n\ny\nsave\n"));
  const Result listed = gpg(
      {QStringLiteral("--with-colons"), QStringLiteral("--list-keys"), stale});
  if (revoke.exitCode != 0 || !listed.out.contains("\nsub:r:")) {
    QSKIP("could not revoke the subkey with this gpg");
  }
  const QString fresh =
      generateKey(QStringLiteral("Rot New <rot@example.net>"), {});
  QVERIFY(!fresh.isEmpty());

  RnpCrypto rnp;
  QCOMPARE(
      rnp.importKeys(exportKey(stale, {}, false) + exportKey(fresh, {}, true))
          .size(),
      2);
  QString error;
  const QByteArray cipher =
      rnp.encrypt("x", {QStringLiteral("rot@example.net")}, &error);
  QVERIFY2(!cipher.isEmpty(), qPrintable(error));
  bool ok = false;
  QCOMPARE(rnp.decrypt(cipher, &error, &ok), QByteArray("x"));
  QVERIFY2(ok, qPrintable(error));
}

void tst_rnpcrypto::recipientFormsAsGpgReadsThem() {
  RnpCrypto rnp;
  QVERIFY(!rnp.importKeys(exportKey(m_alice, {}, true)).isEmpty());
  QString error;
  // Blanks inside a fingerprint may be tabs too.
  QString tabbed;
  for (int i = 0; i < m_alice.size(); i += 4) {
    tabbed += m_alice.mid(i, 4) + QLatin1Char('\t');
  }
  QVERIFY2(!rnp.encrypt("x", {tabbed}, &error).isEmpty(), qPrintable(error));
  // 0x makes a key ID only when hex digits follow; gpg looks "0xAlice" up as
  // a user ID, and Alice's does not contain it.
  QVERIFY(rnp.encrypt("x", {QStringLiteral("0xAlice")}, &error).isEmpty());
  QVERIFY2(error.contains(QLatin1String("0xAlice")), qPrintable(error));
  // 64 hex digits name a v5/v6 fingerprint, not a user ID.
  const QString v6 = QString(64, QLatin1Char('A'));
  QVERIFY(rnp.encrypt("x", {v6}, &error).isEmpty());
  QCOMPARE(error, QStringLiteral("No public key for ") + v6);
}

// CFB without MDC (the kind EFAIL abuses) is refused, as GnuPG has done
// since 2.2.8; the same message with MDC decrypts.
void tst_rnpcrypto::refusesAMessageWithoutIntegrityProtection() {
  const QStringList symmetric{QStringLiteral("--pinentry-mode"),
                              QStringLiteral("loopback"),
                              QStringLiteral("--passphrase"),
                              QStringLiteral("pw"), QStringLiteral("-c")};
  // --rfc2440 writes the old Symmetrically Encrypted Data packet (tag 9),
  // without MDC; GnuPG 2.5 ignores --disable-mdc and writes tag 18 anyway.
  const Result bare =
      gpg(QStringList{QStringLiteral("--rfc2440"),
                      QStringLiteral("--cipher-algo"), QStringLiteral("AES")} +
              symmetric,
          QByteArray("no mdc\n"));
  const Result withMdc = gpg(symmetric, QByteArray("with mdc\n"));
  QCOMPARE(withMdc.exitCode, 0);
  if (bare.exitCode != 0 ||
      !gpg({QStringLiteral("--list-packets"), QStringLiteral("--pinentry-mode"),
            QStringLiteral("loopback"), QStringLiteral("--passphrase"),
            QStringLiteral("pw")},
           bare.out)
           .out.contains(" tag=9 ")) {
    QSKIP("this gpg cannot write a message without MDC");
  }
  RnpCrypto rnp;
  rnp.setPassphraseProvider(
      [](const QString &) { return QStringLiteral("pw"); });
  QString error;
  bool ok = true;
  QVERIFY(rnp.decrypt(bare.out, &error, &ok).isEmpty());
  QVERIFY(!ok);
  QVERIFY2(error.contains(QLatin1String("integrity")), qPrintable(error));

  QCOMPARE(rnp.decrypt(withMdc.out, &error, &ok), QByteArray("with mdc\n"));
  QVERIFY2(ok, qPrintable(error));
}

// A small compressed message must not expand into all of memory.
void tst_rnpcrypto::plaintextLimitStopsADecompressionBomb() {
  const QByteArray zeros(4 * 1024 * 1024, '\0');
  const Result enc =
      gpg({QStringLiteral("--trust-model"), QStringLiteral("always"),
           QStringLiteral("-z"), QStringLiteral("9"), QStringLiteral("-e"),
           QStringLiteral("-r"), m_alice},
          zeros);
  QCOMPARE(enc.exitCode, 0);
  QVERIFY2(enc.out.size() < 64 * 1024, "the message should be small");

  RnpCrypto rnp;
  QVERIFY(!rnp.importKeys(exportKey(m_alice, {}, true)).isEmpty());
  QString error;
  bool ok = false;
  QCOMPARE(rnp.decrypt(enc.out, &error, &ok).size(), zeros.size());
  QVERIFY2(ok, qPrintable(error));

  rnp.setMaxPlaintextBytes(1024 * 1024);
  ok = true;
  QVERIFY(rnp.decrypt(enc.out, &error, &ok).isEmpty());
  QVERIFY(!ok);
  QVERIFY(!error.isEmpty());

  // 0 (RNP's "no limit") and a negative value cannot switch the guard off.
  for (const qint64 bytes : {qint64{0}, qint64{-1}}) {
    rnp.setMaxPlaintextBytes(bytes);
    ok = true;
    QVERIFY(rnp.decrypt(enc.out, &error, &ok).isEmpty());
    QVERIFY(!ok);
  }

  // RNP keeps the limit in 32 bits: 4 GiB + 5 would become 5 there (and
  // 4 GiB exactly 0, no limit). Capped, it stays a large limit.
  rnp.setMaxPlaintextBytes((qint64{1} << 32) + 5);
  ok = false;
  QCOMPARE(rnp.decrypt(enc.out, &error, &ok).size(), zeros.size());
  QVERIFY2(ok, qPrintable(error));
}

// gpg allows blanks inside a fingerprint only: "dead beef" is a user ID,
// and a key ID with a blank in it is no key ID.
void tst_rnpcrypto::blanksOnlyInAFingerprint() {
  const QString beef =
      generateKey(QStringLiteral("dead beef <db@example.org>"), {});
  QVERIFY(!beef.isEmpty());
  RnpCrypto rnp;
  QVERIFY(
      !rnp.importKeys(exportKey(beef, {}, true) + exportKey(m_alice, {}, false))
           .isEmpty());
  QString error;
  const QByteArray cipher =
      rnp.encrypt("x", {QStringLiteral("dead beef")}, &error);
  QVERIFY2(!cipher.isEmpty(), qPrintable(error));
  bool ok = false;
  QCOMPARE(rnp.decrypt(cipher, &error, &ok), QByteArray("x"));

  const QString spacedKeyId =
      m_alice.right(16).left(8) + QLatin1Char(' ') + m_alice.right(8);
  QVERIFY(rnp.encrypt("x", {spacedKeyId}, &error).isEmpty());
  QVERIFY2(error.contains(QLatin1String("No usable public key")),
           qPrintable(error));
}

// A key named by fingerprint is tried alone, so when it cannot be used the
// error says so instead of RNP's generic one.
void tst_rnpcrypto::anExactlyNamedUnusableKeySaysWhy() {
  const Result gen = gpg(
      {QStringLiteral("--status-fd"), QStringLiteral("1"),
       QStringLiteral("--faked-system-time"),
       QStringLiteral("20200101T000000!"), QStringLiteral("--pinentry-mode"),
       QStringLiteral("loopback"), QStringLiteral("--passphrase"), QString(),
       QStringLiteral("--quick-gen-key"),
       QStringLiteral("Expired Erin <erin@example.org>"),
       QStringLiteral("default"), QStringLiteral("default"),
       QStringLiteral("1d")});
  QString erin;
  for (const QByteArray &line : gen.out.split('\n')) {
    if (line.startsWith("[GNUPG:] KEY_CREATED ")) {
      erin = QString::fromLatin1(line.split(' ').value(3)).trimmed();
    }
  }
  if (gen.exitCode != 0 || erin.isEmpty()) {
    QSKIP("this gpg cannot make an expired key (--faked-system-time)");
  }
  RnpCrypto rnp;
  QVERIFY(!rnp.importKeys(exportKey(erin, {}, false)).isEmpty());
  for (const QString &recipient : {erin, erin.right(16)}) {
    QString error;
    QVERIFY(rnp.encrypt("x", {recipient}, &error).isEmpty());
    QVERIFY2(error.contains(QLatin1String("cannot be encrypted to")),
             qPrintable(error));
  }
}

void tst_rnpcrypto::wrongKeyCannotDecrypt() {
  const Result enc =
      gpg({QStringLiteral("--trust-model"), QStringLiteral("always"),
           QStringLiteral("-e"), QStringLiteral("-r"), m_bob},
          QByteArray("for bob only\n"));
  QCOMPARE(enc.exitCode, 0);
  RnpCrypto rnp;
  QVERIFY(!rnp.importKeys(exportKey(m_alice, {}, true)).isEmpty());
  QString error;
  bool ok = true;
  QVERIFY(rnp.decrypt(enc.out, &error, &ok).isEmpty());
  QVERIFY(!ok);
  QVERIFY(!error.isEmpty());
}

auto tst_rnpcrypto::gpgSign(const QString &fpr, const QByteArray &data,
                            const QStringList &extra) -> QByteArray {
  return gpg(extra + QStringList{QStringLiteral("--pinentry-mode"),
                                 QStringLiteral("loopback"),
                                 QStringLiteral("--passphrase"), QString(),
                                 QStringLiteral("--local-user"), fpr,
                                 QStringLiteral("--detach-sign")},
             data)
      .out;
}

auto tst_rnpcrypto::gpgValidSig(const QByteArray &data,
                                const QByteArray &signature) -> QStringList {
  QTemporaryDir dir;
  if (!dir.isValid()) {
    return {};
  }
  QFile sig(dir.filePath(QStringLiteral("list.sig")));
  if (!sig.open(QIODevice::WriteOnly)) {
    return {};
  }
  sig.write(signature);
  sig.close();
  const Result r =
      gpg({QStringLiteral("--status-fd"), QStringLiteral("1"),
           QStringLiteral("--verify"), sig.fileName(), QStringLiteral("-")},
          data);
  for (const QByteArray &line : r.out.split('\n')) {
    if (line.startsWith("[GNUPG:] VALIDSIG ")) {
      const QList<QByteArray> f = line.trimmed().split(' ');
      return {QString::fromLatin1(f.value(2)), QString::fromLatin1(f.last())};
    }
  }
  return {};
}

namespace {
const QByteArray kList = QByteArrayLiteral(
    "# QtPass-GpgId-Generation: 1\nalice@example.org\nbob@example.org\n");
} // namespace

// A .gpg-id.sig RNP makes is one gpg (and so pass) accepts, by that key.
void tst_rnpcrypto::gpgVerifiesWhatRnpSigned() {
  RnpCrypto rnp;
  QString error;
  QVERIFY(!rnp.importKeys(exportKey(m_alice, {}, true), &error).isEmpty());
  const QByteArray sig = rnp.sign(kList, m_alice, &error);
  QVERIFY2(!sig.isEmpty(), qPrintable(error));
  QVERIFY2(!sig.startsWith("-----BEGIN"), "binary, as gpg --detach-sign");
  QCOMPARE(gpgValidSig(kList, sig), (QStringList{m_alice, m_alice}));
  // And RNP itself agrees.
  QCOMPARE(rnp.verify(kList, sig, &error), (QStringList{m_alice, m_alice}));
}

void tst_rnpcrypto::verifiesWhatGpgSignedAndNotATamperedCopy() {
  const QByteArray sig = gpgSign(m_alice, kList);
  QVERIFY(!sig.isEmpty());
  RnpCrypto rnp;
  QVERIFY(!rnp.importKeys(exportKey(m_alice, {}, false)).isEmpty());
  QString error;
  QCOMPARE(rnp.verify(kList, sig, &error), (QStringList{m_alice, m_alice}));

  QByteArray planted = kList;
  planted.append("mallory@example.org\n");
  QVERIFY(rnp.verify(planted, sig, &error).isEmpty());
  QVERIFY(!error.isEmpty());

  // A signature by a key not in the keyring is no signature here.
  RnpCrypto stranger;
  QVERIFY(!stranger.importKeys(exportKey(m_bob, {}, false)).isEmpty());
  QVERIFY(stranger.verify(kList, sig, &error).isEmpty());
}

// As gpg --local-user: the key named, with its secret here, or nothing;
// never another key that could sign.
void tst_rnpcrypto::signNeedsTheNamedKeysSecret() {
  RnpCrypto rnp;
  QVERIFY(!rnp.importKeys(exportKey(m_alice, {}, true) +
                          exportKey(m_bob, {}, false))
               .isEmpty());
  QString error;
  QVERIFY(rnp.sign(kList, m_bob, &error).isEmpty());
  QVERIFY2(error.contains(m_bob), qPrintable(error));
  QVERIFY(
      rnp.sign(kList, QStringLiteral("nobody@example.org"), &error).isEmpty());
  // By e-mail address, as a signing key setting may name it.
  const QByteArray sig =
      rnp.sign(kList, QStringLiteral("alice@example.org"), &error);
  QVERIFY2(!sig.isEmpty(), qPrintable(error));
  QCOMPARE(gpgValidSig(kList, sig).value(1), m_alice);
}

void tst_rnpcrypto::signUnlocksAProtectedKeyThroughTheProvider() {
  RnpCrypto rnp;
  QVERIFY(
      !rnp.importKeys(exportKey(m_carol, QStringLiteral("correct horse"), true))
           .isEmpty());
  QString error;
  rnp.setPassphraseProvider(
      [](const QString &) { return QStringLiteral("wrong"); });
  QVERIFY(rnp.sign(kList, m_carol, &error).isEmpty());
  QVERIFY(!error.isEmpty());
  rnp.setPassphraseProvider(
      [](const QString &) { return QStringLiteral("correct horse"); });
  const QByteArray sig = rnp.sign(kList, m_carol, &error);
  QVERIFY2(!sig.isEmpty(), qPrintable(error));
  QCOMPARE(gpgValidSig(kList, sig).value(1), m_carol);
}

// As GpgIdSigner since #2059: a signing key revoked (after it leaked, say)
// vouches for nothing, also for a signature made before the revocation.
void tst_rnpcrypto::aRevokedSignerCountsForNothing() {
  const QString dave =
      generateKey(QStringLiteral("Dave <dave@example.org>"), {});
  QVERIFY(!dave.isEmpty());
  const QByteArray sig = gpgSign(dave, kList);
  RnpCrypto rnp;
  QVERIFY(!rnp.importKeys(exportKey(dave, {}, false)).isEmpty());
  QString error;
  QCOMPARE(rnp.verify(kList, sig, &error), (QStringList{dave, dave}));

  // Revoked in GnuPG, and the key as exported now (carrying the revocation)
  // imported again, as a refreshed key would be.
  QFile rev(
      m_home.filePath(QStringLiteral("openpgp-revocs.d/%1.rev").arg(dave)));
  QVERIFY(rev.open(QIODevice::ReadOnly));
  QByteArray revocation = rev.readAll();
  // gpg guards the certificate with a leading ':' on its armor line.
  revocation.replace("\n:-----BEGIN", "\n-----BEGIN");
  QCOMPARE(gpg({QStringLiteral("--import")}, revocation).exitCode, 0);
  QVERIFY2(!rnp.importKeys(exportKey(dave, {}, false), &error).isEmpty(),
           qPrintable(error));
  const QList<RnpCrypto::KeyInfo> keys = rnp.listKeys();
  QVERIFY(keys.size() == 1 && keys.first().revoked);
  // RNP already reports such a signature invalid (stricter than gpg's
  // VALIDSIG); RnpCrypto::verify() checks the revocation itself as well.
  QVERIFY(rnp.verify(kList, sig, &error).isEmpty());
  QVERIFY(!error.isEmpty());
}

// gpg calls a signature by a key that has since expired a good one (and
// so does GpgIdSigner): expiry is not compromise.
void tst_rnpcrypto::anExpiredSignersOlderSignatureStillCounts() {
  const QStringList past{QStringLiteral("--faked-system-time"),
                         QStringLiteral("20250101T000000!")};
  const Result gen =
      gpg(past + QStringList{
                     QStringLiteral("--yes"), QStringLiteral("--status-fd"),
                     QStringLiteral("1"), QStringLiteral("--pinentry-mode"),
                     QStringLiteral("loopback"), QStringLiteral("--passphrase"),
                     QString(), QStringLiteral("--quick-gen-key"),
                     QStringLiteral("Erin <erin@example.org>"),
                     QStringLiteral("default"), QStringLiteral("default"),
                     QStringLiteral("1d")});
  QString erin;
  for (const QByteArray &line : gen.out.split('\n')) {
    if (line.startsWith("[GNUPG:] KEY_CREATED ")) {
      erin = QString::fromLatin1(line.split(' ').value(3)).trimmed();
    }
  }
  QVERIFY2(!erin.isEmpty(), gen.err.constData());
  const QByteArray sig = gpgSign(erin, kList, past);
  QVERIFY(!sig.isEmpty());
  RnpCrypto rnp;
  QVERIFY(!rnp.importKeys(exportKey(erin, {}, false)).isEmpty());
  QVERIFY(rnp.listKeys().first().expired);
  QString error;
  QCOMPARE(rnp.verify(kList, sig, &error), (QStringList{erin, erin}));
}

void tst_rnpcrypto::listKeysSaysWhatEachKeyCanDo() {
  RnpCrypto rnp;
  QVERIFY(!rnp.importKeys(exportKey(m_alice, {}, true) +
                          exportKey(m_bob, {}, false))
               .isEmpty());
  const QList<RnpCrypto::KeyInfo> keys = rnp.listKeys();
  QCOMPARE(keys.size(), 2);
  for (const RnpCrypto::KeyInfo &key : keys) {
    const bool alice = key.fingerprint == m_alice;
    QVERIFY(alice || key.fingerprint == m_bob);
    QCOMPARE(key.keyId, key.fingerprint.right(16));
    QCOMPARE(key.userIds,
             QStringList{alice ? QStringLiteral("Alice <alice@example.org>")
                               : QStringLiteral("Bob <bob@example.org>")});
    QVERIFY(key.created.isValid());
    QVERIFY(!key.expires.isValid()); // made with "never"
    QVERIFY(!key.revoked && !key.expired);
    QCOMPARE(key.hasSecret, alice);
    QVERIFY(key.canEncrypt);
    QCOMPARE(key.canSign, alice);
  }
  const QList<RnpCrypto::KeyInfo> secret = rnp.listKeys(true);
  QCOMPARE(secret.size(), 1);
  QCOMPARE(secret.first().fingerprint, m_alice);
}

// verify() takes an armored signature as well as a binary one.
void tst_rnpcrypto::verifiesAnArmoredSignature() {
  const QByteArray sig = gpgSign(m_alice, kList, {QStringLiteral("--armor")});
  QVERIFY(sig.startsWith("-----BEGIN PGP SIGNATURE-----"));
  RnpCrypto rnp;
  QVERIFY(!rnp.importKeys(exportKey(m_alice, {}, false)).isEmpty());
  QString error;
  QCOMPARE(rnp.verify(kList, sig, &error), (QStringList{m_alice, m_alice}));
}

// gpg --export-secret-subkeys leaves the primary's secret at home: the key
// has a secret part here (it decrypts), but nothing that signs.
void tst_rnpcrypto::onlyASecretSubkeyCanDecryptButNotSign() {
  const Result subkeys =
      gpg({QStringLiteral("--armor"), QStringLiteral("--pinentry-mode"),
           QStringLiteral("loopback"), QStringLiteral("--passphrase"),
           QString(), QStringLiteral("--export-secret-subkeys"), m_alice});
  QCOMPARE(subkeys.exitCode, 0);
  RnpCrypto rnp;
  QString error;
  QVERIFY2(!rnp.importKeys(subkeys.out, &error).isEmpty(), qPrintable(error));
  const QList<RnpCrypto::KeyInfo> keys = rnp.listKeys(true);
  QCOMPARE(keys.size(), 1);
  QVERIFY(keys.first().hasSecret);
  QVERIFY(keys.first().canEncrypt);
  QVERIFY2(!keys.first().canSign, "the primary, which signs, is not here");
  QVERIFY(rnp.sign(kList, m_alice, &error).isEmpty());

  const QByteArray entry = "subkey entry\n";
  const Result enc =
      gpg({QStringLiteral("--trust-model"), QStringLiteral("always"),
           QStringLiteral("-e"), QStringLiteral("-r"), m_alice},
          entry);
  bool ok = false;
  QCOMPARE(rnp.decrypt(enc.out, &error, &ok), entry);
  QVERIFY2(ok, qPrintable(error));
}

// A revoked user ID (an address given up) is no longer the key's: not
// listed, and it names no key.
void tst_rnpcrypto::aRevokedUserIdNamesNothing() {
  const QString frank =
      generateKey(QStringLiteral("Frank <frank@example.org>"), {});
  QVERIFY(!frank.isEmpty());
  const QString old = QStringLiteral("Frank <old@example.org>");
  QCOMPARE(gpg({QStringLiteral("--quick-add-uid"), frank, old}).exitCode, 0);
  QCOMPARE(gpg({QStringLiteral("--quick-revoke-uid"), frank, old}).exitCode, 0);
  RnpCrypto rnp;
  QVERIFY(!rnp.importKeys(exportKey(frank, {}, true)).isEmpty());
  const QList<RnpCrypto::KeyInfo> keys = rnp.listKeys();
  QCOMPARE(keys.size(), 1);
  QCOMPARE(keys.first().userIds,
           QStringList{QStringLiteral("Frank <frank@example.org>")});
  QString error;
  QVERIFY(
      rnp.encrypt("x", {QStringLiteral("old@example.org")}, &error).isEmpty());
  QVERIFY(rnp.sign(kList, QStringLiteral("Frank <old@example.org>"), &error)
              .isEmpty());
  QVERIFY(!rnp.encrypt("x", {QStringLiteral("frank@example.org")}, &error)
               .isEmpty());
}

// An e-mail address names the key with that address, in any case (the
// signing key setting comes in upper case), not one whose address merely
// contains it: alice@ is not malice@.
void tst_rnpcrypto::anAddressNamesThatAddressNotASubstring() {
  const QString malice =
      generateKey(QStringLiteral("Mallory <malice@example.org>"), {});
  QVERIFY(!malice.isEmpty());
  QString error;
  {
    RnpCrypto both;
    QVERIFY(!both.importKeys(exportKey(m_alice, {}, true) +
                             exportKey(malice, {}, true))
                 .isEmpty());
    const QByteArray sig =
        both.sign(kList, QStringLiteral("ALICE@EXAMPLE.ORG"), &error);
    QVERIFY2(!sig.isEmpty(), qPrintable(error));
    QCOMPARE(gpgValidSig(kList, sig).value(1), m_alice);
    QVERIFY2(!both.encrypt("x", {QStringLiteral("<alice@example.org>")}, &error)
                  .isEmpty(),
             qPrintable(error));
  }
  {
    // Only Mallory's secret key is here: naming Alice must not sign with it.
    RnpCrypto onlyMallory;
    QVERIFY(!onlyMallory
                 .importKeys(exportKey(m_alice, {}, false) +
                             exportKey(malice, {}, true))
                 .isEmpty());
    QVERIFY(onlyMallory.sign(kList, QStringLiteral("alice@example.org"), &error)
                .isEmpty());
  }
}

namespace {
auto modeOf(const QString &path) -> QFile::Permissions {
  return QFileInfo(path).permissions() &
         (QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
          QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup |
          QFile::ReadOther | QFile::WriteOther | QFile::ExeOther);
}
} // namespace

// What saveKeyring() writes, loadKeyring() reads back: the same keys, the
// secret ones still protected by their own passphrase, the files and the
// folder owner-only.
void tst_rnpcrypto::keyringRoundTripKeepsSecretKeysProtected() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  const QString dir = tmp.filePath(QStringLiteral("keyring"));
  QString error;
  {
    RnpCrypto rnp;
    QVERIFY(!rnp.importKeys(
                    exportKey(m_carol, QStringLiteral("correct horse"), true) +
                    exportKey(m_bob, {}, false))
                 .isEmpty());
    QVERIFY2(rnp.saveKeyring(dir, false, &error), qPrintable(error));
  }
#ifdef Q_OS_UNIX
  const auto owner = QFile::ReadOwner | QFile::WriteOwner;
  QCOMPARE(modeOf(dir), owner | QFile::ExeOwner);
  QCOMPARE(modeOf(QDir(dir).filePath(QStringLiteral("pubring.gpg"))), owner);
  QCOMPARE(modeOf(QDir(dir).filePath(QStringLiteral("secring.gpg"))), owner);
#endif
  RnpCrypto loaded;
  QVERIFY2(loaded.loadKeyring(dir, &error), qPrintable(error));
  const QList<RnpCrypto::KeyInfo> keys = loaded.listKeys();
  QCOMPARE(keys.size(), 2);
  const QList<RnpCrypto::KeyInfo> secret = loaded.listKeys(true);
  QCOMPARE(secret.size(), 1);
  QCOMPARE(secret.first().fingerprint, m_carol);
  QVERIFY(secret.first().secretProtected);

  const QByteArray entry = "kept on disk\n";
  const Result enc =
      gpg({QStringLiteral("--trust-model"), QStringLiteral("always"),
           QStringLiteral("-e"), QStringLiteral("-r"), m_carol},
          entry);
  bool ok = true;
  loaded.setPassphraseProvider(
      [](const QString &) { return QStringLiteral("wrong"); });
  QVERIFY(loaded.decrypt(enc.out, &error, &ok).isEmpty());
  QVERIFY2(!ok, "the key on disk is still protected by its passphrase");
  loaded.setPassphraseProvider(
      [](const QString &) { return QStringLiteral("correct horse"); });
  QCOMPARE(loaded.decrypt(enc.out, &error, &ok), entry);
  QVERIFY2(ok, qPrintable(error));
}

// A secret key reaches the disk only protected by its own passphrase,
// unless the user agreed to it explicitly: refused, and nothing written.
void tst_rnpcrypto::saveRefusesAnUnprotectedSecretKey() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  const QString dir = tmp.filePath(QStringLiteral("keyring"));
  RnpCrypto rnp;
  QVERIFY(!rnp.importKeys(exportKey(m_alice, {}, true)).isEmpty());
  QVERIFY(!rnp.listKeys(true).first().secretProtected);
  QString error;
  QVERIFY(!rnp.saveKeyring(dir, false, &error));
  QVERIFY2(error.contains(m_alice), qPrintable(error));
  QVERIFY2(!QFileInfo::exists(dir), "nothing is written");
  QVERIFY2(rnp.saveKeyring(dir, true, &error), qPrintable(error));
  const QString secring = QDir(dir).filePath(QStringLiteral("secring.gpg"));
  QVERIFY(QFileInfo(secring).size() > 0);

  // Saved again without the secret key: no stale secret file stays behind.
  RnpCrypto publicOnly;
  QVERIFY(!publicOnly.importKeys(exportKey(m_alice, {}, false)).isEmpty());
  QVERIFY2(publicOnly.saveKeyring(dir, false, &error), qPrintable(error));
  QCOMPARE(QFileInfo(secring).size(), qint64{0});
  RnpCrypto reloaded;
  QVERIFY2(reloaded.loadKeyring(dir, &error), qPrintable(error));
  QCOMPARE(reloaded.listKeys().size(), 1);
  QVERIFY(reloaded.listKeys(true).isEmpty());
}

void tst_rnpcrypto::protectKeyThenSaveAndUseIt() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  const QString dir = tmp.filePath(QStringLiteral("keyring"));
  QString error;
  {
    RnpCrypto rnp;
    QVERIFY(!rnp.importKeys(exportKey(m_alice, {}, true)).isEmpty());
    QVERIFY(!rnp.protectKey(m_alice, QString(), &error));
    QVERIFY(!rnp.protectKey(m_bob, QStringLiteral("pw"), &error));
    QVERIFY2(rnp.protectKey(m_alice, QStringLiteral("pw"), &error),
             qPrintable(error));
    QVERIFY(rnp.listKeys(true).first().secretProtected);
    QVERIFY2(!rnp.protectKey(m_alice, QStringLiteral("other"), &error),
             "a passphrase is not changed here");
    QVERIFY2(rnp.saveKeyring(dir, false, &error), qPrintable(error));
  }
  RnpCrypto loaded;
  QVERIFY2(loaded.loadKeyring(dir, &error), qPrintable(error));
  loaded.setPassphraseProvider(
      [](const QString &) { return QStringLiteral("wrong"); });
  QVERIFY(loaded.sign(kList, m_alice, &error).isEmpty());
  loaded.setPassphraseProvider(
      [](const QString &) { return QStringLiteral("pw"); });
  const QByteArray sig = loaded.sign(kList, m_alice, &error);
  QVERIFY2(!sig.isEmpty(), qPrintable(error));
  QCOMPARE(gpgValidSig(kList, sig).value(1), m_alice);
}

void tst_rnpcrypto::loadKeyringOfNothingIsEmpty() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  RnpCrypto rnp;
  QString error;
  QVERIFY2(rnp.loadKeyring(tmp.filePath(QStringLiteral("none")), &error),
           qPrintable(error));
  QVERIFY(rnp.listKeys().isEmpty());
}

// As the .gpg-id readers: a regular file, not through a link, and bounded.
void tst_rnpcrypto::loadKeyringReadsOnlyARegularFileOfBoundedSize() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  QString error;
  {
    QFile big(tmp.filePath(QStringLiteral("pubring.gpg")));
    QVERIFY(big.open(QIODevice::WriteOnly));
    QVERIFY(big.resize(RnpCrypto::kMaxKeyringBytes + 1));
  }
  RnpCrypto rnp;
  QVERIFY(!rnp.loadKeyring(tmp.path(), &error));
  QVERIFY2(error.contains(QStringLiteral("pubring.gpg")), qPrintable(error));
#ifdef Q_OS_UNIX
  QTemporaryDir linked;
  QVERIFY(linked.isValid());
  {
    RnpCrypto source;
    QVERIFY(!source.importKeys(exportKey(m_bob, {}, false)).isEmpty());
    QVERIFY(source.saveKeyring(tmp.filePath(QStringLiteral("real")), false));
  }
  QVERIFY(QFile::link(tmp.filePath(QStringLiteral("real/pubring.gpg")),
                      linked.filePath(QStringLiteral("pubring.gpg"))));
  RnpCrypto viaLink;
  QVERIFY2(!viaLink.loadKeyring(linked.path(), &error),
           "a keyring file behind a link is not read");
#endif
}

QTEST_MAIN(tst_rnpcrypto)
#include "tst_rnpcrypto.moc"
