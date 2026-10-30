// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

#include "../../../src/clipboardmanager.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/secretlineedit.h"
#include "../../../src/secrettextbrowser.h"
#include "../../../src/settingsserializer.h"
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
  void autoclearRestoresWhatWasThereBefore();
  void restoreIsOnByDefault();
  void clearWithNothingTrackedIsSilent();
  void copyIfAlwaysOnlyCopiesInAlwaysMode();
  void copyIfAlwaysDoesNotDisturbTracking();
  void trackedCopyIsClearedWithoutDroppingTheCopiedPassword();
  void secretTextBrowserMarksAndReportsWhatIsCopied();
  void aGrowingSelectionIsTrackedOnce();
  void aCopyOnTheClipboardOutlivesManySelections();
  void secretLineEditCopiesOnlyWhenShownAndMarksIt();
  void secretLineEditContextMenuCopyIsMarked();
  void secretLineEditMarksThePrimarySelection();
  void secretLineEditNeverPublishesAPlainSelection();

private:
  static void saveClipboardSettings(bool autoclear, int seconds,
                                    Enums::ClipBoardType type,
                                    bool restore = false);
};

void tst_clipboardmanager::saveClipboardSettings(bool autoclear, int seconds,
                                                 Enums::ClipBoardType type,
                                                 bool restore) {
  AppSettings s = QtPassSettings::load();
  s.useSelection = false;
  s.useAutoclear = autoclear;
  s.restoreClipboard = restore;
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
 * @brief With restoring on, autoclear puts back what was on the clipboard
 *        before the first copy, as pass -c does: a second copy in the same
 *        window keeps the user's own text, not the first password, and
 *        something copied meanwhile is left alone.
 */
void tst_clipboardmanager::autoclearRestoresWhatWasThereBefore() {
  saveClipboardSettings(true, 1, Enums::CLIPBOARD_ON_DEMAND, true);
  ClipboardManager manager;
  QSignalSpy status(&manager, &ClipboardManager::statusMessage);
  manager.copyText(QStringLiteral("hunter2"));
  manager.copyText(QStringLiteral("correct horse"));
  QTRY_COMPARE_WITH_TIMEOUT(status.count(), 3, 3000);
  QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("sentinel"));
  QCOMPARE(status.at(2).at(0).toString(), QStringLiteral("Clipboard restored"));
  QVERIFY(manager.trackedText().isEmpty());

  // Copied over meanwhile: left alone, nothing put back over it.
  status.clear();
  manager.copyText(QStringLiteral("hunter2"));
  QApplication::clipboard()->setText(QStringLiteral("grocery list"));
  QTRY_COMPARE_WITH_TIMEOUT(status.count(), 2, 3000);
  QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("grocery list"));
  QCOMPARE(status.at(1).at(0).toString(),
           QStringLiteral("Clipboard not cleared"));

  // The very text being copied was there already: not saved as "before",
  // or it would come back and stay.
  status.clear();
  QApplication::clipboard()->setText(QStringLiteral("hunter2"));
  manager.copyText(QStringLiteral("hunter2"));
  QTRY_COMPARE_WITH_TIMEOUT(status.count(), 2, 3000);
  QVERIFY2(QApplication::clipboard()->text() != QStringLiteral("hunter2"),
           "the copied secret is not put back as what was there before");

  // A copy from a shown field after the copy: cleared, not written over
  // with the text from before.
  status.clear();
  QApplication::clipboard()->setText(QStringLiteral("sentinel"));
  manager.copyText(QStringLiteral("hunter2"));
  QApplication::clipboard()->setText(QStringLiteral("pin: 1234"));
  manager.trackCopied(QStringLiteral("pin: 1234"));
  QTRY_COMPARE_WITH_TIMEOUT(status.count(), 2, 3000);
  QVERIFY(QApplication::clipboard()->text().isEmpty());
  QCOMPARE(status.at(1).at(0).toString(), QStringLiteral("Clipboard cleared"));

  // An empty copy saves nothing that a later clear could bring back.
  status.clear();
  QApplication::clipboard()->setText(QStringLiteral("old secret"));
  manager.copyText(QString());
  manager.clear();
  QApplication::clipboard()->setText(QStringLiteral("newer"));
  manager.copyText(QStringLiteral("hunter2"));
  QApplication::clipboard()->clear();
  manager.copyText(QStringLiteral("hunter2"));
  manager.clear();
  QVERIFY2(QApplication::clipboard()->text() != QStringLiteral("old secret"),
           "a text saved before an early return is forgotten");

  // The saved text copied as a secret later (by QtPass, or from a shown
  // field): not put back, cleared.
  for (const bool fromField : {false, true}) {
    status.clear();
    QApplication::clipboard()->setText(QStringLiteral("second secret"));
    manager.copyText(QStringLiteral("hunter2"));
    if (fromField) {
      // Copied from a field, then QtPass copies again: its own copy is
      // what autoclear finds, and the saved text must not come back.
      QApplication::clipboard()->setText(QStringLiteral("second secret"));
      manager.trackCopied(QStringLiteral("second secret"));
      manager.copyText(QStringLiteral("hunter2"));
    } else {
      manager.copyText(QStringLiteral("second secret"));
    }
    QTRY_COMPARE_WITH_TIMEOUT(status.count(), 3, 3000);
    QVERIFY2(QApplication::clipboard()->text().isEmpty(),
             qPrintable(QApplication::clipboard()->text()));
  }

  // Nothing there before: cleared, as without restoring.
  status.clear();
  QApplication::clipboard()->clear();
  manager.copyText(QStringLiteral("hunter2"));
  QTRY_COMPARE_WITH_TIMEOUT(status.count(), 2, 3000);
  QVERIFY(QApplication::clipboard()->text().isEmpty());
  QCOMPARE(status.at(1).at(0).toString(), QStringLiteral("Clipboard cleared"));
}

/**
 * @brief Restoring the previous clipboard is on unless switched off, as
 *        pass -c does it.
 */
void tst_clipboardmanager::restoreIsOnByDefault() {
  QVERIFY(AppSettings{}.restoreClipboard);
  // A settings file from before (1.8) has no such key: on as well.
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QSettings empty(dir.filePath(QStringLiteral("old.ini")),
                  QSettings::IniFormat);
  QVERIFY(SettingsSerializer::load(empty).restoreClipboard);
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

/**
 * @brief A mouse selection reports itself on every change (the X11 primary
 * selection); only where it ends stays tracked, so a passing one-letter
 * selection cannot have autoclear take an unrelated "a" later. A copy still
 * on the clipboard proper stays tracked beside it.
 */
void tst_clipboardmanager::aGrowingSelectionIsTrackedOnce() {
  saveClipboardSettings(true, 1, Enums::CLIPBOARD_ON_DEMAND);
  ClipboardManager manager;
  for (const QString &step :
       {QStringLiteral("a"), QStringLiteral("ab"), QStringLiteral("abc")}) {
    manager.trackCopied(step);
  }
  QApplication::clipboard()->setText(QStringLiteral("a"));
  QSignalSpy status(&manager, &ClipboardManager::statusMessage);
  QTRY_COMPARE_WITH_TIMEOUT(status.count(), 1, 3000);
  QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("a"));

  QApplication::clipboard()->setText(QStringLiteral("pin: 1234"));
  manager.trackCopied(QStringLiteral("pin: 1234"));
  manager.trackCopied(QStringLiteral("1234"));
  QTRY_VERIFY_WITH_TIMEOUT(QApplication::clipboard()->text().isEmpty(), 3000);
}

/**
 * @brief A secret copied with Ctrl+C stays tracked however many selections
 * follow before autoclear: making room in the list must not drop it while
 * it is still on the clipboard.
 */
void tst_clipboardmanager::aCopyOnTheClipboardOutlivesManySelections() {
  saveClipboardSettings(true, 1, Enums::CLIPBOARD_ON_DEMAND);
  ClipboardManager manager;
  QApplication::clipboard()->setText(QStringLiteral("pin: 1234"));
  manager.trackCopied(QStringLiteral("pin: 1234"));
  for (int i = 0; i < 40; ++i) {
    manager.trackCopied(QStringLiteral("selection %1 xyz").arg(i * 7919));
  }
  QTRY_VERIFY_WITH_TIMEOUT(QApplication::clipboard()->text().isEmpty(), 3000);
}

namespace {
/// Whether @p mime carries every hint buildClipboardMimeData() sets.
auto hasSecretHints(const QMimeData *mime) -> bool {
  const std::unique_ptr<QMimeData> expected(
      buildClipboardMimeData(QStringLiteral("x")));
  for (const QString &format : expected->formats()) {
    if (format != QLatin1String("text/plain") && !mime->hasFormat(format)) {
      return false;
    }
  }
  return true;
}
} // namespace

/**
 * @brief A hidden password copies nothing; shown with the eye button, Ctrl+C
 * puts it on the clipboard as plain text with the secret hints and reports
 * it for autoclear.
 */
void tst_clipboardmanager::secretLineEditCopiesOnlyWhenShownAndMarksIt() {
  SecretLineEdit edit;
  edit.setText(QStringLiteral("hunter2"));
  edit.setReadOnly(true);
  edit.setEchoMode(QLineEdit::Password);
  QSignalSpy copied(&edit, &SecretLineEdit::copied);
  QApplication::clipboard()->setText(QStringLiteral("before"));

  edit.selectAll();
  QTest::keySequence(&edit, QKeySequence::Copy);
  QCOMPARE(copied.count(), 0);
  QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("before"));

  edit.setEchoMode(QLineEdit::Normal);
  edit.selectAll();
  QSignalSpy changed(QApplication::clipboard(), &QClipboard::dataChanged);
  QTest::keySequence(&edit, QKeySequence::Copy);
  // Once: never a plain copy first that the marked one then replaces.
  QCOMPARE(changed.count(), 1);
  QCOMPARE(copied.count(), 1);
  QCOMPARE(copied.first().at(0).toString(), QStringLiteral("hunter2"));
  const QMimeData *mime = QApplication::clipboard()->mimeData();
  QVERIFY(mime != nullptr);
  QCOMPARE(mime->text(), QStringLiteral("hunter2"));
  QVERIFY2(!mime->hasHtml(), "a secret goes out as plain text only");
  QVERIFY(hasSecretHints(mime));
}

/**
 * @brief The context menu's Copy goes through the secret copy only. Were
 * QLineEdit::copy() still connected as well, the password would reach the
 * clipboard twice, the first time as plain text without the hints, which a
 * clipboard manager may well record before the second copy replaces it.
 */
void tst_clipboardmanager::secretLineEditContextMenuCopyIsMarked() {
  SecretLineEdit edit;
  edit.setText(QStringLiteral("hunter2"));
  edit.setReadOnly(true);
  edit.setEchoMode(QLineEdit::Normal);
  edit.selectAll();
  QSignalSpy copied(&edit, &SecretLineEdit::copied);
  QApplication::clipboard()->setText(QStringLiteral("before"));

  const std::unique_ptr<QMenu> menu(edit.createSecretContextMenu());
  QAction *copy = nullptr;
  for (QAction *action : menu->actions()) {
    if (action->objectName() == QLatin1String("edit-copy")) {
      copy = action;
    }
  }
  QVERIFY2(copy != nullptr, "the standard menu has an edit-copy action");
  QSignalSpy changed(QApplication::clipboard(), &QClipboard::dataChanged);
  copy->trigger();
  QCOMPARE(changed.count(), 1);
  QCOMPARE(copied.count(), 1);
  const QMimeData *mime = QApplication::clipboard()->mimeData();
  QVERIFY(mime != nullptr);
  QCOMPARE(mime->text(), QStringLiteral("hunter2"));
  QVERIFY(hasSecretHints(mime));
}

/**
 * @brief Where there is a primary selection (X11, Wayland), a selection made
 * in the shown password ends up there with the hints, not as QLineEdit's own
 * plain copy.
 */
void tst_clipboardmanager::secretLineEditMarksThePrimarySelection() {
  if (!QApplication::clipboard()->supportsSelection()) {
    QSKIP("this platform has no primary selection");
  }
  SecretLineEdit edit;
  edit.setText(QStringLiteral("hunter2"));
  edit.setReadOnly(true);
  edit.setEchoMode(QLineEdit::Normal);
  QSignalSpy copied(&edit, &SecretLineEdit::copied);
  edit.setCursorPosition(0);
  QTest::keyClick(&edit, Qt::Key_End, Qt::ShiftModifier);
  QCOMPARE(edit.selectedText(), QStringLiteral("hunter2"));
  QVERIFY(copied.count() >= 1);
  const QMimeData *mime =
      QApplication::clipboard()->mimeData(QClipboard::Selection);
  QVERIFY(mime != nullptr);
  QCOMPARE(mime->text(), QStringLiteral("hunter2"));
  QVERIFY(hasSecretHints(mime));
}

/**
 * @brief The primary selection never holds the shown password unmarked, not
 * even for a moment: QLineEdit's own plain-text copy, which a clipboard
 * manager watching the selection could take before it was replaced, is not
 * made at all. Every change of the selection is checked as it happens, for a
 * keyboard selection, a mouse selection, and a key (Backspace, a no-op in a
 * read-only field) that QLineEdit would answer with another copy.
 */
void tst_clipboardmanager::secretLineEditNeverPublishesAPlainSelection() {
  QClipboard *clipboard = QApplication::clipboard();
  if (!clipboard->supportsSelection()) {
    QSKIP("this platform has no primary selection");
  }
  SecretLineEdit edit;
  edit.setText(QStringLiteral("hunter2 secret"));
  edit.setReadOnly(true);
  edit.setEchoMode(QLineEdit::Normal);
  edit.show();
  QVERIFY(QTest::qWaitForWindowExposed(&edit));

  int changes = 0;
  QStringList unmarked;
  connect(clipboard, &QClipboard::selectionChanged, &edit, [&] {
    const QMimeData *mime = clipboard->mimeData(QClipboard::Selection);
    if (mime != nullptr && mime->hasText() && !mime->text().isEmpty()) {
      ++changes;
      if (!hasSecretHints(mime)) {
        unmarked << mime->text();
      }
    }
  });

  edit.setCursorPosition(0);
  QTest::keyClick(&edit, Qt::Key_End, Qt::ShiftModifier);
  QTest::keyClick(&edit, Qt::Key_Backspace);
  edit.deselect();
  const int y = edit.height() / 2;
  QTest::mousePress(&edit, Qt::LeftButton, {}, QPoint(2, y));
  QTest::mouseMove(&edit, QPoint(edit.width() - 2, y));
  QTest::mouseRelease(&edit, Qt::LeftButton, {}, QPoint(edit.width() - 2, y));
  QApplication::processEvents();

  QVERIFY2(changes > 0, "the selection should have been filled");
  QVERIFY2(unmarked.isEmpty(),
           qPrintable(QStringLiteral("unmarked in the selection: ") +
                      unmarked.join(QStringLiteral(" | "))));
}

QTEST_MAIN(tst_clipboardmanager)
#include "tst_clipboardmanager.moc"
