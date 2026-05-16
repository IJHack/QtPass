// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QString>
#include <QtTest>

#include "../../../src/passwordconfiguration.h"
#include "../../../src/qtpasssettings.h"

class tst_passwordconfig : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void passwordConfigurationDefaults();
  void passwordConfigurationSetters();
  void passwordConfigurationCharacterSets();
  void passwordConfigurationLength();
  void passwordConfigurationCustomChars();
  void charsetCountIsCorrect();
  void alphanumericContainsNoSpecialChars();
  void alphabeticalContainsNoDigits();
  void customCharsDefaultMatchAllChars();
  void allcharsContainsSpecialChars();
  void allcharsContainsUpperAndLower();
  void allcharsContainsDigits();
  void alphabeticalContainsUpperAndLower();
  void alphanumericContainsDigits();
  void allcharsIsSupersetOfAlphanumeric();
  void alphanumericIsSupersetOfAlphabetical();
  void allcharsHasNoDuplicates();
  void alphabeticalHasNoDuplicates();
  void alphanumericHasNoDuplicates();
  void allcharsLargerThanAlphanumeric();
};

void tst_passwordconfig::initTestCase() {
  // Reset any leftover test settings to ensure clean state
  QtPassSettings::setPasswordChars(QString());
  QtPassSettings::setPasswordCharsSelection(PasswordConfiguration::ALLCHARS);
}

void tst_passwordconfig::passwordConfigurationDefaults() {
  PasswordConfiguration config;
  QCOMPARE(config.length, 16);
  QCOMPARE(config.selected, PasswordConfiguration::ALLCHARS);
  QVERIFY(!config.Characters[PasswordConfiguration::ALLCHARS].isEmpty());
  QVERIFY(!config.Characters[PasswordConfiguration::ALPHABETICAL].isEmpty());
  QVERIFY(!config.Characters[PasswordConfiguration::ALPHANUMERIC].isEmpty());
}

void tst_passwordconfig::passwordConfigurationSetters() {
  // Reset any previous test settings to ensure clean state
  QtPassSettings::setPasswordChars(QString());
  QtPassSettings::setPasswordCharsSelection(PasswordConfiguration::ALLCHARS);

  QtPassSettings::setPasswordLength(24);
  QtPassSettings::setPasswordCharsSelection(
      PasswordConfiguration::ALPHANUMERIC);
  QtPassSettings::setPasswordCharsSelection(3);

  PasswordConfiguration config = QtPassSettings::getPasswordConfiguration();
  QCOMPARE(config.length, 24);
  QCOMPARE(config.selected, 3);

  // Reset after test
  QtPassSettings::setPasswordCharsSelection(PasswordConfiguration::ALLCHARS);
  QtPassSettings::setPasswordChars(QString());
}

void tst_passwordconfig::passwordConfigurationCharacterSets() {
  PasswordConfiguration config;
  QVERIFY(config.Characters[PasswordConfiguration::ALLCHARS].length() >
          config.Characters[PasswordConfiguration::ALPHABETICAL].length());
  QVERIFY(config.Characters[PasswordConfiguration::ALPHANUMERIC].length() >
          config.Characters[PasswordConfiguration::ALPHABETICAL].length());
}

void tst_passwordconfig::passwordConfigurationLength() {
  PasswordConfiguration config;
  config.length = 0;
  QCOMPARE(config.length, 0);
  config.length = 100;
  QCOMPARE(config.length, 100);
}

void tst_passwordconfig::passwordConfigurationCustomChars() {
  PasswordConfiguration config;
  QString custom = "abc123";
  config.Characters[PasswordConfiguration::CUSTOM] = custom;
  QCOMPARE(config.Characters[PasswordConfiguration::CUSTOM], custom);
}

void tst_passwordconfig::charsetCountIsCorrect() {
  QCOMPARE(static_cast<int>(PasswordConfiguration::CHARSETS_COUNT), 4);
}

void tst_passwordconfig::alphanumericContainsNoSpecialChars() {
  PasswordConfiguration config;
  const QString &chars = config.Characters[PasswordConfiguration::ALPHANUMERIC];
  for (QChar c : chars) {
    QVERIFY2(c.isLetterOrNumber(),
             qPrintable(QString("ALPHANUMERIC contains non-alphanumeric char: "
                                "'%1'")
                            .arg(c)));
  }
}

void tst_passwordconfig::alphabeticalContainsNoDigits() {
  PasswordConfiguration config;
  const QString &chars = config.Characters[PasswordConfiguration::ALPHABETICAL];
  for (QChar c : chars) {
    QVERIFY2(c.isLetter(),
             qPrintable(QString("ALPHABETICAL contains non-letter char: "
                                "'%1'")
                            .arg(c)));
  }
}

void tst_passwordconfig::customCharsDefaultMatchAllChars() {
  PasswordConfiguration config;
  QVERIFY2(config.Characters[PasswordConfiguration::CUSTOM] ==
               config.Characters[PasswordConfiguration::ALLCHARS],
           "CUSTOM character set must default to ALLCHARS");
}

void tst_passwordconfig::allcharsContainsSpecialChars() {
  PasswordConfiguration config;
  const QString &chars = config.Characters[PasswordConfiguration::ALLCHARS];
  bool hasSpecial = false;
  for (QChar c : chars) {
    if (!c.isLetterOrNumber()) {
      hasSpecial = true;
      break;
    }
  }
  QVERIFY2(hasSpecial, "ALLCHARS must contain at least one special character");
}

void tst_passwordconfig::allcharsContainsUpperAndLower() {
  PasswordConfiguration config;
  const QString &chars = config.Characters[PasswordConfiguration::ALLCHARS];
  bool hasUpper = false;
  bool hasLower = false;
  for (QChar c : chars) {
    if (c.isUpper())
      hasUpper = true;
    if (c.isLower())
      hasLower = true;
  }
  QVERIFY2(hasUpper, "ALLCHARS must contain uppercase letters");
  QVERIFY2(hasLower, "ALLCHARS must contain lowercase letters");
}

void tst_passwordconfig::allcharsContainsDigits() {
  PasswordConfiguration config;
  const QString &chars = config.Characters[PasswordConfiguration::ALLCHARS];
  bool hasDigit = false;
  for (QChar c : chars) {
    if (c.isDigit()) {
      hasDigit = true;
      break;
    }
  }
  QVERIFY2(hasDigit, "ALLCHARS must contain at least one digit");
}

void tst_passwordconfig::alphabeticalContainsUpperAndLower() {
  PasswordConfiguration config;
  const QString &chars = config.Characters[PasswordConfiguration::ALPHABETICAL];
  bool hasUpper = false;
  bool hasLower = false;
  for (QChar c : chars) {
    if (c.isUpper())
      hasUpper = true;
    if (c.isLower())
      hasLower = true;
  }
  QVERIFY2(hasUpper, "ALPHABETICAL must contain uppercase letters");
  QVERIFY2(hasLower, "ALPHABETICAL must contain lowercase letters");
}

void tst_passwordconfig::alphanumericContainsDigits() {
  PasswordConfiguration config;
  const QString &chars = config.Characters[PasswordConfiguration::ALPHANUMERIC];
  bool hasDigit = false;
  for (QChar c : chars) {
    if (c.isDigit()) {
      hasDigit = true;
      break;
    }
  }
  QVERIFY2(hasDigit, "ALPHANUMERIC must contain at least one digit");
}

void tst_passwordconfig::allcharsIsSupersetOfAlphanumeric() {
  PasswordConfiguration config;
  const QString &all = config.Characters[PasswordConfiguration::ALLCHARS];
  const QString &alnum = config.Characters[PasswordConfiguration::ALPHANUMERIC];
  for (QChar c : alnum) {
    QVERIFY2(
        all.contains(c),
        qPrintable(
            QString("ALLCHARS missing char '%1' from ALPHANUMERIC").arg(c)));
  }
}

void tst_passwordconfig::alphanumericIsSupersetOfAlphabetical() {
  PasswordConfiguration config;
  const QString &alnum = config.Characters[PasswordConfiguration::ALPHANUMERIC];
  const QString &alpha = config.Characters[PasswordConfiguration::ALPHABETICAL];
  for (QChar c : alpha) {
    QVERIFY2(
        alnum.contains(c),
        qPrintable(QString("ALPHANUMERIC missing char '%1' from ALPHABETICAL")
                       .arg(c)));
  }
}

void tst_passwordconfig::allcharsHasNoDuplicates() {
  PasswordConfiguration config;
  const QString &chars = config.Characters[PasswordConfiguration::ALLCHARS];
  QSet<QChar> seen;
  for (QChar c : chars) {
    QVERIFY2(
        !seen.contains(c),
        qPrintable(QString("ALLCHARS has duplicate character '%1'").arg(c)));
    seen.insert(c);
  }
}

void tst_passwordconfig::alphabeticalHasNoDuplicates() {
  PasswordConfiguration config;
  const QString &chars = config.Characters[PasswordConfiguration::ALPHABETICAL];
  QSet<QChar> seen;
  for (QChar c : chars) {
    QVERIFY2(!seen.contains(c),
             qPrintable(
                 QString("ALPHABETICAL has duplicate character '%1'").arg(c)));
    seen.insert(c);
  }
}

void tst_passwordconfig::alphanumericHasNoDuplicates() {
  PasswordConfiguration config;
  const QString &chars = config.Characters[PasswordConfiguration::ALPHANUMERIC];
  QSet<QChar> seen;
  for (QChar c : chars) {
    QVERIFY2(!seen.contains(c),
             qPrintable(
                 QString("ALPHANUMERIC has duplicate character '%1'").arg(c)));
    seen.insert(c);
  }
}

void tst_passwordconfig::allcharsLargerThanAlphanumeric() {
  PasswordConfiguration config;
  QVERIFY2(
      config.Characters[PasswordConfiguration::ALLCHARS].length() >
          config.Characters[PasswordConfiguration::ALPHANUMERIC].length(),
      "ALLCHARS must have more characters than ALPHANUMERIC (specials add to "
      "it)");
}

QTEST_MAIN(tst_passwordconfig)
#include "tst_passwordconfig.moc"
