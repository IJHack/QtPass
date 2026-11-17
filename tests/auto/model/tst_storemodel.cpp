// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <memory>

#include <QDataStream>
#include <QDir>
#include <QFileSystemModel>
#include <QMimeData>
#include <QTreeView>
#include <QtTest>

#include <functional>

#include "../../../src/storemodel.h"

class tst_storemodel : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void dataRemovesGpgExtension();
  void dataWithInvalidIndex();
  void flagsWithValidIndex();
  void flagsWithInvalidIndex();
  void mimeTypes();
  void mimeData();
  void mimeDataRoundTripFile();
  void mimeDataRoundTripDirectory();
  void mimeDataDeserializeUnknownKind();
  void canDropNullData();
  void canDropWrongMimeType();
  void canDropEmptyEncodedData();
  void canDropColumnGreaterThanZero();
  void canDropDirOnDir();
  void canDropFileOnDir();
  void canDropFileOnFile();
  void canDropDirOnFile();
  void dropMimeDataRejectsSourceOutsideStore();
  void dropMimeDataRejectsAbsoluteOutsideSource();
  void dropMimeDataRejectsSymlinkEscape();
  void lessThan();
  void lessThanDirsFirst();
  void supportedDropActions();
  void supportedDragActions();
  void filterAcceptsRowHidden();
  void filterExcludesGitDirectory();
  void filterAcceptsRowVisible();
  void filterRejectsAnEarlierStore();
  void aFolderLeadingBackIsNeverEntered();
  void setModelAndStore();
  void showThisWithNullFs();
  void getStoreBasic();
  void filterRegularExpression();
  void setStoreUpdatesPath();
  void dataEditRoleKeepsGpgExtension();
  void filterAcceptsNonGpgFileMatchingRegex();
  void rootIndexForNullFs();
  void rootIndexForMatchesManualMapping();
};

void tst_storemodel::dataRemovesGpgExtension() {
  QTemporaryDir tempDir;
  QFile f(tempDir.path() + "/test.gpg");
  QVERIFY(f.open(QFile::WriteOnly));
  f.close();

  QFileSystemModel fsm;
  fsm.setRootPath(tempDir.path());

  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());

  QModelIndex sourceIndex = fsm.index(tempDir.path() + "/test.gpg");
  QModelIndex proxyIndex = sm.mapFromSource(sourceIndex);
  QVariant displayData = sm.data(proxyIndex, Qt::DisplayRole);
  QVERIFY(displayData.isValid());
  QVERIFY(displayData.canConvert<QString>());
  QString name = displayData.toString();
  QVERIFY(!name.endsWith(".gpg"));
}

void tst_storemodel::flagsWithValidIndex() {
  QTemporaryDir tempDir;
  QFile f(tempDir.path() + "/test.gpg");
  QVERIFY(f.open(QFile::WriteOnly));
  f.close();

  QFileSystemModel fsm;
  fsm.setRootPath(tempDir.path());

  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());

  QModelIndex sourceIndex = fsm.index(tempDir.path() + "/test.gpg");
  QModelIndex proxyIndex = sm.mapFromSource(sourceIndex);
  Qt::ItemFlags flags = sm.flags(proxyIndex);
  QVERIFY(flags & Qt::ItemIsDragEnabled);
  QVERIFY(flags & Qt::ItemIsDropEnabled);
}

void tst_storemodel::flagsWithInvalidIndex() {
  QTemporaryDir tempDir;
  QFileSystemModel fsm;
  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());

  QModelIndex invalidIndex;
  Qt::ItemFlags flags = sm.flags(invalidIndex);
  QVERIFY(flags & Qt::ItemIsDropEnabled);
}

void tst_storemodel::mimeTypes() {
  StoreModel sm;
  QStringList types = sm.mimeTypes();
  QVERIFY2(
      types.contains("application/vnd.qtpass.dragAndDropInfoPasswordStore"),
      "mimeTypes() should advertise "
      "application/vnd.qtpass.dragAndDropInfoPasswordStore");
}

void tst_storemodel::lessThan() {
  QTemporaryDir tempDir;
  QFile passwordA(tempDir.path() + "/aaa.gpg");
  QVERIFY(passwordA.open(QFile::WriteOnly));
  passwordA.close();
  QFile passwordB(tempDir.path() + "/bbb.gpg");
  QVERIFY(passwordB.open(QFile::WriteOnly));
  passwordB.close();

  QFileSystemModel fsm;
  fsm.setRootPath(tempDir.path());

  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());

  QModelIndex indexA = fsm.index(tempDir.path() + "/aaa.gpg");
  QModelIndex indexB = fsm.index(tempDir.path() + "/bbb.gpg");

  QVERIFY(sm.lessThan(indexA, indexB));
}

void tst_storemodel::supportedDropActions() {
  StoreModel sm;
  Qt::DropActions actions = sm.supportedDropActions();
  QVERIFY(actions & Qt::CopyAction);
  QVERIFY(actions & Qt::MoveAction);
}

void tst_storemodel::supportedDragActions() {
  StoreModel sm;
  Qt::DropActions actions = sm.supportedDragActions();
  QVERIFY(actions & Qt::CopyAction);
  QVERIFY(actions & Qt::MoveAction);
}

void tst_storemodel::filterAcceptsRowHidden() {
  QTemporaryDir tempDir;
  QFile f(tempDir.path() + "/secret.gpg");
  QVERIFY(f.open(QFile::WriteOnly));
  f.close();

  QFileSystemModel fsm;
  fsm.setRootPath(tempDir.path());

  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());

  sm.setFilterRegularExpression("nothing-matches-this");
  QModelIndex index = fsm.index(tempDir.path() + "/secret.gpg");
  bool result = sm.filterAcceptsRow(index.row(), index.parent());
  QVERIFY(!result);
}

void tst_storemodel::filterExcludesGitDirectory() {
  QTemporaryDir tempDir;
  QDir dir(tempDir.path());
  QVERIFY(dir.mkdir(".git"));

  QFileSystemModel fsm;
  fsm.setRootPath(tempDir.path());

  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());

  QModelIndex gitIndex = fsm.index(tempDir.path() + "/.git");
  bool result = sm.filterAcceptsRow(gitIndex.row(), gitIndex.parent());
  QVERIFY2(!result, ".git directory should be hidden");
}

/**
 * @brief Only entries of the current store pass the filter. The file system
 *        model keeps the rows of a store shown earlier (a profile switch only
 *        re-roots it); a search matched those, and Return opened one that a
 *        save then re-encrypted to the current store's recipients.
 */
void tst_storemodel::filterRejectsAnEarlierStore() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  const QDir parent(tempDir.path());
  QVERIFY(parent.mkdir(QStringLiteral("private")));
  QVERIFY(parent.mkdir(QStringLiteral("team")));
  const QString earlier = parent.filePath(QStringLiteral("private/bank.gpg"));
  const QString current = parent.filePath(QStringLiteral("team/bank.gpg"));
  for (const QString &path : {earlier, current}) {
    QFile f(path);
    QVERIFY(f.open(QFile::WriteOnly));
  }

  QFileSystemModel fsm;
  fsm.setRootPath(parent.filePath(QStringLiteral("private")));
  QTRY_VERIFY(fsm.index(earlier).isValid());
  fsm.setRootPath(parent.filePath(QStringLiteral("team")));
  QTRY_VERIFY(fsm.index(current).isValid());

  StoreModel sm;
  sm.setModelAndStore(&fsm, parent.filePath(QStringLiteral("team")));
  sm.setFilterRegularExpression(QStringLiteral("bank"));
  const QModelIndex old = fsm.index(earlier);
  QVERIFY2(!sm.filterAcceptsRow(old.row(), old.parent()),
           "an entry of the earlier store must not match");
  const QModelIndex here = fsm.index(current);
  QVERIFY(sm.filterAcceptsRow(here.row(), here.parent()));
}

/**
 * @brief A folder inside the store that leads back into its own path is
 *        listed by its name but never entered. A planted `loop -> .` was an
 *        endlessly expandable folder that the search's expandAll() loaded
 *        level after level until memory ran out. A cycle through two links
 *        ends too, and a link to a folder elsewhere is still entered.
 */
void tst_storemodel::aFolderLeadingBackIsNeverEntered() {
#ifdef Q_OS_WIN
  QSKIP("symbolic links need privileges on Windows");
#else
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  const QDir top(tempDir.path());
  for (const QString &dir :
       {QStringLiteral("store/folder"), QStringLiteral("store/one"),
        QStringLiteral("store/two"), QStringLiteral("outside")}) {
    QVERIFY(top.mkpath(dir));
  }
  const QString store = top.filePath(QStringLiteral("store"));
  const QString inner = store + QStringLiteral("/folder/inner.gpg");
  for (const QString &path :
       {inner, top.filePath(QStringLiteral("outside/secret.gpg"))}) {
    QFile f(path);
    QVERIFY(f.open(QFile::WriteOnly));
  }
  const QString loop = store + QStringLiteral("/loop");
  QVERIFY(QFile::link(QStringLiteral("."), loop));
  QVERIFY(QFile::link(QStringLiteral("../two"),
                      store + QStringLiteral("/one/to-two")));
  QVERIFY(QFile::link(QStringLiteral("../one"),
                      store + QStringLiteral("/two/to-one")));
  QVERIFY(QFile::link(QStringLiteral("../outside"),
                      store + QStringLiteral("/shared")));
  const QString up = store + QStringLiteral("/up");
  QVERIFY(QFile::link(QStringLiteral(".."), up));

  QFileSystemModel fsm;
  fsm.setRootPath(store);
  StoreModel sm;
  sm.setModelAndStore(&fsm, store);
  QTreeView view;
  view.setModel(&sm);
  view.setRootIndex(sm.rootIndexFor(store));
  QTRY_VERIFY(fsm.index(inner).isValid());
  QTRY_VERIFY(fsm.index(loop).isValid());
  QTRY_VERIFY(fsm.index(up).isValid());
  // A link to a folder above the store leads back as much as `x -> .`;
  // a real folder is entered.
  QVERIFY(!sm.hasChildren(sm.mapFromSource(fsm.index(up))));
  QVERIFY(sm.hasChildren(
      sm.mapFromSource(fsm.index(store + QStringLiteral("/folder")))));

  // Before anything behind it is loaded: no children, nothing to fetch,
  // and asking to fetch does not have the file system model list it.
  QModelIndex link = sm.mapFromSource(fsm.index(loop));
  QVERIFY(link.isValid());
  QVERIFY(!sm.hasChildren(link));
  QVERIFY(!sm.canFetchMore(link));
  sm.fetchMore(link);
  QTest::qWait(200);
  QCOMPARE(fsm.rowCount(fsm.index(loop)), 0);
  // Have the file system model list what is behind the link, two levels
  // deep, as expanding it in the view once could.
  for (const QString &level : {loop, loop + QStringLiteral("/folder")}) {
    const QModelIndex source = fsm.index(level);
    fsm.fetchMore(source);
    QTRY_VERIFY(fsm.rowCount(source) > 0);
  }
  sm.setFilterRegularExpression(QString());
  link = sm.mapFromSource(fsm.index(loop));
  QVERIFY(link.isValid());
  QCOMPARE(sm.rowCount(link), 0);

  // Expanding everything ends. Nothing behind the loop is listed, and the
  // loop itself only by its name, which does not match.
  QStringList listed;
  bool tooDeep = false;
  std::function<void(const QModelIndex &, int)> walk =
      [&](const QModelIndex &parent, int depth) {
        if (depth >= 20) {
          tooDeep = true;
          return;
        }
        for (int row = 0; row < sm.rowCount(parent); ++row) {
          const QModelIndex child = sm.index(row, 0, parent);
          listed << fsm.filePath(sm.mapToSource(child));
          walk(child, depth + 1);
        }
      };
  const auto expandAndWalk = [&](const QString &filter) {
    sm.setFilterRegularExpression(filter);
    for (int round = 0; round < 10; ++round) {
      view.expandAll();
      QTest::qWait(50);
    }
    listed.clear();
    walk(view.rootIndex(), 0);
  };
  expandAndWalk(QStringLiteral("inner"));
  QVERIFY(!tooDeep);
  QVERIFY2(listed.contains(inner), qPrintable(listed.join(", ")));
  QVERIFY2(!listed.contains(loop), qPrintable(listed.join(", ")));
  QVERIFY2(listed.filter(QStringLiteral("/loop/")).isEmpty(),
           qPrintable(listed.join(", ")));

  // The two links that point at each other: one/to-two is entered (it
  // leads to a sibling), one/to-two/to-one is not (it leads back to one).
  sm.setFilterRegularExpression(QString());
  const QString one = store + QStringLiteral("/one");
  for (const QString &level : {one, one + QStringLiteral("/to-two")}) {
    const QModelIndex source = fsm.index(level);
    fsm.fetchMore(source);
    QTRY_VERIFY(fsm.rowCount(source) > 0);
  }
  QVERIFY(sm.hasChildren(
      sm.mapFromSource(fsm.index(one + QStringLiteral("/to-two")))));
  const QModelIndex back =
      sm.mapFromSource(fsm.index(one + QStringLiteral("/to-two/to-one")));
  QVERIFY(back.isValid());
  QVERIFY(!sm.hasChildren(back));

  // A link to a folder elsewhere is entered as before.
  const QString shared = store + QStringLiteral("/shared");
  sm.setFilterRegularExpression(QString());
  QVERIFY(sm.hasChildren(sm.mapFromSource(fsm.index(shared))));
  fsm.fetchMore(fsm.index(shared));
  QTRY_VERIFY(fsm.rowCount(fsm.index(shared)) > 0);
  expandAndWalk(QStringLiteral("secret"));
  QVERIFY2(listed.contains(store + QStringLiteral("/shared/secret.gpg")),
           qPrintable(listed.join(", ")));
#endif
}

void tst_storemodel::filterAcceptsRowVisible() {
  QTemporaryDir tempDir;
  QFile passwordFile(tempDir.path() + "/mypassword.gpg");
  QVERIFY(passwordFile.open(QFile::WriteOnly));
  passwordFile.close();

  QFileSystemModel fsm;
  fsm.setRootPath(tempDir.path());

  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());

  sm.setFilterRegularExpression("password");
  QModelIndex index = fsm.index(tempDir.path() + "/mypassword.gpg");
  bool result = sm.filterAcceptsRow(index.row(), index.parent());
  QVERIFY(result);
}

void tst_storemodel::dataWithInvalidIndex() {
  QFileSystemModel fsm;
  QTemporaryDir tempDir;
  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());

  QModelIndex invalidIndex;
  QVariant result = sm.data(invalidIndex, Qt::DisplayRole);
  QVERIFY(result.isNull());
}

void tst_storemodel::mimeData() {
  QTemporaryDir tempDir;
  QFile testFile(tempDir.path() + "/testfile.gpg");
  QVERIFY(testFile.open(QFile::WriteOnly));
  testFile.close();

  QFileSystemModel fsm;
  fsm.setRootPath(tempDir.path());

  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());

  QModelIndex sourceIndex = fsm.index(tempDir.path() + "/testfile.gpg");
  QModelIndex proxyIndex = sm.mapFromSource(sourceIndex);
  QMimeData *data = sm.mimeData(QModelIndexList() << proxyIndex);
  QVERIFY2(data != nullptr, "mimeData() should return a non-null QMimeData");
  QVERIFY2(
      data->hasFormat("application/vnd.qtpass.dragAndDropInfoPasswordStore"),
      "mimeData() should carry "
      "application/vnd.qtpass.dragAndDropInfoPasswordStore");
  delete data;
}

void tst_storemodel::lessThanDirsFirst() {
#ifdef Q_OS_MAC
  QSKIP("Directory sorting differs on macOS");
#else
  QTemporaryDir tempDir;
  QVERIFY(QDir(tempDir.path()).mkdir("folder"));
  QVERIFY(QDir(tempDir.path()).exists("folder"));
  QFile file(tempDir.path() + "/file.gpg");
  QVERIFY(file.open(QFile::WriteOnly));
  file.close();

  QFileSystemModel fsm;
  fsm.setRootPath(tempDir.path());

  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());

  QCoreApplication::processEvents();
  QTRY_VERIFY(fsm.index(tempDir.path() + "/folder").isValid());
  QTRY_VERIFY(fsm.index(tempDir.path() + "/file.gpg").isValid());

  QModelIndex sourceFolderIdx = fsm.index(tempDir.path() + "/folder");
  QModelIndex sourceFileIdx = fsm.index(tempDir.path() + "/file.gpg");

  QVERIFY(sm.lessThan(sourceFolderIdx, sourceFileIdx));
#endif
}

void tst_storemodel::setModelAndStore() {
  QFileSystemModel fsm;
  StoreModel sm;

  QString storePath = "/test/store";
  sm.setModelAndStore(&fsm, storePath);

  QVERIFY(sm.sourceModel() == &fsm);
}

void tst_storemodel::showThisWithNullFs() {
  StoreModel sm;
  QModelIndex index;
  bool result = sm.showThis(index);
  QVERIFY(!result);
}

void tst_storemodel::getStoreBasic() {
  QTemporaryDir tempDir;
  QVERIFY2(tempDir.isValid(), "Temporary directory should be valid");
  QFileSystemModel fsm;
  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());
  QString store = sm.getStore();
  QVERIFY2(store == tempDir.path(), "Store path should match");
}

void tst_storemodel::filterRegularExpression() {
  StoreModel sm;
  sm.setFilterRegularExpression(QRegularExpression("test"));
  QRegularExpression result = sm.filterRegularExpression();
  QVERIFY2(result.pattern() == "test", "Filter should match test");
}

namespace {
auto makeMimeData(dragAndDropInfoPasswordStore::ItemKind kind,
                  const QString &path) -> std::unique_ptr<QMimeData> {
  dragAndDropInfoPasswordStore info;
  info.kind = kind;
  info.path = path;

  QByteArray encoded;
  QDataStream stream(&encoded, QIODevice::WriteOnly);
  stream << info;

  auto mime = std::make_unique<QMimeData>();
  mime->setData("application/vnd.qtpass.dragAndDropInfoPasswordStore", encoded);
  return mime;
}

struct DropFixture {
  QTemporaryDir tempDir;
  QFileSystemModel fsm;
  StoreModel sm;
  QString filePath;
  QString folderPath;

  DropFixture() {
    QFile f(tempDir.path() + "/file.gpg");
    const bool fileOpened = f.open(QFile::WriteOnly);
    QVERIFY2(fileOpened, "QFile::open failed creating temp file.gpg");
    f.close();
    filePath = tempDir.path() + "/file.gpg";

    const bool folderCreated = QDir(tempDir.path()).mkdir("folder");
    QVERIFY2(folderCreated, "QDir::mkdir failed creating folder");
    folderPath = tempDir.path() + "/folder";

    fsm.setRootPath(tempDir.path());
    sm.setModelAndStore(&fsm, tempDir.path());

    // QFileSystemModel populates rows asynchronously; spin until both
    // entries are visible to the proxy before exercising drop logic.
    QTRY_VERIFY(fsm.index(filePath).isValid());
    QTRY_VERIFY(fsm.index(folderPath).isValid());
  }

  [[nodiscard]] auto fileProxy() const -> QModelIndex {
    return sm.mapFromSource(fsm.index(filePath));
  }
  [[nodiscard]] auto folderProxy() const -> QModelIndex {
    return sm.mapFromSource(fsm.index(folderPath));
  }
};
} // namespace

void tst_storemodel::mimeDataRoundTripFile() {
  dragAndDropInfoPasswordStore in;
  in.kind = dragAndDropInfoPasswordStore::ItemKind::File;
  in.path = "/path/to/file.gpg";

  QByteArray buf;
  QDataStream out(&buf, QIODevice::WriteOnly);
  out << in;

  dragAndDropInfoPasswordStore back;
  QDataStream stream(&buf, QIODevice::ReadOnly);
  stream >> back;

  QCOMPARE(back.kind, dragAndDropInfoPasswordStore::ItemKind::File);
  QCOMPARE(back.path, in.path);
}

void tst_storemodel::mimeDataRoundTripDirectory() {
  dragAndDropInfoPasswordStore in;
  in.kind = dragAndDropInfoPasswordStore::ItemKind::Directory;
  in.path = "/path/to/folder";

  QByteArray buf;
  QDataStream out(&buf, QIODevice::WriteOnly);
  out << in;

  dragAndDropInfoPasswordStore back;
  QDataStream stream(&buf, QIODevice::ReadOnly);
  stream >> back;

  QCOMPARE(back.kind, dragAndDropInfoPasswordStore::ItemKind::Directory);
  QCOMPARE(back.path, in.path);
}

void tst_storemodel::mimeDataDeserializeUnknownKind() {
  // An on-the-wire kind byte that doesn't map to File or Directory must
  // resolve to ItemKind::Unknown rather than aliasing onto a real kind.
  QByteArray buf;
  QDataStream out(&buf, QIODevice::WriteOnly);
  out << static_cast<quint8>(99) << QString("/some/path");

  dragAndDropInfoPasswordStore back;
  QDataStream stream(&buf, QIODevice::ReadOnly);
  stream >> back;

  QCOMPARE(back.kind, dragAndDropInfoPasswordStore::ItemKind::Unknown);
  QCOMPARE(back.path, QString("/some/path"));
}

void tst_storemodel::canDropNullData() {
  DropFixture fx;
  QVERIFY(
      !fx.sm.canDropMimeData(nullptr, Qt::MoveAction, 0, 0, fx.folderProxy()));
}

void tst_storemodel::canDropWrongMimeType() {
  DropFixture fx;
  QMimeData mime;
  mime.setText("not the right format");
  QVERIFY(
      !fx.sm.canDropMimeData(&mime, Qt::MoveAction, 0, 0, fx.folderProxy()));
}

void tst_storemodel::canDropEmptyEncodedData() {
  DropFixture fx;
  QMimeData mime;
  mime.setData("application/vnd.qtpass.dragAndDropInfoPasswordStore",
               QByteArray());
  QVERIFY(
      !fx.sm.canDropMimeData(&mime, Qt::MoveAction, 0, 0, fx.folderProxy()));
}

void tst_storemodel::canDropColumnGreaterThanZero() {
  DropFixture fx;
  auto mime =
      makeMimeData(dragAndDropInfoPasswordStore::ItemKind::File, fx.filePath);
  // The first column is the only meaningful drop target in a file
  // browser; secondary columns (size/date/etc.) must reject drops.
  QVERIFY(!fx.sm.canDropMimeData(mime.get(), Qt::MoveAction, 0, 1,
                                 fx.folderProxy()));
}

void tst_storemodel::canDropDirOnDir() {
  DropFixture fx;
  auto mime = makeMimeData(dragAndDropInfoPasswordStore::ItemKind::Directory,
                           fx.folderPath);
  QVERIFY(fx.sm.canDropMimeData(mime.get(), Qt::MoveAction, 0, 0,
                                fx.folderProxy()));
}

void tst_storemodel::canDropFileOnDir() {
  DropFixture fx;
  auto mime =
      makeMimeData(dragAndDropInfoPasswordStore::ItemKind::File, fx.filePath);
  QVERIFY(fx.sm.canDropMimeData(mime.get(), Qt::MoveAction, 0, 0,
                                fx.folderProxy()));
}

void tst_storemodel::canDropFileOnFile() {
  // file-on-file is allowed at the canDrop layer; dropMimeData() then
  // surfaces an overwrite confirmation dialog before the actual move.
  DropFixture fx;
  auto mime =
      makeMimeData(dragAndDropInfoPasswordStore::ItemKind::File, fx.filePath);
  QVERIFY(
      fx.sm.canDropMimeData(mime.get(), Qt::MoveAction, 0, 0, fx.fileProxy()));
}

void tst_storemodel::canDropDirOnFile() {
  // The one combination explicitly disallowed by the #239 spec: a
  // folder dropped onto a file has no sensible interpretation.
  DropFixture fx;
  auto mime = makeMimeData(dragAndDropInfoPasswordStore::ItemKind::Directory,
                           fx.folderPath);
  QVERIFY(
      !fx.sm.canDropMimeData(mime.get(), Qt::MoveAction, 0, 0, fx.fileProxy()));
}

// ---------------------------------------------------------------------------
// dropMimeData() path-traversal rejection (security regression for #1464)
//
// canDropMimeData is a UI policy layer (kind/column/MIME-type constraints)
// and intentionally does not touch the filesystem. The store-boundary check
// lives one layer down in executeDropAction. These tests exercise that
// layer with crafted mime data whose source path escapes the store and
// verify the drop is refused (returns false) without ever reaching Pass::Move
// or Pass::Copy.
// ---------------------------------------------------------------------------

void tst_storemodel::dropMimeDataRejectsSourceOutsideStore() {
  DropFixture fx;
  // The fs model rooted at the store has no concept of /etc/passwd as a
  // child; crafting that path into the mime payload simulates a malicious
  // (or buggy) source that doesn't go through StoreModel::mimeData().
  auto mime = makeMimeData(dragAndDropInfoPasswordStore::ItemKind::File,
                           QStringLiteral("/etc/passwd"));
  QVERIFY2(
      !fx.sm.dropMimeData(mime.get(), Qt::MoveAction, 0, 0, fx.folderProxy()),
      "drop with src=/etc/passwd must be refused");
}

void tst_storemodel::dropMimeDataRejectsAbsoluteOutsideSource() {
  DropFixture fx;
  // Use a path inside QTemporaryDir's parent (system tempdir) but outside
  // the store to exercise the canonical-path check on a path that exists
  // on disk, not just one that the filesystem can't resolve.
  QTemporaryDir outside;
  QVERIFY2(outside.isValid(), "outside temp dir should be valid");
  auto mime = makeMimeData(dragAndDropInfoPasswordStore::ItemKind::File,
                           outside.path() + "/elsewhere.gpg");
  QVERIFY2(
      !fx.sm.dropMimeData(mime.get(), Qt::CopyAction, 0, 0, fx.folderProxy()),
      "drop with src in a sibling tempdir must be refused");
}

void tst_storemodel::dropMimeDataRejectsSymlinkEscape() {
#ifdef Q_OS_WIN
  QSKIP("symlink creation requires elevation on Windows");
#else
  DropFixture fx;
  // A symlink physically inside the store that resolves outside —
  // canonical-path resolution catches this even though the path string
  // starts with the store root.
  QTemporaryDir outside;
  QVERIFY2(outside.isValid(), "outside temp dir should be valid");
  const QString linkPath = fx.tempDir.path() + "/escape";
  QVERIFY2(QFile::link(outside.path(), linkPath),
           "QFile::link should create symlink in store");

  auto mime =
      makeMimeData(dragAndDropInfoPasswordStore::ItemKind::File, linkPath);
  QVERIFY2(
      !fx.sm.dropMimeData(mime.get(), Qt::MoveAction, 0, 0, fx.folderProxy()),
      "drop with src=<symlink-out-of-store> must be refused");
#endif
}

void tst_storemodel::setStoreUpdatesPath() {
  QTemporaryDir tempDir;
  QFileSystemModel fsm;
  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());
  QVERIFY2(sm.getStore() == tempDir.path(),
           qPrintable(QStringLiteral("store should default to temporary "
                                     "directory: expected %1 but was %2")
                          .arg(tempDir.path())
                          .arg(sm.getStore())));

  QString newPath = tempDir.path() + "/substore";
  sm.setStore(newPath);
  QVERIFY2(sm.getStore() == newPath,
           qPrintable(QStringLiteral("store should update to substore after "
                                     "setStore: expected %1 but was %2")
                          .arg(newPath)
                          .arg(sm.getStore())));
}

void tst_storemodel::dataEditRoleKeepsGpgExtension() {
  QTemporaryDir tempDir;
  QFile f(tempDir.path() + "/secret.gpg");
  QVERIFY2(f.open(QFile::WriteOnly), "Failed to open test file for writing");
  f.close();

  QFileSystemModel fsm;
  fsm.setRootPath(tempDir.path());

  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());

  QTRY_VERIFY(fsm.index(tempDir.path() + "/secret.gpg").isValid());
  QModelIndex sourceIndex = fsm.index(tempDir.path() + "/secret.gpg");
  QModelIndex proxyIndex = sm.mapFromSource(sourceIndex);

  QVariant editData = sm.data(proxyIndex, Qt::EditRole);
  QVERIFY2(editData.isValid(), "EditRole data must be valid");
  QVERIFY2(editData.toString().endsWith(".gpg"),
           "EditRole must not strip the .gpg extension");
}

void tst_storemodel::filterAcceptsNonGpgFileMatchingRegex() {
  QTemporaryDir tempDir;
  // This test sets the filter to "readme" explicitly. A plain text file
  // without a .gpg extension should not pass the filter unless the filter
  // explicitly matches its basename. This test only covers the explicit
  // "readme" filter behavior.
  QFile f(tempDir.path() + "/readme.txt");
  QVERIFY2(f.open(QFile::WriteOnly), "Failed to open test file for writing");
  f.close();

  QFileSystemModel fsm;
  fsm.setRootPath(tempDir.path());

  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());

  QTRY_VERIFY(fsm.index(tempDir.path() + "/readme.txt").isValid());
  QModelIndex index = fsm.index(tempDir.path() + "/readme.txt");

  // Set a filter that would match "readme" if extension stripping happened
  sm.setFilterRegularExpression("readme");
  bool visible = sm.filterAcceptsRow(index.row(), index.parent());
  // readme.txt has no .gpg suffix; after stripping nothing, "readme.txt"
  // does contain "readme" so the filter accepts it. This documents the
  // actual behavior: the model does not restrict to .gpg files only.
  QVERIFY2(visible,
           "non-gpg file matching the regex is accepted by the filter");
}

void tst_storemodel::rootIndexForNullFs() {
  StoreModel sm;
  // No source model wired yet.
  QVERIFY2(!sm.rootIndexFor(QStringLiteral("/tmp")).isValid(),
           "rootIndexFor must return an invalid index without a source model");
}

void tst_storemodel::rootIndexForMatchesManualMapping() {
  QTemporaryDir tempDir;
  QVERIFY2(tempDir.isValid(), "precondition: temporary directory created");
  QFileSystemModel fsm;
  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());

  const QModelIndex viaHelper = sm.rootIndexFor(tempDir.path());
  const QModelIndex manual =
      sm.mapFromSource(fsm.setRootPath(QDir::cleanPath(tempDir.path())));
  QCOMPARE(viaHelper, manual);
}

QTEST_MAIN(tst_storemodel)
#include "tst_storemodel.moc"
