// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @brief Widget tests for MainWindow.
 *
 * These tests exercise the public surface of MainWindow in isolation.
 * Each test gets a fresh window instance; a single QTemporaryDir serves as
 * the pass store for the whole run.
 *
 * Coverage deferred here (requires live GPG / pass / git operations):
 * - addPassword / addFolder / renameFolder / renamePassword — invoke Pass
 * - on_treeView_clicked / doubleClicked — need a populated store model
 * - onGrepFinished — depends on a running grep
 * - generateKeyPair — spawns gpg key generation
 */

#include <QApplication>
#include <QFile>
#include <QScopedPointer>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTreeView>
#include <QtTest>

#include "../../../src/mainwindow.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/util.h"

class tst_mainwindow : public QObject {
  Q_OBJECT

  QTemporaryDir m_storeDir;
  QScopedPointer<MainWindow> m_window;
  QString m_gpgPath;
  QString m_savedPassStore;
  bool m_savedUsePass;
  bool m_savedShowProcessOutput;
  QString m_savedGpgExecutable;

private Q_SLOTS:
  void initTestCase();
  void init();
  void cleanup();
  void cleanupTestCase();

  void constructionDoesNotCrash();
  void getKeygenDialogInitiallyNull();
  void cleanKeygenDialogWithNullIsHarmless();
  void setUiElementsEnabledDisablesTreeView();
  void setUiElementsEnabledEnablesTreeView();
  void flashTextSetsContent();
  void flashTextErrorDoesNotCrash();
  void flashTextHtmlRenderedInBrowser();
  void showStatusMessageAppearsInStatusBar();
  void deselectDoesNotCrash();
  void onProcessOutputAppendsToPanel();
  void onProcessOutputSkippedWhenPanelHidden();
};

void tst_mainwindow::initTestCase() {
  QVERIFY2(m_storeDir.isValid(), "temp store dir must be created");

  // Minimal valid pass store: just a .gpg-id file
  QFile gpgId(m_storeDir.path() + QStringLiteral("/.gpg-id"));
  QVERIFY2(gpgId.open(QIODevice::WriteOnly), ".gpg-id must be writable");
  gpgId.write("0000000000000000\n");
  gpgId.close();

  // Save original settings before modifying them
  m_savedPassStore = QtPassSettings::getPassStore();
  m_savedUsePass = QtPassSettings::isUsePass();
  m_savedShowProcessOutput = QtPassSettings::isShowProcessOutput();
  m_savedGpgExecutable = QtPassSettings::getGpgExecutable();

  // Point QtPassSettings at the temp store and use gpg (not pass) mode so
  // configIsValid() only requires the .gpg-id file + a gpg binary.
  QtPassSettings::setPassStore(m_storeDir.path());
  QtPassSettings::setUsePass(false);

  // Verify gpg is reachable. We also pre-set the executable path so that
  // QtPass::init() → initExecutables() finds it even when only "gpg" (not
  // "gpg2") exists in PATH (e.g. Ubuntu CI where gpg2 is not a separate
  // binary). Without this, initExecutables() leaves the path empty,
  // configIsValid() returns false, and config() opens a blocking modal
  // dialog that times out the test after 300 s.
  m_gpgPath = Util::findBinaryInPath(QStringLiteral("gpg2"));
  if (m_gpgPath.isEmpty())
    m_gpgPath = Util::findBinaryInPath(QStringLiteral("gpg"));
  if (m_gpgPath.isEmpty())
    QSKIP("gpg not available — skipping MainWindow construction tests");
  QtPassSettings::setGpgExecutable(m_gpgPath);
}

void tst_mainwindow::init() {
  // Re-apply store settings in case a previous test modified them.
  QtPassSettings::setPassStore(m_storeDir.path());
  QtPassSettings::setUsePass(false);
  QtPassSettings::setShowProcessOutput(true);
  // Re-apply gpg path: initExecutables() inside the constructor overwrites
  // the setting to findBinaryInPath("gpg2"), which is empty on systems where
  // only "gpg" exists. Setting it here ensures configIsValid() sees a valid
  // executable and does not fall back to the blocking config() dialog.
  QtPassSettings::setGpgExecutable(m_gpgPath);
  m_window.reset(new MainWindow);
}

void tst_mainwindow::cleanup() { m_window.reset(); }

void tst_mainwindow::cleanupTestCase() {
  // Restore all saved settings so the user's live config is not left pointing
  // at our temp store (which will be deleted when m_storeDir goes out of
  // scope).
  QtPassSettings::setPassStore(m_savedPassStore);
  QtPassSettings::setUsePass(m_savedUsePass);
  QtPassSettings::setShowProcessOutput(m_savedShowProcessOutput);
  QtPassSettings::setGpgExecutable(m_savedGpgExecutable);
}

// ---------------------------------------------------------------------------

/**
 * @brief Constructor completes without crashing when a valid store exists.
 */
void tst_mainwindow::constructionDoesNotCrash() {
  // init() already constructed the window; reaching this line is the test.
  QVERIFY2(m_window != nullptr, "MainWindow must have been constructed");
}

/**
 * @brief getKeygenDialog() returns nullptr before any keygen is started.
 */
void tst_mainwindow::getKeygenDialogInitiallyNull() {
  QCOMPARE(m_window->getKeygenDialog(), nullptr);
}

/**
 * @brief cleanKeygenDialog() is a no-op (and harmless) when no dialog exists.
 */
void tst_mainwindow::cleanKeygenDialogWithNullIsHarmless() {
  QCOMPARE(m_window->getKeygenDialog(), nullptr);
  m_window->cleanKeygenDialog();
  QCOMPARE(m_window->getKeygenDialog(), nullptr);
}

/**
 * @brief setUiElementsEnabled(false) disables the tree view and search field.
 */
void tst_mainwindow::setUiElementsEnabledDisablesTreeView() {
  auto *treeView = m_window->findChild<QTreeView *>(QStringLiteral("treeView"));
  QVERIFY2(treeView != nullptr, "treeView widget must exist");

  m_window->setUiElementsEnabled(false);
  QVERIFY2(!treeView->isEnabled(), "treeView must be disabled");
}

/**
 * @brief setUiElementsEnabled(true) re-enables the tree view.
 */
void tst_mainwindow::setUiElementsEnabledEnablesTreeView() {
  auto *treeView = m_window->findChild<QTreeView *>(QStringLiteral("treeView"));
  QVERIFY2(treeView != nullptr, "treeView widget must exist");

  m_window->setUiElementsEnabled(false);
  m_window->setUiElementsEnabled(true);
  QVERIFY2(treeView->isEnabled(), "treeView must be re-enabled");
}

/**
 * @brief flashText() sets the text browser content.
 */
void tst_mainwindow::flashTextSetsContent() {
  auto *browser =
      m_window->findChild<QTextBrowser *>(QStringLiteral("textBrowser"));
  QVERIFY2(browser != nullptr, "textBrowser must exist");

  m_window->flashText(QStringLiteral("hello mainwindow test"), false);
  QVERIFY2(
      browser->toPlainText().contains(QStringLiteral("hello mainwindow test")),
      "textBrowser must contain the flashed text");
}

/**
 * @brief flashText() with isError=true does not crash.
 */
void tst_mainwindow::flashTextErrorDoesNotCrash() {
  auto *browser =
      m_window->findChild<QTextBrowser *>(QStringLiteral("textBrowser"));
  QVERIFY2(browser != nullptr, "textBrowser must exist");

  m_window->flashText(QStringLiteral("error text"), true);
  QVERIFY2(browser->toPlainText().contains(QStringLiteral("error text")),
           "textBrowser must contain the error text");
}

/**
 * @brief flashText() with isHtml=true renders HTML into the browser.
 */
void tst_mainwindow::flashTextHtmlRenderedInBrowser() {
  auto *browser =
      m_window->findChild<QTextBrowser *>(QStringLiteral("textBrowser"));
  QVERIFY2(browser != nullptr, "textBrowser must exist");

  m_window->flashText(QStringLiteral("<b>bold</b>"), false, true);
  // toHtml() includes the full document wrapper; the source fragment must
  // survive the round-trip and be rendered (not escaped).
  QVERIFY2(browser->toHtml().contains(QStringLiteral("<b>bold</b>")),
           "flashText with isHtml=true must render <b>bold</b> tags in "
           "browser->toHtml(), not escape them");
  QVERIFY2(!browser->toHtml().contains(QStringLiteral("&lt;b&gt;bold&lt;/b&gt;")),
           "flashText with isHtml=true must NOT produce escaped "
           "&lt;b&gt;bold&lt;/b&gt; in browser->toHtml()");
}

/**
 * @brief showStatusMessage() writes to the status bar.
 */
void tst_mainwindow::showStatusMessageAppearsInStatusBar() {
  m_window->showStatusMessage(QStringLiteral("status test"), 60000);
  QCOMPARE(m_window->statusBar()->currentMessage(),
           QStringLiteral("status test"));
}

/**
 * @brief deselect() does not crash when nothing is selected.
 */
void tst_mainwindow::deselectDoesNotCrash() {
  m_window->deselect();
  // No assertion needed — reaching this line means no crash / assert fired.
}

/**
 * @brief onProcessOutput() appends text to the process output panel when the
 *        panel is visible.
 */
void tst_mainwindow::onProcessOutputAppendsToPanel() {
  auto *outputEdit =
      m_window->findChild<QTextEdit *>(QStringLiteral("processOutputEdit"));
  QVERIFY2(outputEdit != nullptr, "processOutputEdit must exist");

  m_window->onProcessOutput(QStringLiteral("process output line"), false);
  QVERIFY2(
      outputEdit->toPlainText().contains(QStringLiteral("process output line")),
      "processOutputEdit must contain the appended line");
}

/**
 * @brief onProcessOutput() does nothing when the panel is hidden
 *        (isShowProcessOutput == false).
 */
void tst_mainwindow::onProcessOutputSkippedWhenPanelHidden() {
  QtPassSettings::setShowProcessOutput(false);

  auto *outputEdit =
      m_window->findChild<QTextEdit *>(QStringLiteral("processOutputEdit"));
  QVERIFY2(outputEdit != nullptr, "processOutputEdit must exist");

  const QString before = outputEdit->toPlainText();
  m_window->onProcessOutput(QStringLiteral("should not appear"), false);
  QCOMPARE(outputEdit->toPlainText(), before);
}

QTEST_MAIN(tst_mainwindow)
#include "tst_mainwindow.moc"
