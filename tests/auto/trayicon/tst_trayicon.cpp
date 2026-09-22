// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QAction>
#include <QApplication>
#include <QFile>
#include <QLoggingCategory>
#include <QMainWindow>
#include <QMenu>
#include <QPointer>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QtTest>

#include "../../../src/trayicon.h"

#ifndef Q_OS_WIN
/**
 * @brief Test double for QSystemTrayIcon::isSystemTrayAvailable().
 *
 * The offscreen platform has no tray, so the constructor's allocating branch
 * (actions, menu, icon) is unreachable through the real Qt query. libqtpass.a
 * is linked statically into this binary, so a definition of the exported
 * static member here binds first (ELF symbol interposition) and TrayIcon sees
 * whatever `fakeTrayAvailable` says. It defaults to false so the tests that
 * describe the no-tray platform keep their meaning; FakeTray flips it for the
 * duration of one test. On Windows the member is dllimport and cannot be
 * redefined, so those tests QSKIP there.
 */
static bool fakeTrayAvailable = false;
auto QSystemTrayIcon::isSystemTrayAvailable() -> bool {
  return fakeTrayAvailable;
}

/**
 * @brief RAII: pretend a tray is present for the enclosing scope.
 *
 * With the tray faked present, QSystemTrayIcon::show() on the offscreen
 * platform makes Qt's own QSystemTrayWatcher try a string-based connect to
 * QXcbNativeInterface::systemTrayWindowChanged(QScreen*), which the offscreen
 * native interface lacks; the resulting qt.core.qobject.connect warning is Qt
 * noise about a tray that does not exist, not TrayIcon behaviour (TrayIcon
 * itself only uses compile-time checked pointer-to-member connects). It is
 * silenced for the scope and the default rules come back on exit.
 */
struct FakeTray {
  FakeTray() {
    fakeTrayAvailable = true;
    QLoggingCategory::setFilterRules(
        QStringLiteral("qt.core.qobject.connect.warning=false"));
  }
  ~FakeTray() {
    QLoggingCategory::setFilterRules(QString());
    fakeTrayAvailable = false;
  }
};
#define REQUIRE_FAKE_TRAY() FakeTray fakeTray
#else
#define REQUIRE_FAKE_TRAY()                                                    \
  QSKIP("QSystemTrayIcon::isSystemTrayAvailable cannot be interposed on "      \
        "Windows")
#endif

namespace {
/** @brief Find the tray menu action with the given text, or nullptr. */
auto actionNamed(QMenu *menu, const QString &text) -> QAction * {
  if (menu == nullptr) {
    return nullptr;
  }
  for (QAction *a : menu->actions()) {
    if (a->text() == text) {
      return a;
    }
  }
  return nullptr;
}
} // namespace

/**
 * @class tst_trayicon
 * @brief Tests for TrayIcon.
 *
 * TrayIcon wraps QSystemTrayIcon and routes activation events back to the
 * parent QMainWindow. The QSystemTrayIcon side only initialises when the
 * runtime platform reports `isSystemTrayAvailable()` — on a headless CI
 * (offscreen platform) that's false, so the first group of tests covers what
 * is reachable regardless of tray availability:
 * - The parent pointer is always stored.
 * - `showHideParent()` operates on the parent QMainWindow directly.
 * - `iconActivated()` dispatches on the reason enum and only calls
 *   showHideParent for Trigger / DoubleClick. The other branches must be
 *   no-ops.
 *
 * The `tray*` tests then stand in for the platform query (see FakeTray) and
 * cover the allocating branch: the icon, the menu, the order of its entries,
 * and that each entry and the icon's activated signal really drive the window
 * or the application.
 */
class tst_trayicon : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void constructionStoresParent();
  void showHideParentTogglesVisibility();
  void iconActivatedTriggerTogglesVisibility();
  void iconActivatedDoubleClickTogglesVisibility();
  void iconActivatedMiddleClickIsNoOp();
  void iconActivatedUnknownReasonIsNoOp();
  void getIsAllocatedMatchesPlatformTrayAvailability();
  void isOwnedByItsParentWindow();
  void trayAvailableAllocatesIconAndMenu();
  void trayUnavailableAllocatesNothing();
  void trayMenuShowHideDriveParent();
  void trayMenuWindowStateActionsDriveParent();
  void trayMenuQuitStopsEventLoop();
  void trayActivatedSignalTogglesParent();
};

/**
 * @brief Register the application artwork (:/artwork/icon.png).
 *
 * The .qrc is compiled into libqtpass.a, but nothing in a static library runs
 * unless referenced, so the test binary has to ask for it. Without this the
 * tray icon TrayIcon sets is null and the icon tests could not tell a set
 * icon from a missing one.
 */
void tst_trayicon::initTestCase() {
  Q_INIT_RESOURCE(resources);
  QVERIFY2(QFile::exists(QStringLiteral(":/artwork/icon.png")),
           "application artwork must be reachable from the test binary");
}

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
 * @brief The tray icon is owned by the window it belongs to, so destroying the
 *        window destroys it (and with it the QSystemTrayIcon, which is what
 *        removes the icon from the notification area).
 */
void tst_trayicon::isOwnedByItsParentWindow() {
  auto *parent = new QMainWindow;
  auto *tray = new TrayIcon(parent);
  QCOMPARE(tray->parent(), parent);
  QPointer<TrayIcon> guard(tray);
  delete parent;
  QVERIFY2(guard.isNull(), "deleting the window must delete the tray icon");
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
  QTRY_VERIFY2(parent.isVisible(), "parent must be visible after show()");

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
  QTRY_VERIFY2(parent.isVisible(), "parent must be visible after show()");

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
  QTRY_VERIFY2(parent.isVisible(), "parent must be visible after show()");

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
  QTRY_VERIFY2(parent.isVisible(), "parent must be visible after show()");

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

/**
 * @brief With a tray reported present, the constructor allocates one
 *        QSystemTrayIcon (owned by the TrayIcon, shown, with the QtPass
 *        artwork and a context menu), one QMenu (owned by the window), and
 *        the menu lists Show, Hide, Minimize, Maximize, Restore, a separator
 *        and Quit in that order. Pins what users see in the notification
 *        area.
 */
void tst_trayicon::trayAvailableAllocatesIconAndMenu() {
  REQUIRE_FAKE_TRAY();
  QMainWindow parent;
  TrayIcon tray(&parent);

  QVERIFY2(tray.getIsAllocated(), "a present tray must mark isAllocated");

  const auto icons = tray.findChildren<QSystemTrayIcon *>();
  QCOMPARE(icons.size(), 1);
  QSystemTrayIcon *icon = icons.first();
  QVERIFY2(!icon->icon().isNull(),
           "tray icon must carry the qtpass-tray theme icon or the bundled "
           ":/artwork/icon.png fallback");
  QVERIFY2(icon->isVisible(), "the constructor must show() the tray icon");
  QVERIFY2(icon->contextMenu() != nullptr, "tray icon must have a menu");

  const auto menus = parent.findChildren<QMenu *>();
  QCOMPARE(menus.size(), 1);
  QCOMPARE(icon->contextMenu(), menus.first());

  QStringList texts;
  for (QAction *a : icon->contextMenu()->actions()) {
    texts << (a->isSeparator() ? QStringLiteral("---") : a->text());
  }
  const QStringList expected{"&Show",     "&Hide",    "Mi&nimize",
                             "Ma&ximize", "&Restore", QStringLiteral("---"),
                             "&Quit"};
  QCOMPARE(texts, expected);

  for (QAction *a : icon->contextMenu()->actions()) {
    if (!a->isSeparator()) {
      QCOMPARE(a->parent(), &tray);
    }
  }
}

/**
 * @brief Without a tray the constructor allocates neither icon nor menu,
 *        leaves isAllocated false so callers can hide tray options, and says
 *        so once in the qtpass debug log.
 */
void tst_trayicon::trayUnavailableAllocatesNothing() {
#ifndef Q_OS_WIN
  fakeTrayAvailable = false;
#else
  if (QSystemTrayIcon::isSystemTrayAvailable()) {
    QSKIP("a real tray is present");
  }
#endif
  // The no-tray branch explains itself in the debug log; enable the category
  // for this test so the message is emitted and checked (ignoreMessage fails
  // the test if it never arrives), then drop the rule again so the category
  // is back at its compiled-in default (info and up) for the other tests.
  QLoggingCategory::setFilterRules(QStringLiteral("qtpass.debug=true"));
  QTest::ignoreMessage(
      QtDebugMsg, "No tray icon for this OS possibly also not show options?");
  QMainWindow parent;
  TrayIcon tray(&parent);
  QLoggingCategory::setFilterRules(QString());
  QVERIFY2(!tray.getIsAllocated(), "no tray means nothing is allocated");
  QVERIFY2(tray.findChildren<QSystemTrayIcon *>().isEmpty(),
           "no QSystemTrayIcon must be created without a tray");
  QVERIFY2(parent.findChildren<QMenu *>().isEmpty(),
           "no tray menu must be created without a tray");
}

/**
 * @brief The Show and Hide menu entries are wired to the parent window:
 *        Hide hides a visible window, Show brings it back.
 */
void tst_trayicon::trayMenuShowHideDriveParent() {
  REQUIRE_FAKE_TRAY();
  QMainWindow parent;
  TrayIcon tray(&parent);
  QMenu *menu = parent.findChildren<QMenu *>().value(0);
  QAction *show = actionNamed(menu, "&Show");
  QAction *hide = actionNamed(menu, "&Hide");
  QVERIFY2(show != nullptr && hide != nullptr, "Show/Hide actions must exist");

  parent.show();
  QTRY_VERIFY2(parent.isVisible(), "parent must be visible after show()");

  hide->trigger();
  QTRY_VERIFY2(!parent.isVisible(), "Hide entry must hide the window");

  show->trigger();
  QTRY_VERIFY2(parent.isVisible(), "Show entry must show the window again");
}

/**
 * @brief Minimize, Maximize and Restore are wired to showMinimized /
 *        showMaximized / showNormal and change the window state accordingly.
 */
void tst_trayicon::trayMenuWindowStateActionsDriveParent() {
  REQUIRE_FAKE_TRAY();
  QMainWindow parent;
  TrayIcon tray(&parent);
  QMenu *menu = parent.findChildren<QMenu *>().value(0);
  QAction *minimize = actionNamed(menu, "Mi&nimize");
  QAction *maximize = actionNamed(menu, "Ma&ximize");
  QAction *restore = actionNamed(menu, "&Restore");
  QVERIFY2(minimize != nullptr && maximize != nullptr && restore != nullptr,
           "window state actions must exist");

  parent.show();
  QTRY_VERIFY2(parent.isVisible(), "parent must be visible after show()");
  QVERIFY2(!parent.isMinimized() && !parent.isMaximized(),
           "window starts in the normal state");

  minimize->trigger();
  QTRY_VERIFY2(parent.isMinimized(), "Minimize entry must minimize");

  maximize->trigger();
  QTRY_VERIFY2(parent.isMaximized(), "Maximize entry must maximize");
  QVERIFY2(!parent.isMinimized(), "showMaximized clears the minimized state");

  restore->trigger();
  QTRY_VERIFY2(!parent.isMaximized() && !parent.isMinimized(),
               "Restore entry must return to the normal state");
  QVERIFY2(parent.isVisible(), "window stays visible after Restore");
}

/**
 * @brief The Quit entry is connected to QApplication::quit: triggering it
 *        while the application event loop runs ends exec() with 0 and emits
 *        aboutToQuit. A watchdog exits with 1 if the entry did nothing.
 *        (QCoreApplication::exit is a no-op outside exec(), so a nested
 *        QEventLoop would not observe the connection.)
 */
void tst_trayicon::trayMenuQuitStopsEventLoop() {
  REQUIRE_FAKE_TRAY();
  QMainWindow parent;
  TrayIcon tray(&parent);
  QAction *quit = actionNamed(parent.findChildren<QMenu *>().value(0), "&Quit");
  QVERIFY2(quit != nullptr, "Quit action must exist");

  QSignalSpy aboutToQuit(qApp, &QCoreApplication::aboutToQuit);
  // The watchdog is a stack object so it dies with this function: a
  // qApp-owned singleShot would outlive the test and fire into a dead frame.
  QTimer watchdog;
  watchdog.setSingleShot(true);
  bool timedOut = false;
  connect(&watchdog, &QTimer::timeout, this, [&timedOut] {
    timedOut = true;
    QCoreApplication::exit(1);
  });
  watchdog.start(5000);
  QTimer::singleShot(0, quit, &QAction::trigger);
  const int rc = QCoreApplication::exec();
  watchdog.stop();
  QVERIFY2(!timedOut, "Quit entry must end the application event loop");
  QCOMPARE(rc, 0);
  QCOMPARE(aboutToQuit.count(), 1);
}

/**
 * @brief The QSystemTrayIcon::activated signal is connected to
 *        iconActivated: a Trigger emitted by the icon toggles the parent,
 *        a MiddleClick leaves it alone. This pins the connection made in the
 *        constructor, not just the slot's dispatch.
 */
void tst_trayicon::trayActivatedSignalTogglesParent() {
  REQUIRE_FAKE_TRAY();
  QMainWindow parent;
  TrayIcon tray(&parent);
  QSystemTrayIcon *icon = tray.findChildren<QSystemTrayIcon *>().value(0);
  QVERIFY2(icon != nullptr, "QSystemTrayIcon must exist");

  parent.show();
  QTRY_VERIFY2(parent.isVisible(), "parent must be visible after show()");

  emit icon->activated(QSystemTrayIcon::MiddleClick);
  QVERIFY2(parent.isVisible(), "MiddleClick via the icon must not toggle");

  emit icon->activated(QSystemTrayIcon::Trigger);
  QTRY_VERIFY2(!parent.isVisible(), "Trigger via the icon must hide");

  emit icon->activated(QSystemTrayIcon::DoubleClick);
  QTRY_VERIFY2(parent.isVisible(), "DoubleClick via the icon must show");
}

QTEST_MAIN(tst_trayicon)
#include "tst_trayicon.moc"
