// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "../../../src/base32.h"

class tst_base32 : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void decodeVectors_data();
  void decodeVectors();
  void decodeRejectsBadInput_data();
  void decodeRejectsBadInput();
  void encodeVectors_data();
  void encodeVectors();
  void encodeDecodeRoundTrip_data();
  void encodeDecodeRoundTrip();
  void encodeDoesNotSignExtendHighBytes();
  void addPaddingPads_data();
  void addPaddingPads();
  void removePaddingIsInverseOfAddPadding();
  void sanitizeInputAcceptsHumanInput_data();
  void sanitizeInputAcceptsHumanInput();
  void sanitizeInputMapsAmbiguousDigits();
  void sanitizeInputDropsEmbeddedNul();
};

/**
 * @brief RFC 4648 decode vectors, plus the case-folding extras KeePassXC
 * carries in tests/TestBase32.cpp.
 */
void tst_base32::decodeVectors_data() {
  QTest::addColumn<QByteArray>("encoded");
  QTest::addColumn<QByteArray>("decoded");

  QTest::newRow("hello world")
      << QByteArray("JBSWY3DPEB3W64TMMQXC4LQ=") << QByteArray("Hello world...");
  QTest::newRow("rfc6238 seed")
      << QByteArray("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ")
      << QByteArray("12345678901234567890");
  QTest::newRow("rfc6238 seed lowercase")
      << QByteArray("gezdgnbvgy3tqojqgezdgnbvgy3tqojq")
      << QByteArray("12345678901234567890");
  QTest::newRow("rfc6238 seed mixed case")
      << QByteArray("GeZdGnBvGy3TqOjQgEzDgNbVgY3TqOjQ")
      << QByteArray("12345678901234567890");
  QTest::newRow("test") << QByteArray("ORSXG5A=") << QByteArray("test");
  QTest::newRow("underscores") << QByteArray("L5PV6===") << QByteArray("___");
  QTest::newRow("foo bar") << QByteArray("MZXW6IDCMFZA====")
                           << QByteArray("foo bar");
  QTest::newRow("f") << QByteArray("MY======") << QByteArray("f");
  QTest::newRow("fo") << QByteArray("MZXQ====") << QByteArray("fo");
  QTest::newRow("foo") << QByteArray("MZXW6===") << QByteArray("foo");
  QTest::newRow("foob") << QByteArray("MZXW6YQ=") << QByteArray("foob");
  QTest::newRow("fooba") << QByteArray("MZXW6YTB") << QByteArray("fooba");
  QTest::newRow("foobar") << QByteArray("MZXW6YTBOI======")
                          << QByteArray("foobar");
  QTest::newRow("at sign") << QByteArray("IA======") << QByteArray("@");
}

void tst_base32::decodeVectors() {
  QFETCH(QByteArray, encoded);
  QFETCH(QByteArray, decoded);
  QCOMPARE(Base32::decode(encoded), decoded);
}

void tst_base32::decodeRejectsBadInput_data() {
  QTest::addColumn<QByteArray>("encoded");

  QTest::newRow("empty") << QByteArray();
  QTest::newRow("missing padding") << QByteArray("MZXW6YTBOI=====");
  QTest::newRow("illegal leading digit") << QByteArray("1MZXW6YTBOI=====");
  QTest::newRow("illegal character") << QByteArray("MZXW6YT!");
  QTest::newRow("embedded space") << QByteArray("MZXW 6YTB");
  QTest::newRow("not a multiple of eight") << QByteArray("MZXW6YT");
  // Two and five trailing pads cannot be produced by a base32 encoder;
  // KeePassXC silently returned wrong-length data for these.
  QTest::newRow("two pads") << QByteArray("MZXW6Y==");
  QTest::newRow("five pads") << QByteArray("MZX=====");
  // '=' before the trailing run is malformed. countPadding() used to count
  // every '=' in the last few positions, so these slipped through and produced
  // wrong-length data instead of an error.
  QTest::newRow("interior pad") << QByteArray("AA======AAAAAAAA");
  QTest::newRow("interior pad short") << QByteArray("A=A=A=A=");
}

void tst_base32::decodeRejectsBadInput() {
  QFETCH(QByteArray, encoded);
  QVERIFY2(Base32::decode(encoded).isEmpty(),
           "invalid base32 must decode to an empty QByteArray");
}

void tst_base32::encodeVectors_data() {
  QTest::addColumn<QByteArray>("decoded");
  QTest::addColumn<QByteArray>("encoded");

  QTest::newRow("empty") << QByteArray() << QByteArray();
  QTest::newRow("f") << QByteArray("f") << QByteArray("MY======");
  QTest::newRow("fo") << QByteArray("fo") << QByteArray("MZXQ====");
  QTest::newRow("foo") << QByteArray("foo") << QByteArray("MZXW6===");
  QTest::newRow("foob") << QByteArray("foob") << QByteArray("MZXW6YQ=");
  QTest::newRow("fooba") << QByteArray("fooba") << QByteArray("MZXW6YTB");
  QTest::newRow("foobar") << QByteArray("foobar")
                          << QByteArray("MZXW6YTBOI======");
  QTest::newRow("rfc6238 seed")
      << QByteArray("12345678901234567890")
      << QByteArray("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ");
}

void tst_base32::encodeVectors() {
  QFETCH(QByteArray, decoded);
  QFETCH(QByteArray, encoded);
  QCOMPARE(Base32::encode(decoded), encoded);
}

void tst_base32::encodeDecodeRoundTrip_data() {
  QTest::addColumn<QByteArray>("data");

  for (int length = 1; length <= 10; ++length) {
    QByteArray data(length, Qt::Uninitialized);
    for (int i = 0; i < length; ++i) {
      // Deterministic spread over the whole byte range, including >= 0x80.
      data[i] = static_cast<char>((i * 37 + length * 91) & 0xFF);
    }
    QTest::newRow(qPrintable(QStringLiteral("length %1").arg(length))) << data;
  }
}

void tst_base32::encodeDecodeRoundTrip() {
  QFETCH(QByteArray, data);
  QCOMPARE(Base32::decode(Base32::encode(data)), data);
}

/**
 * @brief QByteArray elements are signed char. KeePassXC widened them directly,
 * so any byte >= 0x80 sign-extended and corrupted the neighbouring bytes of
 * the 40-bit quantum.
 */
void tst_base32::encodeDoesNotSignExtendHighBytes() {
  const QByteArray highBytes =
      QByteArray::fromHex(QByteArrayLiteral("80ff0102037f8081"));
  QCOMPARE(Base32::decode(Base32::encode(highBytes)), highBytes);

  // A single high byte in the middle of a full quantum is the case that
  // corrupts its predecessor.
  const QByteArray middle =
      QByteArray::fromHex(QByteArrayLiteral("0102800304"));
  QCOMPARE(Base32::decode(Base32::encode(middle)), middle);
}

void tst_base32::addPaddingPads_data() {
  QTest::addColumn<QByteArray>("input");
  QTest::addColumn<QByteArray>("expected");

  QTest::newRow("empty") << QByteArray() << QByteArray();
  QTest::newRow("already aligned")
      << QByteArray("MZXW6YTB") << QByteArray("MZXW6YTB");
  QTest::newRow("two chars") << QByteArray("MY") << QByteArray("MY======");
  QTest::newRow("four chars") << QByteArray("MZXQ") << QByteArray("MZXQ====");
  QTest::newRow("five chars") << QByteArray("MZXW6") << QByteArray("MZXW6===");
  QTest::newRow("seven chars")
      << QByteArray("MZXW6YQ") << QByteArray("MZXW6YQ=");
  // Remainders of 1, 3 and 6 are impossible for base32 and are left alone.
  QTest::newRow("remainder one") << QByteArray("M") << QByteArray("M");
  QTest::newRow("remainder three") << QByteArray("MZX") << QByteArray("MZX");
  QTest::newRow("remainder six")
      << QByteArray("MZXW6Y") << QByteArray("MZXW6Y");
}

void tst_base32::addPaddingPads() {
  QFETCH(QByteArray, input);
  QFETCH(QByteArray, expected);
  QCOMPARE(Base32::addPadding(input), expected);
}

void tst_base32::removePaddingIsInverseOfAddPadding() {
  const QList<QByteArray> unpadded = {
      QByteArrayLiteral("MY"), QByteArrayLiteral("MZXQ"),
      QByteArrayLiteral("MZXW6"), QByteArrayLiteral("MZXW6YQ"),
      QByteArrayLiteral("MZXW6YTB")};
  for (const QByteArray &value : unpadded) {
    QCOMPARE(Base32::removePadding(Base32::addPadding(value)), value);
  }
  // Non-multiples of 8 are returned unchanged, as bad input.
  QCOMPARE(Base32::removePadding(QByteArrayLiteral("MZXW6YQ")),
           QByteArrayLiteral("MZXW6YQ"));

  // Only the trailing run is padding. Counting every '=' in the last few
  // positions made this strip two characters and destroy the 'F'.
  QCOMPARE(Base32::removePadding(QByteArrayLiteral("AB=CDEF=")),
           QByteArrayLiteral("AB=CDEF"));
  QCOMPARE(Base32::removePadding(QByteArrayLiteral("A=B=C=D=")),
           QByteArrayLiteral("A=B=C=D"));
}

void tst_base32::sanitizeInputAcceptsHumanInput_data() {
  QTest::addColumn<QByteArray>("input");
  QTest::addColumn<QByteArray>("decoded");

  // 23 significant characters: sanitizeInput supplies the single '=' that the
  // final four-byte quantum needs.
  QTest::newRow("grouped with spaces")
      << QByteArray("JBSW Y3DP EB3W 64TM MQXC 4LQ")
      << QByteArray("Hello world...");
  QTest::newRow("grouped with dashes")
      << QByteArray("JBSW-Y3DP-EB3W-64TM-MQXC-4LQ")
      << QByteArray("Hello world...");
  QTest::newRow("lowercase unpadded")
      << QByteArray("jbswy3dpeb3w64tmmqxc4lq") << QByteArray("Hello world...");
  QTest::newRow("already padded")
      << QByteArray("ORSXG5A=") << QByteArray("test");
  QTest::newRow("unpadded full quantum")
      << QByteArray("ORSXG5AA") << QByteArray::fromHex("74657374 00");
}

void tst_base32::sanitizeInputAcceptsHumanInput() {
  QFETCH(QByteArray, input);
  QFETCH(QByteArray, decoded);
  QCOMPARE(Base32::decode(Base32::sanitizeInput(input)), decoded);
}

void tst_base32::sanitizeInputMapsAmbiguousDigits() {
  // 8 -> B, 0 -> O, 1 -> L: the digits that do not exist in the alphabet.
  QCOMPARE(Base32::decode(Base32::sanitizeInput(
               QByteArrayLiteral("J8SWY3DPE83W64TMMQXC4LQ"))),
           QByteArrayLiteral("Hello world..."));
  QCOMPARE(Base32::sanitizeInput(QByteArrayLiteral("01")),
           QByteArrayLiteral("OL======"));
}

void tst_base32::sanitizeInputDropsEmbeddedNul() {
  QByteArray input = QByteArrayLiteral("J8SWY3D[PE83W64TMMQ]XC!4LQ");
  input.insert(3, '\0');
  QCOMPARE(Base32::decode(Base32::sanitizeInput(input)),
           QByteArrayLiteral("Hello world..."));
}

QTEST_MAIN(tst_base32)
#include "tst_base32.moc"
