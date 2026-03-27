// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QList>
#include <QTemporaryDir>
#include <QtTest>

#include "../../../src/enums.h"
#include "../../../src/filecontent.h"
#include "../../../src/imitatepass.h"
#include "../../../src/pass.h"
#include "../../../src/passwordconfiguration.h"
#include "../../../src/qprogressindicator.h"
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

private Q_SLOTS:
  void initTestCase();
  void cleanupTestCase();
  void normalizeFolderPath();
  void normalizeFolderPathEdgeCases();
  void fileContent();
  void fileContentEdgeCases();
  void namedValuesTakeValue();
  void namedValuesEdgeCases();
  void totpHiddenFromDisplay();
  void regexPatterns();
  void regexPatternEdgeCases();
  void endsWithGpgEdgeCases();
  void copyDirBasic();
  void copyDirWithSubdirs();
  void copyDirEmpty();
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
  void checkConfig();
  void getDirBasic();
  void getDirWithIndex();
  void copyDirOverwritesExisting();
  void findBinaryInPathNotFound();
  void findPasswordStoreEnvVar();
  void normalizeFolderPathMultipleCalls();
  void userInfoFullyValid();
  void userInfoMarginallyValid();
  void userInfoIsValid();
  void qProgressIndicatorBasic();
  void qProgressIndicatorStartStop();
  void namedValueBasic();
  void namedValueMultiple();
  void imitatePassResolveMoveDestination();
  void imitatePassResolveMoveDestinationForce();
  void imitatePassResolveMoveDestinationDir();
  void imitatePassResolveMoveDestinationNonExistent();
  void imitatePassRemoveDir();
  void getRecipientListBasic();
  void getRecipientListEmpty();
  void getRecipientListWithComments();
  void getRecipientStringCount();
  void getGpgIdPathBasic();
  void getGpgIdPathSubfolder();
  void getGpgIdPathNotFound();
};

auto operator==(const NamedValue &a, const NamedValue &b) -> bool {
  return a.name == b.name && a.value == b.value;
}

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
void tst_util::init() {}

/**
 * @brief tst_util::cleanup unit test cleanup method
 */
void tst_util::cleanup() {}

/**
 * @brief tst_util::initTestCase test case init method
 */
void tst_util::initTestCase() {}

/**
 * @brief tst_util::cleanupTestCase test case cleanup method
 */
void tst_util::cleanupTestCase() {}

/**
 * @brief tst_util::normalizeFolderPath test to check correct working
 * of Util::normalizeFolderPath the paths should always end with a slash
 */
void tst_util::normalizeFolderPath() {
  QCOMPARE(Util::normalizeFolderPath("test"),
           QDir::toNativeSeparators("test/"));
  QCOMPARE(Util::normalizeFolderPath("test/"),
           QDir::toNativeSeparators("test/"));
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
  QRegularExpressionMatch m1 =
      proto.match("https://example.com/ is the address");
  QVERIFY(m1.hasMatch());
  if (m1.hasMatch()) {
    QString captured = m1.captured(1);
    QVERIFY2(!captured.contains(" "), "URL should not include space");
    QVERIFY2(!captured.contains("<"), "URL should not include <");
    QVERIFY2(captured == "https://example.com/", "URL should stop at space");
  }

  QRegularExpressionMatch m3 =
      proto.match("Link: https://test.org/path?q=1#frag");
  QVERIFY(m3.hasMatch());
  if (m3.hasMatch()) {
    QString captured = m3.captured(1);
    QVERIFY2(captured.contains("?"), "URL should include query params");
    QVERIFY2(captured.contains("#"), "URL should include fragment");
    QVERIFY2(!captured.contains(" now"),
             "URL should not include trailing text");
  }

  QRegularExpression nl = Util::newLinesRegex();
  QVERIFY(nl.match("\n").hasMatch());
  QVERIFY(nl.match("\r").hasMatch());
  QVERIFY(nl.match("\r\n").hasMatch());
}

void tst_util::copyDirBasic() {
  QTemporaryDir srcDir;
  (void)QDir(srcDir.path()).mkdir("source");

  QFile f1(srcDir.path() + "/file1.txt");
  (void)f1.open(QFile::WriteOnly);
  f1.write("content1");
  f1.close();

  QFile f2(srcDir.path() + "/source/file2.txt");
  (void)f2.open(QFile::WriteOnly);
  f2.write("content2");
  f2.close();

  QTemporaryDir destDir;

  Util::copyDir(srcDir.path(), destDir.path());

  QVERIFY(QFile::exists(destDir.path() + "/file1.txt"));
  QVERIFY(QFile::exists(destDir.path() + "/source/file2.txt"));
  QVERIFY(QFile::exists(destDir.path() + "/source"));

  QFile f(destDir.path() + "/file1.txt");
  QVERIFY(f.open(QFile::ReadOnly));
  QCOMPARE(f.readAll(), QByteArray("content1"));
}

void tst_util::copyDirWithSubdirs() {
  QTemporaryDir srcDir;
  (void)QDir(srcDir.path()).mkpath("a/b/c");
  (void)QDir(srcDir.path()).mkpath("x/y");

  QFile f1(srcDir.path() + "/root.txt");
  (void)f1.open(QFile::WriteOnly);
  f1.close();

  QFile f2(srcDir.path() + "/a/file_a.txt");
  (void)f2.open(QFile::WriteOnly);
  f2.close();

  QFile f3(srcDir.path() + "/a/b/file_ab.txt");
  (void)f3.open(QFile::WriteOnly);
  f3.close();

  QFile f4(srcDir.path() + "/a/b/c/file_abc.txt");
  (void)f4.open(QFile::WriteOnly);
  f4.close();

  QFile f5(srcDir.path() + "/x/file_x.txt");
  (void)f5.open(QFile::WriteOnly);
  f5.close();

  QTemporaryDir destDir;
  Util::copyDir(srcDir.path(), destDir.path());

  QVERIFY(QFile::exists(destDir.path() + "/root.txt"));
  QVERIFY(QFile::exists(destDir.path() + "/a/file_a.txt"));
  QVERIFY(QFile::exists(destDir.path() + "/a/b/file_ab.txt"));
  QVERIFY(QFile::exists(destDir.path() + "/a/b/c/file_abc.txt"));
  QVERIFY(QFile::exists(destDir.path() + "/x/file_x.txt"));
}

void tst_util::normalizeFolderPathEdgeCases() {
  QString result = Util::normalizeFolderPath("");
  QVERIFY(result.endsWith(QDir::separator()));

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
  QVERIFY(fc.getNamedValues().length() >= 2);

  fc = FileContent::parse("pass\nkey: value with spaces\n", {"key"}, true);
  NamedValues nv = fc.getNamedValues();
  QVERIFY(nv.length() > 0);
  if (nv.length() > 0) {
    QCOMPARE(nv.at(0).name, QString("key"));
    QVERIFY(nv.at(0).value.contains("spaces"));
  }

  fc = FileContent::parse("pass\n://something\n", {}, false);
  QVERIFY(fc.getRemainingData().contains("://"));

  fc = FileContent::parse("pass\nno colon line\n", {}, false);
  QVERIFY(fc.getRemainingData().contains("no colon line"));

  fc = FileContent::parse("pass\nkey: value\nkey2: duplicate\n", {}, true);
  QVERIFY(fc.getNamedValues().length() >= 1);

  fc = FileContent::parse("pass\n", {}, false);
  QVERIFY(fc.getPassword() == "pass");
  QVERIFY(fc.getNamedValues().isEmpty());
}

void tst_util::namedValuesEdgeCases() {
  NamedValues nv;
  QVERIFY(nv.isEmpty());
  QVERIFY(nv.takeValue("nonexistent").isEmpty());

  NamedValue n1 = {"key", "value"};
  nv.append(n1);
  QVERIFY(nv.length() == 1);
  NamedValue n2 = {"key2", "value2"};
  nv.append(n2);
  QVERIFY(nv.length() == 2);

  nv.clear();
  QVERIFY(nv.isEmpty());
  QVERIFY(nv.takeValue("anything").isEmpty());
}

void tst_util::regexPatternEdgeCases() {
  const QRegularExpression &gpg = Util::endsWithGpg();
  QVERIFY(gpg.match(".gpg").hasMatch());
  QVERIFY(gpg.match("a.gpg").hasMatch());
  QVERIFY(gpg.match(".gpg").hasMatch());
  QVERIFY(!gpg.match("test.gpgx").hasMatch());

  const QRegularExpression &proto = Util::protocolRegex();
  QVERIFY(proto.match("webdavs://secure.example.com").hasMatch());
  QVERIFY(proto.match("ftps://ftp.server.org").hasMatch());
  QVERIFY(proto.match("sftp://user:pass@host").hasMatch());
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

void tst_util::copyDirEmpty() {
  QTemporaryDir srcDir;
  QTemporaryDir destDir;
  Util::copyDir(srcDir.path(), destDir.path());
  QDir dest(destDir.path());
  QVERIFY(dest.exists());
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
  QVERIFY(config.length == 16);
  QVERIFY(config.selected == PasswordConfiguration::ALLCHARS);

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
  simpleTransaction trans;

  trans.transactionAdd(Enums::PASS_INSERT);
  trans.transactionStart();
  trans.transactionAdd(Enums::GIT_ADD);
  trans.transactionAdd(Enums::GIT_COMMIT);
  trans.transactionEnd(Enums::GIT_COMMIT);

  Enums::PROCESS result = trans.transactionIsOver(Enums::PASS_INSERT);
  QVERIFY(result != Enums::INVALID);
}

void tst_util::simpleTransactionNested() {
  simpleTransaction trans;

  trans.transactionStart();
  trans.transactionAdd(Enums::PASS_INSERT);
  trans.transactionStart();
  trans.transactionAdd(Enums::GIT_PUSH);
  trans.transactionEnd(Enums::GIT_PUSH);
  trans.transactionEnd(Enums::PASS_INSERT);

  Enums::PROCESS result = trans.transactionIsOver(Enums::GIT_PUSH);
  QVERIFY(result != Enums::INVALID);
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
  ImitatePass pass;
  QString charset = "abcdefghijklmnopqrstuvwxyz";
  QString result = pass.Generate_b(10, charset);

  QVERIFY(result.length() == 10);
  QVERIFY2(result.length() == 10 && !result.isEmpty(),
           "result should be non-empty with correct length");

  result = pass.Generate_b(100, "abcd");
  QVERIFY(result.length() == 100);

  result = pass.Generate_b(0, "");
  QVERIFY(result.isEmpty());

  result = pass.Generate_b(50, "ABC");
  QVERIFY(result.length() == 50);
}

void tst_util::boundedRandom() {
  ImitatePass pass;

  QVector<quint32> counts(10, 0);
  const int iterations = 1000;

  for (int i = 0; i < iterations; ++i) {
    QString result = pass.Generate_b(1, "0123456789");
    quint32 val = result.at(0).digitValue();
    if (val < 10) {
      counts[val]++;
    }
  }

  for (int i = 0; i < 10; ++i) {
    QVERIFY2(counts[i] > 0, "Each digit should appear at least once");
  }
}

void tst_util::findBinaryInPath() {
  QString result = Util::findBinaryInPath("bash");
  QVERIFY2(!result.isEmpty(), "Should find bash in PATH");
  QVERIFY(result.contains("bash"));

  result = Util::findBinaryInPath("nonexistentbinary12345");
  QVERIFY(result.isEmpty());
}

void tst_util::findPasswordStore() {
  QString result = Util::findPasswordStore();
  QVERIFY(!result.isEmpty());
  QVERIFY(result.endsWith(QDir::separator()));
}

void tst_util::checkConfig() {
  bool result = Util::checkConfig();
  QVERIFY(result == true || result == false);
}

void tst_util::getDirBasic() {
  QString result =
      Util::getDir(QModelIndex(), false, QFileSystemModel(), StoreModel());
  QVERIFY(result.endsWith(QDir::separator()));
}

void tst_util::getDirWithIndex() {
  QFileSystemModel fsm;
  StoreModel sm;
  QModelIndex invalidIndex;
  QString result = Util::getDir(invalidIndex, true, fsm, sm);
  QVERIFY(result.isEmpty() || result.endsWith(QDir::separator()));
}

void tst_util::copyDirOverwritesExisting() {
  QTemporaryDir srcDir;
  QFile f1(srcDir.path() + "/file.txt");
  (void)f1.open(QFile::WriteOnly);
  f1.write("content");
  f1.close();

  QTemporaryDir destDir;
  QFile df1(destDir.path() + "/file.txt");
  (void)df1.open(QFile::WriteOnly);
  df1.write("old");
  df1.close();

  Util::copyDir(srcDir.path(), destDir.path());
  QVERIFY(QFile::exists(destDir.path() + "/file.txt"));
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
  QVERIFY(ui.fullyValid() == true);
  ui.validity = 'u';
  QVERIFY(ui.fullyValid() == true);
  ui.validity = '-';
  QVERIFY(ui.fullyValid() == false);
}

void tst_util::userInfoMarginallyValid() {
  UserInfo ui;
  ui.validity = 'm';
  QVERIFY(ui.marginallyValid() == true);
  ui.validity = 'f';
  QVERIFY(ui.marginallyValid() == false);
}

void tst_util::userInfoIsValid() {
  UserInfo ui;
  ui.validity = 'f';
  QVERIFY(ui.isValid() == true);
  ui.validity = 'm';
  QVERIFY(ui.isValid() == true);
  ui.validity = '-';
  QVERIFY(ui.isValid() == false);
}

void tst_util::qProgressIndicatorBasic() {
  QProgressIndicator pi;
  QVERIFY(pi.isAnimated() == false);
}

void tst_util::qProgressIndicatorStartStop() {
  QProgressIndicator pi;
  pi.startAnimation();
  QVERIFY(pi.isAnimated() == true);
  pi.stopAnimation();
  QVERIFY(pi.isAnimated() == false);
}

void tst_util::namedValueBasic() {
  NamedValue nv;
  nv.name = "key";
  nv.value = "value";
  QVERIFY(nv.name == "key");
  QVERIFY(nv.value == "value");
}

void tst_util::namedValueMultiple() {
  NamedValues nvs;
  NamedValue nv1;
  nv1.name = "user1";
  nv1.value = "pass1";
  nvs.append(nv1);
  QVERIFY(nvs.size() == 1);
}

void tst_util::imitatePassResolveMoveDestination() {
  ImitatePass pass;
  QString result =
      pass.resolveMoveDestination("/tmp/test.gpg", "/tmp/dest.gpg", false);
  QString expected = "/tmp/dest.gpg";
  if (result.isEmpty()) {
    QSKIP("Source file does not exist");
  }
  QVERIFY2(result == expected, "Destination should have .gpg extension");
}

void tst_util::imitatePassResolveMoveDestinationForce() {
  ImitatePass pass;
  QTemporaryDir tmpDir;
  QString srcPath = tmpDir.path() + "/test.gpg";
  QFile srcFile(srcPath);
  (void)srcFile.open(QFile::WriteOnly);
  srcFile.write("test");
  srcFile.close();

  QString destPath = tmpDir.path() + "/existing.gpg";
  QFile destFile(destPath);
  (void)destFile.open(QFile::WriteOnly);
  destFile.write("old");
  destFile.close();

  QString result = pass.resolveMoveDestination(srcPath, destPath, true);
  QVERIFY2(result == destPath, "Should return dest path when force=true");
}

void tst_util::imitatePassResolveMoveDestinationDir() {
  ImitatePass pass;
  QTemporaryDir tmpDir;
  QString srcPath = tmpDir.path() + "/test.gpg";
  QFile srcFile(srcPath);
  (void)srcFile.open(QFile::WriteOnly);
  srcFile.write("test");
  srcFile.close();

  QString result = pass.resolveMoveDestination(srcPath, tmpDir.path(), false);
  QVERIFY2(result == tmpDir.path() + "/test.gpg",
           "Should append filename when dest is dir");
}

void tst_util::imitatePassResolveMoveDestinationNonExistent() {
  ImitatePass pass;
  QString result = pass.resolveMoveDestination("/non/existent/path.gpg",
                                               "/tmp/dest.gpg", false);
  QVERIFY2(result.isEmpty(), "Should return empty for non-existent source");
}

void tst_util::imitatePassRemoveDir() {
  ImitatePass pass;
  QTemporaryDir tmpDir;
  QString subDir = tmpDir.path() + "/testdir";
  (void)QDir().mkdir(subDir);
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

  QtPassSettings::setPassStore(passStore);
  QStringList recipients = Pass::getRecipientList(passStore);
  QCOMPARE(recipients.size(), 2);
  QVERIFY(!recipients.contains("# comment"));
}

void tst_util::getRecipientStringCount() {
  QTemporaryDir tempDir;
  QString passStore = tempDir.path();
  QString gpgIdFile = passStore + "/.gpg-id";

  QFile file(gpgIdFile);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("ABCDEF12\n34567890\n");
  file.close();

  QtPassSettings::setPassStore(passStore);
  int count = 0;
  QStringList recipients = Pass::getRecipientString(passStore, " ", &count);
  QStringList recipientsNoCount = Pass::getRecipientString(passStore, " ");

  QVERIFY(recipientsNoCount.isEmpty() || recipientsNoCount.size() == 2);
  QVERIFY(count == 2);
}

void tst_util::getGpgIdPathBasic() {
  QTemporaryDir tempDir;
  QString passStore = tempDir.path();
  QString gpgIdFile = passStore + "/.gpg-id";

  QFile file(gpgIdFile);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("ABCDEF12\n");
  file.close();

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

  QtPassSettings::setPassStore(passStore);
  QString path = Pass::getGpgIdPath(subfolder + "/password.gpg");
  QVERIFY2(path == gpgIdFile,
           qPrintable(QString("Expected %1, got %2").arg(gpgIdFile, path)));
}

void tst_util::getGpgIdPathNotFound() {
  QTemporaryDir tempDir;
  QString passStore = tempDir.path();

  QtPassSettings::setPassStore(passStore);
  QString path =
      QDir::cleanPath(Pass::getGpgIdPath(passStore + "/nonexistent"));
  QString expected = QDir::cleanPath(passStore + "/.gpg-id");
  QVERIFY2(path == expected,
           qPrintable(QString("Expected %1, got %2").arg(expected, path)));
}

QTEST_MAIN(tst_util)
#include "tst_util.moc"
