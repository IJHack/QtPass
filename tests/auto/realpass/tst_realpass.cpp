// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @brief Tests for the commands RealPass hands to `pass`.
 *
 * No real pass is needed: a stand-in pass script records its arguments and
 * the PASSWORD_STORE_DIR it was started with. pass picks its store from that
 * variable alone, so a call without it works on ~/.password-store instead of
 * the configured store.
 */

#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QScopedPointer>
#include <QTemporaryDir>
#include <QtTest>

#include "../../../src/realpass.h"
#include "../testsettings.h"

class tst_realpass : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void gitPullBlockingRunsInTheConfiguredStore();

private:
  QTemporaryDir m_dir;
  QString m_store;
  QString m_log;
  QString m_script;
};

void tst_realpass::initTestCase() {
  isolateTestSettings();
#ifdef Q_OS_WIN
  QSKIP("the stand-in pass is a shell script");
#else
  QVERIFY(m_dir.isValid());
  m_store =
      QDir(m_dir.path()).filePath(QStringLiteral("store")) + QLatin1Char('/');
  QVERIFY(QDir().mkpath(m_store));
  m_log = QDir(m_dir.path()).filePath(QStringLiteral("call.log"));
  m_script = QDir(m_dir.path()).filePath(QStringLiteral("pass"));

  QFile f(m_script);
  QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
  f.write("#!/bin/sh\n");
  f.write(QStringLiteral("LOG='%1'\n").arg(m_log).toUtf8());
  f.write("{\n  printf 'ARGS\\n'; for a in \"$@\"; do printf '%s\\n' \"$a\"; "
          "done\n");
  f.write("  printf 'STORE\\n%s\\n' \"$PASSWORD_STORE_DIR\"\n} > \"$LOG\"\n");
  f.write("exit 0\n");
  f.close();
  QVERIFY(
      f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
#endif
}

/**
 * @brief GitPull_b, the pull before an edit with "Automatically pull" on,
 *        runs pass in the store's environment: it used a fresh process in
 *        QtPass's own environment, so pass pulled ~/.password-store (or
 *        whatever store the launching shell named) instead of the configured
 *        store, and the edit dialog opened on the old copy of the entry.
 */
void tst_realpass::gitPullBlockingRunsInTheConfiguredStore() {
#ifdef Q_OS_WIN
  QSKIP("the stand-in pass is a shell script");
#else
  // QtPass started from a shell that points pass at another store.
  const QString otherStore =
      QDir(m_dir.path()).filePath(QStringLiteral("other-store"));
  QVERIFY(QDir().mkpath(otherStore));
  const QByteArray launchStore = qgetenv("PASSWORD_STORE_DIR");
  qputenv("PASSWORD_STORE_DIR", QFile::encodeName(otherStore));
  auto restore = qScopeGuard([&launchStore] {
    if (launchStore.isNull())
      qunsetenv("PASSWORD_STORE_DIR");
    else
      qputenv("PASSWORD_STORE_DIR", launchStore);
  });

  AppSettings s;
  s.passExecutable = m_script;
  s.passStore = m_store;
  s.usePass = true;
  QScopedPointer<RealPass> pass(new RealPass);
  pass->init(s);
  pass->updateEnv();

  QFile::remove(m_log);
  pass->GitPull_b();
  QVERIFY2(QFile::exists(m_log),
           "GitPull_b must not return before the stand-in pass finished");

  QFile f(m_log);
  QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString text = QString::fromUtf8(f.readAll());
  const QStringList args = text.section(QStringLiteral("ARGS\n"), 1)
                               .section(QStringLiteral("STORE\n"), 0, 0)
                               .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  const QString storeEnv = text.section(QStringLiteral("STORE\n"), 1).trimmed();
  QCOMPARE(args, (QStringList{QStringLiteral("git"), QStringLiteral("pull")}));
  QVERIFY2(!storeEnv.isEmpty(), "pass ran without PASSWORD_STORE_DIR");
  QCOMPARE(QDir(storeEnv).canonicalPath(), QDir(m_store).canonicalPath());
#endif
}

QTEST_MAIN(tst_realpass)
#include "tst_realpass.moc"
