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
  void flagsWithValidIndex();
  void flagsWithInvalidIndex();
  void mimeTypes();
  void lessThan();
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

  QModelIndex index = fsm.index(tempDir.path() + "/test.gpg");
  QVariant displayData = sm.data(index, Qt::DisplayRole);
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

  QModelIndex index = fsm.index(tempDir.path() + "/test.gpg");
  Qt::ItemFlags flags = sm.flags(index);
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

QTEST_MAIN(tst_storemodel)
#include "tst_storemodel.moc"