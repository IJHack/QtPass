// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "../../../src/enums.h"
#include "../../../src/simpletransaction.h"

class tst_simpletransaction : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void transactionStartEnd();
  void transactionAdd();
  void transactionIsOver();
  void nestedTransaction();
  void cleanupTestCase();
};

void tst_simpletransaction::initTestCase() {}

void tst_simpletransaction::transactionStartEnd() {
  simpleTransaction st;
  st.transactionStart();
  st.transactionEnd(Enums::PASS_SHOW);
  Enums::PROCESS result = st.transactionIsOver(Enums::PASS_SHOW);
  QVERIFY(result == Enums::PASS_SHOW || result == Enums::INVALID);
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
  st.transactionStart();
  st.transactionStart();
  st.transactionAdd(Enums::GIT_PULL);
  st.transactionEnd(Enums::PASS_SHOW);
  Enums::PROCESS result = st.transactionIsOver(Enums::GIT_PULL);
  QVERIFY(result == Enums::PASS_SHOW || result == Enums::INVALID);
}

void tst_simpletransaction::cleanupTestCase() {}

QTEST_MAIN(tst_simpletransaction)
#include "tst_simpletransaction.moc"
