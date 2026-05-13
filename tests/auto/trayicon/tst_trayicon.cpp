// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QtTest>

#include "../../../src/trayicon.h"

/**
 * @class tst_trayicon
 * @brief Widget tests for TrayIcon.
 *
 * TrayIcon wraps QSystemTrayIcon and routes activation events back to the
 * parent QMainWindow. The QSystemTrayIcon side only initialises when the
 * runtime platform reports `isSystemTrayAvailable()` — on a headless CI
 * (offscreen platform) that's typically false, so the menu/icon code path
 * is largely unreachable in unit tests.
 *
 * What IS reachable, regardless of tray availability:
 * - The parent pointer is always stored.
 * - `showHideParent()` operates on the parent QMainWindow directly.
 * - `iconActivated()` dispatches on the reason enum and only calls
 *   showHideParent for Trigger / DoubleClick. The other branches must be
 *   no-ops.
 *
 * That's what these tests cover.
 */
class tst_trayicon : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void constructionStoresParent();
  void showHideParentTogglesVisibility();
  void iconActivatedTriggerTogglesVisibility();
  void iconActivatedDoubleClickTogglesVisibility();
  void iconActivatedMiddleClickIsNoOp();
  void iconActivatedUnknownReasonIsNoOp();
  void getIsAllocatedMatchesPlatformTrayAvailability();
};

/**
 * @brief Constructor stores the parent pointer and returns without
 *        crashing regardless of tray availability.
 */
void tst_trayicon::constructionStoresParent() {
  QMainWindow parent;
  TrayIcon tray(&parent);
  // No public accessor for parentwin; the showHideParent test below
  // verifies the pointer was wired correctly. Here we just assert
  // the constructor returned and getIsAllocated() returns a bool.
  // The compiler enforces the bool return type, so this is really a
  // "doesn't crash" smoke check.
  Q_UNUSED(tray.getIsAllocated());
}

/**
 * @brief showHideParent() toggles the parent window's visibility.
 */
void tst_trayicon::showHideParentTogglesVisibility() {
  QMainWindow parent;
  TrayIcon tray(&parent);

  parent.show();
  // Spin the event loop once so the window-manager show-event lands.
  QTRY_VERIFY2(parent.isVisible(), "parent must be visible after show()");

  tray.showHideParent();
  QTRY_VERIFY2(!parent.isVisible(),
               "showHideParent should hide a visible parent");

  tray.showHideParent();
  QTRY_VERIFY2(parent.isVisible(),
               "showHideParent should re-show a hidden parent");
}

/**
 * @brief A Trigger activation toggles parent visibility (single-click /
 *        keyboard activate on platforms that map them to Trigger).
 */
void tst_trayicon::iconActivatedTriggerTogglesVisibility() {
  QMainWindow parent;
  TrayIcon tray(&parent);
  parent.show();
  QTRY_VERIFY(parent.isVisible());

  tray.iconActivated(QSystemTrayIcon::Trigger);
  QTRY_VERIFY2(!parent.isVisible(), "Trigger should toggle visibility");
}

/**
 * @brief A DoubleClick activation toggles parent visibility (Windows
 *        users typically reach the tray via double-click).
 */
void tst_trayicon::iconActivatedDoubleClickTogglesVisibility() {
  QMainWindow parent;
  TrayIcon tray(&parent);
  parent.show();
  QTRY_VERIFY(parent.isVisible());

  tray.iconActivated(QSystemTrayIcon::DoubleClick);
  QTRY_VERIFY2(!parent.isVisible(), "DoubleClick should toggle visibility");
}

/**
 * @brief A MiddleClick activation does nothing — the switch case is
 *        explicitly handled and falls through without changing state.
 */
void tst_trayicon::iconActivatedMiddleClickIsNoOp() {
  QMainWindow parent;
  TrayIcon tray(&parent);
  parent.show();
  QTRY_VERIFY(parent.isVisible());

  tray.iconActivated(QSystemTrayIcon::MiddleClick);
  // Give any pending toggle a chance to land. The assertion is "still
  // visible after a short spin", which exercises the no-op contract.
  QTest::qWait(50);
  QVERIFY2(parent.isVisible(), "MiddleClick must not change parent visibility");
}

/**
 * @brief Unknown / Context reasons fall to the default branch which is
 *        also a no-op.
 */
void tst_trayicon::iconActivatedUnknownReasonIsNoOp() {
  QMainWindow parent;
  TrayIcon tray(&parent);
  parent.show();
  QTRY_VERIFY(parent.isVisible());

  tray.iconActivated(QSystemTrayIcon::Unknown);
  QTest::qWait(50);
  QVERIFY2(parent.isVisible(),
           "Unknown reason must not change parent visibility");

  tray.iconActivated(QSystemTrayIcon::Context);
  QTest::qWait(50);
  QVERIFY2(parent.isVisible(),
           "Context reason must not change parent visibility");
}

/**
 * @brief getIsAllocated mirrors the runtime
 *        QSystemTrayIcon::isSystemTrayAvailable() check — true when the
 *        platform supports a tray, false on headless / offscreen.
 */
void tst_trayicon::getIsAllocatedMatchesPlatformTrayAvailability() {
  QMainWindow parent;
  TrayIcon tray(&parent);
  QCOMPARE(tray.getIsAllocated(), QSystemTrayIcon::isSystemTrayAvailable());
}

QTEST_MAIN(tst_trayicon)
#include "tst_trayicon.moc"
