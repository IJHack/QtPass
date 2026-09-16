// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QClipboard>
#include <QSignalSpy>
#include <QtTest>

#include "../../../src/clipboardmanager.h"
#include "../../../src/qtpasssettings.h"
#include "../testsettings.h"

/**
 * @brief ClipboardManager on its own: what it copies, what it clears, what
 *        it reports. The MainWindow round trip (#1607) lives in
 *        tst_mainwindow.
 */
class tst_clipboardmanager : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { isolateTestSettings(); }
  void init();
  void copyTextPutsTextOnTheClipboardAndReports();
  void autoclearRemovesOnlyItsOwnText();
  void autoclearLeavesOtherContentAlone();
  void clearWithNothingTrackedIsSilent();
  void copyIfAlwaysOnlyCopiesInAlwaysMode();
  void copyIfAlwaysDoesNotDisturbTracking();

private:
  static void saveClipboardSettings(bool autoclear, int seconds,
                                    Enums::clipBoardType type);
};

void tst_clipboardmanager::saveClipboardSettings(bool autoclear, int seconds,
                                                 Enums::clipBoardType type) {
  AppSettings s = QtPassSettings::load();
  s.useSelection = false;
  s.useAutoclear = autoclear;
  s.autoclearSeconds = seconds;
  s.clipBoardType = type;
  QtPassSettings::save(s);
}

void tst_clipboardmanager::init() {
  saveClipboardSettings(false, 0, Enums::CLIPBOARD_ON_DEMAND);
  QApplication::clipboard()->setText(QStringLiteral("sentinel"));
}

void tst_clipboardmanager::copyTextPutsTextOnTheClipboardAndReports() {
  ClipboardManager manager;
  QSignalSpy status(&manager, &ClipboardManager::statusMessage);
  manager.copyText(QStringLiteral("hunter2"));
  QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("hunter2"));
  QCOMPARE(manager.trackedText(), QStringLiteral("hunter2"));
  QCOMPARE(status.count(), 1);
  QVERIFY(status.first().at(0).toString().contains(QStringLiteral("Copied")));
}

void tst_clipboardmanager::autoclearRemovesOnlyItsOwnText() {
  saveClipboardSettings(true, 1, Enums::CLIPBOARD_ON_DEMAND);
  ClipboardManager manager;
  QSignalSpy status(&manager, &ClipboardManager::statusMessage);
  manager.copyText(QStringLiteral("hunter2"));
  QTRY_VERIFY_WITH_TIMEOUT(QApplication::clipboard()->text().isEmpty(), 3000);
  QVERIFY(manager.trackedText().isEmpty());
  QCOMPARE(status.count(), 2);
  QCOMPARE(status.at(1).at(0).toString(), QStringLiteral("Clipboard cleared"));
}

void tst_clipboardmanager::autoclearLeavesOtherContentAlone() {
  saveClipboardSettings(true, 1, Enums::CLIPBOARD_ON_DEMAND);
  ClipboardManager manager;
  QSignalSpy status(&manager, &ClipboardManager::statusMessage);
  manager.copyText(QStringLiteral("hunter2"));
  QApplication::clipboard()->setText(QStringLiteral("grocery list"));
  QTRY_COMPARE_WITH_TIMEOUT(status.count(), 2, 3000);
  QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("grocery list"));
  QCOMPARE(status.at(1).at(0).toString(),
           QStringLiteral("Clipboard not cleared"));
  QVERIFY2(manager.trackedText().isEmpty(),
           "tracking ends with the timer even when nothing was removed");
}

/**
 * @brief deselect() and quit call clear() unconditionally; with nothing
 *        tracked there is nothing to remove and nothing to announce. (The
 *        old code matched its empty tracker against an empty selection and
 *        reported "Clipboard cleared" on every deselect.)
 */
void tst_clipboardmanager::clearWithNothingTrackedIsSilent() {
  ClipboardManager manager;
  QSignalSpy status(&manager, &ClipboardManager::statusMessage);
  manager.clear();
  QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("sentinel"));
  QCOMPARE(status.count(), 0);
}

void tst_clipboardmanager::copyIfAlwaysOnlyCopiesInAlwaysMode() {
  ClipboardManager manager;
  manager.copyIfAlways(QStringLiteral("hunter2"), QStringLiteral("hunter2\n"));
  QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("sentinel"));

  saveClipboardSettings(false, 0, Enums::CLIPBOARD_ALWAYS);
  manager.copyIfAlways(QStringLiteral("hunter2"), QString());
  QVERIFY2(QApplication::clipboard()->text() == QStringLiteral("sentinel"),
           "an empty decrypt output must not be copied even in always mode");
  manager.copyIfAlways(QStringLiteral("hunter2"), QStringLiteral("hunter2\n"));
  QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("hunter2"));
}

/**
 * @brief The #1607 contract at unit level: showing another entry in
 *        on-demand mode must not touch what is being tracked.
 */
void tst_clipboardmanager::copyIfAlwaysDoesNotDisturbTracking() {
  ClipboardManager manager;
  manager.copyText(QStringLiteral("hunter2"));
  manager.copyIfAlways(QStringLiteral("otherpass"),
                       QStringLiteral("otherpass\n"));
  QCOMPARE(manager.trackedText(), QStringLiteral("hunter2"));
  manager.clear();
  QVERIFY(QApplication::clipboard()->text().isEmpty());
}

QTEST_MAIN(tst_clipboardmanager)
#include "tst_clipboardmanager.moc"
