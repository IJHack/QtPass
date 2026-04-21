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

QTEST_MAIN(tst_simpletransaction)
#include "tst_simpletransaction.moc"