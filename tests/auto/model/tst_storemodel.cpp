// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDataStream>
#include <QDir>
#include <QFileSystemModel>
#include <QMimeData>
#include <QtTest>

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
  void lessThan();
  void lessThanDirsFirst();
  void supportedDropActions();
  void supportedDragActions();
  void filterAcceptsRowHidden();
  void filterExcludesGitDirectory();
  void filterAcceptsRowVisible();
  void setModelAndStore();
  void showThisWithNullFs();
  void getStoreBasic();
  void filterRegularExpression();
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
  QVERIFY(
      types.contains("application/vnd+qtpass.dragAndDropInfoPasswordStore"));
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
  QVERIFY(data != nullptr);
  QVERIFY(
      data->hasFormat("application/vnd+qtpass.dragAndDropInfoPasswordStore"));
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
                  const QString &path) -> QMimeData * {
  dragAndDropInfoPasswordStore info;
  info.kind = kind;
  info.path = path;

  QByteArray encoded;
  QDataStream stream(&encoded, QIODevice::WriteOnly);
  stream << info;

  auto *mime = new QMimeData;
  mime->setData("application/vnd+qtpass.dragAndDropInfoPasswordStore", encoded);
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
    [[maybe_unused]] const bool fileOpened = f.open(QFile::WriteOnly);
    Q_ASSERT(fileOpened);
    f.close();
    filePath = tempDir.path() + "/file.gpg";

    [[maybe_unused]] const bool folderCreated =
        QDir(tempDir.path()).mkdir("folder");
    Q_ASSERT(folderCreated);
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
  mime.setData("application/vnd+qtpass.dragAndDropInfoPasswordStore",
               QByteArray());
  QVERIFY(
      !fx.sm.canDropMimeData(&mime, Qt::MoveAction, 0, 0, fx.folderProxy()));
}

void tst_storemodel::canDropColumnGreaterThanZero() {
  DropFixture fx;
  QScopedPointer<QMimeData> mime(
      makeMimeData(dragAndDropInfoPasswordStore::ItemKind::File, fx.filePath));
  // The first column is the only meaningful drop target in a file
  // browser; secondary columns (size/date/etc.) must reject drops.
  QVERIFY(!fx.sm.canDropMimeData(mime.data(), Qt::MoveAction, 0, 1,
                                 fx.folderProxy()));
}

void tst_storemodel::canDropDirOnDir() {
  DropFixture fx;
  QScopedPointer<QMimeData> mime(makeMimeData(
      dragAndDropInfoPasswordStore::ItemKind::Directory, fx.folderPath));
  QVERIFY(fx.sm.canDropMimeData(mime.data(), Qt::MoveAction, 0, 0,
                                fx.folderProxy()));
}

void tst_storemodel::canDropFileOnDir() {
  DropFixture fx;
  QScopedPointer<QMimeData> mime(
      makeMimeData(dragAndDropInfoPasswordStore::ItemKind::File, fx.filePath));
  QVERIFY(fx.sm.canDropMimeData(mime.data(), Qt::MoveAction, 0, 0,
                                fx.folderProxy()));
}

void tst_storemodel::canDropFileOnFile() {
  // file-on-file is allowed at the canDrop layer; dropMimeData() then
  // surfaces an overwrite confirmation dialog before the actual move.
  DropFixture fx;
  QScopedPointer<QMimeData> mime(
      makeMimeData(dragAndDropInfoPasswordStore::ItemKind::File, fx.filePath));
  QVERIFY(
      fx.sm.canDropMimeData(mime.data(), Qt::MoveAction, 0, 0, fx.fileProxy()));
}

void tst_storemodel::canDropDirOnFile() {
  // The one combination explicitly disallowed by the #239 spec: a
  // folder dropped onto a file has no sensible interpretation.
  DropFixture fx;
  QScopedPointer<QMimeData> mime(makeMimeData(
      dragAndDropInfoPasswordStore::ItemKind::Directory, fx.folderPath));
  QVERIFY(!fx.sm.canDropMimeData(mime.data(), Qt::MoveAction, 0, 0,
                                 fx.fileProxy()));
}

QTEST_MAIN(tst_storemodel)
#include "tst_storemodel.moc"
