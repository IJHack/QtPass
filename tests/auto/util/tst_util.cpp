// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

#include "../../../src/enums.h"
#include "../../../src/filecontent.h"
#include "../../../src/imitatepass.h"
#include "../../../src/pass.h"
#include "../../../src/passwordconfiguration.h"
#include "../../../src/qprogressindicator.h"
#include "../../../src/qtpass.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/simpletransaction.h"
#include "../../../src/userinfo.h"
#include "../../../src/util.h"

/**
 * @brief The tst_util class is our first unit test
 */
class tst_util : public QObject {
  Q_OBJECT

public:
  tst_util();
  ~tst_util() override;

public Q_SLOTS:
  void init();
  void cleanup();

private:
  struct PassStoreGuard {
    QString original;
    explicit PassStoreGuard(const QString &orig) : original(orig) {}
    ~PassStoreGuard() { QtPassSettings::setPassStore(original); }
  };

  // Thin subclass that exposes protected members needed by env tests.
  // NOTE: environment() calls updateEnv(), which adds PASSWORD_STORE_* entries
  // to the internal env list and forwards it to exec via setEnvironment().
  // Because Pass::setEnvVar removes all matching entries before appending,
  // repeated calls to environment() are idempotent for PASSWORD_STORE_* vars.
  // Tests that call callSetEnvVar() directly work on the same env list, so
  // any subsequent environment() call will re-run updateEnv() and may
  // overwrite those entries — use callSetEnvVar() and environment() in the
  // same test only when the keys don't overlap with PASSWORD_STORE_*.
  class TestPass : public ImitatePass {
  public:
    void callSetEnvVar(const QString &key, const QString &value) {
      setEnvVar(key, value);
    }
    QStringList environment() {
      updateEnv();
      return exec.environment();
    }
  };

  template <typename T, void (*Setter)(const T &)> struct SettingGuard {
    T original;
    SettingGuard(T orig, const T &newVal) : original(std::move(orig)) {
      Setter(newVal);
    }
    ~SettingGuard() { Setter(original); }
    SettingGuard(const SettingGuard &) = delete;
    SettingGuard &operator=(const SettingGuard &) = delete;
  };

private Q_SLOTS:
  void cleanupTestCase();
  void normalizeFolderPath();
  void normalizeFolderPathEdgeCases();
  void fileContent();
  void fileContentEdgeCases();
  void namedValuesTakeValue();
  void namedValuesEdgeCases();
  void totpHiddenFromDisplay();
  void testAwsUrl();
  void regexPatterns();
  void regexPatternEdgeCases();
  void endsWithGpgEdgeCases();
  void userInfoValidity();
  void userInfoValidityEdgeCases();
  void passwordConfigurationCharacters();
  void simpleTransactionBasic();
  void simpleTransactionNested();
  void createGpgIdFile();
  void createGpgIdFileEmptyKeys();
  void generateRandomPassword();
  void boundedRandom();
  void findBinaryInPath();
  void findPasswordStore();
  void configIsValid();
  void getDirBasic();
  void getDirWithIndex();
  void findBinaryInPathNotFound();
  void findPasswordStoreEnvVar();
  void normalizeFolderPathMultipleCalls();
  void userInfoFullyValid();
  void userInfoMarginallyValid();
  void userInfoIsValid();
  void userInfoCreatedAndExpiry();
  void qProgressIndicatorBasic();
  void qProgressIndicatorStartStop();
  void namedValueBasic();
  void namedValueMultiple();
  void buildClipboardMimeDataLinux();
  void buildClipboardMimeDataWindows();
  void buildClipboardMimeDataMac();
  void utilRegexEnsuresGpg();
  void utilRegexProtocol();
  void utilRegexNewLines();
  void reencryptPathNormalization();
  void reencryptPathAbsolutePath();
  void buildClipboardMimeDataDword();
  void imitatePassResolveMoveDestination();
  void imitatePassResolveMoveDestinationForce();
  void imitatePassResolveMoveDestinationDestExistsNoForce();
  void imitatePassResolveMoveDestinationDir();
  void imitatePassResolveMoveDestinationNonExistent();
  void imitatePassRemoveDir();
  void getRecipientListBasic();
  void getRecipientListEmpty();
  void getRecipientListWithComments();
  void getRecipientListInvalidKeyId();
  void isValidKeyIdBasic();
  void isValidKeyIdWith0xPrefix();
  void isValidKeyIdWithEmail();
  void isValidKeyIdInvalid();
  void getRecipientStringCount();
  void getGpgIdPathBasic();
  void getGpgIdPathSubfolder();
  void getGpgIdPathNotFound();
  void findBinaryInPathReturnedPathIsAbsolute();
  void findBinaryInPathReturnedPathIsExecutable();
  void findBinaryInPathMultipleKnownBinaries();
  void findBinaryInPathConsistency();
  void findBinaryInPathResultContainsBinaryName();
  void findBinaryInPathTempExecutableInTempDir();
  void findBinaryInPathWithConstQStringRef();
  void findBinaryInPathEmptyString();
  void findBinaryInPathStringLiteral();
  void setEnvVarAdds();
  void setEnvVarUpdates();
  void setEnvVarRemoves();
  void setEnvVarNoopOnMissingRemove();
  void updateEnvSetsExpectedVars();
  void updateEnvEmptyCustomCharsetFallsBackToAllChars();
  void updateEnvWslenvContainsRequiredVars();
  void gpgErrorMessageKeyExpiredStatusToken();
  void gpgErrorMessageKeyRevokedStatusToken();
  void gpgErrorMessageNoPubkeyStatusToken();
  void gpgErrorMessageInvRecpStatusToken();
  void gpgErrorMessageFailureStatusToken();
  void gpgErrorMessageKeyExpiredFallback();
  void gpgErrorMessageRevokedFallback();
  void gpgErrorMessageNoPubkeyFallback();
  void gpgErrorMessageEncryptionFailedFallback();
  void gpgErrorMessageUnknownReturnsEmpty();
  void gpgErrorMessageStatusTokenTakesPriorityOverFallback();
};

/**
 * @brief tst_util::tst_util basic constructor
 */
tst_util::tst_util() = default;

/**
 * @brief tst_util::~tst_util basic destructor
 */
tst_util::~tst_util() = default;

/**
 * @brief tst_util::init unit test init method
 */
void tst_util::init() {
  // Intentionally left empty: no per-test setup required.
}

/**
 * @brief tst_util::cleanup unit test cleanup method
 */
void tst_util::cleanup() {
  // Intentionally left empty: no per-test cleanup required.
}

/**
 * @brief tst_util::cleanupTestCase test case cleanup method
 */
void tst_util::cleanupTestCase() {
  // No test case cleanup required; function intentionally left empty.
}

/**
 * @brief tst_util::normalizeFolderPath test to check correct working
 * of Util::normalizeFolderPath the paths should always end with a slash
 */
void tst_util::normalizeFolderPath() {
  QString result;
  QString sep = QDir::separator();

  // Forward slash path
  result = Util::normalizeFolderPath("test");
  QVERIFY(result.endsWith(sep));
  result = Util::normalizeFolderPath("test/");
  QVERIFY(result.endsWith(sep));
  // Verify exact normalized path content
  result = Util::normalizeFolderPath("test");
  QVERIFY2(result == "test" + sep,
           qPrintable(QString("Expected 'test%1', got '%2'").arg(sep, result)));
  result = Util::normalizeFolderPath("test/");
  QVERIFY2(result == "test" + sep,
           qPrintable(QString("Expected 'test%1', got '%2'").arg(sep, result)));

  // Windows-style backslash path (only on Windows)
  if (QDir::separator() == '\\') {
    result = Util::normalizeFolderPath("test\\subdir");
    QVERIFY(result.endsWith("\\"));
    QVERIFY(result.contains("test"));
    QVERIFY(result.contains("subdir"));
    // Verify exact normalized path content
    QVERIFY2(
        result == "test\\subdir\\",
        qPrintable(
            QString("Expected 'test\\\\subdir\\\\', got '%1'").arg(result)));
  }

  // Mixed separators test
  result = Util::normalizeFolderPath("test/subdir\\folder");
  QVERIFY(result.endsWith(sep));
  QVERIFY(result.contains("test"));
  QVERIFY(result.contains("subdir"));
  QVERIFY(result.contains("folder"));
}

void tst_util::fileContent() {
  NamedValue key = {"key", "val"};
  NamedValue key2 = {"key2", "val2"};
  QString password = "password";

  FileContent fc = FileContent::parse("password\n", {}, false);
  QCOMPARE(fc.getPassword(), password);
  QCOMPARE(fc.getNamedValues(), {});
  QCOMPARE(fc.getRemainingData(), QString());

  fc = FileContent::parse("password", {}, false);
  QCOMPARE(fc.getPassword(), password);
  QCOMPARE(fc.getNamedValues(), {});
  QCOMPARE(fc.getRemainingData(), QString());

  fc = FileContent::parse("password\nfoobar\n", {}, false);
  QCOMPARE(fc.getPassword(), password);
  QCOMPARE(fc.getNamedValues(), {});
  QCOMPARE(fc.getRemainingData(), QString("foobar\n"));

  fc = FileContent::parse("password\nkey: val\nkey2: val2", {"key2"}, false);
  QCOMPARE(fc.getPassword(), password);
  QCOMPARE(fc.getNamedValues(), NamedValues({key2}));
  QCOMPARE(fc.getRemainingData(), QString("key: val"));

  fc = FileContent::parse("password\nkey: val\nkey2: val2", {"key2"}, true);
  QCOMPARE(fc.getPassword(), password);
  QCOMPARE(fc.getNamedValues(), NamedValues({key, key2}));
  QCOMPARE(fc.getRemainingData(), QString());
}

void tst_util::namedValuesTakeValue() {
  NamedValues nv = {{"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}};

  QString val = nv.takeValue("key2");
  QCOMPARE(val, QString("value2"));
  QCOMPARE(nv.length(), 2);
  QVERIFY(!nv.contains({"key2", "value2"}));

  val = nv.takeValue("nonexistent");
  QVERIFY(val.isEmpty());

  val = nv.takeValue("key1");
  QCOMPARE(val, QString("value1"));
  val = nv.takeValue("key3");
  QCOMPARE(val, QString("value3"));
  QVERIFY(nv.isEmpty());
}

void tst_util::totpHiddenFromDisplay() {
  FileContent fc = FileContent::parse(
      "password\notpauth://totp/Test?secret=JBSWY3DPEHPK3PXP\nkey: value\n", {},
      false);

  QString remaining = fc.getRemainingData();
  QVERIFY(remaining.contains("otpauth://"));
  QVERIFY(remaining.contains("key: value"));

  QString display = fc.getRemainingDataForDisplay();
  QVERIFY(!display.contains("otpauth"));
  QVERIFY(display.contains("key: value"));

  fc = FileContent::parse(
      "password\nOTPAUTH://TOTP/Test?secret=JBSWY3DPEHPK3PXP\n", {}, false);
  QVERIFY(fc.getRemainingDataForDisplay().isEmpty());
}

void tst_util::testAwsUrl() {
  QRegularExpression proto = Util::protocolRegex();

  QRegularExpressionMatch match1 =
      proto.match("https://rh-dev.signin.aws.amazon.com/console");
  QVERIFY2(match1.hasMatch(), "Should match AWS console URL");
  QString captured1 = match1.captured(1);
  QVERIFY2(captured1.contains("amazon.com"), "Should include full URL");

  QRegularExpressionMatch match2 = proto.match("https://test-example.com/path");
  QVERIFY2(match2.hasMatch(), "Should match URL with dash");
  QString captured2 = match2.captured(1);
  QVERIFY2(captured2.contains("test-example.com"),
           "Should include full domain");
}

void tst_util::regexPatterns() {
  QRegularExpression gpg = Util::endsWithGpg();
  QVERIFY(gpg.match("test.gpg").hasMatch());
  QVERIFY(gpg.match("folder/test.gpg").hasMatch());
  QVERIFY(!gpg.match("test.gpg~").hasMatch());
  QVERIFY(!gpg.match("test.gpg.bak").hasMatch());

  QRegularExpression proto = Util::protocolRegex();
  QVERIFY(proto.match("https://example.com").hasMatch());
  QVERIFY(proto.match("ssh://user@host/path").hasMatch());
  QVERIFY(proto.match("ftp://server/file").hasMatch());
  QVERIFY(proto.match("webdav://localhost/share").hasMatch());
  QVERIFY(!proto.match("not a url").hasMatch());
  QRegularExpressionMatch urlWithTrailingTextMatch =
      proto.match("https://example.com/ is the address");
  QVERIFY(urlWithTrailingTextMatch.hasMatch());
  QString captured = urlWithTrailingTextMatch.captured(1);
  QVERIFY2(!captured.contains(" "), "URL should not include space");
  QVERIFY2(!captured.contains("<"), "URL should not include <");
  QVERIFY2(captured == "https://example.com/", "URL should stop at space");

  QRegularExpressionMatch urlWithFragmentMatch =
      proto.match("Link: https://test.org/path?q=1#frag");
  QVERIFY(urlWithFragmentMatch.hasMatch());
  captured = urlWithFragmentMatch.captured(1);
  QVERIFY2(captured.contains("?"), "URL should include query params");
  QVERIFY2(captured.contains("#"), "URL should include fragment");
  QVERIFY2(!captured.contains(" now"), "URL should not include trailing text");

  QRegularExpression nl = Util::newLinesRegex();
  QVERIFY(nl.match("\n").hasMatch());
  QVERIFY(nl.match("\r").hasMatch());
  QVERIFY(nl.match("\r\n").hasMatch());
}

void tst_util::normalizeFolderPathEdgeCases() {
  QString result = Util::normalizeFolderPath("");
  QVERIFY(result.endsWith(QDir::separator()));
  QVERIFY2(result == QDir::separator() || result.endsWith(QDir::separator()),
           "Empty path should become separator");

  result = Util::normalizeFolderPath(QDir::separator());
  QVERIFY(result.endsWith(QDir::separator()));

  result = Util::normalizeFolderPath("path/to/dir/");
  QVERIFY(result.endsWith(QDir::separator()));

  QString nativeResult = Util::normalizeFolderPath("path/to/dir");
  QVERIFY(nativeResult.endsWith(QDir::separator()));
}

void tst_util::fileContentEdgeCases() {
  FileContent fc = FileContent::parse("", {}, false);
  QVERIFY(fc.getPassword().isEmpty());

  fc = FileContent::parse("pass\nusername: user@example.com\npassword: "
                          "secret\nurl: https://login.com\n",
                          {"username", "password", "url"}, false);
  QVERIFY(fc.getNamedValues().length() >= 3);

  fc = FileContent::parse("pass\nkey: value with spaces\n", {"key"}, true);
  NamedValues nv = fc.getNamedValues();
  QCOMPARE(nv.length(), 1);
  QCOMPARE(nv.at(0).name, QString("key"));
  QVERIFY(nv.at(0).value.contains("spaces"));

  fc = FileContent::parse("pass\n://something\n", {}, false);
  QVERIFY(fc.getRemainingData().contains("://"));

  fc = FileContent::parse("pass\nno colon line\n", {}, false);
  QVERIFY(fc.getRemainingData().contains("no colon line"));

  fc = FileContent::parse("pass\nkey: value\nkey2: duplicate\n", {}, true);
  QVERIFY(fc.getNamedValues().length() >= 2);

  fc = FileContent::parse("pass\n", {}, false);
  QCOMPARE(fc.getPassword(), QString("pass"));
  QVERIFY(fc.getNamedValues().isEmpty());
}

void tst_util::namedValuesEdgeCases() {
  NamedValues nv;
  QVERIFY(nv.isEmpty());
  QVERIFY(nv.takeValue("nonexistent").isEmpty());

  NamedValue n1 = {"key", "value"};
  nv.append(n1);
  QCOMPARE(nv.length(), 1);
  NamedValue n2 = {"key2", "value2"};
  nv.append(n2);
  QCOMPARE(nv.length(), 2);

  nv.clear();
  QVERIFY(nv.isEmpty());
  QVERIFY(nv.takeValue("anything").isEmpty());
}

void tst_util::regexPatternEdgeCases() {
  const QRegularExpression &gpg = Util::endsWithGpg();
  QVERIFY(gpg.match(".gpg").hasMatch());
  QVERIFY(gpg.match("a.gpg").hasMatch());
  QVERIFY(!gpg.match("test.gpgx").hasMatch());

  const QRegularExpression &proto = Util::protocolRegex();
  QVERIFY(proto.match("webdavs://secure.example.com").hasMatch());
  QVERIFY(proto.match("ftps://ftp.server.org").hasMatch());
  QVERIFY(proto.match("sftp://user:pass@host").hasMatch());
  // file:/// URLs are not matched - see Util::protocolRegex()
  QVERIFY(!proto.match("file:///path/to/file").hasMatch());

  const QRegularExpression &nl = Util::newLinesRegex();
  QVERIFY(nl.match("\n").hasMatch());
  QVERIFY(nl.match("\r").hasMatch());
  QVERIFY(nl.match("\r\n").hasMatch());
}

void tst_util::endsWithGpgEdgeCases() {
  const QRegularExpression &gpg = Util::endsWithGpg();
  QVERIFY(!gpg.match(".gpgx").hasMatch());
  QVERIFY(!gpg.match("test.gpg.bak").hasMatch());
  QVERIFY(!gpg.match("test.gpg~").hasMatch());
  QVERIFY(!gpg.match("test.gpg.orig").hasMatch());
  QVERIFY(gpg.match("test.gpg").hasMatch());
  QVERIFY(gpg.match("test/path/file.gpg").hasMatch());
  QVERIFY(gpg.match("/absolute/path/file.gpg").hasMatch());
  QVERIFY(gpg.match("file name with spaces.gpg").hasMatch());
}

void tst_util::userInfoValidity() {
  UserInfo info;
  info.validity = 'f';
  QVERIFY(info.fullyValid());
  QVERIFY(!info.marginallyValid());
  QVERIFY(info.isValid());

  info.validity = 'u';
  QVERIFY(info.fullyValid());
  QVERIFY(!info.marginallyValid());
  QVERIFY(info.isValid());

  info.validity = 'm';
  QVERIFY(!info.fullyValid());
  QVERIFY(info.marginallyValid());
  QVERIFY(info.isValid());

  info.validity = 'n';
  QVERIFY(!info.fullyValid());
  QVERIFY(!info.marginallyValid());
  QVERIFY(!info.isValid());

  info.validity = 'e';
  QVERIFY(!info.isValid());
}

void tst_util::userInfoValidityEdgeCases() {
  UserInfo info;
  info.validity = '-';
  QVERIFY(!info.isValid());

  info.validity = 'q';
  QVERIFY(!info.isValid());

  char nullChar = '\0';
  info.validity = nullChar;
  QVERIFY(!info.isValid());

  QVERIFY(!info.have_secret);
  QVERIFY(!info.enabled);
}

void tst_util::passwordConfigurationCharacters() {
  PasswordConfiguration config;
  QCOMPARE(config.length, 16);
  QCOMPARE(config.selected, PasswordConfiguration::ALLCHARS);

  QVERIFY(!config.Characters[PasswordConfiguration::ALLCHARS].isEmpty());
  QVERIFY(!config.Characters[PasswordConfiguration::ALPHABETICAL].isEmpty());
  QVERIFY(!config.Characters[PasswordConfiguration::ALPHANUMERIC].isEmpty());
  QVERIFY(!config.Characters[PasswordConfiguration::CUSTOM].isEmpty());

  QVERIFY(config.Characters[PasswordConfiguration::ALLCHARS].length() >
          config.Characters[PasswordConfiguration::ALPHABETICAL].length());

  QVERIFY(config.Characters[PasswordConfiguration::ALPHANUMERIC].length() >
          config.Characters[PasswordConfiguration::ALPHABETICAL].length());
}

void tst_util::simpleTransactionBasic() {
  simpleTransaction transaction;
  transaction.transactionAdd(Enums::PASS_INSERT);
  Enums::PROCESS result = transaction.transactionIsOver(Enums::PASS_INSERT);
  QCOMPARE(result, Enums::PASS_INSERT);
}

void tst_util::simpleTransactionNested() {
  simpleTransaction transaction;
  transaction.transactionAdd(Enums::PASS_INSERT);
  transaction.transactionAdd(Enums::GIT_PUSH);
  Enums::PROCESS passInsertResult =
      transaction.transactionIsOver(Enums::PASS_INSERT);
  QCOMPARE(passInsertResult, Enums::PASS_INSERT);
  Enums::PROCESS gitPushResult = transaction.transactionIsOver(Enums::GIT_PUSH);
  QCOMPARE(gitPushResult, Enums::GIT_PUSH);
}

void tst_util::createGpgIdFile() {
  QTemporaryDir tempDir;
  QString newDir = tempDir.path() + "/testfolder";
  QVERIFY(QDir().mkdir(newDir));

  QString gpgIdFile = newDir + "/.gpg-id";
  QStringList keyIds = {"ABCDEF12", "34567890"};

  QFile gpgId(gpgIdFile);
  QVERIFY(gpgId.open(QIODevice::WriteOnly));
  for (const QString &keyId : keyIds) {
    gpgId.write((keyId + "\n").toUtf8());
  }
  gpgId.close();

  QVERIFY(QFile::exists(gpgIdFile));

  QFile readFile(gpgIdFile);
  QVERIFY(readFile.open(QIODevice::ReadOnly));
  QString content = QString::fromUtf8(readFile.readAll());
  readFile.close();

  QStringList lines = content.trimmed().split('\n');
  QCOMPARE(lines.size(), 2);
  QCOMPARE(lines[0], QString("ABCDEF12"));
  QCOMPARE(lines[1], QString("34567890"));
}

void tst_util::createGpgIdFileEmptyKeys() {
  QTemporaryDir tempDir;
  QString newDir = tempDir.path() + "/testfolder";
  QVERIFY(QDir().mkdir(newDir));

  QString gpgIdFile = newDir + "/.gpg-id";

  QFile gpgId(gpgIdFile);
  QVERIFY(gpgId.open(QIODevice::WriteOnly));
  gpgId.close();

  QVERIFY(QFile::exists(gpgIdFile));

  QFile readFile(gpgIdFile);
  QVERIFY(readFile.open(QIODevice::ReadOnly));
  QString content = QString::fromUtf8(readFile.readAll());
  readFile.close();

  QVERIFY(content.isEmpty());
}

void tst_util::generateRandomPassword() {
  SettingGuard<bool, QtPassSettings::setUsePwgen> pwgenGuard{
      QtPassSettings::isUsePwgen(), false};

  ImitatePass pass;
  QString charset = "abcdefghijklmnopqrstuvwxyz";
  QString result = pass.generatePassword(10, charset);

  QCOMPARE(result.length(), 10);
  for (const QChar &ch : result) {
    QVERIFY2(
        charset.contains(ch),
        "Generated password contains character outside the specified charset");
  }

  result = pass.generatePassword(100, "abcd");
  QCOMPARE(result.length(), 100);
  for (const QChar &ch : result) {
    QVERIFY2(
        QStringLiteral("abcd").contains(ch),
        "Generated password contains character outside the specified charset");
  }

  result = pass.generatePassword(0, "");
  QVERIFY(result.isEmpty());

  result = pass.generatePassword(50, "ABC");
  QCOMPARE(result.length(), 50);
  for (const QChar &ch : result) {
    QVERIFY2(
        QStringLiteral("ABC").contains(ch),
        "Generated password contains character outside the specified charset");
  }

  const QString randomCharset = QStringLiteral("abcd");
  const QString first = pass.generatePassword(32, randomCharset);
  QCOMPARE(first.length(), 32);
  bool foundDifferent = false;
  for (int i = 0; i < 20; ++i) {
    const QString candidate = pass.generatePassword(32, randomCharset);
    QCOMPARE(candidate.length(), 32);
    for (const QChar &ch : candidate) {
      QVERIFY2(randomCharset.contains(ch),
               "Generated password contains character outside the specified "
               "charset");
    }
    if (candidate != first) {
      foundDifferent = true;
      break;
    }
  }
  QVERIFY2(
      foundDifferent,
      "Multiple generated passwords were identical; randomness may be broken");
}

void tst_util::boundedRandom() {
  SettingGuard<bool, QtPassSettings::setUsePwgen> pwgenGuard{
      QtPassSettings::isUsePwgen(), false};

  ImitatePass pass;

  QVector<quint32> counts(10, 0);
  const int iterations = 1000;

  for (int i = 0; i < iterations; ++i) {
    QString result = pass.generatePassword(1, "0123456789");
    quint32 val = result.at(0).digitValue();
    QVERIFY2(val < 10, "generatePassword should only return digit characters");
    counts[val]++;
  }

  for (int i = 0; i < 10; ++i) {
    QVERIFY2(counts[i] > 0, "Each digit should appear at least once");
  }

  const double expected = static_cast<double>(iterations) / 10.0;
  double chi2 = 0.0;
  for (int i = 0; i < 10; ++i) {
    const double count = static_cast<double>(counts[i]);
    chi2 += (count - expected) * (count - expected) / expected;
  }
  const double chi2Critical = 25.0;
  QVERIFY2(chi2 < chi2Critical,
           qPrintable(
               QStringLiteral("Chi-square %1 exceeds critical value %2 (df=9)")
                   .arg(chi2)
                   .arg(chi2Critical)));
}

void tst_util::findBinaryInPath() {
#ifdef Q_OS_WIN
  const QString binaryName = QStringLiteral("cmd.exe");
#else
  const QString binaryName = QStringLiteral("sh");
#endif
  QString result = Util::findBinaryInPath(binaryName);
  QVERIFY2(!result.isEmpty(), "Should find a standard shell in PATH");
  QVERIFY(result.contains(binaryName));

  result = Util::findBinaryInPath("nonexistentbinary12345");
  QVERIFY(result.isEmpty());
}

void tst_util::findPasswordStore() {
  QString result = Util::findPasswordStore();
  QVERIFY(!result.isEmpty());
  QVERIFY(result.endsWith(QDir::separator()));
}

void tst_util::configIsValid() {
  QTemporaryDir tempDir;
  QVERIFY2(tempDir.isValid(), "Temporary directory should be created");

  PassStoreGuard guard(QtPassSettings::getPassStore());

  SettingGuard<bool, QtPassSettings::setUsePass> usePassGuard{
      QtPassSettings::isUsePass(), false};

  // No .gpg-id in this store => config must be invalid.
  QtPassSettings::setPassStore(tempDir.path());
  bool isValid = Util::configIsValid();
  QVERIFY2(!isValid, "Expected invalid config when .gpg-id is missing");

  // Create .gpg-id, then force invalid executable configuration.
  QFile gpgIdFile(tempDir.path() + QDir::separator() +
                  QStringLiteral(".gpg-id"));
  QVERIFY2(gpgIdFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
           "Should be able to create .gpg-id");
  gpgIdFile.write("test@example.com\n");
  gpgIdFile.close();

  SettingGuard<QString, QtPassSettings::setGpgExecutable> gpgGuard{
      QtPassSettings::getGpgExecutable(), QString()};

  isValid = Util::configIsValid();
  QVERIFY2(!isValid, "Expected invalid config when .gpg-id exists but gpg "
                     "executable is missing");

  QtPassSettings::setGpgExecutable(
      QStringLiteral("definitely_nonexistent_gpg_binary_12345"));
  isValid = Util::configIsValid();
  QVERIFY2(!isValid, "Expected invalid config when .gpg-id exists but gpg "
                     "executable is invalid");
}

void tst_util::getDirBasic() {
  QTemporaryDir tempDir;
  QVERIFY2(tempDir.isValid(),
           "Temporary directory should be created successfully");

  QFileSystemModel fileSystemModel;
  fileSystemModel.setRootPath(tempDir.path());
  StoreModel storeModel;
  storeModel.setModelAndStore(&fileSystemModel, tempDir.path());
  QVERIFY(storeModel.sourceModel() != nullptr);
  QVERIFY2(storeModel.getStore() == tempDir.path(),
           "Store path should match the set value");
  QModelIndex rootIndex = fileSystemModel.index(tempDir.path());
  QVERIFY2(rootIndex.isValid(), "Filesystem model root index should be valid");
  const QString originalStore = QtPassSettings::getPassStore();
  QtPassSettings::setPassStore(tempDir.path());

  QString result =
      Util::getDir(QModelIndex(), false, fileSystemModel, storeModel);
  QString expectedDir = QDir(tempDir.path()).absolutePath();
  if (!expectedDir.endsWith(QDir::separator())) {
    expectedDir += QDir::separator();
  }
  QVERIFY2(
      result == expectedDir,
      qPrintable(QString("Expected '%1', got '%2'").arg(expectedDir, result)));
  QtPassSettings::setPassStore(originalStore);
}

void tst_util::getDirWithIndex() {
  QTemporaryDir tempDir;
  QVERIFY2(tempDir.isValid(),
           "Temporary directory should be created successfully");

  const QString dirPath = tempDir.path();
  const QString filePath =
      QDir(dirPath).filePath(QStringLiteral("testfile.txt"));

  QFile file(filePath);
  QVERIFY2(file.open(QIODevice::WriteOnly),
           "Failed to create test file in temporary directory");
  const char testData[] = "dummy";
  const qint64 bytesWritten = file.write(testData, sizeof(testData) - 1);
  QVERIFY2(bytesWritten == static_cast<qint64>(sizeof(testData) - 1),
           "Failed to write test data to file in temporary directory");
  file.close();

  const QString originalPassStore = QtPassSettings::getPassStore();
  PassStoreGuard passStoreGuard(originalPassStore);
  QtPassSettings::setPassStore(dirPath);

  QFileSystemModel fileSystemModel;
  fileSystemModel.setRootPath(dirPath);

  StoreModel storeModel;
  storeModel.setModelAndStore(&fileSystemModel, dirPath);
  QVERIFY2(storeModel.getStore() == dirPath,
           "Store path should match the set value");

  QModelIndex sourceIndex = fileSystemModel.index(filePath);
  QVERIFY2(sourceIndex.isValid(),
           "Source index should be valid for the test file");
  QModelIndex fileIndex = storeModel.mapFromSource(sourceIndex);
  QVERIFY2(fileIndex.isValid(),
           "Proxy index should be valid for the test file");

  QString result = Util::getDir(fileIndex, false, fileSystemModel, storeModel);
  QVERIFY2(!result.isEmpty(),
           "getDir should return a non-empty directory for a valid index");
  QVERIFY(result.endsWith(QDir::separator()));

  QString expectedPath = dirPath;
  if (!expectedPath.endsWith(QDir::separator())) {
    expectedPath += QDir::separator();
  }
  QVERIFY2(
      result == expectedPath,
      qPrintable(
          QStringLiteral("Expected '%1', got '%2'").arg(expectedPath, result)));

  QModelIndex invalidIndex;
  QString invalidResult =
      Util::getDir(invalidIndex, false, fileSystemModel, storeModel);
  QString expectedForInvalid = dirPath;
  if (!expectedForInvalid.endsWith(QDir::separator())) {
    expectedForInvalid += QDir::separator();
  }
  QVERIFY2(invalidResult == expectedForInvalid,
           qPrintable(QStringLiteral("getDir should return pass store for "
                                     "invalid index. Expected '%1', got '%2'")
                          .arg(expectedForInvalid, invalidResult)));
}

void tst_util::findBinaryInPathNotFound() {
  QString result = Util::findBinaryInPath("this-binary-does-not-exist-12345");
  QVERIFY(result.isEmpty());
}

void tst_util::findPasswordStoreEnvVar() {
  QString result = Util::findPasswordStore();
  QVERIFY(!result.isEmpty());
}

void tst_util::normalizeFolderPathMultipleCalls() {
  QString result1 = Util::normalizeFolderPath("test1");
  QString result2 = Util::normalizeFolderPath("test2");
  QVERIFY(result1.endsWith(QDir::separator()));
  QVERIFY(result2.endsWith(QDir::separator()));
}

void tst_util::userInfoFullyValid() {
  UserInfo ui;
  ui.validity = 'f';
  QVERIFY(ui.fullyValid());
  ui.validity = 'u';
  QVERIFY(ui.fullyValid());
  ui.validity = '-';
  QVERIFY(!ui.fullyValid());
}

void tst_util::userInfoMarginallyValid() {
  UserInfo ui;
  ui.validity = 'm';
  QVERIFY(ui.marginallyValid());
  ui.validity = 'f';
  QVERIFY(!ui.marginallyValid());
}

void tst_util::userInfoIsValid() {
  UserInfo ui;
  ui.validity = 'f';
  QVERIFY(ui.isValid());
  ui.validity = 'm';
  QVERIFY(ui.isValid());
  ui.validity = '-';
  QVERIFY(!ui.isValid());
}

void tst_util::userInfoCreatedAndExpiry() {
  UserInfo ui;
  ui.name = "Test User";
  ui.key_id = "ABCDEF12";

  QVERIFY(!ui.created.isValid());
  QVERIFY(!ui.expiry.isValid());

  QDateTime future = QDateTime::currentDateTime().addYears(1);
  ui.expiry = future;
  QVERIFY(ui.expiry.isValid());
  QVERIFY(ui.expiry.toSecsSinceEpoch() > 0);

  QDateTime past = QDateTime::currentDateTime().addYears(-1);
  ui.created = past;
  QVERIFY(ui.created.isValid());
  QVERIFY(ui.created.toSecsSinceEpoch() > 0);
}

void tst_util::qProgressIndicatorBasic() {
  QProgressIndicator pi;
  QVERIFY(!pi.isAnimated());
}

void tst_util::qProgressIndicatorStartStop() {
  QProgressIndicator pi;
  pi.startAnimation();
  QVERIFY(pi.isAnimated());
  pi.stopAnimation();
  QVERIFY(!pi.isAnimated());
}

void tst_util::namedValueBasic() {
  NamedValue nv;
  nv.name = "key";
  nv.value = "value";
  QCOMPARE(nv.name, QString("key"));
  QCOMPARE(nv.value, QString("value"));
}

void tst_util::namedValueMultiple() {
  NamedValues nvs;
  NamedValue nv1;
  nv1.name = "user1";
  nv1.value = "pass1";
  nvs.append(nv1);
  QCOMPARE(nvs.size(), 1);
}

void tst_util::imitatePassResolveMoveDestination() {
  ImitatePass pass;
  QTemporaryDir tmpDir;
  QString srcPath = tmpDir.path() + "/test.gpg";
  QFile srcFile(srcPath);
  QVERIFY(srcFile.open(QFile::WriteOnly));
  srcFile.write("test");
  srcFile.close();

  QString destPath = tmpDir.path() + "/dest.gpg";
  QString result = pass.resolveMoveDestination(srcPath, destPath, false);
  QString expected = destPath;
  QVERIFY2(result == expected, "Destination should have .gpg extension");
}

void tst_util::imitatePassResolveMoveDestinationForce() {
  ImitatePass pass;
  QTemporaryDir tmpDir;
  QString srcPath = tmpDir.path() + "/test.gpg";
  QFile srcFile(srcPath);
  QVERIFY(srcFile.open(QFile::WriteOnly));
  srcFile.write("test");
  srcFile.close();

  QString destPath = tmpDir.path() + "/existing.gpg";
  QFile destFile(destPath);
  QVERIFY(destFile.open(QFile::WriteOnly));
  destFile.write("old");
  destFile.close();

  QString result = pass.resolveMoveDestination(srcPath, destPath, true);
  QVERIFY2(result == destPath, "Should return dest path when force=true");
}

void tst_util::imitatePassResolveMoveDestinationDestExistsNoForce() {
  ImitatePass pass;
  QTemporaryDir tmpDir;
  QString srcPath = tmpDir.path() + "/test.gpg";
  QFile srcFile(srcPath);
  QVERIFY(srcFile.open(QFile::WriteOnly));
  srcFile.write("test");
  srcFile.close();

  QString destPath = tmpDir.path() + "/existing.gpg";
  QFile destFile(destPath);
  QVERIFY(destFile.open(QFile::WriteOnly));
  destFile.write("old");
  destFile.close();

  QString result = pass.resolveMoveDestination(srcPath, destPath, false);
  QVERIFY2(result.isEmpty(),
           "Should return empty when dest exists and force=false");
}

void tst_util::imitatePassResolveMoveDestinationDir() {
  ImitatePass pass;
  QTemporaryDir tmpDir;
  QString srcPath = tmpDir.path() + "/test.gpg";
  QFile srcFile(srcPath);
  QVERIFY(srcFile.open(QFile::WriteOnly));
  srcFile.write("test");
  srcFile.close();

  QString result = pass.resolveMoveDestination(srcPath, tmpDir.path(), false);
  QVERIFY2(result == tmpDir.path() + "/test.gpg",
           "Should append filename when dest is dir");
}

void tst_util::imitatePassResolveMoveDestinationNonExistent() {
  ImitatePass pass;
  QTemporaryDir tmpDir;
  QString destPath = tmpDir.path() + "/dest.gpg";
  QString result =
      pass.resolveMoveDestination("/non/existent/path.gpg", destPath, false);
  QVERIFY2(result.isEmpty(), "Should return empty for non-existent source");
}

void tst_util::imitatePassRemoveDir() {
  ImitatePass pass;
  QTemporaryDir tmpDir;
  QString subDir = tmpDir.path() + "/testdir";
  QVERIFY(QDir().mkpath(subDir));
  QVERIFY(QDir(subDir).exists());
  bool result = pass.removeDir(subDir);
  QVERIFY(result);
  QVERIFY(!QDir(subDir).exists());
}

void tst_util::getRecipientListBasic() {
  QTemporaryDir tempDir;
  QString passStore = tempDir.path();
  QString gpgIdFile = passStore + "/.gpg-id";

  QFile file(gpgIdFile);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("ABCDEF12\n34567890\n");
  file.close();

  PassStoreGuard guard(QtPassSettings::getPassStore());
  QtPassSettings::setPassStore(passStore);
  QStringList recipients = Pass::getRecipientList(passStore);
  QCOMPARE(recipients.size(), 2);
  QCOMPARE(recipients[0], QString("ABCDEF12"));
  QCOMPARE(recipients[1], QString("34567890"));
}

void tst_util::getRecipientListEmpty() {
  QTemporaryDir tempDir;
  QString passStore = tempDir.path();
  QString gpgIdFile = passStore + "/.gpg-id";

  QFile file(gpgIdFile);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.close();

  PassStoreGuard guard(QtPassSettings::getPassStore());
  QtPassSettings::setPassStore(passStore);
  QStringList recipients = Pass::getRecipientList(passStore);
  QVERIFY(recipients.isEmpty());
}

void tst_util::getRecipientListWithComments() {
  QTemporaryDir tempDir;
  QString passStore = tempDir.path();
  QString gpgIdFile = passStore + "/.gpg-id";

  QFile file(gpgIdFile);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("ABCDEF12\n# comment\n34567890\n");
  file.close();

  PassStoreGuard guard(QtPassSettings::getPassStore());
  QtPassSettings::setPassStore(passStore);
  QStringList recipients = Pass::getRecipientList(passStore);
  QCOMPARE(recipients.size(), 2);
  QVERIFY(!recipients.contains("# comment"));
  QVERIFY(!recipients.contains("comment"));
}

void tst_util::getRecipientListInvalidKeyId() {
  QTemporaryDir tempDir;
  QString passStore = tempDir.path();
  QString gpgIdFile = passStore + "/.gpg-id";

  QFile file(gpgIdFile);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("ABCDEF12\ninvalid\n0xABCDEF123456789012\n<a@b>\nuser@qtpass@"
             "example.org\n");
  file.close();

  const QString originalPassStore = QtPassSettings::getPassStore();
  PassStoreGuard originalGuard(originalPassStore);
  QtPassSettings::setPassStore(passStore);
  QStringList recipients = Pass::getRecipientList(passStore);
  QVERIFY(!recipients.contains("invalid"));
  QVERIFY(recipients.contains("ABCDEF12"));
  QVERIFY(recipients.contains("0xABCDEF123456789012"));
  QVERIFY(recipients.contains("user@qtpass@example.org"));
}

void tst_util::isValidKeyIdBasic() {
  QVERIFY(Util::isValidKeyId("ABCDEF12"));
  QVERIFY(Util::isValidKeyId("abcdef12"));
  QVERIFY(Util::isValidKeyId("0123456789ABCDEF"));
  QVERIFY(Util::isValidKeyId("0123456789abcdef"));
}

void tst_util::isValidKeyIdWith0xPrefix() {
  QVERIFY(Util::isValidKeyId("0xABCDEF12"));
  QVERIFY(Util::isValidKeyId("0XABCDEF12"));
  QVERIFY(Util::isValidKeyId("0xabcdef12"));
  QVERIFY(Util::isValidKeyId("0Xabcdef12"));
  QVERIFY(Util::isValidKeyId("0x0123456789ABCDEF"));
}

void tst_util::isValidKeyIdWithEmail() {
  QVERIFY(Util::isValidKeyId("<a@b>"));
  QVERIFY(Util::isValidKeyId("user@qtpass@example.org"));
  QVERIFY(Util::isValidKeyId("/any/text/here"));
  QVERIFY(Util::isValidKeyId("#anything"));
  QVERIFY(Util::isValidKeyId("&anything"));
}

void tst_util::isValidKeyIdInvalid() {
  QVERIFY(!Util::isValidKeyId(""));
  QVERIFY(!Util::isValidKeyId("short"));
  QVERIFY(!Util::isValidKeyId(QString(41, 'a')));
  QVERIFY(!Util::isValidKeyId("invalidchars!"));
  QVERIFY(!Util::isValidKeyId("space in key"));
}

void tst_util::getRecipientStringCount() {
  QTemporaryDir tempDir;
  QString passStore = tempDir.path();
  QString gpgIdFile = passStore + "/.gpg-id";

  QFile file(gpgIdFile);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("ABCDEF12\n34567890\n");
  file.close();

  const QString originalPassStore = QtPassSettings::getPassStore();
  PassStoreGuard originalGuard(originalPassStore);
  QtPassSettings::setPassStore(passStore);
  int count = 0;
  QStringList parsedRecipients =
      Pass::getRecipientString(passStore, " ", &count);
  QStringList recipientsNoCount = Pass::getRecipientString(passStore, " ");

  QStringList expectedRecipients = {"ABCDEF12", "34567890"};
  // Verify count matches the expected number of parsed recipients.
  QVERIFY(count > 0);
  QCOMPARE(count, (int)expectedRecipients.size());
  // Verify both overloads return the same result
  QCOMPARE(parsedRecipients, recipientsNoCount);
  // Verify that the parsed recipients match the expected values.
  QVERIFY(parsedRecipients.contains("ABCDEF12"));
  QVERIFY(parsedRecipients.contains("34567890"));
  // Also verify that the recipients returned without count match the expected
  // values.
  QVERIFY(recipientsNoCount.contains("ABCDEF12"));
  QVERIFY(recipientsNoCount.contains("34567890"));
}

void tst_util::getGpgIdPathBasic() {
  QTemporaryDir tempDir;
  QString passStore = tempDir.path();
  QString gpgIdFile = passStore + "/.gpg-id";

  QFile file(gpgIdFile);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("ABCDEF12\n");
  file.close();

  PassStoreGuard guard(QtPassSettings::getPassStore());
  QtPassSettings::setPassStore(passStore);
  QString path = QDir::cleanPath(Pass::getGpgIdPath(passStore));
  QString expected = QDir::cleanPath(gpgIdFile);
  QVERIFY2(path == expected,
           qPrintable(QString("Expected %1, got %2").arg(expected, path)));
}

void tst_util::getGpgIdPathSubfolder() {
  QTemporaryDir tempDir;
  QString passStore = tempDir.path();
  QString subfolder = passStore + "/subfolder";
  QString gpgIdFile = subfolder + "/.gpg-id";

  QVERIFY(QDir().mkdir(subfolder));
  QFile file(gpgIdFile);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("ABCDEF12\n");
  file.close();

  PassStoreGuard guard(QtPassSettings::getPassStore());
  QtPassSettings::setPassStore(passStore);
  QString path = Pass::getGpgIdPath(subfolder + "/password.gpg");
  QVERIFY2(path == gpgIdFile,
           qPrintable(QString("Expected %1, got %2").arg(gpgIdFile, path)));
}

void tst_util::getGpgIdPathNotFound() {
  QTemporaryDir tempDir;
  QString passStore = tempDir.path();

  PassStoreGuard guard(QtPassSettings::getPassStore());
  QtPassSettings::setPassStore(passStore);
  QString path =
      QDir::cleanPath(Pass::getGpgIdPath(passStore + "/nonexistent"));
  QString expected = QDir::cleanPath(passStore + "/.gpg-id");
  QVERIFY2(path == expected,
           qPrintable(QString("Expected %1, got %2").arg(expected, path)));
}

// Tests for findBinaryInPath - verifies it correctly locates executables in
// PATH.

void tst_util::findBinaryInPathReturnedPathIsAbsolute() {
  // Verify that the returned path is absolute, not a relative fragment.
#ifdef Q_OS_WIN
  const QString binaryName = QStringLiteral("cmd.exe");
#else
  const QString binaryName = QStringLiteral("sh");
#endif
  QString result = Util::findBinaryInPath(binaryName);
  QVERIFY2(!result.isEmpty(), "Should find a standard shell");
  QFileInfo fi(result);
  QVERIFY2(
      fi.isAbsolute(),
      qPrintable(
          QStringLiteral("Returned path '%1' must be absolute").arg(result)));
}

void tst_util::findBinaryInPathReturnedPathIsExecutable() {
  // Verify the returned path satisfies the isExecutable() check that guards
  // the assignment inside the loop.
#ifdef Q_OS_WIN
  const QString binaryName = QStringLiteral("cmd.exe");
#else
  const QString binaryName = QStringLiteral("sh");
#endif
  QString result = Util::findBinaryInPath(binaryName);
  QVERIFY2(!result.isEmpty(), "Should find a standard shell");
  QFileInfo fi(result);
  QVERIFY2(
      fi.isExecutable(),
      qPrintable(
          QStringLiteral("Returned path '%1' must be executable").arg(result)));
}

void tst_util::findBinaryInPathMultipleKnownBinaries() {
  // Test finding multiple common binaries in PATH.
#ifndef Q_OS_WIN
  const QStringList binaries = {QStringLiteral("sh"), QStringLiteral("ls"),
                                QStringLiteral("cat")};
  for (const QString &bin : binaries) {
    QString result = Util::findBinaryInPath(bin);
    QVERIFY2(!result.isEmpty(),
             qPrintable(QStringLiteral("Should find '%1' in PATH").arg(bin)));
    QVERIFY2(result.contains(bin),
             qPrintable(QStringLiteral("Result '%1' should contain '%2'")
                            .arg(result, bin)));
    QVERIFY2(
        QFileInfo(result).isExecutable(),
        qPrintable(
            QStringLiteral("Result '%1' should be executable").arg(result)));
  }
#else
  QSKIP("Non-Windows binary list not applicable on Windows");
#endif
}

void tst_util::findBinaryInPathConsistency() {
  // Calling findBinaryInPath twice for the same binary must return the same
  // result, confirming the loop does not corrupt state across calls.
#ifdef Q_OS_WIN
  const QString binaryName = QStringLiteral("cmd.exe");
#else
  const QString binaryName = QStringLiteral("sh");
#endif
  QString first = Util::findBinaryInPath(binaryName);
  QString second = Util::findBinaryInPath(binaryName);
  QVERIFY2(!first.isEmpty(), "First call should find the binary");
  QCOMPARE(first, second);
}

void tst_util::findBinaryInPathResultContainsBinaryName() {
  // The returned absolute path must end with (or at least contain) the
  // binary name, ruling out any off-by-one concatenation artefact.
#ifdef Q_OS_WIN
  const QString binaryName = QStringLiteral("cmd");
#else
  const QString binaryName = QStringLiteral("sh");
#endif
  QString result = Util::findBinaryInPath(binaryName);
  QVERIFY2(!result.isEmpty(), "Should find the binary");
  QVERIFY2(
      result.endsWith(binaryName) ||
          result.endsWith(binaryName + QStringLiteral(".exe")),
      qPrintable(QStringLiteral("Path '%1' should end with binary name '%2'")
                     .arg(result, binaryName)));
}

void tst_util::findBinaryInPathTempExecutableInTempDir() {
  // Place a real executable in the same directory as "sh" (which is on the
  // cached PATH) and verify findBinaryInPath locates it.
  //
  // This test is skipped in restricted environments where writing to the "sh"
  // directory is not allowed. An alternative approach (QTemporaryDir + PATH
  // manipulation) doesn't work because Util::_env is cached on first use.
#ifndef Q_OS_WIN
  QString shPath = Util::findBinaryInPath(QStringLiteral("sh"));
  if (shPath.isEmpty()) {
    QSKIP("Cannot find 'sh' to determine a writable PATH directory");
  }
  const QString pathDir = QFileInfo(shPath).absolutePath();
  const QString uniqueName = QStringLiteral("qtpass_test_exec_") +
                             QUuid::createUuid().toString(QUuid::WithoutBraces);
  const QString uniquePath = pathDir + QDir::separator() + uniqueName;

  QFile exec(uniquePath);
  if (!exec.open(QIODevice::WriteOnly)) {
    QSKIP("Cannot write to the PATH directory containing 'sh' (need write "
          "access)");
  }
  QVERIFY2(exec.exists(), "File should exist after opening for writing");
  exec.write("#!/bin/sh\n");
  exec.close();
  exec.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                      QFileDevice::ExeOwner);

  QString result = Util::findBinaryInPath(uniqueName);

  // Remove file before assertions so it is always cleaned up.
  const bool removed = QFile::remove(uniquePath);
  QVERIFY2(
      removed,
      qPrintable(
          QStringLiteral("Failed to clean up test file '%1'").arg(uniquePath)));

  QVERIFY2(!result.isEmpty(),
           "findBinaryInPath should locate the executable placed in a PATH "
           "directory");
  QVERIFY2(result.endsWith(uniqueName),
           qPrintable(QStringLiteral("Result '%1' should end with '%2'")
                          .arg(result, uniqueName)));
  QVERIFY2(QFileInfo(result).isAbsolute(), "Result must be an absolute path");
#else
  QSKIP("Temp-executable test is Unix-only");
#endif
}

void tst_util::buildClipboardMimeDataLinux() {
#ifdef Q_OS_LINUX
  QMimeData *mime = buildClipboardMimeData(QStringLiteral("testpassword"));
  QVERIFY(mime != nullptr);
  QVERIFY2(mime->hasText(), "Mime data should contain text");
  QVERIFY2(mime->text() == "testpassword", "Text should match");
  QVERIFY2(mime->data("x-kde-passwordManagerHint") == QByteArray("secret"),
           "Linux should set password hint");
  delete mime;
#else
  QSKIP("Linux-only test");
#endif
}

void tst_util::buildClipboardMimeDataWindows() {
#ifdef Q_OS_WIN
  QMimeData *mime = buildClipboardMimeData(QStringLiteral("testpassword"));
  QVERIFY(mime != nullptr);
  QVERIFY2(mime->hasText(), "Mime data should contain text");
  QVERIFY2(mime->text() == "testpassword", "Text should match");
  QByteArray excl = mime->data("ExcludeClipboardContentFromMonitorProcessing");
  QVERIFY2(excl.size() == 4, "Windows ExcludeClipboard should be 4 bytes");
  QVERIFY2(excl == dwordBytes(1), "Windows ExcludeClipboard should be DWORD 1");
  QVERIFY(mime->hasFormat("ExcludeClipboardContentFromMonitorProcessing"));
  QVERIFY(mime->hasFormat("CanIncludeInClipboardHistory"));
  QVERIFY(mime->hasFormat("CanUploadToCloudClipboard"));
  QByteArray canHistory = mime->data("CanIncludeInClipboardHistory");
  QVERIFY2(canHistory.size() == 4,
           "CanIncludeInClipboardHistory should be 4 bytes");
  QVERIFY2(canHistory == dwordBytes(0),
           "CanIncludeInClipboardHistory should be DWORD 0");
  QByteArray cloudClip = mime->data("CanUploadToCloudClipboard");
  QVERIFY2(cloudClip.size() == 4,
           "CanUploadToCloudClipboard should be 4 bytes");
  QVERIFY2(cloudClip == dwordBytes(0),
           "CanUploadToCloudClipboard should be DWORD 0");
  delete mime;
#else
  QSKIP("Windows-only test");
#endif
}

void tst_util::buildClipboardMimeDataDword() {
#ifdef Q_OS_WIN
  QByteArray zero = dwordBytes(0);
  QVERIFY2(zero.size() == 4, "DWORD should be 4 bytes");
  QVERIFY2(zero.at(0) == char(0), "DWORD 0 should be 0x00");
  QVERIFY2(zero.at(1) == char(0), "DWORD 0 should be 0x00");
  QVERIFY2(zero.at(2) == char(0), "DWORD 0 should be 0x00");
  QVERIFY2(zero.at(3) == char(0), "DWORD 0 should be 0x00");

  QByteArray one = dwordBytes(1);
  QVERIFY2(one.size() == 4, "DWORD should be 4 bytes");
  QVERIFY2(one.at(0) == char(1), "DWORD 1 should be 0x01");
  QVERIFY2(one.at(1) == char(0), "DWORD 1 should be 0x00");
  QVERIFY2(one.at(2) == char(0), "DWORD 1 should be 0x00");
  QVERIFY2(one.at(3) == char(0), "DWORD 1 should be 0x00");
#else
  QSKIP("Windows-only test");
#endif
}

void tst_util::buildClipboardMimeDataMac() {
#ifdef Q_OS_MAC
  QMimeData *mime = buildClipboardMimeData(QStringLiteral("testpassword"));
  QVERIFY(mime != nullptr);
  QVERIFY2(mime->hasText(), "Mime data should contain text");
  QVERIFY2(mime->text() == "testpassword", "Text should match");
  QVERIFY2(mime->hasFormat("application/x-nspasteboard-concealed-type"),
           "macOS should have concealed type format");
  QVERIFY2(mime->data("application/x-nspasteboard-concealed-type") ==
               QByteArray(),
           "macOS concealed type should be empty");
  delete mime;
#else
  QSKIP("macOS-only test");
#endif
}

void tst_util::utilRegexEnsuresGpg() {
  const QRegularExpression &rex = Util::endsWithGpg();
  QVERIFY2(rex.isValid(), "Regex should be valid");
  QVERIFY2(rex.match("file.gpg").hasMatch(), "Should match .gpg extension");
  QVERIFY2(!rex.match("file.txt").hasMatch(), "Should not match .txt");
  QVERIFY2(!rex.match("test.gpgx").hasMatch(), "Should not match .gpgx");
}

void tst_util::utilRegexProtocol() {
  const QRegularExpression &rex = Util::protocolRegex();
  QVERIFY2(rex.isValid(), "Protocol regex should be valid");
  QVERIFY2(rex.match("http://example.com").hasMatch(), "Should match http://");
  QVERIFY2(rex.match("https://secure.com").hasMatch(), "Should match https://");
  QVERIFY2(rex.match("ssh://host").hasMatch(), "Should match ssh://");
  QVERIFY2(!rex.match("://no-protocol").hasMatch(), "Should not match invalid");
}

void tst_util::utilRegexNewLines() {
  const QRegularExpression &rex = Util::newLinesRegex();
  QVERIFY2(rex.isValid(), "Newlines regex should be valid");
  QVERIFY2(rex.match("\n").hasMatch(), "Should match newline");
  QVERIFY2(rex.match("line1\nline2").hasMatch(),
           "Should match embedded newline");
}

void tst_util::reencryptPathNormalization() {
  QTemporaryDir tempDir;
  QVERIFY2(tempDir.isValid(), "Temporary directory should be created");

  QString basePath = tempDir.path();
  QString withExtraSlashes = basePath + "/./subdir/../";
  QString cleaned = QDir::cleanPath(withExtraSlashes);
  QString normalized = QDir::cleanPath(basePath);
  QVERIFY2(cleaned == normalized,
           qPrintable(QString("cleanPath should normalize: expected %1, got %2")
                          .arg(normalized, cleaned)));
}

void tst_util::reencryptPathAbsolutePath() {
  QTemporaryDir tempDir;
  QVERIFY2(tempDir.isValid(), "Temporary directory should be created");

  QString tempPath = tempDir.path();
  QDir(tempPath).mkdir("testdir");
  QString relativePathFromTemp = tempPath + "/testdir";
  QDir dir;
  QString result = QDir::cleanPath(QDir(relativePathFromTemp).absolutePath());
  QString expected = QDir::cleanPath(tempPath + "/testdir");
  QVERIFY2(
      result == expected,
      qPrintable(
          QString("Absolute path: expected %1, got %2").arg(expected, result)));
}

// Tests targeting the const-ref refactor of findBinaryInPath.
// The PR changed the signature from findBinaryInPath(QString) to
// findBinaryInPath(const QString &). These tests verify that callers using
// const-qualified variables continue to work correctly.

void tst_util::findBinaryInPathWithConstQStringRef() {
  // Pass a const-qualified variable to verify the const-ref signature compiles
  // and executes correctly.
#ifdef Q_OS_WIN
  const QString binaryName = QStringLiteral("cmd.exe");
#else
  const QString binaryName = QStringLiteral("sh");
#endif
  const QString result = Util::findBinaryInPath(binaryName);
  QVERIFY2(!result.isEmpty(),
           "findBinaryInPath should find shell with const QString& arg");
  QVERIFY2(result.contains(binaryName),
           "Result should contain the binary name");
  QVERIFY2(QFileInfo(result).isAbsolute(), "Returned path should be absolute");
}

void tst_util::findBinaryInPathEmptyString() {
  // Boundary case: passing an empty string should return an empty result
  // without crashing. This exercises the loop guard when 'binary' is empty.
  const QString result = Util::findBinaryInPath(QString());
  QVERIFY2(result.isEmpty(),
           "findBinaryInPath(\"\") should return empty QString");
}

void tst_util::findBinaryInPathStringLiteral() {
  // Passing a string literal directly (temporary, binds to const ref) must
  // work identically to passing a named variable.
#ifndef Q_OS_WIN
  const QString resultDirect = Util::findBinaryInPath("sh");
  const QString binaryName = QStringLiteral("sh");
  const QString resultNamed = Util::findBinaryInPath(binaryName);
  QVERIFY2(!resultDirect.isEmpty(),
           "findBinaryInPath with string literal should succeed");
  QCOMPARE(resultDirect, resultNamed);
#else
  QSKIP("Unix-only test");
#endif
}

void tst_util::setEnvVarAdds() {
  TestPass pass;
  pass.callSetEnvVar(QStringLiteral("TEST_KEY="), QStringLiteral("hello"));
  QStringList env = pass.environment();
  QVERIFY2(env.contains(QStringLiteral("TEST_KEY=hello")),
           "setEnvVar should append key=value when absent");
}

void tst_util::setEnvVarUpdates() {
  TestPass pass;
  pass.callSetEnvVar(QStringLiteral("TEST_KEY="), QStringLiteral("first"));
  pass.callSetEnvVar(QStringLiteral("TEST_KEY="), QStringLiteral("second"));
  QStringList env = pass.environment();
  QVERIFY2(env.contains(QStringLiteral("TEST_KEY=second")),
           "setEnvVar should update existing entry");
  QVERIFY2(!env.contains(QStringLiteral("TEST_KEY=first")),
           "setEnvVar should remove old value");
  QCOMPARE(env.filter(QStringLiteral("TEST_KEY=")).size(), 1);
}

void tst_util::setEnvVarRemoves() {
  TestPass pass;
  pass.callSetEnvVar(QStringLiteral("TEST_KEY="), QStringLiteral("value"));
  pass.callSetEnvVar(QStringLiteral("TEST_KEY="), QString());
  QStringList env = pass.environment();
  QVERIFY2(env.filter(QStringLiteral("TEST_KEY=")).isEmpty(),
           "setEnvVar with empty value should remove the entry");
}

void tst_util::setEnvVarNoopOnMissingRemove() {
  TestPass pass;
  QStringList before = pass.environment();
  pass.callSetEnvVar(QStringLiteral("NONEXISTENT_KEY="), QString());
  QStringList after = pass.environment();
  QCOMPARE(before, after);
}

void tst_util::updateEnvSetsExpectedVars() {
  TestPass pass;
  QTemporaryDir tmpDir;
  QVERIFY(tmpDir.isValid());
  PassStoreGuard storeGuard(QtPassSettings::getPassStore());
  QtPassSettings::setPassStore(tmpDir.path());

  QStringList env = pass.environment();
  QVERIFY2(env.filter(QStringLiteral("PASSWORD_STORE_DIR=")).size() == 1,
           "updateEnv should set PASSWORD_STORE_DIR");
  QVERIFY2(env.filter(QStringLiteral("PASSWORD_STORE_DIR="))
               .first()
               .contains(tmpDir.path()),
           "updateEnv should set PASSWORD_STORE_DIR to configured store path");
  QVERIFY2(
      env.filter(QStringLiteral("PASSWORD_STORE_GENERATED_LENGTH=")).size() ==
          1,
      "updateEnv should set PASSWORD_STORE_GENERATED_LENGTH");
  QVERIFY2(env.filter(QStringLiteral("PASSWORD_STORE_CHARACTER_SET=")).size() ==
               1,
           "updateEnv should set PASSWORD_STORE_CHARACTER_SET");
}

void tst_util::updateEnvEmptyCustomCharsetFallsBackToAllChars() {
  TestPass pass;
  PasswordConfiguration original = QtPassSettings::getPasswordConfiguration();
  struct ConfigRollback {
    PasswordConfiguration value;
    ~ConfigRollback() { QtPassSettings::setPasswordConfiguration(value); }
  } rollback{original};

  PasswordConfiguration config = original;
  config.selected = PasswordConfiguration::CUSTOM;
  config.Characters[PasswordConfiguration::CUSTOM] = QString();
  QtPassSettings::setPasswordConfiguration(config);

  QStringList env = pass.environment();
  QStringList charsetEntries =
      env.filter(QStringLiteral("PASSWORD_STORE_CHARACTER_SET="));
  QVERIFY2(
      charsetEntries.size() == 1,
      "PASSWORD_STORE_CHARACTER_SET should be set even when CUSTOM is empty");
  QString val = charsetEntries.first().mid(
      QStringLiteral("PASSWORD_STORE_CHARACTER_SET=").size());
  QVERIFY2(!val.isEmpty(),
           "charset should fall back to ALLCHARS, not be empty");
}

void tst_util::updateEnvWslenvContainsRequiredVars() {
  TestPass pass;
  const QStringList env = pass.environment();
  // Use startsWith to avoid substring false-positives (e.g. MY_WSLENV=).
  const QStringList wslenvEntries =
      env.filter(QRegularExpression(QStringLiteral("^WSLENV=")));
  QVERIFY2(!wslenvEntries.isEmpty(),
           "At least one WSLENV entry expected after Pass construction");
  // Verify Pass::Pass() merged all required keys with correct WSL flags.
  const QString wslenv = wslenvEntries.first();
  QVERIFY2(wslenv.contains(QStringLiteral("PASSWORD_STORE_DIR/p")),
           "WSLENV should include PASSWORD_STORE_DIR/p");
  QVERIFY2(wslenv.contains(QStringLiteral("PASSWORD_STORE_GENERATED_LENGTH/w")),
           "WSLENV should include PASSWORD_STORE_GENERATED_LENGTH/w");
  QVERIFY2(wslenv.contains(QStringLiteral("PASSWORD_STORE_CHARACTER_SET/w")),
           "WSLENV should include PASSWORD_STORE_CHARACTER_SET/w");
}

// --- gpgErrorMessage tests ---

void tst_util::gpgErrorMessageKeyExpiredStatusToken() {
  QString err = "[GNUPG:] KEY_EXPIRED 1234567890\n"
                "gpg: key DEADBEEF: key has expired\n"
                "gpg: [stdin]: encryption failed: Unusable public key";
  QString msg = gpgErrorMessage(err);
  QVERIFY2(!msg.isEmpty(), "Should recognise KEY_EXPIRED status token");
  QVERIFY2(msg.contains("expired", Qt::CaseInsensitive),
           qPrintable("Expected 'expired' in: " + msg));
}

void tst_util::gpgErrorMessageKeyRevokedStatusToken() {
  QString err = "[GNUPG:] KEY_REVOKED\n"
                "gpg: [stdin]: encryption failed: Unusable public key";
  QString msg = gpgErrorMessage(err);
  QVERIFY2(!msg.isEmpty(), "Should recognise KEY_REVOKED status token");
  QVERIFY2(msg.contains("revoked", Qt::CaseInsensitive),
           qPrintable("Expected 'revoked' in: " + msg));
}

void tst_util::gpgErrorMessageNoPubkeyStatusToken() {
  QString err = "[GNUPG:] NO_PUBKEY DEADBEEFDEADBEEF\n"
                "gpg: DEADBEEF: skipped: No public key";
  QString msg = gpgErrorMessage(err);
  QVERIFY2(!msg.isEmpty(), "Should recognise NO_PUBKEY status token");
  QVERIFY2(msg.contains("not found", Qt::CaseInsensitive) ||
               msg.contains("invalid", Qt::CaseInsensitive),
           qPrintable("Expected 'not found' or 'invalid' in: " + msg));
}

void tst_util::gpgErrorMessageInvRecpStatusToken() {
  QString err = "[GNUPG:] INV_RECP 10 DEADBEEF\n"
                "gpg: [stdin]: encryption failed: Unusable public key";
  QString msg = gpgErrorMessage(err);
  QVERIFY2(!msg.isEmpty(), "Should recognise INV_RECP status token");
  QVERIFY2(msg.contains("not found", Qt::CaseInsensitive) ||
               msg.contains("invalid", Qt::CaseInsensitive),
           qPrintable("Expected 'not found' or 'invalid' in: " + msg));
}

void tst_util::gpgErrorMessageFailureStatusToken() {
  QString err = "[GNUPG:] FAILURE encrypt 67108949";
  QString msg = gpgErrorMessage(err);
  QVERIFY2(!msg.isEmpty(), "Should recognise FAILURE status token");
  QVERIFY2(msg.contains("failed", Qt::CaseInsensitive),
           qPrintable("Expected 'failed' in: " + msg));
}

void tst_util::gpgErrorMessageKeyExpiredFallback() {
  QString err = "gpg: key DEADBEEF: key has expired\n"
                "gpg: [stdin]: encryption failed: Unusable public key";
  QString msg = gpgErrorMessage(err);
  QVERIFY2(!msg.isEmpty(), "Should recognise 'key has expired' fallback");
  QVERIFY2(msg.contains("expired", Qt::CaseInsensitive),
           qPrintable("Expected 'expired' in: " + msg));
}

void tst_util::gpgErrorMessageRevokedFallback() {
  QString err = "gpg: key DEADBEEF: key has been revoked\n"
                "gpg: [stdin]: encryption failed: Unusable public key";
  QString msg = gpgErrorMessage(err);
  QVERIFY2(!msg.isEmpty(), "Should recognise 'key has been revoked' fallback");
  QVERIFY2(msg.contains("revoked", Qt::CaseInsensitive),
           qPrintable("Expected 'revoked' in: " + msg));
}

void tst_util::gpgErrorMessageNoPubkeyFallback() {
  QString err = "gpg: DEADBEEF: skipped: No public key\n"
                "gpg: [stdin]: encryption failed: No public key";
  QString msg = gpgErrorMessage(err);
  QVERIFY2(!msg.isEmpty(), "Should recognise 'No public key' fallback");
  QVERIFY2(msg.contains("not found", Qt::CaseInsensitive) ||
               msg.contains("invalid", Qt::CaseInsensitive),
           qPrintable("Expected 'not found' or 'invalid' in: " + msg));
}

void tst_util::gpgErrorMessageEncryptionFailedFallback() {
  QString err = "gpg: [stdin]: encryption failed: Unusable public key";
  QString msg = gpgErrorMessage(err);
  QVERIFY2(!msg.isEmpty(),
           "Should recognise generic 'encryption failed' fallback");
  QVERIFY2(msg.contains("failed", Qt::CaseInsensitive),
           qPrintable("Expected 'failed' in: " + msg));
}

void tst_util::gpgErrorMessageUnknownReturnsEmpty() {
  QString err = "some unrelated process error output";
  QString msg = gpgErrorMessage(err);
  QVERIFY2(msg.isEmpty(),
           "Should return empty string for unrecognised GPG output");
}

void tst_util::gpgErrorMessageStatusTokenTakesPriorityOverFallback() {
  // KEY_EXPIRED token present alongside a generic "encryption failed" line —
  // the specific expired message should be returned, not the generic fallback.
  QString err = "[GNUPG:] KEY_EXPIRED 1234567890\n"
                "gpg: [stdin]: encryption failed: Unusable public key";
  QString msg = gpgErrorMessage(err);
  QVERIFY2(msg.contains("expired", Qt::CaseInsensitive),
           "KEY_EXPIRED token should take priority and mention 'expired'");
}

QTEST_MAIN(tst_util)
#include "tst_util.moc"
