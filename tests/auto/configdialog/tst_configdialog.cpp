// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QSystemTrayIcon>
#include <QTableWidget>
#include <QtTest>

#include "../../../src/configdialog.h"
#include "../../../src/passwordconfiguration.h"
#include "../../../src/qtpasssettings.h"

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
  void customCharsetRoundTrip();
  void customCharsetPreservedWhenBuiltinSelected();
  void addProfileSelectsNewRowAfterSort();
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
 * @brief useOtp toggles the OTP support checkbox.
 */
void tst_configdialog::useOtpTogglesCheckbox() {
  ConfigDialog dialog(nullptr);
  auto *cb = dialog.findChild<QCheckBox *>(QStringLiteral("checkBoxUseOtp"));
  QVERIFY2(cb != nullptr, "checkBoxUseOtp widget must exist");
  // OTP is generated in-process, so the checkbox is no longer gated on
  // probing for the pass-otp extension, nor hidden on Windows.
  QVERIFY2(cb->isEnabled(),
           "checkBoxUseOtp must not be disabled by an availability probe");

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

/**
 * @brief A CUSTOM charset survives the set/get round-trip.
 *
 * getPasswordConfiguration() only reads the character line edit while CUSTOM is
 * selected; this guards that path so a user-defined charset is not lost or
 * replaced by a builtin string on save.
 */
void tst_configdialog::customCharsetRoundTrip() {
  ConfigDialog dialog(nullptr);
  PasswordConfiguration cfg;
  cfg.selected = PasswordConfiguration::CUSTOM;
  cfg.Characters[PasswordConfiguration::CUSTOM] = QStringLiteral("abc123!@#");
  dialog.setPasswordConfiguration(cfg);
  PasswordConfiguration result = dialog.getPasswordConfiguration();
  QCOMPARE(static_cast<int>(result.selected),
           static_cast<int>(PasswordConfiguration::CUSTOM));
  QCOMPARE(result.Characters[PasswordConfiguration::CUSTOM],
           QStringLiteral("abc123!@#"));
}

/**
 * @brief The custom charset survives while a builtin set is selected.
 *
 * This is the branch the fix is actually about: with a builtin (non-CUSTOM)
 * selection the line edit shows the builtin's characters, so
 * getPasswordConfiguration() must fall back to the retained custom charset
 * rather than reading the line edit and clobbering it with a builtin string.
 */
void tst_configdialog::customCharsetPreservedWhenBuiltinSelected() {
  ConfigDialog dialog(nullptr);
  PasswordConfiguration cfg;
  cfg.selected = PasswordConfiguration::CUSTOM;
  cfg.Characters[PasswordConfiguration::CUSTOM] = QStringLiteral("abc123!@#");
  dialog.setPasswordConfiguration(cfg);

  // Switch to a builtin selection while keeping the same custom charset.
  PasswordConfiguration builtin = dialog.getPasswordConfiguration();
  builtin.selected = PasswordConfiguration::ALPHANUMERIC;
  dialog.setPasswordConfiguration(builtin);

  PasswordConfiguration result = dialog.getPasswordConfiguration();
  QCOMPARE(static_cast<int>(result.selected),
           static_cast<int>(PasswordConfiguration::ALPHANUMERIC));
  QCOMPARE(result.Characters[PasswordConfiguration::CUSTOM],
           QStringLiteral("abc123!@#"));
}

/**
 * @brief Adding a profile after the user sorted by name selects the new row.
 *
 * Regression for on_addButton_clicked(): after the user sorts the table by the
 * name column, re-enabling sorting moves the freshly inserted row, so reading
 * item(n, 0) by the stale insertion index returned an existing profile — the
 * edit and selection then landed on (and would rename) that profile instead of
 * the new one. The new row must be located by item pointer.
 */
void tst_configdialog::addProfileSelectsNewRowAfterSort() {
  // Restore the real profile state even if an assertion fails early (QVERIFY/
  // QCOMPARE return immediately), so this test never leaks its synthetic
  // profiles into later tests in the run.
  struct ProfileRestorer {
    QHash<QString, QHash<QString, QString>> saved;
    ~ProfileRestorer() { QtPassSettings::setProfiles(saved); }
  } restorer{QtPassSettings::getProfiles()};

  QHash<QString, QHash<QString, QString>> profiles;
  for (const QString &name :
       {QStringLiteral("alpha"), QStringLiteral("beta"),
        QStringLiteral("gamma"), QStringLiteral("delta")}) {
    QHash<QString, QString> profile;
    profile.insert("path", "/store/" + name);
    profiles.insert(name, profile);
  }
  QtPassSettings::setProfiles(profiles);

  {
    ConfigDialog dialog(nullptr);
    auto *table =
        dialog.findChild<QTableWidget *>(QStringLiteral("profileTable"));
    QVERIFY2(table != nullptr, "profileTable widget must exist");

    // Mimic the user sorting by the name column; this makes a later insert
    // re-sort and move the new row away from its insertion index.
    table->sortItems(0, Qt::DescendingOrder);

    QVERIFY(QMetaObject::invokeMethod(&dialog, "on_addButton_clicked"));

    const QList<QTableWidgetItem *> selected = table->selectedItems();
    QVERIFY(!selected.isEmpty());
    QTableWidgetItem *nameItem = table->item(selected.first()->row(), 0);
    QVERIFY(nameItem != nullptr);
    QCOMPARE(nameItem->text(), QStringLiteral("New Profile"));
  }
}

QTEST_MAIN(tst_configdialog)
#include "tst_configdialog.moc"
