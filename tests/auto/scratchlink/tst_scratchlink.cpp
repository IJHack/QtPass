#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileSystemModel>
#include <QLabel>
#include <QMessageBox>
#include <QRegularExpression>
#include <QScopedPointer>
#include <QSortFilterProxyModel>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimer>
#include <QTreeView>
#include <QtTest>

#include "../../../src/mainwindow.h"
#include "../../../src/otpcodewidget.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/util.h"
#include "../testsettings.h"

class tst_scratchlink : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void refusedShowLeavesPreviousEntryAndOtpFastPathCopiesIt();

private:
  QTemporaryDir m_storeDir;
  QTemporaryDir m_outsideDir;
  QString m_gpgPath;
};

void tst_scratchlink::initTestCase() {
  isolateTestSettings();
  QVERIFY(m_storeDir.isValid());
  QVERIFY(m_outsideDir.isValid());
  QFile gpgId(QDir(m_storeDir.path()).filePath(".gpg-id"));
  QVERIFY(gpgId.open(QIODevice::WriteOnly));
  gpgId.write("0000000000000000\n");
  gpgId.close();
  QtPassSettings::setPassStore(QDir::cleanPath(m_storeDir.path()));
  QtPassSettings::setUsePass(false);
  m_gpgPath = Util::findBinaryInPath("gpg2");
  if (m_gpgPath.isEmpty())
    m_gpgPath = Util::findBinaryInPath("gpg");
  if (m_gpgPath.isEmpty())
    QSKIP("no gpg");
  AppSettings s = QtPassSettings::load();
  s.gpgExecutable = m_gpgPath;
  QtPassSettings::save(s);
}

#define QTRY_VERIFY_WITH_TIMEOUT_RETURN(expr, timeout, ret)                    \
  do {                                                                         \
    QElapsedTimer timer;                                                       \
    timer.start();                                                             \
    while (!(expr) && timer.elapsed() < (timeout)) {                           \
      QTest::qWait(20);                                                        \
    }                                                                          \
    if (!(expr)) {                                                             \
      return ret;                                                              \
    }                                                                          \
  } while (false)

namespace {
const QString kOtpUri =
    "otpauth://totp/Example:alice?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ&"
    "issuer=Example";
const QString kOtpEntry = "hunter2\n" + kOtpUri + "\n";

auto looksLikeOtpCode(const QString &s) -> bool {
  static const QRegularExpression six("^[0-9]{6}$");
  return six.match(s).hasMatch();
}

auto selectPath(MainWindow *window, const QString &path) -> bool {
  auto *tree = window->findChild<QTreeView *>("treeView");
  auto *proxy = qobject_cast<QSortFilterProxyModel *>(tree->model());
  auto *fs = qobject_cast<QFileSystemModel *>(proxy->sourceModel());
  QModelIndex src;
  QTRY_VERIFY_WITH_TIMEOUT_RETURN((src = fs->index(path)).isValid(), 5000,
                                  false);
  tree->setCurrentIndex(proxy->mapFromSource(src));
  return true;
}

void clickCurrent(MainWindow *window) {
  auto *tree = window->findChild<QTreeView *>("treeView");
  QMetaObject::invokeMethod(window, "on_treeView_clicked",
                            Qt::DirectConnection,
                            Q_ARG(QModelIndex, tree->currentIndex()));
}
} // namespace

void tst_scratchlink::refusedShowLeavesPreviousEntryAndOtpFastPathCopiesIt() {
  QtPassSettings::setPassStore(QDir::cleanPath(m_storeDir.path()));
  QtPassSettings::setUsePass(false);
  {
    AppSettings s = QtPassSettings::load();
    s.gpgExecutable = m_gpgPath;
    s.showProcessOutput = true;
    s.useSelection = false;
    s.useAutoclear = false;
    s.useAutoclearPanel = false;
    s.useOtp = true;
    s.hideContent = false;
    s.displayAsIs = false;
    s.clipBoardType = Enums::CLIPBOARD_ON_DEMAND;
    QtPassSettings::save(s);
  }
  QScopedPointer<MainWindow> window(new MainWindow);
  QClipboard *clip = QApplication::clipboard();

  // 1. A real entry "github" with an OTP field is shown.
  const QString github = QDir(m_storeDir.path()).filePath("github.gpg");
  {
    QFile f(github);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("not really encrypted");
  }
  QVERIFY(selectPath(window.data(), github));
  clickCurrent(window.data());
  window->passShowHandler(kOtpEntry, "github");
  auto *otpWidget = window->findChild<OtpCodeWidget *>();
  QVERIFY2(otpWidget != nullptr, "github's OTP row must be on screen");
  const QString githubCode = otpWidget->code();
  QVERIFY(looksLikeOtpCode(githubCode));
  auto *nameLabel = window->findChild<QLabel *>("passwordName");
  QVERIFY(nameLabel);
  QCOMPARE(nameLabel->text(), QString("github"));

  // 2. A planted link Bank.gpg -> /elsewhere/x.gpg.
  const QString outside = QDir(m_outsideDir.path()).filePath("x.gpg");
  {
    QFile f(outside);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("elsewhere");
  }
  const QString bank = QDir(m_storeDir.path()).filePath("Bank.gpg");
  QVERIFY(QFile::link(outside, bank));
  QVERIFY(QFileInfo(bank).isSymLink());
  QVERIFY(selectPath(window.data(), bank));

  // Close the critical box the refusal pops up.
  int boxesSeen = 0;
  QTimer poker;
  poker.setInterval(50);
  QObject::connect(&poker, &QTimer::timeout, [&]() {
    for (QWidget *w : QApplication::topLevelWidgets()) {
      if (auto *box = qobject_cast<QMessageBox *>(w); box && box->isVisible()) {
        ++boxesSeen;
        qInfo() << "closing message box:" << box->windowTitle() << box->text();
        box->reject();
      }
    }
  });
  poker.start();
  clickCurrent(window.data());
  poker.stop();
  QVERIFY2(boxesSeen > 0, "the refusal must have shown a critical box");

  // 3. What is on screen now?
  qInfo() << "passwordName label:" << nameLabel->text();
  auto *stillOtp = window->findChild<OtpCodeWidget *>();
  qInfo() << "OTP row still present after refusal:" << (stillOtp != nullptr);
  const bool panelStale = (stillOtp != nullptr);

  // 4. The user presses the OTP action on the (refused) Bank entry.
  clip->setText("sentinel");
  {
    auto *tree = window->findChild<QTreeView *>("treeView");
    auto *proxy = qobject_cast<QSortFilterProxyModel *>(tree->model());
    auto *fs = qobject_cast<QFileSystemModel *>(proxy->sourceModel());
    qInfo() << "current index valid:" << tree->currentIndex().isValid()
            << "path:" << fs->filePath(proxy->mapToSource(tree->currentIndex()))
            << "isFile:" << fs->fileInfo(proxy->mapToSource(tree->currentIndex())).isFile()
            << "useOtp:" << QtPassSettings::isUseOtp();
  }
  QVERIFY(QMetaObject::invokeMethod(window.data(), "onOtp",
                                    Qt::DirectConnection));
  qInfo() << "clipboard after onOtp on Bank:" << clip->text()
          << " github's code was:" << githubCode;
  auto *status = window->findChild<QStatusBar *>();
  qInfo() << "status bar:" << (status ? status->currentMessage() : "-");
  auto *browser = window->findChild<QTextBrowser *>("textBrowser");
  qInfo() << "text browser:" << (browser ? browser->toPlainText() : "-");

  QCOMPARE(nameLabel->text(), QString("Bank"));
  QVERIFY2(!panelStale,
           "DEFECT: panel still shows github's fields under the name Bank");
  QVERIFY2(clip->text() != githubCode,
           "DEFECT: github's live TOTP code copied as Bank's");
}

// Second check as its own assertion so we see both outcomes even when the
// first fails.
QTEST_MAIN(tst_scratchlink)
#include "tst_scratchlink.moc"
