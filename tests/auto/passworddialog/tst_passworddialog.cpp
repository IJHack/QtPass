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
  void OtpGenerate(QString) override {}
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

QTEST_MAIN(tst_passworddialog)
#include "tst_passworddialog.moc"
