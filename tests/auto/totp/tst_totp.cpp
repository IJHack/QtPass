// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QUrl>
#include <QUrlQuery>
#include <QtTest>

#include "../../../src/base32.h"
#include "../../../src/totp.h"

/// RFC 6238 appendix B seed, base32 encoded: the ASCII "12345678901234567890".
static const QString kRfcSeedSha1 =
    QStringLiteral("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ");
/// Steam test secret; the two expected codes came from the Steam mobile app.
static const QString kSteamSecret =
    QStringLiteral("63BEDWCQZKTQWPESARIERL5DTTQFCJTK");

class tst_totp : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void parseCanonicalUri();
  void parseUriWithoutOptionalParameters();
  void parseBareSecret();
  void parseSpacedSecret();
  void parseAlgorithmNames_data();
  void parseAlgorithmNames();
  void parseClampsDigitsAndPeriod();
  void parseNonNumericParametersFallBackToDefaults_data();
  void parseNonNumericParametersFallBackToDefaults();
  void toUriPreservesUnknownParameters();
  void toUriOmitsSecretPadding();
  void parseRejectsBadInput_data();
  void parseRejectsBadInput();
  void generateRfc6238Vectors_data();
  void generateRfc6238Vectors();
  void generateSixDigitVectors_data();
  void generateSixDigitVectors();
  void generateTenDigitsDoesNotOverflow();
  void generateSteamVectors_data();
  void generateSteamVectors();
  void steamEncoderForcesFiveDigits();
  void counterAdvancesWithStep();
  void secondsRemainingIsFullStepOnBoundary();
  void toUriOmitsDefaultAlgorithm();
  void toUriEmitsNonDefaults();
  void normalizeRoundTrips_data();
  void normalizeRoundTrips();
  void normalizeAppliesFallbackLabelAndIssuer();
  void normalizeRejectsInvalidInput();
  void isValidMatchesParse();
};

void tst_totp::parseCanonicalUri() {
  const auto settings = Totp::parse(QStringLiteral(
      "otpauth://totp/ACME%20Co:john@example.com?secret="
      "HXDMVJECJJWSRB3HWIZR4IFUGFTMXBOZ&issuer=ACME%20Co&algorithm=SHA1&"
      "digits=6&period=30"));
  QVERIFY(settings.has_value());
  QCOMPARE(settings->base32Key,
           QStringLiteral("HXDMVJECJJWSRB3HWIZR4IFUGFTMXBOZ"));
  QCOMPARE(settings->digits, 6U);
  QCOMPARE(settings->step, 30U);
  QVERIFY(settings->algorithm == Totp::Algorithm::Sha1);
  QVERIFY(settings->encoder == Totp::Encoder::Base10);
  QCOMPARE(settings->issuer, QStringLiteral("ACME Co"));
  QCOMPARE(settings->label, QStringLiteral("ACME Co:john@example.com"));
}

void tst_totp::parseUriWithoutOptionalParameters() {
  const auto settings = Totp::parse(
      QStringLiteral("otpauth://totp/Example?secret=JBSWY3DPEHPK3PXP"));
  QVERIFY(settings.has_value());
  QCOMPARE(settings->digits, Totp::DEFAULT_DIGITS);
  QCOMPARE(settings->step, Totp::DEFAULT_STEP);
  QVERIFY(settings->algorithm == Totp::Algorithm::Sha1);
  QCOMPARE(settings->label, QStringLiteral("Example"));
  QVERIFY(settings->issuer.isEmpty());
}

void tst_totp::parseBareSecret() {
  const auto settings = Totp::parse(QStringLiteral("JBSWY3DPEHPK3PXP"));
  QVERIFY(settings.has_value());
  QCOMPARE(settings->base32Key, QStringLiteral("JBSWY3DPEHPK3PXP"));
  QCOMPARE(settings->digits, Totp::DEFAULT_DIGITS);
  QCOMPARE(settings->step, Totp::DEFAULT_STEP);
  QVERIFY(settings->label.isEmpty());
}

void tst_totp::parseSpacedSecret() {
  // Secrets are commonly printed in groups of four.
  const auto spaced = Totp::parse(QStringLiteral("JBSW Y3DP EHPK 3PXP"));
  const auto plain = Totp::parse(QStringLiteral("JBSWY3DPEHPK3PXP"));
  QVERIFY(spaced.has_value());
  QVERIFY(plain.has_value());
  QCOMPARE(spaced->key, plain->key);
}

void tst_totp::parseAlgorithmNames_data() {
  QTest::addColumn<QString>("name");
  QTest::addColumn<int>("expected");

  QTest::newRow("SHA1") << QStringLiteral("SHA1")
                        << static_cast<int>(Totp::Algorithm::Sha1);
  QTest::newRow("sha256") << QStringLiteral("sha256")
                          << static_cast<int>(Totp::Algorithm::Sha256);
  QTest::newRow("SHA512") << QStringLiteral("SHA512")
                          << static_cast<int>(Totp::Algorithm::Sha512);
  QTest::newRow("HMAC-SHA-256") << QStringLiteral("HMAC-SHA-256")
                                << static_cast<int>(Totp::Algorithm::Sha256);
  QTest::newRow("HMAC-SHA-512") << QStringLiteral("HMAC-SHA-512")
                                << static_cast<int>(Totp::Algorithm::Sha512);
  QTest::newRow("unknown falls back to SHA1")
      << QStringLiteral("MD5") << static_cast<int>(Totp::Algorithm::Sha1);
}

void tst_totp::parseAlgorithmNames() {
  QFETCH(QString, name);
  QFETCH(int, expected);
  const auto settings = Totp::parse(
      QStringLiteral("otpauth://totp/x?secret=JBSWY3DPEHPK3PXP&algorithm=%1")
          .arg(name));
  QVERIFY(settings.has_value());
  QCOMPARE(static_cast<int>(settings->algorithm), expected);
}

void tst_totp::parseClampsDigitsAndPeriod() {
  const auto tooBig = Totp::parse(QStringLiteral(
      "otpauth://totp/x?secret=JBSWY3DPEHPK3PXP&digits=99&period=999999"));
  QVERIFY(tooBig.has_value());
  QCOMPARE(tooBig->digits, Totp::MAX_DIGITS);
  QCOMPARE(tooBig->step, Totp::MAX_STEP);

  // Zero is not a usable digit count or period, so fall back to the RFC
  // defaults rather than clamping up to 1 (which produced a one-character code
  // rotating every second, and normalize() then persisted it).
  const auto zero = Totp::parse(QStringLiteral(
      "otpauth://totp/x?secret=JBSWY3DPEHPK3PXP&digits=0&period=0"));
  QVERIFY(zero.has_value());
  QCOMPARE(zero->digits, Totp::DEFAULT_DIGITS);
  QCOMPARE(zero->step, Totp::DEFAULT_STEP);
}

/// toUInt() returns 0 with no error signal for anything non-numeric.
void tst_totp::parseNonNumericParametersFallBackToDefaults_data() {
  QTest::addColumn<QString>("query");

  QTest::newRow("letter O in period") << QStringLiteral("period=3O");
  QTest::newRow("empty values") << QStringLiteral("digits=&period=");
  QTest::newRow("words") << QStringLiteral("digits=six&period=thirty");
  QTest::newRow("negative") << QStringLiteral("digits=-6&period=-30");
}

void tst_totp::parseNonNumericParametersFallBackToDefaults() {
  QFETCH(QString, query);
  const auto settings = Totp::parse(
      QStringLiteral("otpauth://totp/x?secret=JBSWY3DPEHPK3PXP&%1").arg(query));
  QVERIFY(settings.has_value());
  QCOMPARE(settings->digits, Totp::DEFAULT_DIGITS);
  QCOMPARE(settings->step, Totp::DEFAULT_STEP);
}

/// Canonicalising must not discard parameters other authenticators honour.
void tst_totp::toUriPreservesUnknownParameters() {
  const QString original =
      QStringLiteral("otpauth://totp/Example:alice?secret=JBSWY3DPEHPK3PXP&"
                     "issuer=Example&image=https%3A%2F%2Fexample.com%2Fl.png&"
                     "digits=6&period=30");
  const auto settings = Totp::parse(original);
  QVERIFY(settings.has_value());
  QCOMPARE(settings->extraParams.size(), 1);
  QCOMPARE(settings->extraParams.at(0).first, QStringLiteral("image"));

  // The value must be decoded on read, or re-encoding double-escapes it.
  QCOMPARE(settings->extraParams.at(0).second,
           QStringLiteral("https://example.com/l.png"));

  const QString uri = Totp::toUri(*settings);
  QVERIFY2(uri.contains(QStringLiteral("image=")),
           "an unmodelled parameter must survive a round trip");
  // And it survives another round trip with its value intact — checking only
  // that "image=" is present would pass while the value was mangled.
  const auto again = Totp::parse(uri);
  QVERIFY(again.has_value());
  QCOMPARE(again->extraParams.size(), 1);
  QCOMPARE(again->extraParams.at(0).second,
           QStringLiteral("https://example.com/l.png"));

  // A value containing a literal percent sign is the case that double-encodes.
  const auto pct = Totp::parse(
      QStringLiteral("otpauth://totp/x?secret=JBSWY3DPEHPK3PXP&note=100%25"));
  QVERIFY(pct.has_value());
  QCOMPARE(pct->extraParams.at(0).second, QStringLiteral("100%"));
  const auto pctAgain = Totp::parse(Totp::toUri(*pct));
  QVERIFY(pctAgain.has_value());
  QCOMPARE(pctAgain->extraParams.at(0).second, QStringLiteral("100%"));
}

/// The Key-Uri-Format spec omits the base32 padding of the secret.
void tst_totp::toUriOmitsSecretPadding() {
  // A 16-byte secret encodes to 26 base32 characters plus six '='.
  const QString padded =
      QString::fromLatin1(Base32::encode(QByteArray(16, 'x')));
  QVERIFY2(padded.endsWith(QLatin1Char('=')), "precondition: padded input");

  const auto settings = Totp::parse(padded);
  QVERIFY(settings.has_value());
  const QString uri = Totp::toUri(*settings);

  // Inspect the secret parameter itself: the URI legitimately contains '=' as
  // the key/value separator of every parameter.
  const QUrl parsedUrl(uri);
  const QUrlQuery query(parsedUrl);
  const QString emitted =
      query.queryItemValue(QStringLiteral("secret"), QUrl::FullyDecoded);
  QVERIFY2(!emitted.isEmpty(), "the URI must carry a secret");
  QVERIFY2(!emitted.contains(QLatin1Char('=')),
           "the emitted secret must carry no base32 padding");

  // Still decodes to the same key, because sanitizeInput() re-pads.
  const auto reparsed = Totp::parse(uri);
  QVERIFY(reparsed.has_value());
  QCOMPARE(reparsed->key, settings->key);
}

void tst_totp::parseRejectsBadInput_data() {
  QTest::addColumn<QString>("config");

  QTest::newRow("empty") << QString();
  QTest::newRow("whitespace only") << QStringLiteral("   ");
  QTest::newRow("uri without secret")
      << QStringLiteral("otpauth://totp/Example?issuer=Example");
  QTest::newRow("uri with empty secret")
      << QStringLiteral("otpauth://totp/Example?secret=");
  QTest::newRow("secret that is not base32")
      << QStringLiteral("otpauth://totp/Example?secret=!!!!");
  // Counter-based HOTP cannot be derived from a clock.
  QTest::newRow("hotp is rejected")
      << QStringLiteral("otpauth://hotp/Example?secret=JBSWY3DPEHPK3PXP&"
                        "counter=1");
  QTest::newRow("bare non-base32") << QStringLiteral("!!!!");
  // An otpauth: prefix that does not resolve to a valid totp URI must be
  // rejected, not reinterpreted as a bare secret: sanitizeInput() would strip
  // the punctuation and leave decodable base32, yielding a silently wrong code.
  QTest::newRow("otpauth scheme only") << QStringLiteral("otpauth:");
  QTest::newRow("otpauth without host")
      << QStringLiteral("otpauth:totp/x?secret=JBSWY3DPEHPK3PXP");
  QTest::newRow("otpauth uppercase prefix, no host")
      << QStringLiteral("OTPAUTH:garbage");
  QTest::newRow("otpauth with unparseable authority")
      << QStringLiteral("otpauth://[::bad/totp?secret=JBSWY3DPEHPK3PXP");
}

void tst_totp::parseRejectsBadInput() {
  QFETCH(QString, config);
  QVERIFY2(!Totp::parse(config).has_value(),
           "invalid OTP configuration must not parse");
  QVERIFY(!Totp::isValid(config));
}

/**
 * @brief RFC 6238 appendix B, all three hash functions at eight digits.
 *
 * The SHA-256 and SHA-512 seeds are the 32- and 64-byte variants the RFC
 * specifies; they are built here with Base32::encode so the vectors also
 * cross-check the encoder.
 */
void tst_totp::generateRfc6238Vectors_data() {
  QTest::addColumn<QString>("secret");
  QTest::addColumn<QString>("algorithm");
  QTest::addColumn<quint64>("time");
  QTest::addColumn<QString>("expected");

  const QString seedSha1 = kRfcSeedSha1;
  const QString seedSha256 = QString::fromLatin1(
      Base32::encode(QByteArrayLiteral("12345678901234567890123456789012")));
  const QString seedSha512 = QString::fromLatin1(Base32::encode(
      QByteArrayLiteral("1234567890123456789012345678901234567890123456789012"
                        "345678901234")));

  const struct {
    quint64 time;
    const char *sha1;
    const char *sha256;
    const char *sha512;
  } vectors[] = {
      {59ULL, "94287082", "46119246", "90693936"},
      {1111111109ULL, "07081804", "68084774", "25091201"},
      {1111111111ULL, "14050471", "67062674", "99943326"},
      {1234567890ULL, "89005924", "91819424", "93441116"},
      {2000000000ULL, "69279037", "90698825", "38618901"},
      {20000000000ULL, "65353130", "77737706", "47863826"},
  };

  for (const auto &vector : vectors) {
    QTest::newRow(qPrintable(QStringLiteral("SHA1 t=%1").arg(vector.time)))
        << seedSha1 << QStringLiteral("SHA1") << vector.time
        << QString::fromLatin1(vector.sha1);
    QTest::newRow(qPrintable(QStringLiteral("SHA256 t=%1").arg(vector.time)))
        << seedSha256 << QStringLiteral("SHA256") << vector.time
        << QString::fromLatin1(vector.sha256);
    QTest::newRow(qPrintable(QStringLiteral("SHA512 t=%1").arg(vector.time)))
        << seedSha512 << QStringLiteral("SHA512") << vector.time
        << QString::fromLatin1(vector.sha512);
  }
}

void tst_totp::generateRfc6238Vectors() {
  QFETCH(QString, secret);
  QFETCH(QString, algorithm);
  QFETCH(quint64, time);
  QFETCH(QString, expected);

  const auto settings = Totp::parse(
      QStringLiteral(
          "otpauth://totp/rfc6238?secret=%1&algorithm=%2&digits=8&period=30")
          .arg(secret, algorithm));
  QVERIFY(settings.has_value());
  QCOMPARE(Totp::generate(*settings, time), expected);
}

/// The six-digit form is what the UI shows by default.
void tst_totp::generateSixDigitVectors_data() {
  QTest::addColumn<quint64>("time");
  QTest::addColumn<QString>("expected");

  QTest::newRow("t=1234567890") << 1234567890ULL << QStringLiteral("005924");
  QTest::newRow("t=1111111109") << 1111111109ULL << QStringLiteral("081804");
  QTest::newRow("t=59") << 59ULL << QStringLiteral("287082");
}

void tst_totp::generateSixDigitVectors() {
  QFETCH(quint64, time);
  QFETCH(QString, expected);

  const auto settings = Totp::parse(kRfcSeedSha1);
  QVERIFY(settings.has_value());
  QCOMPARE(settings->digits, 6U);
  QCOMPARE(Totp::generate(*settings, time), expected);
}

/**
 * @brief KeePassXC computed the modulus as `quint32 pow(10, digits)`, which
 * overflows at ten digits and silently corrupts the code.
 */
void tst_totp::generateTenDigitsDoesNotOverflow() {
  const auto settings = Totp::parse(
      QStringLiteral("otpauth://totp/x?secret=%1&digits=10").arg(kRfcSeedSha1));
  QVERIFY(settings.has_value());
  QCOMPARE(settings->digits, 10U);

  const QString code = Totp::generate(*settings, 1234567890ULL);
  QCOMPARE(code.size(), 10);
  // The full 31-bit dynamic truncation is smaller than 10^10, so a ten-digit
  // code is the zero-padded truncation itself.
  QCOMPARE(code, QStringLiteral("0689005924"));
}

void tst_totp::generateSteamVectors_data() {
  QTest::addColumn<quint64>("time");
  QTest::addColumn<QString>("expected");

  QTest::newRow("t=1511200518") << 1511200518ULL << QStringLiteral("FR8RV");
  QTest::newRow("t=1511200714") << 1511200714ULL << QStringLiteral("9P3VP");
}

void tst_totp::generateSteamVectors() {
  QFETCH(quint64, time);
  QFETCH(QString, expected);

  const auto settings = Totp::parse(
      QStringLiteral("otpauth://totp/test:test@example.com?secret=%1&issuer="
                     "Valve&algorithm=SHA1&digits=5&period=30&encoder=steam")
          .arg(kSteamSecret));
  QVERIFY(settings.has_value());
  QVERIFY(settings->encoder == Totp::Encoder::Steam);
  QCOMPARE(Totp::generate(*settings, time), expected);
}

/// A steam encoder always means five characters, whatever `digits` claims.
void tst_totp::steamEncoderForcesFiveDigits() {
  const auto absent =
      Totp::parse(QStringLiteral("otpauth://totp/x?secret=%1&encoder=steam")
                      .arg(kSteamSecret));
  QVERIFY(absent.has_value());
  QCOMPARE(absent->digits, Totp::STEAM_DIGITS);

  const auto contradictory = Totp::parse(
      QStringLiteral("otpauth://totp/x?secret=%1&digits=8&encoder=STEAM")
          .arg(kSteamSecret));
  QVERIFY(contradictory.has_value());
  QCOMPARE(contradictory->digits, Totp::STEAM_DIGITS);
  QCOMPARE(Totp::generate(*contradictory, 1511200518ULL).size(), 5);
}

void tst_totp::counterAdvancesWithStep() {
  const auto settings = Totp::parse(kRfcSeedSha1);
  QVERIFY(settings.has_value());
  QCOMPARE(Totp::counter(*settings, 0ULL), 0ULL);
  QCOMPARE(Totp::counter(*settings, 29ULL), 0ULL);
  QCOMPARE(Totp::counter(*settings, 30ULL), 1ULL);
  QCOMPARE(Totp::counter(*settings, 59ULL), 1ULL);
}

void tst_totp::secondsRemainingIsFullStepOnBoundary() {
  const auto settings = Totp::parse(kRfcSeedSha1);
  QVERIFY(settings.has_value());
  // A boundary must read as a whole step, never as zero, so the countdown
  // never renders an already-expired code as having no time left.
  QCOMPARE(Totp::secondsRemaining(*settings, 0ULL), 30U);
  QCOMPARE(Totp::secondsRemaining(*settings, 30ULL), 30U);
  QCOMPARE(Totp::secondsRemaining(*settings, 1ULL), 29U);
  QCOMPARE(Totp::secondsRemaining(*settings, 29ULL), 1U);
}

void tst_totp::toUriOmitsDefaultAlgorithm() {
  const auto settings = Totp::parse(QStringLiteral("JBSWY3DPEHPK3PXP"));
  QVERIFY(settings.has_value());
  const QString uri = Totp::toUri(*settings);
  QVERIFY2(!uri.contains(QStringLiteral("algorithm")),
           "SHA1 is the default and must not be spelled out");
  QVERIFY2(!uri.contains(QStringLiteral("encoder")),
           "base10 is the default and must not be spelled out");
  QVERIFY(uri.contains(QStringLiteral("secret=JBSWY3DPEHPK3PXP")));
  QVERIFY(uri.contains(QStringLiteral("digits=6")));
  QVERIFY(uri.contains(QStringLiteral("period=30")));
  // An empty label would produce a malformed URI.
  QVERIFY(uri.startsWith(QStringLiteral("otpauth://totp/QtPass?")));
}

void tst_totp::toUriEmitsNonDefaults() {
  const auto settings = Totp::parse(QStringLiteral(
      "otpauth://totp/Example:alice?secret=JBSWY3DPEHPK3PXP&issuer=Example&"
      "algorithm=SHA512&digits=8&period=60"));
  QVERIFY(settings.has_value());
  const QString uri = Totp::toUri(*settings);
  QVERIFY(uri.contains(QStringLiteral("algorithm=SHA512")));
  QVERIFY(uri.contains(QStringLiteral("digits=8")));
  QVERIFY(uri.contains(QStringLiteral("period=60")));
  QVERIFY(uri.contains(QStringLiteral("issuer=Example")));
  // ':' stays readable in the label, as authenticators expect.
  QVERIFY(uri.startsWith(QStringLiteral("otpauth://totp/Example:alice?")));
}

void tst_totp::normalizeRoundTrips_data() {
  QTest::addColumn<QString>("config");

  QTest::newRow("bare secret") << QStringLiteral("JBSWY3DPEHPK3PXP");
  QTest::newRow("spaced bare secret") << QStringLiteral("JBSW Y3DP EHPK 3PXP");
  QTest::newRow("canonical uri") << QStringLiteral(
      "otpauth://totp/Example:alice?secret=JBSWY3DPEHPK3PXP&issuer="
      "Example&digits=6&period=30");
  QTest::newRow("sha512 uri")
      << QStringLiteral("otpauth://totp/x?secret=JBSWY3DPEHPK3PXP&algorithm="
                        "SHA512&digits=8&period=60");
  QTest::newRow("steam uri")
      << QStringLiteral("otpauth://totp/x?secret=63BEDWCQZKTQWPESARIERL5DTTQ"
                        "FCJTK&encoder=steam");
}

void tst_totp::normalizeRoundTrips() {
  QFETCH(QString, config);

  const QString canonical = Totp::normalize(config);
  QVERIFY(!canonical.isEmpty());

  const auto original = Totp::parse(config);
  const auto reparsed = Totp::parse(canonical);
  QVERIFY(original.has_value());
  QVERIFY(reparsed.has_value());

  QCOMPARE(reparsed->key, original->key);
  QCOMPARE(reparsed->digits, original->digits);
  QCOMPARE(reparsed->step, original->step);
  QVERIFY(reparsed->algorithm == original->algorithm);
  QVERIFY(reparsed->encoder == original->encoder);

  // Normalising an already canonical URI must be idempotent.
  QCOMPARE(Totp::normalize(canonical), canonical);
}

void tst_totp::normalizeAppliesFallbackLabelAndIssuer() {
  const QString uri =
      Totp::normalize(QStringLiteral("JBSWY3DPEHPK3PXP"),
                      QStringLiteral("github.com"), QStringLiteral("GitHub"));
  QVERIFY(uri.startsWith(QStringLiteral("otpauth://totp/github.com?")));
  QVERIFY(uri.contains(QStringLiteral("issuer=GitHub")));

  // A label already present in the URI wins over the fallback.
  const QString kept = Totp::normalize(
      QStringLiteral("otpauth://totp/Example:alice?secret=JBSWY3DPEHPK3PXP"),
      QStringLiteral("github.com"));
  QVERIFY(kept.startsWith(QStringLiteral("otpauth://totp/Example:alice?")));
}

void tst_totp::normalizeRejectsInvalidInput() {
  QVERIFY(Totp::normalize(QStringLiteral("!!!!")).isEmpty());
  QVERIFY(Totp::normalize(QString()).isEmpty());
  QVERIFY(Totp::normalize(
              QStringLiteral("otpauth://hotp/x?secret=JBSWY3DPEHPK3PXP"))
              .isEmpty());
}

void tst_totp::isValidMatchesParse() {
  QVERIFY(Totp::isValid(QStringLiteral("JBSWY3DPEHPK3PXP")));
  QVERIFY(Totp::isValid(
      QStringLiteral("otpauth://totp/x?secret=JBSWY3DPEHPK3PXP")));
  // "nope" is, unhelpfully, valid base32 once sanitized; these are not.
  QVERIFY(!Totp::isValid(QStringLiteral("!!!!")));
  QVERIFY(!Totp::isValid(QStringLiteral("1")));
  QVERIFY(!Totp::isValid(QString()));
}

QTEST_MAIN(tst_totp)
#include "tst_totp.moc"
