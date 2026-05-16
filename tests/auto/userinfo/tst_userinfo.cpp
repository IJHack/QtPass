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
  QVERIFY2(u.fullyValid(), "fullyValid() must return true for validity='f'");
}

void tst_userinfo::fullyValidWithU() {
  UserInfo u;
  u.validity = 'u';
  QVERIFY2(u.fullyValid(), "fullyValid() must return true for validity='u'");
}

void tst_userinfo::fullyValidFalseForM() {
  UserInfo u;
  u.validity = 'm';
  QVERIFY2(!u.fullyValid(), "fullyValid() must return false for validity='m'");
}

void tst_userinfo::fullyValidFalseForOther() {
  UserInfo u;
  u.validity = '-';
  QVERIFY2(!u.fullyValid(), "fullyValid() must return false for validity='-'");
}

void tst_userinfo::marginallyValidWithM() {
  UserInfo u;
  u.validity = 'm';
  QVERIFY2(u.marginallyValid(),
           "marginallyValid() must return true for validity='m'");
}

void tst_userinfo::marginallyValidFalseForF() {
  UserInfo u;
  u.validity = 'f';
  QVERIFY2(!u.marginallyValid(),
           "marginallyValid() must return false for validity='f'");
}

void tst_userinfo::marginallyValidFalseForU() {
  UserInfo u;
  u.validity = 'u';
  QVERIFY2(!u.marginallyValid(),
           "marginallyValid() must return false for validity='u'");
}

void tst_userinfo::isValidWithF() {
  UserInfo u;
  u.validity = 'f';
  QVERIFY2(u.isValid(), "isValid() must return true for validity='f'");
}

void tst_userinfo::isValidWithU() {
  UserInfo u;
  u.validity = 'u';
  QVERIFY2(u.isValid(), "isValid() must return true for validity='u'");
}

void tst_userinfo::isValidWithM() {
  UserInfo u;
  u.validity = 'm';
  QVERIFY2(u.isValid(), "isValid() must return true for validity='m'");
}

void tst_userinfo::isValidFalseForDash() {
  UserInfo u;
  u.validity = '-';
  QVERIFY2(!u.isValid(), "isValid() must return false for validity='-'");
}

void tst_userinfo::isValidFalseForN() {
  UserInfo u;
  u.validity = 'n';
  QVERIFY2(!u.isValid(), "isValid() must return false for validity='n'");
}

void tst_userinfo::isValidFalseForUnknown() {
  UserInfo u;
  u.validity = '?';
  QVERIFY2(!u.isValid(), "isValid() must return false for validity='?'");
}

void tst_userinfo::defaultFieldValues() {
  UserInfo u;
  QCOMPARE(u.validity, '-');
  QVERIFY2(!u.have_secret, "default have_secret must be false");
  QVERIFY2(!u.enabled, "default enabled must be false");
  QVERIFY2(u.name.isEmpty(), "default name must be empty");
  QVERIFY2(u.key_id.isEmpty(), "default key_id must be empty");
  QVERIFY2(!u.expiry.isValid(), "default expiry must be invalid");
  QVERIFY2(!u.created.isValid(), "default created must be invalid");
}

QTEST_MAIN(tst_userinfo)
#include "tst_userinfo.moc"
