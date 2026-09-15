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
 *
 * The suite also pins the Pass signal surface the backends share, so a signal
 * that nothing emits or connects does not quietly reappear.
 */

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

#include "../../../src/appsettings.h"
#include "../../../src/imitatepass.h"
#include "../../../src/passbackendfactory.h"
#include "../../../src/qtpass.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/realpass.h"
#include "../testsettings.h"

class tst_passbackendfactory : public QObject {
  Q_OBJECT

  QTemporaryDir m_storeDir;

private Q_SLOTS:
  void initTestCase();
  void cleanup();
  void cleanupTestCase();

  void imitateModeReturnsImitatePass();
  void passModeReturnsRealPass();
  void getPassCachesInstance();
  void switchingModeRebuildsBackend();
  void createsMissingStoreDirectory();
  void passSignalSurfaceHasNoDeadSignals();
};

void tst_passbackendfactory::initTestCase() {
  isolateTestSettings();
  QVERIFY2(m_storeDir.isValid(), "temp store dir must be created");
}

void tst_passbackendfactory::cleanup() {
  // Drop the cached backend so each test starts from a clean selection.
  PassBackendFactory::invalidate();
}

void tst_passbackendfactory::cleanupTestCase() {
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

/**
 * Pass::error, Pass::finishedAny and Pass::finishedGenerate were never
 * emitted (start failures reach Pass::finished through Executor::error, and
 * finishedAnyWithPid is the live "any" signal), and QtPass::processError was
 * the only receiver of Pass::error. Pin the meta-object so they do not creep
 * back in as a second, silently unused, notification path.
 */
void tst_passbackendfactory::passSignalSurfaceHasNoDeadSignals() {
  const QMetaObject &pass = Pass::staticMetaObject;
  QCOMPARE(pass.indexOfSignal("error(QProcess::ProcessError)"), -1);
  QCOMPARE(pass.indexOfSignal("finishedAny(QString,QString)"), -1);
  QCOMPARE(pass.indexOfSignal("finishedGenerate(QString,QString)"), -1);
  QCOMPARE(QtPass::staticMetaObject.indexOfSlot(
               "processError(QProcess::ProcessError)"),
           -1);

  // The live counterparts must still be there, so the check above cannot be
  // satisfied by a renamed or mistyped signature.
  QVERIFY(pass.indexOfSignal("processErrorExit(int,QString)") >= 0);
  QVERIFY(pass.indexOfSignal(
              "finishedAnyWithPid(QString,QString,Enums::PROCESS)") >= 0);
}

QTEST_MAIN(tst_passbackendfactory)
#include "tst_passbackendfactory.moc"
