#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileSystemModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeView>
#include <QtTest>

#include "../../../src/mainwindow.h"
#include "../../../src/passworddialog.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/util.h"
#include "../testsettings.h"

class tst_probeotplink : public QObject {
  Q_OBJECT
  QTemporaryDir m_storeDir;
  QTemporaryDir m_outside;
  QString m_gpgPath;
  int m_boxesSeen = 0;
  QStringList m_boxTitles;

  void startPoker(QTimer &poker) {
    poker.setInterval(20);
    connect(&poker, &QTimer::timeout, this, [this]() {
      auto *modal = qobject_cast<QDialog *>(QApplication::activeModalWidget());
      if (modal == nullptr || modal->property("tst_driven").toBool()) {
        return;
      }
      if (auto *box = qobject_cast<QMessageBox *>(modal)) {
        modal->setProperty("tst_driven", true);
        ++m_boxesSeen;
        m_boxTitles << box->windowTitle() + QStringLiteral(" :: ") + box->text();
        box->reject();
      }
    });
    poker.start();
  }

private Q_SLOTS:
  void initTestCase() {
    isolateTestSettings();
    QVERIFY(m_storeDir.isValid());
    QVERIFY(m_outside.isValid());
    QFile gpgId(QDir(m_storeDir.path()).filePath(".gpg-id"));
    QVERIFY(gpgId.open(QIODevice::WriteOnly));
    gpgId.write("0000000000000000\n");
    gpgId.close();
    QtPassSettings::setPassStore(QDir::cleanPath(m_storeDir.path()));
    QtPassSettings::setUsePass(false);
    m_gpgPath = Util::findBinaryInPath(QStringLiteral("gpg2"));
    if (m_gpgPath.isEmpty())
      m_gpgPath = Util::findBinaryInPath(QStringLiteral("gpg"));
    QVERIFY(!m_gpgPath.isEmpty());
    AppSettings s = QtPassSettings::load();
    s.gpgExecutable = m_gpgPath;
    s.useOtp = true;
    s.useGit = false;
    s.autoPull = false;
    s.clipBoardType = Enums::CLIPBOARD_ON_DEMAND;
    s.useSelection = false;
    s.useAutoclear = false;
    QtPassSettings::save(s);

    // Planted link: Bank.gpg -> outside/x.gpg
    QFile x(QDir(m_outside.path()).filePath("x.gpg"));
    QVERIFY(x.open(QIODevice::WriteOnly));
    x.write("not really encrypted");
    x.close();
    QVERIFY(QFile::link(QDir(m_outside.path()).filePath("x.gpg"),
                        QDir(m_storeDir.path()).filePath("Bank.gpg")));
    QVERIFY(QFileInfo(QDir(m_storeDir.path()).filePath("Bank.gpg")).isSymLink());
  }

  void init() {
    QtPassSettings::setPassStore(QDir::cleanPath(m_storeDir.path()));
    QtPassSettings::setUsePass(false);
    AppSettings s = QtPassSettings::load();
    s.gpgExecutable = m_gpgPath;
    QtPassSettings::save(s);
    m_boxesSeen = 0;
    m_boxTitles.clear();
  }

  void otpOnLinkedEntryLeavesUiDisabled() {
    QTimer poker;
    startPoker(poker);
    MainWindow w;
    // configIsValid path must not have opened dialogs
    QCOMPARE(m_boxesSeen, 0);

    auto *tree = w.findChild<QTreeView *>(QStringLiteral("treeView"));
    QVERIFY(tree);
    auto *proxy = qobject_cast<QSortFilterProxyModel *>(tree->model());
    auto *fs = qobject_cast<QFileSystemModel *>(proxy->sourceModel());
    const QString path = QDir(m_storeDir.path()).filePath("Bank.gpg");
    QModelIndex src;
    QTRY_VERIFY_WITH_TIMEOUT((src = fs->index(path)).isValid(), 5000);
    tree->setCurrentIndex(proxy->mapFromSource(src));
    QVERIFY2(tree->isEnabled(), "precondition: tree enabled before onOtp");

    QVERIFY(QMetaObject::invokeMethod(&w, "onOtp", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTest::qWait(200);
    qInfo() << "message boxes:" << m_boxTitles;
    QCOMPARE(m_boxesSeen, 1);
    qInfo() << "tree enabled after refused Show:" << tree->isEnabled();
    auto *search = w.findChild<QLineEdit *>(QStringLiteral("lineEdit"));
    qInfo() << "search enabled after refused Show:" << search->isEnabled();

    // Is the OTP request still pending? Feed a decrypt for Bank and see
    // whether it is taken as the answer.
    QClipboard *clip = QApplication::clipboard();
    clip->setText(QStringLiteral("sentinel"));
    const QString entry = QStringLiteral(
        "hunter2\notpauth://totp/Example:alice?secret="
        "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ&issuer=Example\n");
    w.otpFromFileToClipboard(entry, QStringLiteral("Bank"));
    qInfo() << "clipboard after stray decrypt of Bank:" << clip->text();

    QVERIFY2(!tree->isEnabled(), "REFUTED: tree was re-enabled");
  }

  void treeClickOnLinkedEntryKeepsUiEnabled() {
    QTimer poker;
    startPoker(poker);
    MainWindow w;
    auto *tree = w.findChild<QTreeView *>(QStringLiteral("treeView"));
    auto *proxy = qobject_cast<QSortFilterProxyModel *>(tree->model());
    auto *fs = qobject_cast<QFileSystemModel *>(proxy->sourceModel());
    const QString path = QDir(m_storeDir.path()).filePath("Bank.gpg");
    QModelIndex src;
    QTRY_VERIFY_WITH_TIMEOUT((src = fs->index(path)).isValid(), 5000);
    tree->setCurrentIndex(proxy->mapFromSource(src));
    QMetaObject::invokeMethod(&w, "on_treeView_clicked", Qt::DirectConnection,
                              Q_ARG(QModelIndex, tree->currentIndex()));
    QTest::qWait(200);
    QCOMPARE(m_boxesSeen, 1);
    qInfo() << "tree enabled after refused tree-click Show:" << tree->isEnabled();
    QVERIFY(tree->isEnabled());
  }

  void editDialogOnLinkedEntryStuckDecrypting() {
    QTimer poker;
    startPoker(poker);
    MainWindow w;
    const AppSettings s = QtPassSettings::load();
    PasswordDialog d(QtPassSettings::getPass(), s, QStringLiteral("Bank"),
                     false, &w);
    QTest::qWait(300);
    QCOMPARE(m_boxesSeen, 1);
    auto *status = d.findChild<QLabel *>(QStringLiteral("statusLabel"));
    auto *bb = d.findChild<QDialogButtonBox *>();
    QVERIFY(status);
    QVERIFY(bb);
    qInfo() << "status label:" << status->text();
    qInfo() << "Ok enabled:" << bb->button(QDialogButtonBox::Ok)->isEnabled();
    QCOMPARE(status->text(), QStringLiteral("Decrypting…"));
    QVERIFY(!bb->button(QDialogButtonBox::Ok)->isEnabled());
  }
};

QTEST_MAIN(tst_probeotplink)
#include "tst_probeotplink.moc"
