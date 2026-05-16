// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "../../../src/enums.h"
#include "../../../src/simpletransaction.h"

class tst_simpletransaction : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void transactionAddAndComplete();
  void transactionAdd();
  void transactionIsOver();
  void nestedTransaction();
  void transactionStartEndExplicit();
  void transactionQueueOrder();
  void transactionIsOverWrongIdReturnsInvalid();
  void transactionIsOverEmptyQueueReturnsInvalid();
  void transactionEndWithoutStartIsNoop();
  void transactionStartEndWithoutAddIsNoop();
  void transactionEndResultDiffersFromAdd();
  void deeplyNestedTransactionUsesLastAdd();
};

void tst_simpletransaction::transactionAddAndComplete() {
  simpleTransaction st;
  st.transactionAdd(Enums::PASS_SHOW);
  Enums::PROCESS result = st.transactionIsOver(Enums::PASS_SHOW);
  QCOMPARE(result, Enums::PASS_SHOW);
}

void tst_simpletransaction::transactionAdd() {
  simpleTransaction st;
  st.transactionAdd(Enums::PASS_SHOW);
  st.transactionAdd(Enums::PASS_REMOVE);
  Enums::PROCESS first = st.transactionIsOver(Enums::PASS_SHOW);
  Enums::PROCESS second = st.transactionIsOver(Enums::PASS_REMOVE);
  QCOMPARE(first, Enums::PASS_SHOW);
  QCOMPARE(second, Enums::PASS_REMOVE);
}

void tst_simpletransaction::transactionIsOver() {
  simpleTransaction st;
  st.transactionAdd(Enums::PASS_INSERT);
  Enums::PROCESS result = st.transactionIsOver(Enums::PASS_INSERT);
  QCOMPARE(result, Enums::PASS_INSERT);
}

void tst_simpletransaction::nestedTransaction() {
  simpleTransaction st;
  st.transactionAdd(Enums::PASS_SHOW);
  st.transactionAdd(Enums::GIT_PULL);
  Enums::PROCESS passShowResult = st.transactionIsOver(Enums::PASS_SHOW);
  QCOMPARE(passShowResult, Enums::PASS_SHOW);
  Enums::PROCESS gitPullResult = st.transactionIsOver(Enums::GIT_PULL);
  QCOMPARE(gitPullResult, Enums::GIT_PULL);
}

void tst_simpletransaction::transactionStartEndExplicit() {
  simpleTransaction st;
  st.transactionAdd(Enums::PASS_INSERT);
  st.transactionStart();
  st.transactionAdd(Enums::PASS_SHOW);
  st.transactionEnd(Enums::PASS_SHOW);
  Enums::PROCESS passInsertResult = st.transactionIsOver(Enums::PASS_INSERT);
  QVERIFY2(
      passInsertResult == Enums::PASS_INSERT,
      "PASS_INSERT should still be in queue after transactionEnd(PASS_SHOW)");
  Enums::PROCESS result = st.transactionIsOver(Enums::PASS_SHOW);
  QVERIFY2(result == Enums::PASS_SHOW,
           "transactionIsOver(PASS_SHOW) should return PASS_SHOW after end");
}

void tst_simpletransaction::transactionQueueOrder() {
  simpleTransaction st;
  st.transactionAdd(Enums::PASS_INSERT);
  st.transactionAdd(Enums::PASS_REMOVE);
  st.transactionAdd(Enums::PASS_SHOW);
  Enums::PROCESS r1 = st.transactionIsOver(Enums::PASS_INSERT);
  QVERIFY2(
      r1 == Enums::PASS_INSERT,
      "first item should complete immediately when no transaction started");
  Enums::PROCESS r2 = st.transactionIsOver(Enums::PASS_REMOVE);
  QVERIFY2(r2 == Enums::PASS_REMOVE, "PASS_REMOVE should complete second");
  Enums::PROCESS r3 = st.transactionIsOver(Enums::PASS_SHOW);
  QVERIFY2(r3 == Enums::PASS_SHOW, "PASS_SHOW should complete third");
}

void tst_simpletransaction::transactionIsOverWrongIdReturnsInvalid() {
  simpleTransaction st;
  st.transactionAdd(Enums::PASS_SHOW);
  QVERIFY2(
      st.transactionIsOver(Enums::PASS_INSERT) == Enums::INVALID,
      "transactionIsOver(PASS_INSERT) must return INVALID when queue front "
      "holds PASS_SHOW");
}

void tst_simpletransaction::transactionIsOverEmptyQueueReturnsInvalid() {
  simpleTransaction st;
  QVERIFY2(st.transactionIsOver(Enums::PASS_SHOW) == Enums::INVALID,
           "transactionIsOver(PASS_SHOW) must return INVALID on empty queue");
}

void tst_simpletransaction::transactionEndWithoutStartIsNoop() {
  simpleTransaction st;
  st.transactionEnd(Enums::PASS_SHOW);
  QVERIFY2(
      st.transactionIsOver(Enums::PASS_SHOW) == Enums::INVALID,
      "transactionEnd(PASS_SHOW) without transactionStart must not enqueue "
      "anything; transactionIsOver(PASS_SHOW) must return INVALID");
}

void tst_simpletransaction::transactionStartEndWithoutAddIsNoop() {
  simpleTransaction st;
  st.transactionStart();
  st.transactionEnd(Enums::PASS_SHOW);
  QVERIFY2(
      st.transactionIsOver(Enums::PASS_SHOW) == Enums::INVALID,
      "transactionStart + transactionEnd(PASS_SHOW) without transactionAdd "
      "must not enqueue anything; transactionIsOver(PASS_SHOW) must return "
      "INVALID");
}

void tst_simpletransaction::transactionEndResultDiffersFromAdd() {
  simpleTransaction st;
  st.transactionStart();
  st.transactionAdd(Enums::PASS_SHOW);
  st.transactionEnd(Enums::PASS_INSERT);
  QVERIFY2(
      st.transactionIsOver(Enums::PASS_SHOW) == Enums::PASS_INSERT,
      "transactionIsOver(PASS_SHOW) must return PASS_INSERT: transactionAdd "
      "sets the trigger (PASS_SHOW), transactionEnd sets the result "
      "(PASS_INSERT)");
}

void tst_simpletransaction::deeplyNestedTransactionUsesLastAdd() {
  simpleTransaction st;
  st.transactionStart();
  st.transactionAdd(Enums::GIT_ADD);
  st.transactionStart();
  st.transactionAdd(Enums::GIT_COMMIT);
  st.transactionEnd(Enums::GIT_COMMIT);
  st.transactionEnd(Enums::GIT_PUSH);
  QVERIFY2(
      st.transactionIsOver(Enums::GIT_COMMIT) == Enums::GIT_PUSH,
      "transactionIsOver(GIT_COMMIT) must return GIT_PUSH: inner "
      "transactionAdd(GIT_COMMIT) overwrites GIT_ADD as the trigger; outer "
      "transactionEnd(GIT_PUSH) sets the result");
}

QTEST_MAIN(tst_simpletransaction)
#include "tst_simpletransaction.moc"
