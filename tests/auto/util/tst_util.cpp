#include "../../../src/util.h"
#include <QCoreApplication>
#include <QtTest>

class tst_util : public QObject {
  Q_OBJECT

public:
  tst_util();
  ~tst_util();

public Q_SLOTS:
  void init();
  void cleanup();

private Q_SLOTS:
  void initTestCase();
  void cleanupTestCase();
  void normalizeFolderPath();
};

tst_util::tst_util() {}

tst_util::~tst_util() {}

void tst_util::init() {}

void tst_util::cleanup() {}

void tst_util::initTestCase() {}

void tst_util::cleanupTestCase() {}

void tst_util::normalizeFolderPath() {
  QCOMPARE(Util::normalizeFolderPath("test"), QString("test/"));
  QCOMPARE(Util::normalizeFolderPath("test/"), QString("test/"));
}

QTEST_MAIN(tst_util)
#include "tst_util.moc"
