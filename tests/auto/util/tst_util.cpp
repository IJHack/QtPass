#include "../../../src/util.h"
#include <QCoreApplication>
#include <QtTest>

/**
 * @brief The tst_util class is our first unit test
 */
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

/**
 * @brief tst_util::tst_util basic constructor
 */
tst_util::tst_util() {}

/**
 * @brief tst_util::~tst_util basic destructor
 */
tst_util::~tst_util() {}

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
 * @brief tst_util::normalizeFolderPath test to check weather
 * Util::normalizeFolderPath makes paths always end with a slash
 */
void tst_util::normalizeFolderPath() {
  QCOMPARE(Util::normalizeFolderPath("test"), QString("test/"));
  QCOMPARE(Util::normalizeFolderPath("test/"), QString("test/"));
}

QTEST_MAIN(tst_util)
#include "tst_util.moc"
