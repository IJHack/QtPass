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
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTimer>
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
  void Insert(QString file, QString, bool) override {
    inserted = file;
    // The backends refuse before starting anything through critical(),
    // and report a started insert asynchronously.
    if (!refuseWith.isEmpty()) {
      emit critical(QStringLiteral("Can not edit"), refuseWith);
      return;
    }
    if (holdResult) {
      return;
    }
    if (!failWith.isEmpty()) {
      QTimer::singleShot(0, this, [this, why = failWith] {
        emit insertFailed(why, failWritten);
      });
      return;
    }
    emit finishedInsert(QString(), QString());
  }
  QString inserted;
  /// Refuse the next Insert() synchronously with this reason.
  QString refuseWith;
  /// Fail the next Insert() asynchronously with this reason.
  QString failWith;
  /// Whether that failure says the entry was written (a git step failed).
  bool failWritten = false;
  /// Report nothing yet, as while gpg is still running.
  bool holdResult = false;
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
  void failedInsertKeepsWhatWasTyped();
  void refusedInsertKeepsWhatWasTyped();
  void savingCancelAsksBeforeClosing();
  void savingIgnoresATemplateSwitch();
  void newEntryWrittenBeforeAFailedGitStepCloses();
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

/**
 * @brief #1944: when encrypting fails (a missing recipient key, say) the
 *        dialog stays open with what was typed and says why; OK then tries
 *        again. It used to close before gpg ran, and the entry was lost.
 */
void tst_passworddialog::failedInsertKeepsWhatWasTyped() {
  FakePass pass;
  pass.failWith = QStringLiteral("gpg: alice@example.org: No public key");
  PasswordDialog d(&pass, QtPassSettings::load(), QStringLiteral("mail"), true);
  auto *password = d.findChild<QLineEdit *>(QStringLiteral("lineEditPassword"));
  auto *status = d.findChild<QLabel *>(QStringLiteral("statusLabel"));
  QVERIFY(password != nullptr && status != nullptr);
  password->setText(QStringLiteral("s3cret"));
  QSignalSpy accepted(&d, &QDialog::accepted);

  okButton(d)->click();
  QTRY_VERIFY2(status->text().contains(QStringLiteral("No public key")),
               qPrintable("the reason must be shown: " + status->text()));
  QVERIFY2(accepted.isEmpty(), "a failed save must not close the dialog");
  QCOMPARE(password->text(), QStringLiteral("s3cret"));
  QVERIFY(password->isEnabled() && okButton(d)->isEnabled());

  pass.failWith.clear();
  okButton(d)->click();
  QCOMPARE(accepted.size(), 1);
  QCOMPARE(pass.inserted, QStringLiteral("mail"));
}

/**
 * @brief A refusal before anything starts (an unusable .gpg-id) keeps the
 *        dialog open the same way.
 */
void tst_passworddialog::refusedInsertKeepsWhatWasTyped() {
  FakePass pass;
  pass.refuseWith = QStringLiteral("Could not read encryption key to use");
  PasswordDialog d(&pass, QtPassSettings::load(), QStringLiteral("mail"), true);
  auto *password = d.findChild<QLineEdit *>(QStringLiteral("lineEditPassword"));
  password->setText(QStringLiteral("s3cret"));
  QSignalSpy accepted(&d, &QDialog::accepted);

  okButton(d)->click();
  QVERIFY2(accepted.isEmpty(), "a refused save must not close the dialog");
  QVERIFY(d.findChild<QLabel *>(QStringLiteral("statusLabel"))
              ->text()
              .contains(QStringLiteral("encryption key")));
  QCOMPARE(password->text(), QStringLiteral("s3cret"));
  QVERIFY(okButton(d)->isEnabled());
}

namespace {
/// Answer the next modal question box with @p button once it is up.
void answerNextQuestion(QMessageBox::StandardButton button, int tries = 50) {
  QTimer::singleShot(10, [button, tries] {
    auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
    if (box == nullptr) {
      if (tries > 0) {
        answerNextQuestion(button, tries - 1);
      }
      return;
    }
    box->button(button)->click();
  });
}
} // namespace

/**
 * @brief A save that never reports (an unanswered PIN prompt, a queue held
 *        up by a hanging pull) must not trap the user: Cancel stays on while
 *        saving, and Cancel, Esc or closing ask first. No keeps waiting; Yes
 *        closes, and a result arriving later no longer touches the dialog.
 */
void tst_passworddialog::savingCancelAsksBeforeClosing() {
  FakePass pass;
  pass.holdResult = true;
  PasswordDialog d(&pass, QtPassSettings::load(), QStringLiteral("mail"), true);
  d.findChild<QLineEdit *>(QStringLiteral("lineEditPassword"))
      ->setText(QStringLiteral("s3cret"));
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  QSignalSpy rejected(&d, &QDialog::rejected);
  QSignalSpy accepted(&d, &QDialog::accepted);
  auto *box = d.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));

  okButton(d)->click();
  QVERIFY2(box->button(QDialogButtonBox::Cancel)->isEnabled(),
           "Cancel is the way out of a save that never reports");
  answerNextQuestion(QMessageBox::No);
  d.reject();
  QVERIFY2(rejected.isEmpty() && d.isVisible(), "No keeps waiting");
  answerNextQuestion(QMessageBox::No);
  QTest::keyClick(&d, Qt::Key_Escape);
  QVERIFY2(rejected.isEmpty() && d.isVisible(), "Esc asks the same");

  answerNextQuestion(QMessageBox::Yes);
  box->button(QDialogButtonBox::Cancel)->click();
  QCOMPARE(rejected.size(), 1);
  emit pass.finishedInsert(QString(), QString());
  emit pass.insertFailed(QStringLiteral("late"), false);
  QVERIFY2(accepted.isEmpty(), "a late result does not reopen or accept");
}

/**
 * @brief Ctrl+T while an insert is pending would rebuild the field rows
 *        under it; the switch is ignored until the result.
 */
void tst_passworddialog::savingIgnoresATemplateSwitch() {
  FakePass pass;
  pass.holdResult = true;
  PasswordDialog d(&pass, QtPassSettings::load(), QStringLiteral("mail"), true);
  QHash<QString, QStringList> templates;
  templates.insert(QStringLiteral("login"), {QStringLiteral("login")});
  templates.insert(QStringLiteral("card"), {QStringLiteral("number")});
  d.setAvailableTemplates(templates, QStringLiteral("login"));
  auto *login = d.findChild<QLineEdit *>(QStringLiteral("login"));
  QVERIFY(login != nullptr);
  login->setText(QStringLiteral("bob"));

  okButton(d)->click();
  d.cycleTemplate();
  QVERIFY2(d.findChild<QLineEdit *>(QStringLiteral("login")) != nullptr,
           "the typed field must survive a template switch while saving");
  emit pass.insertFailed(QStringLiteral("gpg: no public key"), false);
  QCOMPARE(d.findChild<QLineEdit *>(QStringLiteral("login"))->text(),
           QStringLiteral("bob"));
}

/**
 * @brief When the entry was written and only a later step failed (git add
 *        or commit), the dialog closes: the entry is saved, and the main
 *        window reports the git error.
 */
void tst_passworddialog::newEntryWrittenBeforeAFailedGitStepCloses() {
  FakePass pass;
  pass.failWith = QStringLiteral("fatal: unable to write new index file");
  pass.failWritten = true;
  PasswordDialog d(&pass, QtPassSettings::load(), QStringLiteral("mail"), true);
  QSignalSpy accepted(&d, &QDialog::accepted);

  okButton(d)->click();
  QTRY_COMPARE(accepted.size(), 1);
}

QTEST_MAIN(tst_passworddialog)
#include "tst_passworddialog.moc"
