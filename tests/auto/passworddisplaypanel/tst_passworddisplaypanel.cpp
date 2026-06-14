// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include <QGridLayout>
#include <QVBoxLayout>
#include <QWidget>

#include "../../../src/filecontent.h"
#include "../../../src/passworddisplaypanel.h"

class tst_passworddisplaypanel : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void displayFieldsAddsRows();
  void displayFieldsSkipsEmptyPassword();
  void clearRemovesAllRows();
  void appendFieldAddsOneRow();
};

void tst_passworddisplaypanel::displayFieldsAddsRows() {
  QWidget parent;
  auto *grid = new QGridLayout();
  auto *container = new QVBoxLayout(&parent);
  container->addLayout(grid);
  PasswordDisplayPanel panel(grid, container, &parent);

  panel.displayFields(QStringLiteral("secret"),
                      NamedValues{{"url", "https://example.org"}});
  // Two fields (password + url), each a label + value widget => 4 grid items.
  QVERIFY2(grid->count() > 0, "displayFields must populate the grid");
}

void tst_passworddisplaypanel::displayFieldsSkipsEmptyPassword() {
  QWidget parent;
  auto *grid = new QGridLayout();
  auto *container = new QVBoxLayout(&parent);
  container->addLayout(grid);
  PasswordDisplayPanel panel(grid, container, &parent);

  panel.displayFields(QString(), NamedValues{});
  QCOMPARE(grid->count(), 0);
}

void tst_passworddisplaypanel::clearRemovesAllRows() {
  QWidget parent;
  auto *grid = new QGridLayout();
  auto *container = new QVBoxLayout(&parent);
  container->addLayout(grid);
  PasswordDisplayPanel panel(grid, container, &parent);

  panel.displayFields(QStringLiteral("secret"), NamedValues{});
  QVERIFY2(grid->count() > 0, "precondition: grid populated");
  panel.clear();
  QCOMPARE(grid->count(), 0);
}

void tst_passworddisplaypanel::appendFieldAddsOneRow() {
  QWidget parent;
  auto *grid = new QGridLayout();
  auto *container = new QVBoxLayout(&parent);
  container->addLayout(grid);
  PasswordDisplayPanel panel(grid, container, &parent);

  const int before = grid->count();
  panel.appendField(QStringLiteral("OTP Code"), QStringLiteral("123456"));
  QVERIFY2(grid->count() > before, "appendField must add a row");
}

QTEST_MAIN(tst_passworddisplaypanel)
#include "tst_passworddisplaypanel.moc"
