// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFileSystemModel>
#include <QtTest>

#include "../../../src/storemodel.h"

class tst_storemodel : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void dataRemovesGpgExtension();
  void dataWithInvalidIndex();
  void flagsWithValidIndex();
  void flagsWithInvalidIndex();
  void mimeTypes();
  void mimeData();
  void lessThan();
  void lessThanDirsFirst();
  void supportedDropActions();
  void supportedDragActions();
  void filterAcceptsRowHidden();
  void filterAcceptsRowVisible();
  void setModelAndStore();
  void showThisWithNullFs();
};

void tst_storemodel::initTestCase() {}

void tst_storemodel::dataRemovesGpgExtension() {
  QTemporaryDir tempDir;
  QFile f(tempDir.path() + "/test.gpg");
  (void)f.open(QFile::WriteOnly);
  f.close();

  QFileSystemModel fsm;
  fsm.setRootPath(tempDir.path());

  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());

  QModelIndex sourceIndex = fsm.index(tempDir.path() + "/test.gpg");
  QModelIndex proxyIndex = sm.mapFromSource(sourceIndex);
  QVariant displayData = sm.data(proxyIndex, Qt::DisplayRole);
  QString name = displayData.toString();
  QVERIFY(!name.endsWith(".gpg"));
}

void tst_storemodel::flagsWithValidIndex() {
  QTemporaryDir tempDir;
  QFile f(tempDir.path() + "/test.gpg");
  (void)f.open(QFile::WriteOnly);
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
  QFileSystemModel fsm;
  StoreModel sm;
  sm.setModelAndStore(&fsm, "/tmp");

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
  QFile f1(tempDir.path() + "/aaa.gpg");
  (void)f1.open(QFile::WriteOnly);
  f1.close();
  QFile f2(tempDir.path() + "/bbb.gpg");
  (void)f2.open(QFile::WriteOnly);
  f2.close();

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
  (void)f.open(QFile::WriteOnly);
  f.close();

  QFileSystemModel fsm;
  fsm.setRootPath(tempDir.path());

  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());

  sm.setFilterRegularExpression("nothing-matches-this");
  QModelIndex index = fsm.index(tempDir.path() + "/secret.gpg");
  bool result = sm.filterAcceptsRow(0, index.parent());
  QVERIFY(!result);
}

void tst_storemodel::filterAcceptsRowVisible() {
  QTemporaryDir tempDir;
  QFile f(tempDir.path() + "/mypassword.gpg");
  (void)f.open(QFile::WriteOnly);
  f.close();

  QFileSystemModel fsm;
  fsm.setRootPath(tempDir.path());

  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());

  sm.setFilterRegularExpression("password");
  QModelIndex index = fsm.index(tempDir.path() + "/mypassword.gpg");
  bool result = sm.filterAcceptsRow(0, index.parent());
  QVERIFY(result);
}

void tst_storemodel::dataWithInvalidIndex() {
  QFileSystemModel fsm;
  StoreModel sm;
  sm.setModelAndStore(&fsm, "/tmp");

  QModelIndex invalidIndex;
  QVariant result = sm.data(invalidIndex, Qt::DisplayRole);
  QVERIFY(result.isNull());
}

void tst_storemodel::mimeData() {
  QTemporaryDir tempDir;
  QFile f(tempDir.path() + "/testfile.gpg");
  (void)f.open(QFile::WriteOnly);
  f.close();

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
  QDir(tempDir.path()).mkdir("folder");
  QFile f(tempDir.path() + "/file.gpg");
  (void)f.open(QFile::WriteOnly);
  f.close();

  QFileSystemModel fsm;
  fsm.setRootPath(tempDir.path());

  StoreModel sm;
  sm.setModelAndStore(&fsm, tempDir.path());

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
  bool result = sm.ShowThis(index);
  QVERIFY(!result);
}

QTEST_MAIN(tst_storemodel)
#include "tst_storemodel.moc"