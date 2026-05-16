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
  QVERIFY(!ProfileInit::needsInit(QStringLiteral("/nonexistent/path/abc123")));
}

void tst_profileinit::needsInitFalseForEmptyPath() {
  QVERIFY(!ProfileInit::needsInit(QString()));
}

void tst_profileinit::needsInitTrueForDirWithoutGpgId() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QVERIFY(ProfileInit::needsInit(dir.path()));
}

void tst_profileinit::needsInitFalseForDirWithGpgId() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  QFile gpgId(QDir(dir.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly));
  gpgId.close();

  QVERIFY(!ProfileInit::needsInit(dir.path()));
}

void tst_profileinit::needsInitFalseAfterGpgIdAdded() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  QVERIFY(ProfileInit::needsInit(dir.path()));

  QFile gpgId(QDir(dir.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly));
  gpgId.close();

  QVERIFY(!ProfileInit::needsInit(dir.path()));
}

QTEST_APPLESS_MAIN(tst_profileinit)
#include "tst_profileinit.moc"
