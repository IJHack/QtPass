// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "../../../src/userinfo.h"

class tst_userinfo : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void defaultValidityIsInvalid();
  void fullyValidWithF();
  void fullyValidWithU();
  void fullyValidFalseForM();
  void fullyValidFalseForOther();
  void marginallyValidWithM();
  void marginallyValidFalseForF();
  void marginallyValidFalseForU();
  void isValidWithF();
  void isValidWithU();
  void isValidWithM();
  void isValidFalseForDash();
  void isValidFalseForN();
  void isValidFalseForUnknown();
  void defaultFieldValues();
};

void tst_userinfo::defaultValidityIsInvalid() {
  UserInfo u;
  QVERIFY2(!u.isValid(), "default UserInfo must not be valid");
}

void tst_userinfo::fullyValidWithF() {
  UserInfo u;
  u.validity = 'f';
  QVERIFY(u.fullyValid());
}

void tst_userinfo::fullyValidWithU() {
  UserInfo u;
  u.validity = 'u';
  QVERIFY(u.fullyValid());
}

void tst_userinfo::fullyValidFalseForM() {
  UserInfo u;
  u.validity = 'm';
  QVERIFY(!u.fullyValid());
}

void tst_userinfo::fullyValidFalseForOther() {
  UserInfo u;
  u.validity = '-';
  QVERIFY(!u.fullyValid());
}

void tst_userinfo::marginallyValidWithM() {
  UserInfo u;
  u.validity = 'm';
  QVERIFY(u.marginallyValid());
}

void tst_userinfo::marginallyValidFalseForF() {
  UserInfo u;
  u.validity = 'f';
  QVERIFY(!u.marginallyValid());
}

void tst_userinfo::marginallyValidFalseForU() {
  UserInfo u;
  u.validity = 'u';
  QVERIFY(!u.marginallyValid());
}

void tst_userinfo::isValidWithF() {
  UserInfo u;
  u.validity = 'f';
  QVERIFY(u.isValid());
}

void tst_userinfo::isValidWithU() {
  UserInfo u;
  u.validity = 'u';
  QVERIFY(u.isValid());
}

void tst_userinfo::isValidWithM() {
  UserInfo u;
  u.validity = 'm';
  QVERIFY(u.isValid());
}

void tst_userinfo::isValidFalseForDash() {
  UserInfo u;
  u.validity = '-';
  QVERIFY(!u.isValid());
}

void tst_userinfo::isValidFalseForN() {
  UserInfo u;
  u.validity = 'n'; // not valid per GPG
  QVERIFY(!u.isValid());
}

void tst_userinfo::isValidFalseForUnknown() {
  UserInfo u;
  u.validity = '?';
  QVERIFY(!u.isValid());
}

void tst_userinfo::defaultFieldValues() {
  UserInfo u;
  QCOMPARE(u.validity, '-');
  QVERIFY(!u.have_secret);
  QVERIFY(!u.enabled);
  QVERIFY(u.name.isEmpty());
  QVERIFY(u.key_id.isEmpty());
  QVERIFY(!u.expiry.isValid());
  QVERIFY(!u.created.isValid());
}

QTEST_APPLESS_MAIN(tst_userinfo)
#include "tst_userinfo.moc"
