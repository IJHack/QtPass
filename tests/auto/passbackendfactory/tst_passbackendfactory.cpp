// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @brief Unit tests for PassBackendFactory backend selection.
 *
 * PassBackendFactory decides whether QtPass talks to the real `pass` CLI
 * (RealPass) or drives gpg/git directly (ImitatePass), caches the choice, and
 * exposes invalidate() so a settings change can force a rebuild. These tests
 * exercise that routing without needing gpg/pass installed — they only check
 * which backend type is returned and the caching/rebuild lifecycle.
 */

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

#include "../../../src/appsettings.h"
#include "../../../src/imitatepass.h"
#include "../../../src/passbackendfactory.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/realpass.h"

class tst_passbackendfactory : public QObject {
  Q_OBJECT

  QTemporaryDir m_storeDir;
  bool m_savedUsePass{};
  QString m_savedPassStore;

private Q_SLOTS:
  void initTestCase();
  void cleanup();
  void cleanupTestCase();

  void imitateModeReturnsImitatePass();
  void passModeReturnsRealPass();
  void getPassCachesInstance();
  void switchingModeRebuildsBackend();
  void createsMissingStoreDirectory();
};

void tst_passbackendfactory::initTestCase() {
  QVERIFY2(m_storeDir.isValid(), "temp store dir must be created");
  const AppSettings s = QtPassSettings::load();
  m_savedUsePass = s.usePass;
  m_savedPassStore = QtPassSettings::getPassStore();
}

void tst_passbackendfactory::cleanup() {
  // Drop the cached backend so each test starts from a clean selection.
  PassBackendFactory::invalidate();
}

void tst_passbackendfactory::cleanupTestCase() {
  QtPassSettings::setUsePass(m_savedUsePass);
  QtPassSettings::setPassStore(m_savedPassStore);
  PassBackendFactory::invalidate();
}

void tst_passbackendfactory::imitateModeReturnsImitatePass() {
  QtPassSettings::setPassStore(m_storeDir.path());
  QtPassSettings::setUsePass(false);
  PassBackendFactory::invalidate();

  Pass *p = PassBackendFactory::getPass();
  QVERIFY2(dynamic_cast<ImitatePass *>(p) != nullptr,
           "usePass=false must yield an ImitatePass backend");
}

void tst_passbackendfactory::passModeReturnsRealPass() {
  QtPassSettings::setPassStore(m_storeDir.path());
  QtPassSettings::setUsePass(true);
  PassBackendFactory::invalidate();

  Pass *p = PassBackendFactory::getPass();
  QVERIFY2(dynamic_cast<RealPass *>(p) != nullptr,
           "usePass=true must yield a RealPass backend");
}

void tst_passbackendfactory::getPassCachesInstance() {
  QtPassSettings::setPassStore(m_storeDir.path());
  QtPassSettings::setUsePass(false);
  PassBackendFactory::invalidate();

  Pass *first = PassBackendFactory::getPass();
  Pass *second = PassBackendFactory::getPass();
  QCOMPARE(first, second);
}

void tst_passbackendfactory::switchingModeRebuildsBackend() {
  QtPassSettings::setPassStore(m_storeDir.path());
  QtPassSettings::setUsePass(false);
  QVERIFY2(dynamic_cast<ImitatePass *>(PassBackendFactory::getPass()) !=
               nullptr,
           "usePass=false yields ImitatePass");

  // setUsePass() invalidates the cached backend, so the next getPass() reflects
  // the new mode without a manual invalidate().
  QtPassSettings::setUsePass(true);
  QVERIFY2(dynamic_cast<RealPass *>(PassBackendFactory::getPass()) != nullptr,
           "switching to usePass=true rebuilds the backend as RealPass");

  QtPassSettings::setUsePass(false);
  QVERIFY2(dynamic_cast<ImitatePass *>(PassBackendFactory::getPass()) !=
               nullptr,
           "switching back to usePass=false rebuilds as ImitatePass");
}

void tst_passbackendfactory::createsMissingStoreDirectory() {
  const QString sub =
      QDir::cleanPath(m_storeDir.path() + "/newly/created/store");
  QVERIFY2(!QDir(sub).exists(),
           "precondition: nested store dir must not exist");

  QtPassSettings::setPassStore(sub);
  QtPassSettings::setUsePass(false);
  PassBackendFactory::invalidate();
  PassBackendFactory::getPass();

  QVERIFY2(QDir(sub).exists(),
           "getPass() must create the store directory when it is missing");
}

QTEST_MAIN(tst_passbackendfactory)
#include "tst_passbackendfactory.moc"
