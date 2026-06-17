// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>
#ifndef Q_OS_WIN
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#endif

#include "../../../src/enums.h"
#include "../../../src/filecontent.h"
#include "../../../src/imitatepass.h"
#include "../../../src/pass.h"
#include "../../../src/passwordconfiguration.h"
#include "../../../src/pathvalidator.h"
#include "../../../src/qprogressindicator.h"
#include "../../../src/qtpass.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/simpletransaction.h"
#include "../../../src/sshauthsock.h"
#include "../../../src/templateio.h"
#include "../../../src/userinfo.h"
#include "../../../src/util.h"

using GrepResults = QList<QPair<QString, QStringList>>;
Q_DECLARE_METATYPE(GrepResults)

static constexpr int TEST_SIGNAL_TIMEOUT_MS = 3000;
static constexpr int DISTRIBUTION_MIN_PERCENT = 80;
static constexpr int DISTRIBUTION_MAX_PERCENT = 120;
static constexpr int PERCENT_BASE = 100;
static constexpr int RANDOMNESS_TEST_SAMPLE_COUNT = 200;
static constexpr int RANDOMNESS_TEST_PASSWORD_LENGTH = 32;
// Permissive chi-square cutoff for df=9: set above the p=0.995 critical
// value (~23.59) to reduce false test failures while still catching
// meaningful distribution bias.
static constexpr double CHI_SQUARE_PERMISSIVE_THRESHOLD_DF9 = 30.0;

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
    QProcessEnvironment environment() {
      updateEnv();
      return exec.environment();
    }
    void callFinished(int id, int exitCode, const QString &out,
                      const QString &err) {
      finished(id, exitCode, out, err);
    }
    void callPassFinished(int id, int exitCode, const QString &out,
                          const QString &err) {
      Pass::finished(id, exitCode, out, err);
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
  void isLaunchableWebUrlAccepts();
  void isLaunchableWebUrlRejects();
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
  // parseGrepOutput
  void parseGrepOutputEmpty();
  void parseGrepOutputSingleEntry();
  void parseGrepOutputMultipleEntries();
  void parseGrepOutputAnsiStripped();
  void parseGrepOutputHeaderColonStripped();
  void parseGrepOutputCrlfHandled();
  void parseGrepOutputOrphanMatchesIgnored();
  void parseGrepOutputEmptyMatchLinesIgnored();
  void parseGrepOutputLastEntryIncluded();
  void parseGrepOutputEmbeddedBlueNotHeader();
  void parseGrepOutputPlainTextHeaders();
  // Pass::finished PASS_GREP exit-code handling
  void passFinishedGrepNoMatchEmitsEmpty();
  void passFinishedGrepErrorEmitsProcessError();
  void passFinishedGrepSuccessEmitsResults();
  // ImitatePass::Grep / helpers
  void grepMatchFileFailedDecryptReturnsEmpty();
  void grepScanStoreEmptyDirReturnsEmpty();
  void grepImitatePassEmptyStoreEmitsEmpty();
  void grepImitatePassInvalidRegexEmitsEmpty();
  // SSH_AUTH_SOCK auto-probe (issue #543)
  void sshAuthSockOverrideRoundtrip();
  void sshAuthSockOverrideEmptyByDefault();
  void initialiseSshAuthSockHonoursExistingEnv();
  void initialiseSshAuthSockUsesOverride();
  void initialiseSshAuthSockNoOverrideNoEnvProbes();
  void initialiseSshAuthSockOverrideSkipsAgentValidation();
  void initialiseSshAuthSockEmptyOverrideFallsThrough();
  // Path-traversal hardening (security)
  void isPathInStoreHappyPath();
  void isPathInStoreRejectsDotDotEscape();
  void isPathInStoreRejectsAbsoluteOutside();
  void isPathInStoreRejectsSymlinkEscape();
  void isPathInStoreAllowsNewChild();
  void isPathInStoreRejectsEmptyArgs();
  // .gpg-id permission hardening (security)
  void writeGpgIdFileSetsOwnerOnlyPerms();
  // SSH_AUTH_SOCK override soft-validation (Settings dialog warning)
  void sshAuthSockOverrideStatusDoesNotExist();
  void sshAuthSockOverrideStatusRegularFileRejected();
  void sshAuthSockOverrideStatusNotReadable();
  void sshAuthSockOverrideStatusValid();
  void readTemplatesNoFile();
  void readTemplatesSingleSection();
  void readTemplatesMultipleSections();
  void readTemplatesEmptySectionIgnored();
  void readTemplatesCommentsIgnored();
  void writeTemplatesRoundTrip();
  void writeTemplatesEmptyHash();
  void writeTemplatesSortedKeys();
  void getFolderTemplateInCurrent();
  void getFolderTemplateInParent();
  void getFolderTemplateNoneFound();
  void getFolderTemplateCommentIgnored();
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
  qRegisterMetaType<GrepResults>("GrepResults");
  // Qt5 QSignalSpy looks up by the normalized signal type string, not the alias
  qRegisterMetaType<GrepResults>("QList<QPair<QString,QStringList>>");
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
  SettingGuard<bool, QtPassSettings::setUsePwgen> disablePwgenGuard{
      QtPassSettings::load().usePwgen, false};

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
  bool foundDifferent = false;

  // Distribution sanity check for small charset: all characters should appear
  // and none should be excessively under/over-represented.
  QMap<QChar, int> frequencies;
  for (const QChar &ch : randomCharset) {
    frequencies.insert(ch, 0);
  }

  // Keep total draws high enough for a stable sanity check while staying fast:
  // RANDOMNESS_TEST_SAMPLE_COUNT * RANDOMNESS_TEST_PASSWORD_LENGTH = 6400
  // generated characters. For a 4-character charset this yields an expected
  // ~1600 occurrences per character, which is sufficient to detect obvious
  // under/over-representation in this unit test.
  const int samplePasswords = RANDOMNESS_TEST_SAMPLE_COUNT;
  const int sampleLength = RANDOMNESS_TEST_PASSWORD_LENGTH;
  for (int i = 0; i < samplePasswords; ++i) {
    const QString candidate =
        pass.generatePassword(sampleLength, randomCharset);
    QVERIFY2(candidate.length() == sampleLength,
             "Generated password should have expected length");
    if (!foundDifferent && candidate != first) {
      foundDifferent = true;
    }
    for (const QChar &ch : candidate) {
      QVERIFY2(randomCharset.contains(ch),
               "Generated password contains character outside the specified "
               "charset");
      ++frequencies[ch];
    }
  }

  const int totalChars = samplePasswords * sampleLength;
  const int expectedPerChar = totalChars / randomCharset.size();
  // Use ±20% bounds for stricter distribution check
  const int minAllowed =
      expectedPerChar * DISTRIBUTION_MIN_PERCENT / PERCENT_BASE;
  const int maxAllowed =
      expectedPerChar * DISTRIBUTION_MAX_PERCENT / PERCENT_BASE;
  for (const QChar &ch : randomCharset) {
    const int count = frequencies.value(ch);
    QVERIFY2(
        count > 0,
        "A charset character never appeared; randomness coverage is broken");
    QVERIFY2(count >= minAllowed && count <= maxAllowed,
             "Character distribution is unexpectedly skewed");
  }

  QVERIFY2(
      foundDifferent,
      "Multiple generated passwords were identical; randomness may be broken");
}

void tst_util::boundedRandom() {
  SettingGuard<bool, QtPassSettings::setUsePwgen> disablePwgenGuard{
      QtPassSettings::load().usePwgen, false};

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
  // For 10 buckets, df = 9. The chi-square critical value at p = 0.995 is
  // about 23.59. We use 30.0 as a more permissive threshold to
  // reduce false failures from random variation while still catching bias.
  QVERIFY2(
      chi2 < CHI_SQUARE_PERMISSIVE_THRESHOLD_DF9,
      qPrintable(
          QStringLiteral("Chi-square %1 exceeds permissive threshold %2 (df=9)")
              .arg(chi2)
              .arg(CHI_SQUARE_PERMISSIVE_THRESHOLD_DF9)));
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
      QtPassSettings::load().usePass, false};

  // No .gpg-id in this store => config must be invalid.
  QtPassSettings::setPassStore(tempDir.path());
  bool isValid = Util::configIsValid(QtPassSettings::load());
  QVERIFY2(!isValid, "Expected invalid config when .gpg-id is missing");

  // Create .gpg-id, then force invalid executable configuration.
  QFile gpgIdFile(tempDir.path() + QDir::separator() +
                  QStringLiteral(".gpg-id"));
  QVERIFY2(gpgIdFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
           "Should be able to create .gpg-id");
  gpgIdFile.write("test@example.com\n");
  gpgIdFile.close();

  const QString savedGpgExe = QtPassSettings::load().gpgExecutable;
  struct RestoreGpg {
    QString saved;
    ~RestoreGpg() {
      AppSettings s = QtPassSettings::load();
      s.gpgExecutable = saved;
      QtPassSettings::save(s);
    }
  } gpgRestore{savedGpgExe};

  {
    AppSettings s = QtPassSettings::load();
    s.gpgExecutable = QString();
    QtPassSettings::save(s);
  }
  isValid = Util::configIsValid(QtPassSettings::load());
  QVERIFY2(!isValid, "Expected invalid config when .gpg-id exists but gpg "
                     "executable is missing");

  {
    AppSettings s = QtPassSettings::load();
    s.gpgExecutable = QStringLiteral("definitely_nonexistent_gpg_binary_12345");
    QtPassSettings::save(s);
  }
  isValid = Util::configIsValid(QtPassSettings::load());
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

  QString result = Util::getDir(QModelIndex(), false, fileSystemModel,
                                storeModel, tempDir.path());
  QString expectedDir = QDir(tempDir.path()).absolutePath();
  if (!expectedDir.endsWith(QDir::separator())) {
    expectedDir += QDir::separator();
  }
  QVERIFY2(
      result == expectedDir,
      qPrintable(QString("Expected '%1', got '%2'").arg(expectedDir, result)));
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

  QString result =
      Util::getDir(fileIndex, false, fileSystemModel, storeModel, dirPath);
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
      Util::getDir(invalidIndex, false, fileSystemModel, storeModel, dirPath);
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

  // Verify fields were set correctly
  QVERIFY2(ui.name == QString("Test User"),
           "UserInfo name field should be set.");
  QVERIFY2(ui.key_id == QString("ABCDEF12"),
           "UserInfo key_id field should be set.");

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

  QStringList recipients = Pass::getRecipientList(passStore, passStore);
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

  QStringList recipients = Pass::getRecipientList(passStore, passStore);
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

  QStringList recipients = Pass::getRecipientList(passStore, passStore);
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

  QStringList recipients = Pass::getRecipientList(passStore, passStore);
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
  // Boundary check: one character above the currently accepted maximum
  // hexadecimal key-id length should be rejected.
  constexpr int kTooLongKeyIdLength = 41; // current max (40) + 1
  QVERIFY(!Util::isValidKeyId(""));
  QVERIFY(!Util::isValidKeyId("short"));
  QVERIFY(!Util::isValidKeyId(QString(kTooLongKeyIdLength, 'a')));
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

  int count = 0;
  QStringList parsedRecipients =
      Pass::getRecipientString(passStore, passStore, " ", &count);
  QStringList recipientsNoCount =
      Pass::getRecipientString(passStore, passStore, " ");

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

  QString path = QDir::cleanPath(Pass::getGpgIdPath(passStore, passStore));
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

  QString path = QDir::cleanPath(
      Pass::getGpgIdPath(subfolder + "/password.gpg", passStore));
  QString expected = QDir::cleanPath(gpgIdFile);
  QVERIFY2(path == expected,
           qPrintable(QString("Expected %1, got %2").arg(expected, path)));
}

void tst_util::getGpgIdPathNotFound() {
  QTemporaryDir tempDir;
  QString passStore = tempDir.path();

  QString path = QDir::cleanPath(
      Pass::getGpgIdPath(passStore + "/nonexistent", passStore));
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
  // Avoid writing into real system PATH directories from tests.
  // A robust variant of this test requires refactoring Util::_env to allow
  // test-time PATH injection before first use.
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
  const QString uniqueName =
      QStringLiteral("qtpass_test_exec_") +
      QUuid::createUuid().toString().remove('{').remove('}');
  const QString uniquePath = pathDir + QDir::separator() + uniqueName;

  QFile exec(uniquePath);
  if (!exec.open(QIODevice::WriteOnly)) {
    QSKIP("Cannot write to the PATH directory containing 'sh' (need write "
          "access)");
  }
  const QByteArray scriptContent = QByteArrayLiteral("#!/bin/sh\n");
  const qint64 bytesWritten = exec.write(scriptContent);
  if (bytesWritten != scriptContent.size()) {
    exec.close();
    exec.remove();
    QVERIFY2(false, "Failed to write executable content");
  }
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

void tst_util::isLaunchableWebUrlAccepts() {
  const QStringList accepted = {
      QStringLiteral("https://example.com"),
      QStringLiteral("http://example.com"),
      QStringLiteral("https://example.com/path?q=1#frag"),
      QStringLiteral("https://nas.local:8080"),
      QStringLiteral("  https://example.com  "), // surrounding whitespace
      QStringLiteral("HTTPS://example.com"),     // scheme case-insensitive
      QStringLiteral("HtTp://Example.com"),
  };
  for (const QString &url : accepted) {
    QVERIFY2(
        Util::isLaunchableWebUrl(url),
        qPrintable(QStringLiteral("expected launchable web URL: %1").arg(url)));
  }
}

void tst_util::isLaunchableWebUrlRejects() {
  QVERIFY2(!Util::isLaunchableWebUrl(""), "empty");
  QVERIFY2(!Util::isLaunchableWebUrl("   "), "whitespace only");
  // Scheme-less inputs must be rejected (no auto-prefixing).
  QVERIFY2(!Util::isLaunchableWebUrl("www.example.com"), "scheme-less");
  QVERIFY2(!Util::isLaunchableWebUrl("example.com"), "bare host");
  // Non-web schemes the launcher must never hand to the OS.
  QVERIFY2(!Util::isLaunchableWebUrl("file:///etc/passwd"), "file");
  QVERIFY2(!Util::isLaunchableWebUrl("javascript:alert(1)"), "javascript");
  QVERIFY2(!Util::isLaunchableWebUrl("data:text/html,<b>x</b>"), "data");
  QVERIFY2(!Util::isLaunchableWebUrl("ftp://host/file"), "ftp");
  QVERIFY2(!Util::isLaunchableWebUrl("ssh://host"), "ssh");
  QVERIFY2(!Util::isLaunchableWebUrl("webdav://host"), "webdav");
  // Embedded credentials leak into browser history.
  QVERIFY2(!Util::isLaunchableWebUrl("https://user:pass@example.com"), "creds");
  // Missing host.
  QVERIFY2(!Util::isLaunchableWebUrl("https://"), "no host");
  // Control-character injection.
  QVERIFY2(!Util::isLaunchableWebUrl("https://example.com\r\nHost: evil"),
           "CRLF");
  QVERIFY2(
      !Util::isLaunchableWebUrl(QString("https://e.com") + QChar(0) + ".com"),
      "NUL");
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
  QVERIFY(QDir(tempPath).mkdir("testdir"));
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
  const QProcessEnvironment env = pass.environment();
  QVERIFY2(env.value(QStringLiteral("TEST_KEY")) == QStringLiteral("hello"),
           "setEnvVar should set key=value when absent");
}

void tst_util::setEnvVarUpdates() {
  TestPass pass;
  pass.callSetEnvVar(QStringLiteral("TEST_KEY="), QStringLiteral("first"));
  pass.callSetEnvVar(QStringLiteral("TEST_KEY="), QStringLiteral("second"));
  const QProcessEnvironment env = pass.environment();
  QCOMPARE(env.value(QStringLiteral("TEST_KEY")), QStringLiteral("second"));
}

void tst_util::setEnvVarRemoves() {
  TestPass pass;
  pass.callSetEnvVar(QStringLiteral("TEST_KEY="), QStringLiteral("value"));
  pass.callSetEnvVar(QStringLiteral("TEST_KEY="), QString());
  const QProcessEnvironment env = pass.environment();
  QVERIFY2(!env.contains(QStringLiteral("TEST_KEY")),
           "setEnvVar with empty value should remove the entry");
}

void tst_util::setEnvVarNoopOnMissingRemove() {
  TestPass pass;
  const QProcessEnvironment before = pass.environment();
  pass.callSetEnvVar(QStringLiteral("NONEXISTENT_KEY="), QString());
  QCOMPARE(pass.environment(), before);
}

void tst_util::updateEnvSetsExpectedVars() {
  TestPass pass;
  QTemporaryDir tmpDir;
  QVERIFY(tmpDir.isValid());
  PassStoreGuard storeGuard(QtPassSettings::getPassStore());
  QtPassSettings::setPassStore(tmpDir.path());
  AppSettings s = QtPassSettings::load();
  pass.init(s);

  const QProcessEnvironment env = pass.environment();
  QVERIFY2(env.contains(QStringLiteral("PASSWORD_STORE_DIR")),
           "updateEnv should set PASSWORD_STORE_DIR");
  QCOMPARE(QDir::cleanPath(env.value(QStringLiteral("PASSWORD_STORE_DIR"))),
           QDir::cleanPath(tmpDir.path()));
  QVERIFY2(env.contains(QStringLiteral("PASSWORD_STORE_GENERATED_LENGTH")),
           "updateEnv should set PASSWORD_STORE_GENERATED_LENGTH");
  QVERIFY2(env.contains(QStringLiteral("PASSWORD_STORE_CHARACTER_SET")),
           "updateEnv should set PASSWORD_STORE_CHARACTER_SET");
}

void tst_util::updateEnvEmptyCustomCharsetFallsBackToAllChars() {
  TestPass pass;
  PasswordConfiguration original = QtPassSettings::getPasswordConfiguration();
  struct ConfigRollback {
    PasswordConfiguration value;
    ~ConfigRollback() {
      AppSettings s = QtPassSettings::load();
      s.passwordConfiguration = value;
      QtPassSettings::save(s);
    }
  } rollback{original};

  PasswordConfiguration config = original;
  config.selected = PasswordConfiguration::CUSTOM;
  config.Characters[PasswordConfiguration::CUSTOM] = QString();
  AppSettings toSave = QtPassSettings::load();
  toSave.passwordConfiguration = config;
  QtPassSettings::save(toSave);
  AppSettings s = QtPassSettings::load();
  pass.init(s);

  const QProcessEnvironment env = pass.environment();
  QVERIFY2(
      env.contains(QStringLiteral("PASSWORD_STORE_CHARACTER_SET")),
      "PASSWORD_STORE_CHARACTER_SET should be set even when CUSTOM is empty");
  QVERIFY2(!env.value(QStringLiteral("PASSWORD_STORE_CHARACTER_SET")).isEmpty(),
           "charset should fall back to ALLCHARS, not be empty");
}

void tst_util::updateEnvWslenvContainsRequiredVars() {
  TestPass pass;
  AppSettings s = QtPassSettings::load();
  pass.init(s);
  const QProcessEnvironment env = pass.environment();
  QVERIFY2(env.contains(QStringLiteral("WSLENV")),
           "At least one WSLENV entry expected after Pass construction");
  // Verify Pass::Pass() merged all required keys with correct WSL flags.
  const QString wslenv = env.value(QStringLiteral("WSLENV"));
  QVERIFY2(wslenv.contains(QStringLiteral("PASSWORD_STORE_DIR/p")),
           "WSLENV should include PASSWORD_STORE_DIR/p");
  QVERIFY2(wslenv.contains(QStringLiteral("PASSWORD_STORE_GENERATED_LENGTH/w")),
           "WSLENV should include PASSWORD_STORE_GENERATED_LENGTH/w");
  QVERIFY2(wslenv.contains(QStringLiteral("PASSWORD_STORE_CHARACTER_SET/w")),
           "WSLENV should include PASSWORD_STORE_CHARACTER_SET/w");
}

// --- gpgErrorMessage tests ---

void tst_util::gpgErrorMessageKeyExpiredStatusToken() {
  QString err = "[GNUPG:] KEYEXPIRED 1234567890\n"
                "gpg: key DEADBEEF: key has expired\n"
                "gpg: [stdin]: encryption failed: Unusable public key";
  QString msg = gpgErrorMessage(err);
  QVERIFY2(!msg.isEmpty(), "Should recognise KEYEXPIRED status token");
  QVERIFY2(msg.contains("expired", Qt::CaseInsensitive),
           qPrintable("Expected 'expired' in: " + msg));
}

void tst_util::gpgErrorMessageKeyRevokedStatusToken() {
  QString err = "[GNUPG:] KEYREVOKED\n"
                "gpg: [stdin]: encryption failed: Unusable public key";
  QString msg = gpgErrorMessage(err);
  QVERIFY2(!msg.isEmpty(), "Should recognise KEYREVOKED status token");
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
  // reason code 5 = expired
  QString errExpired = "[GNUPG:] INV_RECP 5 DEADBEEF\n"
                       "gpg: [stdin]: encryption failed: Unusable public key";
  QString msgExpired = gpgErrorMessage(errExpired);
  QVERIFY2(!msgExpired.isEmpty(), "Should recognise INV_RECP 5 (expired)");
  QVERIFY2(msgExpired.contains("expired", Qt::CaseInsensitive),
           qPrintable("Expected 'expired' for INV_RECP 5: " + msgExpired));

  // reason code 4 = revoked
  QString errRevoked = "[GNUPG:] INV_RECP 4 DEADBEEF\n"
                       "gpg: [stdin]: encryption failed: Unusable public key";
  QString msgRevoked = gpgErrorMessage(errRevoked);
  QVERIFY2(!msgRevoked.isEmpty(), "Should recognise INV_RECP 4 (revoked)");
  QVERIFY2(msgRevoked.contains("revoked", Qt::CaseInsensitive),
           qPrintable("Expected 'revoked' for INV_RECP 4: " + msgRevoked));

  // generic reason code
  QString errGeneric = "[GNUPG:] INV_RECP 10 DEADBEEF\n"
                       "gpg: [stdin]: encryption failed: Unusable public key";
  QString msgGeneric = gpgErrorMessage(errGeneric);
  QVERIFY2(!msgGeneric.isEmpty(), "Should recognise INV_RECP status token");
  QVERIFY2(msgGeneric.contains("not found", Qt::CaseInsensitive) ||
               msgGeneric.contains("invalid", Qt::CaseInsensitive),
           qPrintable("Expected 'not found' or 'invalid' in: " + msgGeneric));
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
  // KEYEXPIRED token present alongside a generic "encryption failed" line —
  // the specific expired message should be returned, not the generic fallback.
  QString err = "[GNUPG:] KEYEXPIRED 1234567890\n"
                "gpg: [stdin]: encryption failed: Unusable public key";
  QString msg = gpgErrorMessage(err);
  QVERIFY2(msg.contains("expired", Qt::CaseInsensitive),
           "KEYEXPIRED token should take priority and mention 'expired'");
}

// --- parseGrepOutput tests ---

void tst_util::parseGrepOutputEmpty() {
  auto results = parseGrepOutput(QString());
  QVERIFY(results.isEmpty());
}

void tst_util::parseGrepOutputSingleEntry() {
  // Simulate: header with ANSI blue, then one match line
  const QString raw = QStringLiteral("\x1B[94mmy/entry\x1B[0m:\nsome match\n");
  auto results = parseGrepOutput(raw);
  QCOMPARE(results.size(), 1);
  QCOMPARE(results[0].first, QStringLiteral("my/entry"));
  QCOMPARE(results[0].second.size(), 1);
  QCOMPARE(results[0].second[0], QStringLiteral("some match"));
}

void tst_util::parseGrepOutputMultipleEntries() {
  const QString raw = QStringLiteral("\x1B[94mentry/a\x1B[0m:\nline1\nline2\n"
                                     "\x1B[94mentry/b\x1B[0m:\nlineX\n");
  auto results = parseGrepOutput(raw);
  QCOMPARE(results.size(), 2);
  QCOMPARE(results[0].first, QStringLiteral("entry/a"));
  QCOMPARE(results[0].second.size(), 2);
  QCOMPARE(results[1].first, QStringLiteral("entry/b"));
  QCOMPARE(results[1].second.size(), 1);
}

void tst_util::parseGrepOutputAnsiStripped() {
  // Match line itself contains ANSI colour (grep highlights the match)
  const QString raw =
      QStringLiteral("\x1B[94mfoo\x1B[0m:\n\x1B[1mhi\x1B[0m there\n");
  auto results = parseGrepOutput(raw);
  QCOMPARE(results.size(), 1);
  QCOMPARE(results[0].second[0], QStringLiteral("hi there"));
}

void tst_util::parseGrepOutputHeaderColonStripped() {
  const QString raw = QStringLiteral("\x1B[94msome/path:\x1B[0m\nvalue\n");
  auto results = parseGrepOutput(raw);
  QCOMPARE(results.size(), 1);
  // Trailing colon must be removed from the entry name
  QVERIFY2(
      !results[0].first.endsWith(':'),
      qPrintable("Entry should not end with ':', got: " + results[0].first));
}

void tst_util::parseGrepOutputCrlfHandled() {
  const QString raw = QStringLiteral("\x1B[94mentry\x1B[0m:\r\nmatch line\r\n");
  auto results = parseGrepOutput(raw);
  QCOMPARE(results.size(), 1);
  QVERIFY2(!results[0].second[0].contains('\r'),
           "Match line must not contain CR");
}

void tst_util::parseGrepOutputOrphanMatchesIgnored() {
  // Lines before any header should be silently dropped
  const QString raw =
      QStringLiteral("orphan line\n\x1B[94mentry\x1B[0m:\nreal match\n");
  auto results = parseGrepOutput(raw);
  QCOMPARE(results.size(), 1);
  QCOMPARE(results[0].second.size(), 1);
  QCOMPARE(results[0].second[0], QStringLiteral("real match"));
}

void tst_util::parseGrepOutputEmptyMatchLinesIgnored() {
  const QString raw = QStringLiteral("\x1B[94mentry\x1B[0m:\n\n  \nmatch\n");
  auto results = parseGrepOutput(raw);
  QCOMPARE(results.size(), 1);
  QCOMPARE(results[0].second.size(), 1);
}

void tst_util::parseGrepOutputLastEntryIncluded() {
  // Ensure final entry is flushed even without a trailing newline
  const QString raw = QStringLiteral("\x1B[94mfinal\x1B[0m:\nvalue");
  auto results = parseGrepOutput(raw);
  QCOMPARE(results.size(), 1);
  QCOMPARE(results[0].first, QStringLiteral("final"));
}

void tst_util::parseGrepOutputEmbeddedBlueNotHeader() {
  // Match line contains \x1B[94m mid-line (grep highlights the search term in
  // blue); must not be mistaken for a header — previous matches must not be
  // lost
  const QString raw = QStringLiteral(
      "\x1B[94mentry/a\x1B[0m:\nfirst match\ncontains \x1B[94mblue\x1B[0m "
      "highlight\n\x1B[94mentry/b\x1B[0m:\nother\n");
  auto results = parseGrepOutput(raw);
  QCOMPARE(results.size(), 2);
  QCOMPARE(results[0].first, QStringLiteral("entry/a"));
  QCOMPARE(results[0].second.size(), 2);
  QCOMPARE(results[0].second[1], QStringLiteral("contains blue highlight"));
}

void tst_util::parseGrepOutputPlainTextHeaders() {
  // pass grep without ANSI colours (e.g. NO_COLOR or non-TTY output)
  const QString raw =
      QStringLiteral("entry/a:\n  match one\n  match two\nentry/b:\n  other\n");
  auto results = parseGrepOutput(raw);
  QCOMPARE(results.size(), 2);
  QCOMPARE(results[0].first, QStringLiteral("entry/a"));
  QCOMPARE(results[0].second.size(), 2);
  QCOMPARE(results[1].first, QStringLiteral("entry/b"));
  QCOMPARE(results[1].second.size(), 1);
}

// --- Pass::finished PASS_GREP exit-code tests ---

void tst_util::passFinishedGrepNoMatchEmitsEmpty() {
  TestPass pass;
  QSignalSpy spy(&pass, &Pass::finishedGrep);
  QSignalSpy errSpy(&pass, &Pass::processErrorExit);
  // exit code 1 = no matches (standard grep behaviour)
  pass.callPassFinished(static_cast<int>(Enums::PASS_GREP), 1, QString(),
                        QString());
  QCOMPARE(spy.count(), 1);
  QCOMPARE(errSpy.count(), 0);
  const auto results = spy[0][0].value<GrepResults>();
  QVERIFY(results.isEmpty());
}

void tst_util::passFinishedGrepErrorEmitsProcessError() {
  TestPass pass;
  QSignalSpy spy(&pass, &Pass::finishedGrep);
  QSignalSpy errSpy(&pass, &Pass::processErrorExit);
  // exit code 2 = real grep error
  pass.callPassFinished(static_cast<int>(Enums::PASS_GREP), 2, QString(),
                        QStringLiteral("some gpg error"));
  QCOMPARE(errSpy.count(), 1);
  QCOMPARE(spy.count(), 1);
  QVERIFY(spy[0][0].value<GrepResults>().isEmpty());
}

void tst_util::passFinishedGrepSuccessEmitsResults() {
  TestPass pass;
  QSignalSpy spy(&pass, &Pass::finishedGrep);
  // Simulate output from 'pass grep' with one matching entry
  const QString out =
      QStringLiteral("\x1B[94mwork/github\x1B[0m:\ntoken: abc123\n");
  pass.callPassFinished(static_cast<int>(Enums::PASS_GREP), 0, out, QString());
  // Verify signal was emitted
  QVERIFY2(spy.count() == 1, "finishedGrep should be emitted exactly once");
  const auto results = spy[0][0].value<GrepResults>();
  QVERIFY2(results.size() == 1, "Should have one matching entry");
  const auto &entry = results[0];
  QVERIFY2(entry.first == "work/github", "Entry path should be 'work/github'");
  QVERIFY2(entry.second.size() == 1, "Should have one matched line");
  QVERIFY2(entry.second[0] == "token: abc123",
           "Matched line should be 'token: abc123'");
}

// --- ImitatePass helper / Grep tests ---

void tst_util::grepMatchFileFailedDecryptReturnsEmpty() {
  // grepMatchFile with a non-existent file: executeBlocking fails (rc != 0)
  // so the result must be empty regardless of the regex
  QRegularExpression rx(QStringLiteral(".*"));
  const QProcessEnvironment env;
  const QStringList matches =
      ImitatePass::grepMatchFile(env, QStringLiteral("/nonexistent/gpg"),
                                 QStringLiteral("/no/such.gpg"), rx);
  QVERIFY(matches.isEmpty());
}

void tst_util::grepScanStoreEmptyDirReturnsEmpty() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  QRegularExpression rx(QStringLiteral(".*"));
  const QProcessEnvironment env;
  const auto results = ImitatePass::grepScanStore(
      env, QStringLiteral("/nonexistent/gpg"), tmp.path(), rx);
  QVERIFY(results.isEmpty());
}

void tst_util::grepImitatePassEmptyStoreEmitsEmpty() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  PassStoreGuard guard(QtPassSettings::getPassStore());
  QtPassSettings::setPassStore(tmp.path());

  TestPass pass;
  QSignalSpy spy(&pass, &Pass::finishedGrep);
  pass.Grep(QStringLiteral("anything"), false);
  // Wait up to 3 s for the background thread to emit
  QVERIFY2(spy.wait(TEST_SIGNAL_TIMEOUT_MS),
           "Timed out waiting for Pass::finishedGrep signal");
  QCOMPARE(spy.count(), 1);
  const auto results = spy[0][0].value<GrepResults>();
  QVERIFY(results.isEmpty());
}

void tst_util::grepImitatePassInvalidRegexEmitsEmpty() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  PassStoreGuard guard(QtPassSettings::getPassStore());
  QtPassSettings::setPassStore(tmp.path());

  TestPass pass;
  QSignalSpy spy(&pass, &Pass::finishedGrep);
  // An invalid regex (unmatched '[') must still emit an empty result
  pass.Grep(QStringLiteral("[invalid"), false);
  QVERIFY2(spy.wait(TEST_SIGNAL_TIMEOUT_MS),
           "Timed out waiting for Pass::finishedGrep signal");
  QCOMPARE(spy.count(), 1);
  const auto results = spy[0][0].value<GrepResults>();
  QVERIFY(results.isEmpty());
}

// ---------------------------------------------------------------------------
// SSH_AUTH_SOCK auto-probe tests (issue #543)
//
// RAII helper restores SSH_AUTH_SOCK + the override setting around each test
// so we don't pollute the developer's environment or each other's state.
// ---------------------------------------------------------------------------
namespace testutils {
class SshAuthSockGuard {
public:
  SshAuthSockGuard()
      : hadEnv_(qEnvironmentVariableIsSet("SSH_AUTH_SOCK")),
        prevEnv_(qgetenv("SSH_AUTH_SOCK")),
        prevOverride_(QtPassSettings::load().sshAuthSockOverride) {}
  ~SshAuthSockGuard() {
    if (hadEnv_) {
      qputenv("SSH_AUTH_SOCK", prevEnv_);
    } else {
      qunsetenv("SSH_AUTH_SOCK");
    }
    AppSettings s = QtPassSettings::load();
    s.sshAuthSockOverride = prevOverride_;
    QtPassSettings::save(s);
  }
  SshAuthSockGuard(const SshAuthSockGuard &) = delete;
  auto operator=(const SshAuthSockGuard &) -> SshAuthSockGuard & = delete;

private:
  bool hadEnv_;
  QByteArray prevEnv_;
  QString prevOverride_;
};
} // namespace testutils

/**
 * @brief getter/setter roundtrip for sshAuthSockOverride.
 */
void tst_util::sshAuthSockOverrideRoundtrip() {
  testutils::SshAuthSockGuard guard;
  const QString sentinel =
      QStringLiteral("/tmp/qtpass-test-sock-") + QUuid::createUuid().toString();
  AppSettings s = QtPassSettings::load();
  s.sshAuthSockOverride = sentinel;
  QtPassSettings::save(s);
  QCOMPARE(QtPassSettings::load().sshAuthSockOverride, sentinel);
  s = QtPassSettings::load();
  s.sshAuthSockOverride = QString{};
  QtPassSettings::save(s);
  QCOMPARE(QtPassSettings::load().sshAuthSockOverride, QString{});
}

/**
 * @brief default value of sshAuthSockOverride is empty.
 */
void tst_util::sshAuthSockOverrideEmptyByDefault() {
  testutils::SshAuthSockGuard guard;
  AppSettings s = QtPassSettings::load();
  s.sshAuthSockOverride = QString{};
  QtPassSettings::save(s);
  QCOMPARE(QtPassSettings::load().sshAuthSockOverride, QString{});
}

/**
 * @brief initialiseSshAuthSock leaves the env unchanged when SSH_AUTH_SOCK is
 *        already set.
 */
void tst_util::initialiseSshAuthSockHonoursExistingEnv() {
  testutils::SshAuthSockGuard guard;
  const QByteArray existing("/tmp/qtpass-existing-sock");
  qputenv("SSH_AUTH_SOCK", existing);
  // Even if an override is supplied, the existing env wins.
  SshAuthSock::initialise(QStringLiteral("/tmp/qtpass-override-sock"));
  QCOMPARE(qgetenv("SSH_AUTH_SOCK"), existing);
}

/**
 * @brief initialiseSshAuthSock applies the user-supplied override when env is
 *        unset.
 */
void tst_util::initialiseSshAuthSockUsesOverride() {
  testutils::SshAuthSockGuard guard;
  qunsetenv("SSH_AUTH_SOCK");
  const QString override = QStringLiteral("/tmp/qtpass-override-sock");
  SshAuthSock::initialise(override);
  QCOMPARE(qgetenv("SSH_AUTH_SOCK"), override.toUtf8());
}

/**
 * @brief With env unset and no override, initialiseSshAuthSock either probes
 *        gpgconf successfully (CI may have it installed) or leaves env empty.
 *        Both outcomes are valid; the test verifies it doesn't crash and
 *        the resulting state is consistent.
 */
void tst_util::initialiseSshAuthSockNoOverrideNoEnvProbes() {
  testutils::SshAuthSockGuard guard;
  qunsetenv("SSH_AUTH_SOCK");
  SshAuthSock::initialise();
  // Either gpgconf set it (non-empty), or it stayed empty. Both fine.
  // The contract is: never crash, never set garbage.
  const QByteArray result = qgetenv("SSH_AUTH_SOCK");
  if (!result.isEmpty()) {
    // If anything was set, it should look like a path (start with /).
    QVERIFY2(result.startsWith('/'),
             qPrintable(QStringLiteral("Probed value should be a path: ") +
                        QString::fromUtf8(result)));
  }
}

/**
 * @brief An explicit override is adopted verbatim — no ssh-add validation
 *        runs against it. The user explicitly asked for THIS path, so
 *        QtPass trusts it even if no agent is currently listening (e.g.
 *        the user starts their agent after launching QtPass).
 */
void tst_util::initialiseSshAuthSockOverrideSkipsAgentValidation() {
  testutils::SshAuthSockGuard guard;
  qunsetenv("SSH_AUTH_SOCK");
  // A path that almost certainly has no listener — validation would fail.
  // Override should win regardless.
  const QString deadSocket =
      QStringLiteral("/tmp/qtpass-deliberately-dead-sock-") +
      QUuid::createUuid().toString();
  SshAuthSock::initialise(deadSocket);
  QCOMPARE(qgetenv("SSH_AUTH_SOCK"), deadSocket.toUtf8());
}

/**
 * @brief An empty override string should not be treated as a "set" override —
 *        it should fall through to the auto-probe.
 */
void tst_util::initialiseSshAuthSockEmptyOverrideFallsThrough() {
  testutils::SshAuthSockGuard guard;
  qunsetenv("SSH_AUTH_SOCK");
  SshAuthSock::initialise();
  // After calling: SSH_AUTH_SOCK is either set by gpgconf probe or unset.
  // Both are acceptable; importantly, it must NOT be set to the empty string.
  if (qEnvironmentVariableIsSet("SSH_AUTH_SOCK")) {
    QVERIFY(!qgetenv("SSH_AUTH_SOCK").isEmpty());
  }
}

/**
 * @brief isPathInStore returns true when the candidate is the store root or
 *        a path strictly inside it.
 */
void tst_util::isPathInStoreHappyPath() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QString root = store.path();
  // Root itself.
  QVERIFY(PathValidator::isPathInStore(root, root));
  // A child file (not yet existing).
  QVERIFY(PathValidator::isPathInStore(root, root + "/foo.gpg"));
  // A grandchild.
  QDir(root).mkpath("nested/dir");
  QVERIFY(PathValidator::isPathInStore(root, root + "/nested/dir/bar.gpg"));
}

/**
 * @brief User-typed names containing .. components must not resolve outside
 *        the store.
 */
void tst_util::isPathInStoreRejectsDotDotEscape() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QString root = store.path();
  QVERIFY(!PathValidator::isPathInStore(root, root + "/../escape.gpg"));
  QVERIFY(!PathValidator::isPathInStore(root, root + "/sub/../../escape.gpg"));
  QVERIFY(!PathValidator::isPathInStore(
      root, root + "/../" + QFileInfo(root).fileName() + "-sibling/file"));
}

/**
 * @brief Absolute paths outside the store must be rejected.
 */
void tst_util::isPathInStoreRejectsAbsoluteOutside() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QString root = store.path();
  QVERIFY(!PathValidator::isPathInStore(root, QStringLiteral("/etc/passwd")));
  QVERIFY(!PathValidator::isPathInStore(root,
                                        QStringLiteral("/tmp/elsewhere.gpg")));
}

/**
 * @brief A symlink inside the store pointing outside must be rejected
 *        because canonicalisation resolves the link.
 */
void tst_util::isPathInStoreRejectsSymlinkEscape() {
#ifdef Q_OS_WIN
  QSKIP("symlink creation requires elevation on Windows");
#else
  QTemporaryDir store;
  QTemporaryDir outside;
  QVERIFY(store.isValid());
  QVERIFY(outside.isValid());
  const QString root = store.path();
  // Create a symlink inside the store that points to a path outside.
  const QString linkPath = root + "/escape";
  QVERIFY(QFile::link(outside.path(), linkPath));
  // Path itself, and any child under it, must be rejected.
  QVERIFY(!PathValidator::isPathInStore(root, linkPath));
  QVERIFY(!PathValidator::isPathInStore(root, linkPath + "/sneaky.gpg"));
#endif
}

/**
 * @brief A not-yet-existing child whose parent is inside the store is
 *        accepted — this is the common "create new password" case.
 */
void tst_util::isPathInStoreAllowsNewChild() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QString root = store.path();
  QDir(root).mkpath("sub");
  QVERIFY(PathValidator::isPathInStore(root, root + "/sub/new-file.gpg"));
  QVERIFY(
      PathValidator::isPathInStore(root, root + "/sub/deeper/yet-deeper.gpg"));
}

/**
 * @brief Empty arguments are rejected without crashing.
 */
void tst_util::isPathInStoreRejectsEmptyArgs() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  QVERIFY(!PathValidator::isPathInStore(QString(), store.path() + "/foo"));
  QVERIFY(!PathValidator::isPathInStore(store.path(), QString()));
  QVERIFY(!PathValidator::isPathInStore(QString(), QString()));
  // Non-existent root.
  QVERIFY(!PathValidator::isPathInStore(QStringLiteral("/no/such/dir"),
                                        QStringLiteral("/no/such/dir/foo")));
}

/**
 * @brief writeGpgIdFile should lock the produced .gpg-id to owner-only
 *        permissions, regardless of the process umask. On Windows
 *        setPermissions is best-effort and Qt's Unix-permission bits don't
 *        round-trip, so the assertion is Unix-only.
 */
void tst_util::writeGpgIdFileSetsOwnerOnlyPerms() {
#ifdef Q_OS_WIN
  QSKIP("Unix permission bits don't round-trip through Qt on Windows");
#else
  QTemporaryDir tempDir;
  QVERIFY2(tempDir.isValid(), "tempDir should be valid after creation");
  const QString gpgIdFile = tempDir.path() + "/.gpg-id";

  // Force a permissive umask so we know the close() would have produced
  // 0644 in the absence of our setPermissions call.
  const mode_t oldUmask = ::umask(0022);

  UserInfo user;
  user.key_id = QStringLiteral("ABCDEF1234567890");
  user.enabled = true;
  user.have_secret = true;
  QList<UserInfo> users{user};

  ImitatePass pass;
  pass.writeGpgIdFile(gpgIdFile, users);

  ::umask(oldUmask);

  QVERIFY2(QFile::exists(gpgIdFile),
           "gpg id file must exist after writeGpgIdFile");
  const QFile::Permissions perms = QFile(gpgIdFile).permissions();
  QVERIFY2(perms.testFlag(QFile::ReadOwner), "expected ReadOwner to be set");
  QVERIFY2(perms.testFlag(QFile::WriteOwner), "expected WriteOwner to be set");
  QVERIFY2(!perms.testFlag(QFile::ReadGroup), "expected ReadGroup to be unset");
  QVERIFY2(!perms.testFlag(QFile::WriteGroup),
           "expected WriteGroup to be unset");
  QVERIFY2(!perms.testFlag(QFile::ReadOther), "expected ReadOther to be unset");
  QVERIFY2(!perms.testFlag(QFile::WriteOther),
           "expected WriteOther to be unset");
  QVERIFY2(!perms.testFlag(QFile::ExeOwner), "expected ExeOwner to be unset");
  QVERIFY2(!perms.testFlag(QFile::ExeGroup), "expected ExeGroup to be unset");
  QVERIFY2(!perms.testFlag(QFile::ExeOther), "expected ExeOther to be unset");
#endif
}

// ---------------------------------------------------------------------------
// Util::sshAuthSockOverrideStatus tests
//
// Backs the Settings dialog soft-warning logic added in #1439. The dialog
// shows a non-blocking warning when the user-typed override path is bogus
// (missing, unreadable, or not a Unix domain socket) but still saves the
// value as entered. These tests drive the pure validation function directly
// so we don't need a Qt event loop / QMessageBox spy.
// ---------------------------------------------------------------------------

/**
 * @brief A non-existent path is classified DoesNotExist.
 */
void tst_util::sshAuthSockOverrideStatusDoesNotExist() {
  QTemporaryDir tmp;
  QVERIFY2(tmp.isValid(), "tmp dir should be valid");
  QCOMPARE(SshAuthSock::overrideStatus(tmp.path() + "/no-such-socket"),
           SshAuthSock::OverrideStatus::DoesNotExist);
}

/**
 * @brief A regular file that exists but isn't a Unix domain socket is
 *        rejected on Unix. Skipped on Windows where the socket check is a
 *        no-op (ssh-agent uses a named pipe there).
 */
void tst_util::sshAuthSockOverrideStatusRegularFileRejected() {
#ifdef Q_OS_WIN
  QSKIP("socket-type check is Unix-only");
#else
  QTemporaryDir tmp;
  QVERIFY2(tmp.isValid(), "tmp dir should be valid");
  const QString filePath = tmp.path() + "/regular-file";
  QFile f(filePath);
  QVERIFY2(f.open(QIODevice::WriteOnly), "should be able to create a file");
  f.close();
  QCOMPARE(SshAuthSock::overrideStatus(filePath),
           SshAuthSock::OverrideStatus::NotUnixDomainSocket);
#endif
}

/**
 * @brief A file that exists but has no read permission is classified
 *        NotReadable. Skipped on Windows because Qt's permission bits
 *        don't round-trip the same way.
 */
void tst_util::sshAuthSockOverrideStatusNotReadable() {
#ifdef Q_OS_WIN
  QSKIP("Unix permission bits don't round-trip on Windows");
#else
  // Root user on Linux ignores read-mode bits, so the test would
  // falsely return Valid. Skip in that case.
  if (::geteuid() == 0) {
    QSKIP("root sees all files as readable; skip");
  }
  QTemporaryDir tmp;
  QVERIFY2(tmp.isValid(), "tmp dir should be valid");
  const QString filePath = tmp.path() + "/unreadable";
  QFile f(filePath);
  QVERIFY2(f.open(QIODevice::WriteOnly), "should be able to create a file");
  f.close();
  QVERIFY2(QFile::setPermissions(filePath, QFile::Permissions{}),
           "should be able to chmod 0 on the file");
  QCOMPARE(SshAuthSock::overrideStatus(filePath),
           SshAuthSock::OverrideStatus::NotReadable);
  // Restore something so QTemporaryDir can clean up.
  QFile::setPermissions(filePath, QFile::ReadOwner | QFile::WriteOwner);
#endif
}

/**
 * @brief A real Unix domain socket (bind(2)) is classified Valid.
 *        Unix-only.
 */
void tst_util::sshAuthSockOverrideStatusValid() {
#ifdef Q_OS_WIN
  QSKIP("socket creation API differs on Windows");
#else
  QTemporaryDir tmp;
  QVERIFY2(tmp.isValid(), "tmp dir should be valid");
  const QString sockPath = tmp.path() + "/agent.sock";
  const QByteArray sockBytes = sockPath.toLocal8Bit();

  // sun_path is typically 108 bytes on Linux; QTemporaryDir gives us a
  // short /tmp/qttest_XXXXXX prefix, so this should fit easily.
  QVERIFY2(sockBytes.size() < static_cast<int>(sizeof(sockaddr_un{}.sun_path)),
           "socket path too long for sockaddr_un");

  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  QVERIFY2(fd >= 0, "socket(AF_UNIX) failed");

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::memcpy(addr.sun_path, sockBytes.constData(),
              static_cast<size_t>(sockBytes.size()));

  const int br = ::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
  if (br != 0) {
    ::close(fd);
    QFAIL("bind(AF_UNIX) failed");
  }

  QCOMPARE(SshAuthSock::overrideStatus(sockPath),
           SshAuthSock::OverrideStatus::Valid);

  ::close(fd);
  // QTemporaryDir will rm -rf the directory on destruction, removing the
  // socket file along with it.
#endif
}

void tst_util::readTemplatesNoFile() {
  QTemporaryDir tmp;
  QVERIFY2(tmp.isValid(), "tmp dir must be valid");
  auto result = TemplateIO::readTemplates(tmp.path());
  QVERIFY2(result.isEmpty(), "missing .templates file must return empty hash");
}

void tst_util::readTemplatesSingleSection() {
  QTemporaryDir tmp;
  QVERIFY2(tmp.isValid(), "tmp dir must be valid");
  QFile f(QDir(tmp.path()).filePath(".templates"));
  QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Text),
           ".templates must open");
  QTextStream out(&f);
  out << "[login]\nusername\npassword\n";
  f.close();

  auto result = TemplateIO::readTemplates(tmp.path());
  QVERIFY2(result.size() == 1, "one section expected");
  QVERIFY2(result.contains("login"), "section 'login' must be present");
  QCOMPARE(result.value("login"), QStringList({"username", "password"}));
}

void tst_util::readTemplatesMultipleSections() {
  QTemporaryDir tmp;
  QVERIFY2(tmp.isValid(), "tmp dir must be valid");
  QFile f(QDir(tmp.path()).filePath(".templates"));
  QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Text),
           ".templates must open");
  QTextStream out(&f);
  out << "[web]\nurl\nusername\n\n[ssh]\nhost\nport\n";
  f.close();

  auto result = TemplateIO::readTemplates(tmp.path());
  QVERIFY2(result.size() == 2, "two sections expected");
  QCOMPARE(result.value("web"), QStringList({"url", "username"}));
  QCOMPARE(result.value("ssh"), QStringList({"host", "port"}));
}

void tst_util::readTemplatesEmptySectionIgnored() {
  QTemporaryDir tmp;
  QVERIFY2(tmp.isValid(), "tmp dir must be valid");
  QFile f(QDir(tmp.path()).filePath(".templates"));
  QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Text),
           ".templates must open");
  QTextStream out(&f);
  out << "[]\nfield\n[valid]\nkey\n";
  f.close();

  QTest::ignoreMessage(
      QtWarningMsg, QRegularExpression("Empty template section in "
                                       "\\.templates file, ignoring fields"));
  auto result = TemplateIO::readTemplates(tmp.path());
  QVERIFY2(!result.contains(""), "empty section name must be ignored");
  QVERIFY2(result.contains("valid"), "valid section must still be parsed");
}

void tst_util::readTemplatesCommentsIgnored() {
  QTemporaryDir tmp;
  QVERIFY2(tmp.isValid(), "tmp dir must be valid");
  QFile f(QDir(tmp.path()).filePath(".templates"));
  QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Text),
           ".templates must open");
  QTextStream out(&f);
  out << "# top-level comment\n[section]\n# inline comment\nfield\n";
  f.close();

  auto result = TemplateIO::readTemplates(tmp.path());
  QVERIFY2(result.contains("section"), "section must be parsed");
  QCOMPARE(result.value("section"), QStringList({"field"}));
}

void tst_util::writeTemplatesRoundTrip() {
  QTemporaryDir tmp;
  QVERIFY2(tmp.isValid(), "tmp dir must be valid");
  QHash<QString, QStringList> original;
  original.insert("login", {"username", "password"});
  original.insert("ssh", {"host", "port"});

  QVERIFY2(TemplateIO::writeTemplates(tmp.path(), original),
           "write must succeed");
  auto roundTripped = TemplateIO::readTemplates(tmp.path());
  QCOMPARE(roundTripped.value("login"), original.value("login"));
  QCOMPARE(roundTripped.value("ssh"), original.value("ssh"));
}

void tst_util::writeTemplatesEmptyHash() {
  QTemporaryDir tmp;
  QVERIFY2(tmp.isValid(), "tmp dir must be valid");
  QHash<QString, QStringList> empty;
  QVERIFY2(TemplateIO::writeTemplates(tmp.path(), empty),
           "write of empty hash must succeed");
  auto result = TemplateIO::readTemplates(tmp.path());
  QVERIFY2(result.isEmpty(), "reading back empty write must yield empty hash");
}

void tst_util::writeTemplatesSortedKeys() {
  QTemporaryDir tmp;
  QVERIFY2(tmp.isValid(), "tmp dir must be valid");
  QHash<QString, QStringList> tmpl;
  tmpl.insert("zebra", {"z"});
  tmpl.insert("alpha", {"a"});
  QVERIFY2(TemplateIO::writeTemplates(tmp.path(), tmpl), "write must succeed");

  QFile f(QDir(tmp.path()).filePath(".templates"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
           ".templates must open");
  QString content = QString::fromUtf8(f.readAll());
  f.close();
  QVERIFY2(content.indexOf("[alpha]") != -1,
           "[alpha] section must be present in written file");
  QVERIFY2(content.indexOf("[zebra]") != -1,
           "[zebra] section must be present in written file");
  QVERIFY2(content.indexOf("[alpha]") < content.indexOf("[zebra]"),
           "sections must be written in alphabetical order");
}

void tst_util::getFolderTemplateInCurrent() {
  QTemporaryDir store;
  QVERIFY2(store.isValid(), "store dir must be valid");
  QDir storeDir(store.path());
  storeDir.mkdir("sub");
  const QString subPath = storeDir.filePath("sub");

  QFile f(QDir(subPath).filePath(".default_template"));
  QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Text), "file must open");
  QTextStream out(&f);
  out << "mytemplate\n";
  f.close();

  QString result = TemplateIO::getFolderTemplate(subPath, store.path());
  QCOMPARE(result, QStringLiteral("mytemplate"));
}

void tst_util::getFolderTemplateInParent() {
  QTemporaryDir store;
  QVERIFY2(store.isValid(), "store dir must be valid");
  QDir storeDir(store.path());
  storeDir.mkpath("a/b");
  const QString deepPath = storeDir.filePath("a/b");

  QFile f(QDir(storeDir.filePath("a")).filePath(".default_template"));
  QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Text), "file must open");
  QTextStream out(&f);
  out << "parenttemplate\n";
  f.close();

  QString result = TemplateIO::getFolderTemplate(deepPath, store.path());
  QCOMPARE(result, QStringLiteral("parenttemplate"));
}

void tst_util::getFolderTemplateNoneFound() {
  QTemporaryDir store;
  QVERIFY2(store.isValid(), "store dir must be valid");
  QDir storeDir(store.path());
  storeDir.mkdir("sub");
  const QString subPath = storeDir.filePath("sub");

  QString result = TemplateIO::getFolderTemplate(subPath, store.path());
  QVERIFY2(result.isEmpty(), "no .default_template must return empty string");
}

void tst_util::getFolderTemplateCommentIgnored() {
  QTemporaryDir store;
  QVERIFY2(store.isValid(), "store dir must be valid");
  QDir storeDir(store.path());
  storeDir.mkdir("sub");
  const QString subPath = storeDir.filePath("sub");

  QFile f(QDir(subPath).filePath(".default_template"));
  QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Text), "file must open");
  QTextStream out(&f);
  out << "# this is a comment\n";
  f.close();

  QString result = TemplateIO::getFolderTemplate(subPath, store.path());
  QVERIFY2(result.isEmpty(),
           ".default_template with only a comment must return empty string");
}

QTEST_MAIN(tst_util)
#include "tst_util.moc"
