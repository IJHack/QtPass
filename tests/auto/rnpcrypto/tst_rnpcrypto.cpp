// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
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

QTEST_MAIN(tst_rnpcrypto)
#include "tst_rnpcrypto.moc"
