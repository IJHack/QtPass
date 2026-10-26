// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QClipboard>
#include <QSignalSpy>
#include <QtTest>

#include "../../../src/clipboardmanager.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/secrettextbrowser.h"
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
  void trackedCopyIsClearedWithoutDroppingTheCopiedPassword();
  void secretTextBrowserMarksAndReportsWhatIsCopied();

private:
  static void saveClipboardSettings(bool autoclear, int seconds,
                                    Enums::ClipBoardType type);
};

void tst_clipboardmanager::saveClipboardSettings(bool autoclear, int seconds,
                                                 Enums::ClipBoardType type) {
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

/**
 * @brief Text copied out of a secret view is tracked beside the password a
 * copy button put there, not instead of it: both are cleared (#1607 again
 * otherwise, the password staying past its autoclear).
 */
void tst_clipboardmanager::
    trackedCopyIsClearedWithoutDroppingTheCopiedPassword() {
  saveClipboardSettings(true, 1, Enums::CLIPBOARD_ON_DEMAND);
  ClipboardManager manager;
  manager.copyText(QStringLiteral("hunter2"));
  manager.trackCopied(QStringLiteral("pin: 1234"));
  QCOMPARE(manager.trackedText(), QStringLiteral("hunter2"));
  QTRY_VERIFY_WITH_TIMEOUT(QApplication::clipboard()->text().isEmpty(), 3000);

  manager.copyText(QStringLiteral("hunter2"));
  QApplication::clipboard()->setText(QStringLiteral("pin: 1234"));
  manager.trackCopied(QStringLiteral("pin: 1234"));
  QTRY_VERIFY_WITH_TIMEOUT(QApplication::clipboard()->text().isEmpty(), 3000);
  QVERIFY(manager.trackedText().isEmpty());
}

/**
 * @brief What is copied out of a SecretTextBrowser carries the same secret
 * hints as ClipboardManager's own copies, as plain text, and is reported for
 * autoclear.
 */
void tst_clipboardmanager::secretTextBrowserMarksAndReportsWhatIsCopied() {
  SecretTextBrowser browser;
  browser.setPlainText(QStringLiteral("pin: 1234"));
  browser.selectAll();
  QSignalSpy copied(&browser, &SecretTextBrowser::copied);
  browser.copy();
  QCOMPARE(copied.count(), 1);
  QCOMPARE(copied.first().at(0).toString(), QStringLiteral("pin: 1234"));
  const QMimeData *mime = QApplication::clipboard()->mimeData();
  QVERIFY(mime != nullptr);
  QCOMPARE(mime->text(), QStringLiteral("pin: 1234"));
  QVERIFY2(!mime->hasHtml(), "a secret goes out as plain text only");
  QMimeData *expected = buildClipboardMimeData(QStringLiteral("x"));
  for (const QString &format : expected->formats()) {
    if (format != QLatin1String("text/plain")) {
      QVERIFY2(mime->hasFormat(format), qPrintable(format));
    }
  }
  delete expected;
}

QTEST_MAIN(tst_clipboardmanager)
#include "tst_clipboardmanager.moc"
