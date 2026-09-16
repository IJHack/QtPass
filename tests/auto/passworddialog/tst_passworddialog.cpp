// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @brief Tests for PasswordDialog's handling of the asynchronous Show().
 *
 * For an existing entry, Show() is asynchronous: the decrypted content only
 * lands after the event loop runs again, so right after construction the
 * fields are empty. These tests pin the behaviour that fixes that window:
 * the editor and Ok button stay locked until setPass() runs, and a decrypt
 * error keeps the dialog open with the reason shown instead of closing it.
 */

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QToolButton>
#include <QtTest>

#include "../../../src/pass.h"
#include "../../../src/passworddialog.h"
#include "../../../src/qtpasssettings.h"
#include "../testsettings.h"

namespace {

/**
 * @brief Minimal Pass stub with no-op operations.
 *
 * Show() never touches the executor, so finishedShow/processErrorExit are
 * only emitted when the test calls deliverShow()/deliverError() – making the
 * asynchronous decrypt deterministic to test.
 */
class FakePass : public Pass {
public:
  FakePass() { init(QtPassSettings::load()); }

  void GitInit() override {}
  void GitPull() override {}
  void GitPull_b() override {}
  void GitPush() override {}
  void Show(QString) override {}
  void Insert(QString, QString, bool) override {}
  void Remove(QString, bool) override {}
  void Move(const QString, const QString, const bool) override {}
  void Copy(const QString, const QString, const bool) override {}
  void Init(QString, const QList<UserInfo> &) override {}
  void Grep(QString, bool) override {}

  void deliverShow(const QString &out) { emit finishedShow(out); }
  void deliverError(int exitCode, const QString &err) {
    emit processErrorExit(exitCode, err);
  }
};

auto okButton(QDialog &d) -> QPushButton * {
  auto *box = d.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
  return box == nullptr ? nullptr : box->button(QDialogButtonBox::Ok);
}

} // namespace

class tst_passworddialog : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase() { isolateTestSettings(); }

  void existingEntryLocksEditorUntilContentLoads();
  void contentLoadUnlocksEditorAndOk();
  void decryptErrorKeepsDialogOpenWithReason();
  void lateErrorAfterContentLoadIsIgnored();
  void newEntryStartsEditable();
  void showCheckBoxTogglesPasswordEcho();
  void templateRowHiddenWithoutTemplates();
  void templateBoxListsAndAppliesTemplates();
  void ctrlTCyclesTemplatesAndUpdatesBox();
};

void tst_passworddialog::existingEntryLocksEditorUntilContentLoads() {
  FakePass pass;
  const AppSettings s = QtPassSettings::load();
  PasswordDialog d(&pass, s, QStringLiteral("entry.gpg"), false);

  QVERIFY2(okButton(d) != nullptr, "the dialog must have an Ok button");
  QVERIFY2(!okButton(d)->isEnabled(),
           "Ok must be disabled until the decrypted content has loaded");
  auto *pw = d.findChild<QLineEdit *>(QStringLiteral("lineEditPassword"));
  QVERIFY2(pw != nullptr, "the dialog must have the password line edit");
  QVERIFY2(!pw->isEnabled(),
           "the editor must be locked until the content has loaded");
  auto *generate =
      d.findChild<QToolButton *>(QStringLiteral("createPasswordButton"));
  QVERIFY2(generate != nullptr, "the dialog must have the generate button");
  QVERIFY2(!generate->isEnabled(),
           "the generate button must be locked while loading");
  auto *status = d.findChild<QLabel *>(QStringLiteral("statusLabel"));
  QVERIFY2(status != nullptr, "the dialog must have a status label");
  QVERIFY2(!status->text().isEmpty(),
           "the status label must indicate the entry is being decrypted");
}

void tst_passworddialog::contentLoadUnlocksEditorAndOk() {
  FakePass pass;
  const AppSettings s = QtPassSettings::load();
  PasswordDialog d(&pass, s, QStringLiteral("entry.gpg"), false);
  QVERIFY2(!okButton(d)->isEnabled(), "Ok must start disabled");

  pass.deliverShow(QStringLiteral("secret\nurl: example.com\n"));

  QVERIFY2(okButton(d)->isEnabled(),
           "Ok must be re-enabled once the content has loaded");
  auto *pw = d.findChild<QLineEdit *>(QStringLiteral("lineEditPassword"));
  QVERIFY2(pw != nullptr, "the dialog must have the password line edit");
  QVERIFY2(pw->isEnabled(), "the editor must be unlocked after the load");
  QCOMPARE(pw->text(), QStringLiteral("secret"));
  auto *generate =
      d.findChild<QToolButton *>(QStringLiteral("createPasswordButton"));
  QVERIFY2(generate->isEnabled(),
           "the generate button must be re-enabled after the load");
  auto *status = d.findChild<QLabel *>(QStringLiteral("statusLabel"));
  QVERIFY2(status->text().isEmpty(),
           "the status label must be cleared once the content has loaded");
}

void tst_passworddialog::decryptErrorKeepsDialogOpenWithReason() {
  FakePass pass;
  const AppSettings s = QtPassSettings::load();
  PasswordDialog d(&pass, s, QStringLiteral("entry.gpg"), false);
  d.show();
  QVERIFY2(d.isVisible(), "the dialog must be shown before the error");

  QSignalSpy rejectedSpy(&d, &QDialog::rejected);
  pass.deliverError(1, QStringLiteral("gpg: decryption failed"));

  QVERIFY2(d.isVisible(),
           "a decrypt error must keep the dialog open, not close it");
  QVERIFY2(rejectedSpy.isEmpty(), "a decrypt error must not reject the dialog");
  QVERIFY2(!okButton(d)->isEnabled(),
           "Ok must stay disabled after a decrypt error");
  auto *status = d.findChild<QLabel *>(QStringLiteral("statusLabel"));
  QVERIFY2(status != nullptr, "the dialog must have a status label");
  QCOMPARE(status->text(), QStringLiteral("gpg: decryption failed"));
}

void tst_passworddialog::lateErrorAfterContentLoadIsIgnored() {
  FakePass pass;
  const AppSettings s = QtPassSettings::load();
  PasswordDialog d(&pass, s, QStringLiteral("entry.gpg"), false);
  pass.deliverShow(QStringLiteral("secret\n"));

  pass.deliverError(1, QStringLiteral("unrelated late error"));

  auto *status = d.findChild<QLabel *>(QStringLiteral("statusLabel"));
  QVERIFY2(status->text().isEmpty(),
           "an error after the content loaded belongs to another operation "
           "and must not touch the dialog");
}

void tst_passworddialog::newEntryStartsEditable() {
  FakePass pass;
  const AppSettings s = QtPassSettings::load();
  PasswordDialog d(&pass, s, QStringLiteral("newentry.gpg"), true);

  QVERIFY2(okButton(d)->isEnabled(),
           "a new entry has nothing to fetch, so Ok must be usable");
  auto *pw = d.findChild<QLineEdit *>(QStringLiteral("lineEditPassword"));
  QVERIFY2(pw->isEnabled(), "a new entry must be editable immediately");
}

/**
 * @brief The "Show password" box is wired explicitly to QCheckBox::toggled
 *        (not the deprecated stateChanged auto-slot); toggling it must switch
 *        the password field between masked and clear text.
 */
void tst_passworddialog::showCheckBoxTogglesPasswordEcho() {
  FakePass pass;
  const AppSettings s = QtPassSettings::load();
  PasswordDialog d(&pass, s, QStringLiteral("newentry.gpg"), true);

  auto *show = d.findChild<QCheckBox *>(QStringLiteral("checkBoxShow"));
  auto *pw = d.findChild<QLineEdit *>(QStringLiteral("lineEditPassword"));
  QVERIFY2(show != nullptr, "checkBoxShow widget must exist");
  QVERIFY2(pw != nullptr, "lineEditPassword widget must exist");

  QVERIFY2(!show->isChecked(), "Show password should start unchecked");
  QCOMPARE(pw->echoMode(), QLineEdit::Password);

  show->setChecked(true);
  QCOMPARE(pw->echoMode(), QLineEdit::Normal);

  show->setChecked(false);
  QCOMPARE(pw->echoMode(), QLineEdit::Password);
}

namespace {
QHash<QString, QStringList> twoTemplates() {
  return {{QStringLiteral("login"),
           {QStringLiteral("username"), QStringLiteral("url")}},
          {QStringLiteral("wifi"), {QStringLiteral("ssid")}}};
}
} // namespace

/**
 * @brief Stores without a .templates file must not grow an empty combo box.
 */
void tst_passworddialog::templateRowHiddenWithoutTemplates() {
  FakePass pass;
  PasswordDialog d(&pass, QtPassSettings::load(), QStringLiteral("new.gpg"),
                   true);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  auto *box = d.findChild<QComboBox *>(QStringLiteral("templateBox"));
  auto *label = d.findChild<QLabel *>(QStringLiteral("label_template"));
  QVERIFY(box != nullptr && label != nullptr);
  QVERIFY2(!box->isVisible() && !label->isVisible(),
           "the template row must stay hidden without templates");
}

/**
 * @brief The active template used to be invisible: the box now names it, and
 *        picking another one rebuilds the field rows.
 */
void tst_passworddialog::templateBoxListsAndAppliesTemplates() {
  FakePass pass;
  PasswordDialog d(&pass, QtPassSettings::load(), QStringLiteral("new.gpg"),
                   true);
  d.setAvailableTemplates(twoTemplates(), QStringLiteral("wifi"));
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));

  auto *box = d.findChild<QComboBox *>(QStringLiteral("templateBox"));
  QVERIFY(box != nullptr);
  QVERIFY2(box->isVisible(), "the template row must show once templates exist");
  QCOMPARE(box->count(), 2);
  QCOMPARE(box->currentText(), QStringLiteral("wifi"));
  QVERIFY2(d.findChild<QLineEdit *>(QStringLiteral("ssid")) != nullptr,
           "the default template's field must be present");
  QVERIFY(d.findChild<QLineEdit *>(QStringLiteral("username")) == nullptr);

  box->setCurrentText(QStringLiteral("login"));
  QVERIFY2(d.findChild<QLineEdit *>(QStringLiteral("username")) != nullptr,
           "choosing a template in the box must apply it");
  QVERIFY(d.findChild<QLineEdit *>(QStringLiteral("ssid")) == nullptr);
}

/**
 * @brief Ctrl+T is owned by the dialog now and keeps the box in sync.
 */
void tst_passworddialog::ctrlTCyclesTemplatesAndUpdatesBox() {
  FakePass pass;
  PasswordDialog d(&pass, QtPassSettings::load(), QStringLiteral("new.gpg"),
                   true);
  d.setAvailableTemplates(twoTemplates(), QStringLiteral("login"));
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  d.activateWindow();

  auto *box = d.findChild<QComboBox *>(QStringLiteral("templateBox"));
  QVERIFY(box != nullptr);
  QCOMPARE(box->currentText(), QStringLiteral("login"));

  QTest::keyClick(&d, Qt::Key_T, Qt::ControlModifier);
  QTRY_COMPARE(box->currentText(), QStringLiteral("wifi"));
  QVERIFY(d.findChild<QLineEdit *>(QStringLiteral("ssid")) != nullptr);

  QTest::keyClick(&d, Qt::Key_T, Qt::ControlModifier);
  QTRY_COMPARE(box->currentText(), QStringLiteral("login"));
}

QTEST_MAIN(tst_passworddialog)
#include "tst_passworddialog.moc"
