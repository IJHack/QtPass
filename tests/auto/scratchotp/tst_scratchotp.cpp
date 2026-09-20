#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileSystemModel>
#include <QMessageBox>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QSortFilterProxyModel>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeView>
#include <QtTest>

#include "../../../src/mainwindow.h"
#include "../../../src/pass.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/util.h"
#include "../testsettings.h"

class tst_scratchotp : public QObject {
  Q_OBJECT
  QTemporaryDir m_storeDir;
  QTemporaryDir m_outside;
  QString m_gpgPath;
  QScopedPointer<MainWindow> m_window;
  int m_dismissed = 0;
  QString m_lastBoxTitle;

  auto selectPath(const QString &path) -> bool {
    auto *tree =
        m_window->findChild<QTreeView *>(QStringLiteral("treeView"));
    auto *proxy = qobject_cast<QSortFilterProxyModel *>(tree->model());
    auto *fs = qobject_cast<QFileSystemModel *>(proxy->sourceModel());
    QModelIndex src;
    QElapsedTimer t;
    t.start();
    while (!(src = fs->index(path)).isValid() && t.elapsed() < 5000) {
      QTest::qWait(20);
    }
    if (!src.isValid())
      return false;
    tree->setCurrentIndex(proxy->mapFromSource(src));
    return true;
  }

  void armDismisser(QTimer *timer) {
    timer->setInterval(30);
    connect(timer, &QTimer::timeout, this, [this]() {
      if (auto *box =
              qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
        m_lastBoxTitle = box->windowTitle();
        ++m_dismissed;
        box->reject();
      }
    });
    timer->start();
  }

private Q_SLOTS:
  void initTestCase() {
    isolateTestSettings();
    QVERIFY(m_storeDir.isValid());
    QVERIFY(m_outside.isValid());
    QFile gpgId(QDir(m_storeDir.path()).filePath(QStringLiteral(".gpg-id")));
    QVERIFY(gpgId.open(QIODevice::WriteOnly));
    gpgId.write("0000000000000000\n");
    gpgId.close();
    QtPassSettings::setPassStore(QDir::cleanPath(m_storeDir.path()));
    QtPassSettings::setUsePass(false);
    m_gpgPath = Util::findBinaryInPath(QStringLiteral("gpg2"));
    if (m_gpgPath.isEmpty())
      m_gpgPath = Util::findBinaryInPath(QStringLiteral("gpg"));
    QVERIFY(!m_gpgPath.isEmpty());
  }
  void init() {
    QtPassSettings::setPassStore(QDir::cleanPath(m_storeDir.path()));
    QtPassSettings::setUsePass(false);
    AppSettings s = QtPassSettings::load();
    s.gpgExecutable = m_gpgPath;
    s.useOtp = true;
    s.useSelection = false;
    s.useAutoclear = false;
    s.clipBoardType = Enums::CLIPBOARD_ON_DEMAND;
    QtPassSettings::save(s);
    m_dismissed = 0;
    m_lastBoxTitle.clear();
    m_window.reset(new MainWindow);
    m_window->show();
  }
  void cleanup() { m_window.reset(); }

  // Baseline: a plain (fake) entry whose gpg decrypt fails re-enables the UI
  // via processErrorExit -> operationFinished.
  void plainOtpFailureReenablesUi() {
    const QString path =
        QDir(m_storeDir.path()).filePath(QStringLiteral("plain.gpg"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("not really encrypted");
    f.close();
    QVERIFY(selectPath(path));
    auto *tree = m_window->findChild<QTreeView *>(QStringLiteral("treeView"));
    QVERIFY(tree->isEnabled());
    QSignalSpy errSpy(QtPassSettings::getPass(), &Pass::processErrorExit);
    QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onOtp",
                                      Qt::DirectConnection));
    QVERIFY2(!tree->isEnabled(), "onOtp disables the UI before Show");
    QTRY_VERIFY_WITH_TIMEOUT(errSpy.count() >= 1, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(tree->isEnabled(), 2000);
    qInfo() << "baseline: UI re-enabled after failed decrypt";
  }

  // Claim: a linked entry is refused with critical() only; UI stays disabled.
  void linkedOtpLeavesUiDisabled() {
    const QString target =
        QDir(m_outside.path()).filePath(QStringLiteral("x.gpg"));
    QFile f(target);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("outside");
    f.close();
    const QString link =
        QDir(m_storeDir.path()).filePath(QStringLiteral("Bank.gpg"));
    QVERIFY(QFile::link(target, link));
    QVERIFY(QFileInfo(link).isSymLink());
    QVERIFY2(selectPath(link), "the tree must show the linked entry");
    auto *tree = m_window->findChild<QTreeView *>(QStringLiteral("treeView"));
    QVERIFY(tree->isEnabled());

    QSignalSpy critSpy(QtPassSettings::getPass(), &Pass::critical);
    QSignalSpy errSpy(QtPassSettings::getPass(), &Pass::processErrorExit);
    QSignalSpy startSpy(QtPassSettings::getPass(),
                        &Pass::startingExecuteWrapper);
    QTimer dismisser;
    armDismisser(&dismisser);
    QVERIFY(QMetaObject::invokeMethod(m_window.data(), "onOtp",
                                      Qt::DirectConnection));
    dismisser.stop();
    qInfo() << "critical emitted:" << critSpy.count()
            << "boxes dismissed:" << m_dismissed << m_lastBoxTitle
            << "processErrorExit:" << errSpy.count()
            << "executeWrapper started:" << startSpy.count();
    QCOMPARE(critSpy.count(), 1);
    QCOMPARE(m_dismissed, 1);
    QCOMPARE(startSpy.count(), 0);
    QCOMPARE(errSpy.count(), 0);
    // Let the event loop settle well past any normal completion.
    QTest::qWait(3000);
    qInfo() << "tree enabled 3 s after the refusal:" << tree->isEnabled()
            << "processErrorExit since:" << errSpy.count();
    QVERIFY2(!tree->isEnabled(),
             "UI is still disabled 3 s after the refused Show (would be "
             "released only by the 30 s watchdog)");
    // Wait for the watchdog to prove that is what eventually frees it.
    QElapsedTimer t;
    t.start();
    while (!tree->isEnabled() && t.elapsed() < 35000) {
      QTest::qWait(200);
    }
    qInfo() << "tree re-enabled after ms:" << t.elapsed();
    QVERIFY(tree->isEnabled());
    QVERIFY2(t.elapsed() > 20000, "released by the watchdog, not earlier");
  }
};

QTEST_MAIN(tst_scratchotp)
#include "tst_scratchotp.moc"
