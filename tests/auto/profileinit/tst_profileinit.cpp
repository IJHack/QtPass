// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "../../../src/profileinit.h"

class tst_profileinit : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void needsInitFalseForNonexistentPath();
  void needsInitFalseForEmptyPath();
  void needsInitTrueForDirWithoutGpgId();
  void needsInitFalseForDirWithGpgId();
  void needsInitFalseAfterGpgIdAdded();
};

void tst_profileinit::needsInitFalseForNonexistentPath() {
  // Build a guaranteed-absent path inside the system temp tree.
  QTemporaryDir base;
  QVERIFY2(base.isValid(), "base temp dir must be created");
  const QString absent =
      QDir(base.path()).filePath(QStringLiteral("nonexistent_subdir"));
  QVERIFY2(!ProfileInit::needsInit(absent),
           "needsInit must return false for a path that does not exist");
}

void tst_profileinit::needsInitFalseForEmptyPath() {
  QVERIFY2(!ProfileInit::needsInit(QString()),
           "needsInit must return false for an empty path");
}

void tst_profileinit::needsInitTrueForDirWithoutGpgId() {
  QTemporaryDir dir;
  QVERIFY2(dir.isValid(), "temp dir must be created");
  QVERIFY2(ProfileInit::needsInit(dir.path()),
           "needsInit must return true for a directory without .gpg-id");
}

void tst_profileinit::needsInitFalseForDirWithGpgId() {
  QTemporaryDir dir;
  QVERIFY2(dir.isValid(), "temp dir must be created");

  QFile gpgId(QDir(dir.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY2(gpgId.open(QIODevice::WriteOnly), ".gpg-id must be writable");
  gpgId.close();

  QVERIFY2(!ProfileInit::needsInit(dir.path()),
           "needsInit must return false for a directory that has .gpg-id");
}

void tst_profileinit::needsInitFalseAfterGpgIdAdded() {
  QTemporaryDir dir;
  QVERIFY2(dir.isValid(), "temp dir must be created");

  QVERIFY2(ProfileInit::needsInit(dir.path()),
           "needsInit must return true before .gpg-id is created");

  QFile gpgId(QDir(dir.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY2(gpgId.open(QIODevice::WriteOnly), ".gpg-id must be writable");
  gpgId.close();

  QVERIFY2(!ProfileInit::needsInit(dir.path()),
           "needsInit must return false after .gpg-id is created");
}

QTEST_MAIN(tst_profileinit)
#include "tst_profileinit.moc"
