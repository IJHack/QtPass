// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "../../../src/grepsearchcontroller.h"

class tst_grepsearchcontroller : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void defaultsToTreeMode();
  void enterAndLeaveMode();
  void beginSearchShowsCursorOnce();
  void cancelRestoresCursorOnlyWhenBusy();
  void finishRestoresCursorAndKeepsResults();
  void finishDiscardsWhenCancelled();
  void leaveWhileBusyInterruptsAndDiscards();
  void clearGrepModeLeavesBusyUntouched();
};

void tst_grepsearchcontroller::defaultsToTreeMode() {
  GrepSearchController c;
  QVERIFY2(!c.inGrepMode(), "A fresh controller must start in tree mode");
}

void tst_grepsearchcontroller::enterAndLeaveMode() {
  GrepSearchController c;
  c.enterGrepMode();
  QVERIFY2(c.inGrepMode(), "enterGrepMode must activate grep mode");
  // No search in flight: leaving does not ask to restore the cursor.
  QCOMPARE(c.leaveGrepMode(), false);
  QVERIFY2(!c.inGrepMode(), "leaveGrepMode must deactivate grep mode");
}

void tst_grepsearchcontroller::beginSearchShowsCursorOnce() {
  GrepSearchController c;
  c.enterGrepMode();
  // First search: newly busy -> show wait cursor.
  QCOMPARE(c.beginSearch(), true);
  // Second search while still busy: no second cursor push.
  QCOMPARE(c.beginSearch(), false);
}

void tst_grepsearchcontroller::cancelRestoresCursorOnlyWhenBusy() {
  GrepSearchController c;
  c.enterGrepMode();
  // Cancel with no busy search: nothing to restore.
  QCOMPARE(c.cancelSearch(), false);
  // Now make it busy, then cancel: restore the cursor.
  QVERIFY2(c.beginSearch(), "First search must report newly busy");
  QCOMPARE(c.cancelSearch(), true);
}

void tst_grepsearchcontroller::finishRestoresCursorAndKeepsResults() {
  GrepSearchController c;
  c.enterGrepMode();
  QVERIFY2(c.beginSearch(), "First search must report newly busy");
  const GrepSearchController::FinishOutcome outcome = c.finishSearch();
  QCOMPARE(outcome.restoreCursor, true);
  QCOMPARE(outcome.discard, false);
}

void tst_grepsearchcontroller::finishDiscardsWhenCancelled() {
  GrepSearchController c;
  c.enterGrepMode();
  QVERIFY2(c.beginSearch(), "First search must report newly busy");
  QVERIFY2(c.cancelSearch(),
           "Cancelling a busy search must report restore-cursor");
  const GrepSearchController::FinishOutcome outcome = c.finishSearch();
  // cancelSearch already cleared busy, so no cursor restore here, but the
  // late-arriving results must be discarded.
  QCOMPARE(outcome.restoreCursor, false);
  QCOMPARE(outcome.discard, true);
  // The discard flag is consumed: a subsequent finish keeps results.
  const GrepSearchController::FinishOutcome subsequent = c.finishSearch();
  QCOMPARE(subsequent.restoreCursor, false);
  QCOMPARE(subsequent.discard, false);
}

void tst_grepsearchcontroller::leaveWhileBusyInterruptsAndDiscards() {
  GrepSearchController c;
  c.enterGrepMode();
  QVERIFY2(c.beginSearch(), "First search must report newly busy");
  // Toggling grep off mid-search restores the cursor...
  QCOMPARE(c.leaveGrepMode(), true);
  QVERIFY2(!c.inGrepMode(), "leaveGrepMode must deactivate grep mode");
  // ...and the in-flight results are discarded when they arrive.
  QCOMPARE(c.finishSearch().discard, true);
}

void tst_grepsearchcontroller::clearGrepModeLeavesBusyUntouched() {
  GrepSearchController c;
  c.enterGrepMode();
  QVERIFY2(c.beginSearch(), "First search must report newly busy");
  c.clearGrepMode();
  QVERIFY2(!c.inGrepMode(), "clearGrepMode must deactivate grep mode");
  // clearGrepMode does not cancel: the busy search still restores the cursor
  // and keeps its results on completion.
  const GrepSearchController::FinishOutcome outcome = c.finishSearch();
  QCOMPARE(outcome.restoreCursor, true);
  QCOMPARE(outcome.discard, false);
}

QTEST_MAIN(tst_grepsearchcontroller)
#include "tst_grepsearchcontroller.moc"
