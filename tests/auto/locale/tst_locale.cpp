// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QLocale>
#include <QObject>
#include <QScopeGuard>
#include <QString>
#include <QTranslator>
#include <QtTest>

/**
 * @class TestLocale
 * @brief Exercises QtPass's translation-loading and RTL detection.
 *
 * The production code in main/main.cpp uses the QLocale-aware overload of
 * QTranslator::load() so that a system locale of e.g. "ar_MA" or "da_DK"
 * falls back to the language-only "ar" / "da" .qm file when no exact match
 * is bundled. These tests pin that behaviour against the embedded
 * localization resource.
 */
class TestLocale : public QObject {
  Q_OBJECT

private slots:
  /**
   * @brief Locales whose system code is shipped verbatim as a .qm file load
   *        directly with no fallback needed.
   */
  void loadExactMatch_data();
  void loadExactMatch();

  /**
   * @brief Locales whose country-suffixed name has no .qm of its own load via
   *        Qt's locale fallback chain to the language-only .qm.
   *
   * This is the regression test for the renaming PRs (#1328, #1350) that
   * dropped country suffixes from single-variant locales.
   */
  void loadFallback_data();
  void loadFallback();

  /**
   * @brief A wholly unknown locale must return false (no .qm), so the app
   *        falls back to the source-language strings rather than aborting.
   */
  void loadUnknownLocale();

  /**
   * @brief The "LTR"/"RTL" pivot string used by main.cpp to set layout
   *        direction must produce "RTL" for Arabic/Persian/Urdu/Hebrew and
   *        "LTR" (or empty fallback) for Latin-script locales.
   */
  void layoutDirection_data();
  void layoutDirection();

  /**
   * @brief Loading a translator must not stick to the QObject between tests:
   *        each load() call should clear the previous state.
   */
  void loadIsIdempotent();
};

void TestLocale::loadExactMatch_data() {
  QTest::addColumn<QString>("locale");
  // A representative spread that shipped as <lang>_<COUNTRY>.qm.
  QTest::newRow("en_US") << "en_US";
  QTest::newRow("en_GB") << "en_GB";
  QTest::newRow("de_DE") << "de_DE";
  QTest::newRow("nl_BE") << "nl_BE";
  QTest::newRow("nl_NL") << "nl_NL";
  QTest::newRow("zh_CN") << "zh_CN";
  QTest::newRow("zh_Hant") << "zh_Hant";
  QTest::newRow("pt_BR") << "pt_BR";
  QTest::newRow("pt_PT") << "pt_PT";
  QTest::newRow("es_MX") << "es_MX";
  QTest::newRow("fr_FR") << "fr_FR";
}

void TestLocale::loadExactMatch() {
  QFETCH(QString, locale);
  QTranslator translator;
  const bool loaded = translator.load(
      QLocale(locale), QStringLiteral("localization"), QStringLiteral("_"),
      QStringLiteral(":/localization"), QStringLiteral(".qm"));
  QVERIFY2(loaded,
           qUtf8Printable(QStringLiteral("Failed to load %1").arg(locale)));
}

void TestLocale::loadFallback_data() {
  QTest::addColumn<QString>("locale");
  // Locales whose country-suffixed file used to exist but was renamed in
  // PR #1328 (ar_MA -> ar) and PR #1350 (31 single-variant locales).
  // Qt's locale fallback should walk down to the language-only .qm.
  QTest::newRow("ar_MA -> ar") << "ar_MA";
  QTest::newRow("ar_SA -> ar") << "ar_SA";
  QTest::newRow("ar_EG -> ar") << "ar_EG";
  QTest::newRow("da_DK -> da") << "da_DK";
  QTest::newRow("ja_JP -> ja") << "ja_JP";
  QTest::newRow("ko_KR -> ko") << "ko_KR";
  QTest::newRow("pl_PL -> pl") << "pl_PL";
  QTest::newRow("ru_RU -> ru") << "ru_RU";
  QTest::newRow("sv_SE -> sv") << "sv_SE";
  QTest::newRow("uk_UA -> uk") << "uk_UA";
  QTest::newRow("ja_JP_TRADITIONAL -> ja") << "ja_JP_TRADITIONAL";
  QTest::newRow("fa_IR -> fa") << "fa_IR";
  QTest::newRow("fa_AF -> fa") << "fa_AF";
  QTest::newRow("hi_IN -> hi") << "hi_IN";
  QTest::newRow("vi_VN -> vi") << "vi_VN";
  QTest::newRow("th_TH -> th") << "th_TH";
  QTest::newRow("ur_PK -> ur") << "ur_PK";
  QTest::newRow("bn_BD -> bn") << "bn_BD";
  QTest::newRow("bn_IN -> bn") << "bn_IN";
  QTest::newRow("sw_KE -> sw") << "sw_KE";
}

void TestLocale::loadFallback() {
  QFETCH(QString, locale);
  QTranslator translator;
  const bool loaded = translator.load(
      QLocale(locale), QStringLiteral("localization"), QStringLiteral("_"),
      QStringLiteral(":/localization"), QStringLiteral(".qm"));
  QVERIFY2(
      loaded,
      qUtf8Printable(
          QStringLiteral("Failed to load %1 via locale fallback").arg(locale)));
}

void TestLocale::loadUnknownLocale() {
  QTranslator translator;
  // Klingon: no .qm exists, no fallback should hit.
  const bool loaded = translator.load(
      QLocale("tlh_KX"), QStringLiteral("localization"), QStringLiteral("_"),
      QStringLiteral(":/localization"), QStringLiteral(".qm"));
  QVERIFY2(!loaded, "load() should return false for an unknown locale");
}

void TestLocale::layoutDirection_data() {
  QTest::addColumn<QString>("locale");
  QTest::addColumn<QString>("expected");
  QTest::newRow("ar -> RTL") << "ar"
                             << "RTL";
  QTest::newRow("fa -> RTL") << "fa"
                             << "RTL";
  QTest::newRow("ur -> RTL") << "ur"
                             << "RTL";
  QTest::newRow("he -> RTL") << "he"
                             << "RTL";
  QTest::newRow("en_US -> LTR") << "en_US"
                                << "LTR";
  QTest::newRow("nl_NL -> LTR") << "nl_NL"
                                << "LTR";
  QTest::newRow("ja -> LTR") << "ja"
                             << "LTR";
  QTest::newRow("zh_CN -> LTR") << "zh_CN"
                                << "LTR";
}

void TestLocale::layoutDirection() {
  QFETCH(QString, locale);
  QFETCH(QString, expected);
  QTranslator translator;
  QVERIFY2(
      translator.load(QLocale(locale), QStringLiteral("localization"),
                      QStringLiteral("_"), QStringLiteral(":/localization"),
                      QStringLiteral(".qm")),
      qUtf8Printable(QStringLiteral("Failed to load %1 for layoutDirection "
                                    "test")
                         .arg(locale)));
  QCoreApplication::installTranslator(&translator);
  // RAII guard: ensure the translator is uninstalled even if the assertion
  // below short-circuits the function — otherwise we'd leak it into the next
  // data row and pollute QObject::tr() globally.
  auto cleanup = qScopeGuard(
      [&translator] { QCoreApplication::removeTranslator(&translator); });
  // main.cpp does: tr("LTR") == "RTL" ? RightToLeft : LeftToRight.
  // Each .ts file ships an "LTR" source string whose translation is "RTL"
  // for right-to-left scripts.
  QCOMPARE(QObject::tr("LTR"), expected);
}

void TestLocale::loadIsIdempotent() {
  QTranslator translator;
  // Loading multiple locales in sequence on the same translator should
  // not leak state — Qt's contract is that each load() resets it. Each step
  // not only verifies load() succeeded but also confirms the translator
  // returns text from the just-loaded locale (and not the previous one) by
  // probing the LTR/RTL pivot string from main.cpp.
  const auto loadAndCheck = [&translator](const QString &locale,
                                          const QString &expectedLTR) {
    const bool loaded = translator.load(
        QLocale(locale), QStringLiteral("localization"), QStringLiteral("_"),
        QStringLiteral(":/localization"), QStringLiteral(".qm"));
    QVERIFY2(loaded,
             qUtf8Printable(QStringLiteral("load(%1) failed").arg(locale)));
    const QString got = translator.translate("QObject", "LTR");
    QVERIFY2(got == expectedLTR,
             qUtf8Printable(QStringLiteral("after load(%1): tr(\"LTR\") "
                                           "expected %2 but got %3")
                                .arg(locale, expectedLTR, got)));
  };

  // Negative case: per QTranslator's contract ("The previous translation is
  // cleared regardless of whether the new translation was successfully
  // loaded") a failed load() must wipe whatever the translator was holding.
  // Probe with an unknown locale and verify the prior LTR/RTL value is gone.
  const auto loadUnknownAndCheckCleared =
      [&translator](const QString &locale, const QString &staleLTR) {
        const bool loaded = translator.load(
            QLocale(locale), QStringLiteral("localization"),
            QStringLiteral("_"), QStringLiteral(":/localization"),
            QStringLiteral(".qm"));
        QVERIFY2(!loaded, qUtf8Printable(QStringLiteral("load(%1) unexpectedly "
                                                        "succeeded")
                                             .arg(locale)));
        const QString got = translator.translate("QObject", "LTR");
        QVERIFY2(got != staleLTR,
                 qUtf8Printable(QStringLiteral("after failed load(%1): stale "
                                               "translation %2 remained active")
                                    .arg(locale, staleLTR)));
      };

  loadAndCheck("ar_MA", "RTL");                // falls back to ar (RTL)
  loadUnknownAndCheckCleared("tlh_KX", "RTL"); // failed load must clear state
  loadAndCheck("nl_NL", "LTR");                // exact Dutch (LTR)
  loadAndCheck("en_US", "LTR");                // exact English (LTR)
  loadAndCheck("fa_IR", "RTL");                // falls back to fa (RTL)
}

QTEST_MAIN(TestLocale)
#include "tst_locale.moc"
