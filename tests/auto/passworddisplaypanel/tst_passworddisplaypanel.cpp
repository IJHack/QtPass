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

  QWidget *m_parent = nullptr;
  QGridLayout *m_grid = nullptr;
  QVBoxLayout *m_container = nullptr;
  PasswordDisplayPanel *m_panel = nullptr;

private Q_SLOTS:
  void init();
  void cleanup();
  void displayFieldsAddsRows();
  void displayFieldsSkipsEmptyPassword();
  void clearRemovesAllRows();
  void appendFieldAddsOneRow();
};

void tst_passworddisplaypanel::init() {
  m_parent = new QWidget;
  m_grid = new QGridLayout;
  m_container = new QVBoxLayout(m_parent);
  m_container->addLayout(m_grid);
  m_panel = new PasswordDisplayPanel(m_grid, m_container, m_parent);
}

void tst_passworddisplaypanel::cleanup() {
  delete m_panel;
  m_panel = nullptr;
  delete m_parent;
  m_parent = nullptr;
  m_grid = nullptr;
  m_container = nullptr;
}

void tst_passworddisplaypanel::displayFieldsAddsRows() {
  m_panel->displayFields(QStringLiteral("secret"),
                         NamedValues{{"url", "https://example.org"}});
  // Two fields (password + url), each a label + value widget => 4 grid items.
  QCOMPARE(m_grid->count(), 4);
}

void tst_passworddisplaypanel::displayFieldsSkipsEmptyPassword() {
  m_panel->displayFields(QString(), NamedValues{});
  QVERIFY2(m_grid->count() == 0,
           "An empty password with no fields must leave the grid empty");
}

void tst_passworddisplaypanel::clearRemovesAllRows() {
  m_panel->displayFields(QStringLiteral("secret"), NamedValues{});
  QVERIFY2(m_grid->count() > 0, "precondition: grid populated");
  m_panel->clear();
  QVERIFY2(m_grid->count() == 0, "clear() must remove every grid row");
}

void tst_passworddisplaypanel::appendFieldAddsOneRow() {
  const int before = m_grid->count();
  m_panel->appendField(QStringLiteral("OTP Code"), QStringLiteral("123456"));
  QCOMPARE(m_grid->count(), before + 2);
}

QTEST_MAIN(tst_passworddisplaypanel)
#include "tst_passworddisplaypanel.moc"
