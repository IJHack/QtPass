// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QSystemTrayIcon>
#include <QtTest>

#include "../../../src/configdialog.h"
#include "../../../src/passwordconfiguration.h"

/**
 * @class tst_configdialog
 * @brief Widget tests for ConfigDialog's `useX(bool)` setting-loaders.
 *
 * ConfigDialog is the main Settings dialog and is fed by QtPassSettings on
 * construction. Most of its public-facing behaviour lives in a family of
 * `useX(bool)` methods that read settings into checkbox state; those are
 * pure widget-state setters and testable in isolation by passing nullptr
 * as the parent MainWindow.
 *
 * Coverage avoided here (needs a real MainWindow / Pass singleton):
 * - genKey() — tunnels to mainWindow->generateKeyPair()
 * - on_pushButtonGenerateKey_clicked() — calls into KeygenDialog
 * - setProfiles() / profile-table flows — interact with QtPassSettings
 *   profile map
 * - Settings persistence (on_accepted) — already covered by the
 *   tst_util sshAuthSockOverrideStatus tests in #1469
 */
class tst_configdialog : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void constructionDoesNotCrash();
  void useSelectionTogglesCheckbox();
  void useAutoclearTogglesCheckbox();
  void useAutoclearPanelTogglesCheckbox();
  void useGitTogglesCheckbox();
  void useOtpTogglesCheckbox();
  void useGrepSearchTogglesCheckbox();
  void usePwgenTogglesCheckbox();
  void useTemplateTogglesCheckbox();
  void useTrayIconTogglesCheckbox();
  void useQrencodeTogglesCheckbox();
  void setPwgenPathSetsLineEdit();
  void setPwgenPathEmptyDisablesPwgenCheckbox();
  void setAndGetPasswordConfigurationRoundTrip();
};

/**
 * @brief Construct ConfigDialog with a nullptr MainWindow and return — the
 *        constructor doesn't dereference its parent.
 */
void tst_configdialog::constructionDoesNotCrash() {
  ConfigDialog dialog(nullptr);
  // Reaching this line means the constructor's setting-load + widget-
  // wiring path completed without a segfault.
  Q_UNUSED(dialog.isModal());
}

/**
 * @brief useSelection toggles the X11 primary-selection checkbox.
 */
void tst_configdialog::useSelectionTogglesCheckbox() {
  ConfigDialog dialog(nullptr);
  auto *cb = dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxSelection"));
  QVERIFY2(cb != nullptr, "checkBoxSelection widget must exist");

  dialog.useSelection(true);
  QVERIFY2(cb->isChecked(),
           "useSelection(true) should check checkBoxSelection");
  dialog.useSelection(false);
  QVERIFY2(!cb->isChecked(),
           "useSelection(false) should uncheck checkBoxSelection");
}

/**
 * @brief useAutoclear toggles the clipboard auto-clear checkbox.
 */
void tst_configdialog::useAutoclearTogglesCheckbox() {
  ConfigDialog dialog(nullptr);
  auto *cb = dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxAutoclear"));
  QVERIFY2(cb != nullptr, "checkBoxAutoclear widget must exist");

  dialog.useAutoclear(true);
  QVERIFY2(cb->isChecked(),
           "useAutoclear(true) should check checkBoxAutoclear");
  dialog.useAutoclear(false);
  QVERIFY2(!cb->isChecked(),
           "useAutoclear(false) should uncheck checkBoxAutoclear");
}

/**
 * @brief useAutoclearPanel toggles the panel auto-clear checkbox.
 */
void tst_configdialog::useAutoclearPanelTogglesCheckbox() {
  ConfigDialog dialog(nullptr);
  auto *cb =
      dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxAutoclearPanel"));
  QVERIFY2(cb != nullptr, "checkBoxAutoclearPanel widget must exist");

  dialog.useAutoclearPanel(true);
  QVERIFY2(cb->isChecked(),
           "useAutoclearPanel(true) should check checkBoxAutoclearPanel");
  dialog.useAutoclearPanel(false);
  QVERIFY2(!cb->isChecked(),
           "useAutoclearPanel(false) should uncheck checkBoxAutoclearPanel");
}

/**
 * @brief useGit toggles the "use git" checkbox.
 */
void tst_configdialog::useGitTogglesCheckbox() {
  ConfigDialog dialog(nullptr);
  auto *cb = dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxUseGit"));
  QVERIFY2(cb != nullptr, "checkBoxUseGit widget must exist");

  dialog.useGit(true);
  QVERIFY2(cb->isChecked(), "useGit(true) should check checkBoxUseGit");
  dialog.useGit(false);
  QVERIFY2(!cb->isChecked(), "useGit(false) should uncheck checkBoxUseGit");
}

/**
 * @brief useOtp toggles the "use OTP extension" checkbox.
 */
void tst_configdialog::useOtpTogglesCheckbox() {
  ConfigDialog dialog(nullptr);
  auto *cb = dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxUseOtp"));
  QVERIFY2(cb != nullptr, "checkBoxUseOtp widget must exist");

  dialog.useOtp(true);
  QVERIFY2(cb->isChecked(), "useOtp(true) should check checkBoxUseOtp");
  dialog.useOtp(false);
  QVERIFY2(!cb->isChecked(), "useOtp(false) should uncheck checkBoxUseOtp");
}

/**
 * @brief useGrepSearch toggles the "content search" checkbox.
 */
void tst_configdialog::useGrepSearchTogglesCheckbox() {
  ConfigDialog dialog(nullptr);
  auto *cb =
      dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxUseGrepSearch"));
  QVERIFY2(cb != nullptr, "checkBoxUseGrepSearch widget must exist");

  dialog.useGrepSearch(true);
  QVERIFY2(cb->isChecked(),
           "useGrepSearch(true) should check checkBoxUseGrepSearch");
  dialog.useGrepSearch(false);
  QVERIFY2(!cb->isChecked(),
           "useGrepSearch(false) should uncheck checkBoxUseGrepSearch");
}

/**
 * @brief usePwgen toggles the pwgen-generator checkbox — but only if a
 *        pwgen path is configured. With an empty pwgenPath, usePwgen(true)
 *        is intentionally clamped to false (you can't use a tool that
 *        isn't there).
 */
void tst_configdialog::usePwgenTogglesCheckbox() {
  ConfigDialog dialog(nullptr);
  auto *cb = dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxUsePwgen"));
  auto *pwgenPath = dialog.findChild<QLineEdit *>(QStringLiteral("pwgenPath"));
  QVERIFY2(cb != nullptr, "checkBoxUsePwgen widget must exist");
  QVERIFY2(pwgenPath != nullptr, "pwgenPath widget must exist");

  // First verify the empty-path branch: even usePwgen(true) leaves the
  // checkbox unchecked when pwgenPath is empty.
  pwgenPath->setText(QString());
  dialog.usePwgen(true);
  QVERIFY2(!cb->isChecked(),
           "usePwgen(true) with empty pwgenPath must stay unchecked");

  // Now with a non-empty path the value flows through.
  pwgenPath->setText(QStringLiteral("/usr/bin/pwgen"));
  dialog.usePwgen(true);
  QVERIFY2(cb->isChecked(),
           "usePwgen(true) with a configured pwgenPath should check "
           "checkBoxUsePwgen");
  dialog.usePwgen(false);
  QVERIFY2(!cb->isChecked(), "usePwgen(false) should uncheck checkBoxUsePwgen");
}

/**
 * @brief useTemplate toggles the password-template checkbox.
 */
void tst_configdialog::useTemplateTogglesCheckbox() {
  ConfigDialog dialog(nullptr);
  auto *cb =
      dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxUseTemplate"));
  QVERIFY2(cb != nullptr, "checkBoxUseTemplate widget must exist");

  dialog.useTemplate(true);
  QVERIFY2(cb->isChecked(),
           "useTemplate(true) should check checkBoxUseTemplate");
  dialog.useTemplate(false);
  QVERIFY2(!cb->isChecked(),
           "useTemplate(false) should uncheck checkBoxUseTemplate");
}

void tst_configdialog::useTrayIconTogglesCheckbox() {
  if (!QSystemTrayIcon::isSystemTrayAvailable())
    QSKIP("system tray not available in this environment");
  ConfigDialog dialog(nullptr);
  auto *cb =
      dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxUseTrayIcon"));
  QVERIFY2(cb != nullptr, "checkBoxUseTrayIcon widget must exist");
  dialog.useTrayIcon(true);
  QVERIFY2(cb->isChecked(),
           "useTrayIcon(true) should check checkBoxUseTrayIcon");
  dialog.useTrayIcon(false);
  QVERIFY2(!cb->isChecked(),
           "useTrayIcon(false) should uncheck checkBoxUseTrayIcon");
}

void tst_configdialog::useQrencodeTogglesCheckbox() {
  ConfigDialog dialog(nullptr);
  auto *cb =
      dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxUseQrencode"));
  QVERIFY2(cb != nullptr, "checkBoxUseQrencode widget must exist");
  dialog.useQrencode(true);
  QVERIFY2(cb->isChecked(),
           "useQrencode(true) should check checkBoxUseQrencode");
  dialog.useQrencode(false);
  QVERIFY2(!cb->isChecked(),
           "useQrencode(false) should uncheck checkBoxUseQrencode");
}

void tst_configdialog::setPwgenPathSetsLineEdit() {
  ConfigDialog dialog(nullptr);
  auto *pathEdit = dialog.findChild<QLineEdit *>(QStringLiteral("pwgenPath"));
  QVERIFY2(pathEdit != nullptr, "pwgenPath widget must exist");
  dialog.setPwgenPath(QStringLiteral("/usr/bin/pwgen"));
  QCOMPARE(pathEdit->text(), QStringLiteral("/usr/bin/pwgen"));
}

void tst_configdialog::setPwgenPathEmptyDisablesPwgenCheckbox() {
  ConfigDialog dialog(nullptr);
  auto *cb = dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxUsePwgen"));
  QVERIFY2(cb != nullptr, "checkBoxUsePwgen widget must exist");
  dialog.setPwgenPath(QString());
  QVERIFY2(!cb->isChecked(), "setPwgenPath('') must uncheck checkBoxUsePwgen");
  QVERIFY2(!cb->isEnabled(), "setPwgenPath('') must disable checkBoxUsePwgen");
}

void tst_configdialog::setAndGetPasswordConfigurationRoundTrip() {
  ConfigDialog dialog(nullptr);
  PasswordConfiguration cfg;
  cfg.length = 32;
  cfg.selected = PasswordConfiguration::ALPHANUMERIC;
  dialog.setPasswordConfiguration(cfg);
  PasswordConfiguration result = dialog.getPasswordConfiguration();
  QCOMPARE(result.length, 32);
  QCOMPARE(static_cast<int>(result.selected),
           static_cast<int>(PasswordConfiguration::ALPHANUMERIC));
}

QTEST_MAIN(tst_configdialog)
#include "tst_configdialog.moc"
