// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QScrollBar>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolButton>
#include <QtTest>

#include "../../../src/processoutputpanel.h"

/**
 * @brief The console on its own: numbering, trimming, prefixes, the line
 *        cap and the auto-scroll hysteresis. None of this needed a
 *        gpg-backed MainWindow, and none of it was tested while it lived
 *        there.
 */
class tst_processoutputpanel : public QObject {
  Q_OBJECT

private slots:
  void numbersAndPrefixesEveryLine();
  void trimsCarriageReturnsAndTrailingSpaceButKeepsIndent();
  void errorsAreRed();
  void escapesHtml();
  void capsAtMaxLinesDroppingTheOldest();
  void clearResetsCounterAndAutoScroll();
  void scrollingUpStopsAutoScrollUntilBackAtBottom();
  void draggingTheSliderDoesNotReArmAutoScroll();
  void processNamesAndSensitivity();
  void everyShownProcessHasItsCommandLabel_data();
  void everyShownProcessHasItsCommandLabel();
  void processNameServesAsTheAppendPrefix();

private:
  static auto edit(ProcessOutputPanel &panel) -> QTextEdit * {
    return panel.findChild<QTextEdit *>(QStringLiteral("processOutputEdit"));
  }
};

void tst_processoutputpanel::numbersAndPrefixesEveryLine() {
  ProcessOutputPanel panel;
  panel.append(QStringLiteral("one\ntwo\n"), false, QStringLiteral("git push"));
  panel.append(QStringLiteral("three"), false);
  QCOMPARE(panel.lineCount(), 3);
  const QString text = edit(panel)->toPlainText();
  QCOMPARE(text,
           QStringLiteral("1: git push: one\n2: git push: two\n3: three"));
}

void tst_processoutputpanel::
    trimsCarriageReturnsAndTrailingSpaceButKeepsIndent() {
  ProcessOutputPanel panel;
  panel.append(QStringLiteral("  indented\r\n\n   \nplain  \t\r\n"), false);
  QCOMPARE(panel.lineCount(), 2);
  QCOMPARE(edit(panel)->toPlainText(),
           QStringLiteral("1:   indented\n2: plain"));
}

void tst_processoutputpanel::errorsAreRed() {
  ProcessOutputPanel panel;
  panel.append(QStringLiteral("fatal: nope"), true);
  QVERIFY2(edit(panel)->toHtml().contains(QStringLiteral("color:#ff0000")),
           qPrintable(edit(panel)->toHtml()));
  panel.append(QStringLiteral("fine"), false);
  QCOMPARE(edit(panel)->document()->blockCount(), 2);
}

void tst_processoutputpanel::escapesHtml() {
  ProcessOutputPanel panel;
  panel.append(QStringLiteral("<b>&</b>"), false);
  QCOMPARE(edit(panel)->toPlainText(), QStringLiteral("1: <b>&</b>"));
}

void tst_processoutputpanel::capsAtMaxLinesDroppingTheOldest() {
  ProcessOutputPanel panel;
  QStringList many;
  for (int i = 0; i < ProcessOutputPanel::MaxLines + 5; ++i) {
    many << QStringLiteral("line");
  }
  panel.append(many.join(QLatin1Char('\n')), false);
  QCOMPARE(edit(panel)->document()->blockCount(), ProcessOutputPanel::MaxLines);
  QVERIFY2(edit(panel)->toPlainText().startsWith(QStringLiteral("6: line")),
           "the five oldest lines must be the ones dropped");
  QCOMPARE(panel.lineCount(), ProcessOutputPanel::MaxLines + 5);
}

void tst_processoutputpanel::clearResetsCounterAndAutoScroll() {
  ProcessOutputPanel panel;
  panel.append(QStringLiteral("a\nb"), false);
  auto *button =
      panel.findChild<QToolButton *>(QStringLiteral("clearOutputButton"));
  QVERIFY(button != nullptr);
  button->click();
  QCOMPARE(panel.lineCount(), 0);
  QVERIFY(edit(panel)->toPlainText().isEmpty());
  QVERIFY(panel.autoScroll());
  panel.append(QStringLiteral("again"), false);
  QCOMPARE(edit(panel)->toPlainText(), QStringLiteral("1: again"));
}

namespace {
void fillPastTheViewport(ProcessOutputPanel &panel) {
  panel.resize(300, 80);
  panel.show();
  QStringList many;
  for (int i = 0; i < 60; ++i) {
    many << QStringLiteral("line %1").arg(i);
  }
  panel.append(many.join(QLatin1Char('\n')), false);
  QCoreApplication::processEvents();
}
} // namespace

void tst_processoutputpanel::scrollingUpStopsAutoScrollUntilBackAtBottom() {
  ProcessOutputPanel panel;
  fillPastTheViewport(panel);
  QVERIFY(QTest::qWaitForWindowExposed(&panel));
  QScrollBar *bar = edit(panel)->verticalScrollBar();
  QVERIFY2(bar->maximum() > 0, "the test needs more lines than fit");
  QVERIFY(panel.autoScroll());

  bar->setValue(0);
  QVERIFY2(!panel.autoScroll(), "scrolling up must switch auto-scroll off");
  const int before = bar->value();
  panel.append(QStringLiteral("more"), false);
  QCOMPARE(bar->value(), before);

  bar->setValue(bar->maximum());
  QVERIFY2(panel.autoScroll(), "back at the bottom re-arms auto-scroll");
}

void tst_processoutputpanel::draggingTheSliderDoesNotReArmAutoScroll() {
  ProcessOutputPanel panel;
  fillPastTheViewport(panel);
  QVERIFY(QTest::qWaitForWindowExposed(&panel));
  QScrollBar *bar = edit(panel)->verticalScrollBar();
  bar->setValue(0);
  QVERIFY(!panel.autoScroll());

  // A drag that overshoots to the bottom mid-way must not commit until
  // release.
  bar->setSliderDown(true);
  bar->setValue(bar->maximum());
  QVERIFY2(!panel.autoScroll(),
           "a value change while the slider is held must not re-arm");
  bar->setValue(bar->maximum() / 2);
  bar->setSliderDown(false);
  QVERIFY2(!panel.autoScroll(), "released half-way: still off");
  bar->setSliderDown(true);
  bar->setValue(bar->maximum());
  bar->setSliderDown(false);
  QVERIFY2(panel.autoScroll(), "released at the bottom: on again");
}

void tst_processoutputpanel::processNamesAndSensitivity() {
  QCOMPARE(ProcessOutputPanel::processName(Enums::GIT_PUSH),
           QStringLiteral("git push"));
  QCOMPARE(ProcessOutputPanel::processName(Enums::GPG_GENKEYS),
           QStringLiteral("gpg --gen-key"));
  QVERIFY(ProcessOutputPanel::processName(Enums::PASS_SHOW).isEmpty());
  QVERIFY(ProcessOutputPanel::processName(Enums::INVALID).isEmpty());
  for (Enums::PROCESS pid :
       {Enums::PASS_SHOW, Enums::PASS_GREP, Enums::PASS_INSERT}) {
    QVERIFY2(ProcessOutputPanel::isSensitiveProcess(pid),
             "secret-bearing processes must never reach the panel");
  }
  for (Enums::PROCESS pid : {Enums::GIT_PUSH, Enums::PASS_INIT,
                             Enums::GPG_GENKEYS, Enums::INVALID}) {
    QVERIFY(!ProcessOutputPanel::isSensitiveProcess(pid));
  }
}

/**
 * @brief Pin the exact label of every process the panel can show. The
 *        labels are what the user reads next to each output line, so a
 *        renamed or swapped case (e.g. "git mv" vs "git move") must fail
 *        here rather than go unnoticed.
 */
void tst_processoutputpanel::everyShownProcessHasItsCommandLabel_data() {
  // One row per enum value below, plus PROCESS_COUNT and INVALID themselves.
  static_assert(Enums::PROCESS_COUNT == 16,
                "new Enums::PROCESS value: add its label row to this table");
  QTest::addColumn<Enums::PROCESS>("pid");
  QTest::addColumn<QString>("label");
  QTest::newRow("git init") << Enums::GIT_INIT << QStringLiteral("git init");
  QTest::newRow("git add") << Enums::GIT_ADD << QStringLiteral("git add");
  QTest::newRow("git commit")
      << Enums::GIT_COMMIT << QStringLiteral("git commit");
  QTest::newRow("git rm") << Enums::GIT_RM << QStringLiteral("git rm");
  QTest::newRow("git pull") << Enums::GIT_PULL << QStringLiteral("git pull");
  QTest::newRow("git push") << Enums::GIT_PUSH << QStringLiteral("git push");
  QTest::newRow("git mv") << Enums::GIT_MOVE << QStringLiteral("git mv");
  QTest::newRow("git cp") << Enums::GIT_COPY << QStringLiteral("git cp");
  QTest::newRow("pass insert")
      << Enums::PASS_INSERT << QStringLiteral("pass insert");
  QTest::newRow("pass rm") << Enums::PASS_REMOVE << QStringLiteral("pass rm");
  QTest::newRow("pass init") << Enums::PASS_INIT << QStringLiteral("pass init");
  QTest::newRow("pass mv") << Enums::PASS_MOVE << QStringLiteral("pass mv");
  QTest::newRow("pass cp") << Enums::PASS_COPY << QStringLiteral("pass cp");
  QTest::newRow("pass grep") << Enums::PASS_GREP << QStringLiteral("pass grep");
  QTest::newRow("gpg --gen-key")
      << Enums::GPG_GENKEYS << QStringLiteral("gpg --gen-key");
  QTest::newRow("pass show (never shown)") << Enums::PASS_SHOW << QString();
  QTest::newRow("PROCESS_COUNT (sentinel)")
      << Enums::PROCESS_COUNT << QString();
  QTest::newRow("INVALID") << Enums::INVALID << QString();
}

void tst_processoutputpanel::everyShownProcessHasItsCommandLabel() {
  QFETCH(Enums::PROCESS, pid);
  QFETCH(QString, label);
  QCOMPARE(ProcessOutputPanel::processName(pid), label);
}

/**
 * @brief The label processName() returns is meant to be fed straight into
 *        append() as the line prefix; check the round trip for the cases
 *        with a distinct spelling ("git mv", "git cp", "pass rm") so the
 *        rendered line reads "N: git mv: ...".
 */
void tst_processoutputpanel::processNameServesAsTheAppendPrefix() {
  ProcessOutputPanel panel;
  panel.append(QStringLiteral("renamed"), false,
               ProcessOutputPanel::processName(Enums::GIT_MOVE));
  panel.append(QStringLiteral("copied"), false,
               ProcessOutputPanel::processName(Enums::GIT_COPY));
  panel.append(QStringLiteral("removed"), true,
               ProcessOutputPanel::processName(Enums::PASS_REMOVE));
  QCOMPARE(edit(panel)->toPlainText(),
           QStringLiteral(
               "1: git mv: renamed\n2: git cp: copied\n3: pass rm: removed"));
}

QTEST_MAIN(tst_processoutputpanel)
#include "tst_processoutputpanel.moc"
