#include "../../../src/filecontent.h"
#include "../../../src/util.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QList>
#include <QTemporaryDir>
#include <QtTest>

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
  void fileContent();
  void namedValuesTakeValue();
  void totpHiddenFromDisplay();
  void regexPatterns();
  void copyDirBasic();
  void copyDirWithSubdirs();
};

bool operator==(const NamedValue &a, const NamedValue &b) {
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

  QRegularExpression nl = Util::newLinesRegex();
  QVERIFY(nl.match("\n").hasMatch());
  QVERIFY(nl.match("\r").hasMatch());
  QVERIFY(nl.match("\r\n").hasMatch());
}

void tst_util::copyDirBasic() {
  QTemporaryDir srcDir;
  (void)QDir(srcDir.path()).mkdir("source");

  QFile f1(srcDir.path() + "/file1.txt");
  f1.open(QFile::WriteOnly);
  f1.write("content1");
  f1.close();

  QFile f2(srcDir.path() + "/source/file2.txt");
  f2.open(QFile::WriteOnly);
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
  f1.open(QFile::WriteOnly);
  f1.close();

  QFile f2(srcDir.path() + "/a/file_a.txt");
  f2.open(QFile::WriteOnly);
  f2.close();

  QFile f3(srcDir.path() + "/a/b/file_ab.txt");
  f3.open(QFile::WriteOnly);
  f3.close();

  QFile f4(srcDir.path() + "/a/b/c/file_abc.txt");
  f4.open(QFile::WriteOnly);
  f4.close();

  QFile f5(srcDir.path() + "/x/file_x.txt");
  f5.open(QFile::WriteOnly);
  f5.close();

  QTemporaryDir destDir;
  Util::copyDir(srcDir.path(), destDir.path());

  QVERIFY(QFile::exists(destDir.path() + "/root.txt"));
  QVERIFY(QFile::exists(destDir.path() + "/a/file_a.txt"));
  QVERIFY(QFile::exists(destDir.path() + "/a/b/file_ab.txt"));
  QVERIFY(QFile::exists(destDir.path() + "/a/b/c/file_abc.txt"));
  QVERIFY(QFile::exists(destDir.path() + "/x/file_x.txt"));
}

QTEST_MAIN(tst_util)
#include "tst_util.moc"
