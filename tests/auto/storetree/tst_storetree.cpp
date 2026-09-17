// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTreeView>
#include <QtTest>

#include "../../../src/storetree.h"

/**
 * @brief StoreTree without a MainWindow: re-rooting, filtering and the two
 *        path mappings (folder for an index, pass name for an index) that
 *        every tree action depends on.
 */
class tst_storetree : public QObject {
  Q_OBJECT

private slots:
  void init();
  void setStoreRootsViewAndProxyTogether();
  void dirForFolderFileAndInvalidIndex();
  void fileForEntryFolderAndInvalidIndex();
  void filterHidesNonMatchesAndSetStoreClearsIt();
  void switchingStoresMovesTheRoot();

private:
  auto makeEntry(const QString &relative) -> QString;
  auto waitFor(StoreTree &tree, const QString &absolute) -> QModelIndex;

  QTemporaryDir m_dir;
  QString m_store;
};

void tst_storetree::init() {
  QVERIFY(m_dir.isValid());
  m_store = QDir(m_dir.path()).filePath(QStringLiteral("store"));
  QVERIFY(QDir().mkpath(m_store + QStringLiteral("/folder/deeper")));
  makeEntry(QStringLiteral("top.gpg"));
  makeEntry(QStringLiteral("folder/inner.gpg"));
  makeEntry(QStringLiteral("folder/deeper/leaf.gpg"));
  makeEntry(QStringLiteral("folder/notes.txt"));
}

auto tst_storetree::makeEntry(const QString &relative) -> QString {
  const QString path = QDir(m_store).filePath(relative);
  QFile f(path);
  if (f.open(QIODevice::WriteOnly)) {
    f.write("x");
  }
  return path;
}

// QTRY_* macros return void; this variant returns a value from a helper.
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

auto tst_storetree::waitFor(StoreTree &tree, const QString &absolute)
    -> QModelIndex {
  // QFileSystemModel populates asynchronously; the proxy sees rows only once
  // the source model has them.
  QModelIndex index;
  QTRY_VERIFY_WITH_TIMEOUT_RETURN((index = tree.indexFor(absolute)).isValid(),
                                  5000, QModelIndex());
  return index;
}

void tst_storetree::setStoreRootsViewAndProxyTogether() {
  QTreeView view;
  StoreTree tree(&view);
  tree.setStore(m_store);
  QCOMPARE(tree.store(), m_store);
  QCOMPARE(view.model(), &tree.proxy());
  QCOMPARE(tree.proxy().getStore(), m_store);
  QVERIFY(waitFor(tree, m_store + QStringLiteral("/top.gpg")).isValid());
  QCOMPARE(view.rootIndex(), tree.rootIndex());
  QVERIFY2(view.isColumnHidden(1) && view.isColumnHidden(2) &&
               view.isColumnHidden(3),
           "only the name column is shown");
  QCOMPARE(view.header()->sectionResizeMode(0), QHeaderView::Stretch);
  QCOMPARE(view.header()->sortIndicatorSection(), 0);
}

void tst_storetree::dirForFolderFileAndInvalidIndex() {
  QTreeView view;
  StoreTree tree(&view);
  tree.setStore(m_store);
  const QString root = QDir(m_store).absolutePath() + QDir::separator();

  QCOMPARE(tree.dirFor(QModelIndex(), false), root);
  QCOMPARE(tree.dirFor(QModelIndex(), true), QString());

  const QModelIndex folder = waitFor(tree, m_store + QStringLiteral("/folder"));
  QVERIFY(folder.isValid());
  QCOMPARE(tree.dirFor(folder, false),
           QDir(m_store).absoluteFilePath(QStringLiteral("folder")) +
               QDir::separator());
  QCOMPARE(tree.dirFor(folder, true),
           QStringLiteral("folder") + QDir::separator());

  const QModelIndex leaf =
      waitFor(tree, m_store + QStringLiteral("/folder/deeper/leaf.gpg"));
  QVERIFY(leaf.isValid());
  QCOMPARE(tree.dirFor(leaf, true),
           QStringLiteral("folder/deeper") + QDir::separator());

  view.setCurrentIndex(leaf);
  QCOMPARE(tree.currentDir(true), tree.dirFor(leaf, true));
}

void tst_storetree::fileForEntryFolderAndInvalidIndex() {
  QTreeView view;
  StoreTree tree(&view);
  tree.setStore(m_store);
  const QModelIndex inner =
      waitFor(tree, m_store + QStringLiteral("/folder/inner.gpg"));
  QVERIFY(inner.isValid());
  QCOMPARE(tree.fileFor(inner, true), QStringLiteral("folder/inner"));
  QCOMPARE(tree.fileFor(inner, false),
           QDir(m_store).absoluteFilePath(QStringLiteral("folder/inner.gpg")));
  QVERIFY(tree.fileInfo(inner).isFile());

  const QModelIndex folder = waitFor(tree, m_store + QStringLiteral("/folder"));
  QVERIFY2(tree.fileFor(folder, true).isEmpty(), "a folder is not an entry");
  QVERIFY(tree.fileFor(QModelIndex(), true).isEmpty());
  view.setCurrentIndex(inner);
  QCOMPARE(tree.currentFile(true), QStringLiteral("folder/inner"));
}

void tst_storetree::filterHidesNonMatchesAndSetStoreClearsIt() {
  QTreeView view;
  StoreTree tree(&view);
  tree.setStore(m_store);
  QVERIFY(waitFor(tree, m_store + QStringLiteral("/top.gpg")).isValid());
  QVERIFY(
      waitFor(tree, m_store + QStringLiteral("/folder/inner.gpg")).isValid());

  tree.setFilter(QRegularExpression(QStringLiteral("inner")));
  QCOMPARE(view.rootIndex(), tree.rootIndex());
  QVERIFY2(!tree.indexFor(m_store + QStringLiteral("/top.gpg")).isValid(),
           "an entry that does not match must be filtered out");
  QVERIFY(
      tree.indexFor(m_store + QStringLiteral("/folder/inner.gpg")).isValid());

  tree.setStore(m_store);
  QVERIFY2(waitFor(tree, m_store + QStringLiteral("/top.gpg")).isValid(),
           "setStore() drops the filter");
}

void tst_storetree::switchingStoresMovesTheRoot() {
  const QString other = QDir(m_dir.path()).filePath(QStringLiteral("other"));
  QVERIFY(QDir().mkpath(other));
  {
    QFile f(QDir(other).filePath(QStringLiteral("elsewhere.gpg")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
  }
  QTreeView view;
  StoreTree tree(&view);
  tree.setStore(m_store);
  QVERIFY(waitFor(tree, m_store + QStringLiteral("/top.gpg")).isValid());

  tree.setStore(other);
  QCOMPARE(tree.store(), other);
  QCOMPARE(tree.proxy().getStore(), other);
  QVERIFY(waitFor(tree, other + QStringLiteral("/elsewhere.gpg")).isValid());
  QCOMPARE(view.rootIndex(), tree.rootIndex());
  QCOMPARE(tree.dirFor(QModelIndex(), false),
           QDir(other).absolutePath() + QDir::separator());
}

QTEST_MAIN(tst_storetree)
#include "tst_storetree.moc"
