// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTreeView>
#include <QtTest>

#include "../../../src/storetree.h"
#include "../testpass.h"

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
  void searchNeverMatchesAnEarlierStore();
  void aStoreReachedThroughALinkStillLists();
  void aLinkInsideTheStoreIsListedButNeverEntered();

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

/**
 * @brief After a switch, a search matches only the current store. The file
 *        system model keeps the rows of the store shown before, and a search
 *        matched those: with nothing matching in the current store the view
 *        fell back to the file system's top and listed the earlier store's
 *        entry, which Return then opened and a save re-encrypted to the
 *        current store's recipients.
 */
void tst_storetree::searchNeverMatchesAnEarlierStore() {
  const QString other = QDir(m_dir.path()).filePath(QStringLiteral("other"));
  QVERIFY(QDir().mkpath(other));
  {
    QFile f(QDir(other).filePath(QStringLiteral("work.gpg")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
  }
  QTreeView view;
  StoreTree tree(&view);
  tree.setStore(m_store);
  QVERIFY(waitFor(tree, m_store + QStringLiteral("/top.gpg")).isValid());
  QVERIFY(
      waitFor(tree, m_store + QStringLiteral("/folder/inner.gpg")).isValid());

  tree.setStore(other);
  QVERIFY(waitFor(tree, other + QStringLiteral("/work.gpg")).isValid());

  // Nothing in the current store matches: nothing at all is listed.
  tree.setFilter(QRegularExpression(QStringLiteral("inner")));
  QVERIFY2(
      !tree.indexFor(m_store + QStringLiteral("/folder/inner.gpg")).isValid(),
      "the earlier store's entry must not match");
  const QModelIndex top = view.rootIndex();
  QStringList listed;
  std::function<void(const QModelIndex &)> walk =
      [&](const QModelIndex &parent) {
        for (int row = 0; row < tree.proxy().rowCount(parent); ++row) {
          const QModelIndex child = tree.proxy().index(row, 0, parent);
          listed << tree.fileSystem().filePath(tree.proxy().mapToSource(child));
          walk(child);
        }
      };
  walk(top);
  QVERIFY2(listed.filter(m_store).isEmpty(), qPrintable(listed.join(", ")));

  // A match in the current store still shows, the earlier store's does not.
  tree.setFilter(QRegularExpression(QStringLiteral("work|top")));
  QVERIFY(tree.indexFor(other + QStringLiteral("/work.gpg")).isValid());
  QVERIFY(!tree.indexFor(m_store + QStringLiteral("/top.gpg")).isValid());
}

/**
 * @brief A store configured through a link lists its entries, filtered or
 *        not: the boundary check compares the model's paths with the store
 *        as configured, so the model must spell them the same way, or every
 *        entry would count as outside the store and the tree go empty.
 */
void tst_storetree::aStoreReachedThroughALinkStillLists() {
#ifdef Q_OS_WIN
  QSKIP("symbolic links need privileges on Windows");
#else
  const QString link = QDir(m_dir.path()).filePath(QStringLiteral("via-link"));
  QVERIFY(QFile::link(m_store, link));
  QTreeView view;
  StoreTree tree(&view);
  tree.setStore(link);
  QVERIFY(waitFor(tree, link + QStringLiteral("/top.gpg")).isValid());
  QVERIFY(waitFor(tree, link + QStringLiteral("/folder/inner.gpg")).isValid());
  tree.setFilter(QRegularExpression(QStringLiteral("inner")));
  QVERIFY2(tree.indexFor(link + QStringLiteral("/folder/inner.gpg")).isValid(),
           "a match in a store reached through a link is listed");
  QVERIFY(!tree.indexFor(link + QStringLiteral("/top.gpg")).isValid());
#endif
}

/**
 * @brief A link inside the store is neither an entry nor a folder: it is
 *        listed by name (so it can be deleted) but has no children, and a
 *        search never reaches what is behind it. A planted `loop -> .` was
 *        an endlessly expandable folder that the search's expandAll() loaded
 *        level after level until memory ran out.
 */
void tst_storetree::aLinkInsideTheStoreIsListedButNeverEntered() {
#ifdef Q_OS_WIN
  QSKIP("symbolic links need privileges on Windows");
#else
  const QString loop = QDir(m_store).filePath(QStringLiteral("loop"));
  QVERIFY(QFile::link(QStringLiteral("."), loop));
  QTreeView view;
  StoreTree tree(&view);
  tree.setStore(m_store);
  QVERIFY(waitFor(tree, m_store + QStringLiteral("/top.gpg")).isValid());
  QVERIFY(
      waitFor(tree, m_store + QStringLiteral("/folder/inner.gpg")).isValid());
  QModelIndex link;
  QTRY_VERIFY((link = tree.indexFor(loop)).isValid());
  // Before anything behind it is loaded: no children, nothing to fetch,
  // and asking to fetch does not have the file system model list it.
  QFileSystemModel &fs = tree.fileSystem();
  QVERIFY(!tree.proxy().hasChildren(link));
  QVERIFY(!tree.proxy().canFetchMore(link));
  tree.proxy().fetchMore(link);
  QTest::qWait(200);
  QCOMPARE(fs.rowCount(fs.index(loop)), 0);
  // Have the file system model list what is behind the link, two levels
  // deep, as expanding it in the view once could.
  for (const QString &level : {loop, loop + QStringLiteral("/folder")}) {
    const QModelIndex source = fs.index(level);
    fs.fetchMore(source);
    QTRY_VERIFY(fs.rowCount(source) > 0);
  }
  tree.setFilter(QRegularExpression());
  QTRY_VERIFY((link = tree.indexFor(loop)).isValid());
  QCOMPARE(tree.proxy().rowCount(link), 0);

  // Expanding everything ends, and nothing behind the link is listed.
  tree.setFilter(QRegularExpression(QStringLiteral("inner")));
  view.expandAll();
  QStringList listed;
  std::function<void(const QModelIndex &, int)> walk =
      [&](const QModelIndex &parent, int depth) {
        QVERIFY(depth < 20);
        for (int row = 0; row < tree.proxy().rowCount(parent); ++row) {
          const QModelIndex child = tree.proxy().index(row, 0, parent);
          listed << tree.fileSystem().filePath(tree.proxy().mapToSource(child));
          walk(child, depth + 1);
        }
      };
  walk(view.rootIndex(), 0);
  QVERIFY2(listed.filter(QStringLiteral("/loop/")).isEmpty(),
           qPrintable(listed.join(", ")));
  QVERIFY2(listed.contains(m_store + QStringLiteral("/folder/inner.gpg")),
           qPrintable(listed.join(", ")));
#endif
}

QTEST_MAIN(tst_storetree)
#include "tst_storetree.moc"
