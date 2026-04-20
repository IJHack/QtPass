// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "../../../src/enums.h"
#include "../../../src/simpletransaction.h"

class tst_simpletransaction : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void transactionStartEnd();
  void transactionAdd();
  void transactionIsOver();
  void nestedTransaction();
  void transactionStartEndExplicit();
  void transactionQueueOrder();
  void cleanupTestCase();
};

void tst_simpletransaction::transactionStartEnd() {
  simpleTransaction st;
  st.transactionAdd(Enums::PASS_SHOW);
  Enums::PROCESS result = st.transactionIsOver(Enums::PASS_SHOW);
  QCOMPARE(result, Enums::PASS_SHOW);
}

void tst_simpletransaction::transactionAdd() {
  simpleTransaction st;
  st.transactionAdd(Enums::PASS_SHOW);
  st.transactionAdd(Enums::GIT_PULL);
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
  st.transactionStart();
  st.transactionAdd(Enums::PASS_INSERT);
  st.transactionAdd(Enums::PASS_SHOW);
  st.transactionEnd(Enums::PASS_SHOW);
  Enums::PROCESS result = st.transactionIsOver(Enums::PASS_SHOW);
  QVERIFY2(result != Enums::INVALID,
           "transaction should be complete after transactionEnd");
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
}

void tst_simpletransaction::cleanupTestCase() {}

QTEST_MAIN(tst_simpletransaction)
#include "tst_simpletransaction.moc"
