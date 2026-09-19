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

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>

#include "../../../src/fieldlabel.h"
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
  void Show(QString file) override { shown = file; }
  void Insert(QString file, QString, bool overwrite) override {
    inserted = file;
    insertedOverwrite = overwrite;
  }
  QString inserted;
  bool insertedOverwrite = false;
  void Remove(QString, bool) override {}
  void Move(const QString, const QString, const bool) override {}
  void Copy(const QString, const QString, const bool) override {}
  void Init(QString, const QList<UserInfo> &) override {}
  void Grep(QString, bool) override {}

  /// Answer the last Show() with @p out, or another entry's decrypt when
  /// @p file is given.
  void deliverShow(const QString &out, const QString &file = QString()) {
    emit finishedShow(out, file.isEmpty() ? shown : file);
  }
  QString shown;
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
  void anotherEntrysDecryptIsIgnored();
  void decryptErrorKeepsDialogOpenWithReason();
  void lateErrorAfterContentLoadIsIgnored();
  void newEntryStartsEditable();
  void nameRowOnlyForANewEntryWithALocation();
  void newEntryNameIsValidatedAsTyped();
  void acceptResolvesTheNameAndCreatesItsFolder();
  void showCheckBoxTogglesPasswordEcho();
  void templateRowHiddenWithoutTemplates();
  void templateBoxListsAndAppliesTemplates();
  void ctrlTCyclesTemplatesAndUpdatesBox();
  void fieldLabelRenamesTheField();
  void fieldLabelRefusesADuplicateName();
  void fieldLabelEscapeCancelsAndRemoveDropsTheRow();
  void templateFieldsKeepPlainLabels();
};

namespace {
/**
 * @brief Settings with an empty template and "Template all fields" on, so
 *        every `key: value` line of the entry becomes a form row (all-fields
 *        only applies while templating is on, see #1766).
 */
auto allFieldsSettings() -> AppSettings {
  AppSettings s = QtPassSettings::load();
  s.useTemplate = true;
  s.passTemplate.clear();
  s.templateAllFields = true;
  return s;
}

auto fieldLabel(PasswordDialog &d, const QString &name) -> FieldLabel * {
  const auto labels = d.findChildren<FieldLabel *>();
  for (FieldLabel *label : labels) {
    if (label->text() == name) {
      return label;
    }
  }
  return nullptr;
}
} // namespace

/**
 * @brief #132: double-click on a field name (or Rename in its context menu)
 *        edits it in place; Enter renames the field and the entry is written
 *        back under the new key.
 */
void tst_passworddialog::fieldLabelRenamesTheField() {
  FakePass pass;
  PasswordDialog d(&pass, allFieldsSettings(), QStringLiteral("entry.gpg"),
                   false);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  pass.deliverShow(QStringLiteral("secret\nlogin: bob\nurl: example.com\n"));

  FieldLabel *label = fieldLabel(d, QStringLiteral("login"));
  QVERIFY2(label != nullptr, "an all-fields row must get a FieldLabel");
  QVERIFY2(!label->toolTip().isEmpty(), "the label must say it can be renamed");
  label->startEdit();
  auto *editor = d.findChild<QLineEdit *>(QStringLiteral("fieldNameEditor"));
  QVERIFY2(editor != nullptr, "startEdit() must open an editor");
  QCOMPARE(editor->text(), QStringLiteral("login"));
  editor->setText(QStringLiteral(" user:name "));
  QTest::keyClick(editor, Qt::Key_Return);

  QCOMPARE(label->text(), QStringLiteral("username"));
  const QString written = d.getPassword();
  QVERIFY2(
      written.contains(QStringLiteral("username: bob\n")),
      qPrintable("the entry must be written under the new key:\n" + written));
  QVERIFY2(!written.contains(QStringLiteral("login:")),
           "the old key must be gone");
  QVERIFY2(written.contains(QStringLiteral("url: example.com\n")),
           "other fields must be untouched");
}

/**
 * @brief Two fields cannot share a key: the rename is refused, the status
 *        label says why, and the field keeps its name.
 */
void tst_passworddialog::fieldLabelRefusesADuplicateName() {
  FakePass pass;
  PasswordDialog d(&pass, allFieldsSettings(), QStringLiteral("entry.gpg"),
                   false);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  pass.deliverShow(QStringLiteral("secret\nlogin: bob\nurl: example.com\n"));

  FieldLabel *label = fieldLabel(d, QStringLiteral("login"));
  QVERIFY(label != nullptr);
  label->startEdit();
  auto *editor = d.findChild<QLineEdit *>(QStringLiteral("fieldNameEditor"));
  QVERIFY(editor != nullptr);
  editor->setText(QStringLiteral("url"));
  QTest::keyClick(editor, Qt::Key_Return);

  QCOMPARE(label->text(), QStringLiteral("login"));
  auto *status = d.findChild<QLabel *>(QStringLiteral("statusLabel"));
  QVERIFY(status != nullptr);
  QVERIFY2(
      status->text().contains(QStringLiteral("url")),
      qPrintable("the status label must name the clash: " + status->text()));
  QVERIFY(d.getPassword().contains(QStringLiteral("login: bob\n")));
}

/**
 * @brief Escape leaves the name alone; "Remove field" drops the row and the
 *        line is no longer written back.
 */
void tst_passworddialog::fieldLabelEscapeCancelsAndRemoveDropsTheRow() {
  FakePass pass;
  PasswordDialog d(&pass, allFieldsSettings(), QStringLiteral("entry.gpg"),
                   false);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  pass.deliverShow(QStringLiteral("secret\nlogin: bob\nurl: example.com\n"));

  FieldLabel *label = fieldLabel(d, QStringLiteral("login"));
  QVERIFY(label != nullptr);
  label->startEdit();
  auto *editor = d.findChild<QLineEdit *>(QStringLiteral("fieldNameEditor"));
  QVERIFY(editor != nullptr);
  editor->setText(QStringLiteral("nope"));
  QTest::keyClick(editor, Qt::Key_Escape);
  QCOMPARE(label->text(), QStringLiteral("login"));
  QVERIFY(d.getPassword().contains(QStringLiteral("login: bob\n")));

  QPointer<QLineEdit> line = d.findChild<QLineEdit *>(QStringLiteral("login"));
  QVERIFY(line != nullptr);
  auto *remove =
      line->findChild<QAction *>(QStringLiteral("removeFieldAction"));
  QVERIFY2(remove != nullptr, "the value field must carry a remove action");
  QCOMPARE(remove->toolTip(), QStringLiteral("Remove field"));
  remove->trigger();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QVERIFY2(line.isNull(), "the field's line edit must be deleted with its row");
  const QString written = d.getPassword();
  QVERIFY2(!written.contains(QStringLiteral("login")),
           qPrintable("a removed field must not be written back:\n" + written));
  QVERIFY(written.contains(QStringLiteral("url: example.com\n")));
}

/**
 * @brief Fields that come from the template are named by the template, so
 *        their labels stay plain: renaming one would only create a stray key.
 */
void tst_passworddialog::templateFieldsKeepPlainLabels() {
  FakePass pass;
  AppSettings s = allFieldsSettings();
  s.useTemplate = true;
  s.passTemplate = QStringLiteral("login");
  PasswordDialog d(&pass, s, QStringLiteral("entry.gpg"), false);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  pass.deliverShow(QStringLiteral("secret\nlogin: bob\nurl: example.com\n"));

  QVERIFY2(fieldLabel(d, QStringLiteral("login")) == nullptr,
           "the template's own field must not get a FieldLabel");
  QVERIFY2(fieldLabel(d, QStringLiteral("url")) != nullptr,
           "the entry's extra field must still be renameable");
}

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

/**
 * @brief finishedShow names its file: a decrypt still queued from a tree
 *        click when the dialog opened must not unlock it or fill its fields.
 */
void tst_passworddialog::anotherEntrysDecryptIsIgnored() {
  FakePass pass;
  const AppSettings s = QtPassSettings::load();
  PasswordDialog d(&pass, s, QStringLiteral("entry.gpg"), false);
  QCOMPARE(pass.shown, QStringLiteral("entry.gpg"));

  pass.deliverShow(QStringLiteral("other-secret\n"),
                   QStringLiteral("other.gpg"));
  QVERIFY2(!okButton(d)->isEnabled(),
           "another entry's decrypt must not unlock Ok");
  auto *pw = d.findChild<QLineEdit *>(QStringLiteral("lineEditPassword"));
  QVERIFY(pw->text().isEmpty());

  pass.deliverShow(QStringLiteral("mine\n"));
  QVERIFY(okButton(d)->isEnabled());
  QCOMPARE(pw->text(), QStringLiteral("mine"));
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
 * @brief The folder/name row appears only when the caller says where a new
 *        entry may go; an existing entry keeps the name it was opened with.
 */
void tst_passworddialog::nameRowOnlyForANewEntryWithALocation() {
  FakePass pass;
  const AppSettings s = QtPassSettings::load();
  PasswordDialog existing(&pass, s, QStringLiteral("entry.gpg"), false);
  QVERIFY(!existing.findChild<QWidget *>(QStringLiteral("nameRow"))
               ->isVisibleTo(&existing));
  PasswordDialog fresh(&pass, s, QString(), true);
  QVERIFY(!fresh.findChild<QWidget *>(QStringLiteral("nameRow"))
               ->isVisibleTo(&fresh));
  QCOMPARE(fresh.windowTitle(), QStringLiteral("New password"));
  fresh.setNewEntryLocation(QDir::tempPath(), {QString()}, QString());
  QVERIFY(fresh.findChild<QWidget *>(QStringLiteral("nameRow"))
              ->isVisibleTo(&fresh));
}

/**
 * @brief OK follows the name: empty, escaping the store, taken by an entry
 *        or a folder keep it off with the reason in the status line.
 */
void tst_passworddialog::newEntryNameIsValidatedAsTyped() {
  QTemporaryDir store;
  QVERIFY(QDir(store.path()).mkpath(QStringLiteral("work")));
  QFile vpn(QDir(store.path()).filePath(QStringLiteral("work/vpn.gpg")));
  QVERIFY(vpn.open(QIODevice::WriteOnly));
  vpn.close();

  FakePass pass;
  const AppSettings s = QtPassSettings::load();
  PasswordDialog d(&pass, s, QString(), true);
  d.setNewEntryLocation(store.path(), {QString(), QStringLiteral("work")},
                        QStringLiteral("work"));
  auto *folder = d.findChild<QComboBox *>(QStringLiteral("folderBox"));
  auto *name = d.findChild<QLineEdit *>(QStringLiteral("nameEdit"));
  auto *status = d.findChild<QLabel *>(QStringLiteral("statusLabel"));
  QVERIFY(folder != nullptr && name != nullptr && status != nullptr);
  QCOMPARE(folder->currentData().toString(), QStringLiteral("work"));
  QVERIFY2(!okButton(d)->isEnabled(), "no name yet");
  QVERIFY(status->text().contains(QStringLiteral("name")));

  name->setText(QStringLiteral("vpn"));
  QVERIFY2(!okButton(d)->isEnabled(), "work/vpn exists");
  QVERIFY(status->text().contains(QStringLiteral("already exists")));

  name->setText(QStringLiteral("../../etc/passwd"));
  QVERIFY2(!okButton(d)->isEnabled(), "escapes the store");
  QVERIFY(status->text().contains(QStringLiteral("outside")));

  folder->setCurrentIndex(0);
  name->setText(QStringLiteral("work"));
  QVERIFY2(!okButton(d)->isEnabled(), "work is a folder");
  QVERIFY(status->text().contains(QStringLiteral("folder")));

  name->setText(QStringLiteral("mail"));
  QVERIFY(okButton(d)->isEnabled());
  QVERIFY(status->text().isEmpty());

  // A typed .gpg suffix is the file's, not the entry's.
  folder->setCurrentIndex(1);
  name->setText(QStringLiteral("vpn.GPG"));
  QVERIFY2(!okButton(d)->isEnabled(), "still the existing work/vpn");
  name->setText(QStringLiteral("mail.gpg"));
  QVERIFY(okButton(d)->isEnabled());
  okButton(d)->click();
  QCOMPARE(d.entryPath(), QStringLiteral("work/mail"));
}

/**
 * @brief Accepting composes folder and name into the entry path, creates a
 *        subfolder the name asks for, and hands that path to Insert.
 */
void tst_passworddialog::acceptResolvesTheNameAndCreatesItsFolder() {
  QTemporaryDir store;
  QVERIFY(QDir(store.path()).mkpath(QStringLiteral("work")));

  FakePass pass;
  const AppSettings s = QtPassSettings::load();
  PasswordDialog d(&pass, s, QString(), true);
  d.setNewEntryLocation(store.path(), {QString(), QStringLiteral("work")},
                        QStringLiteral("work"));
  auto *name = d.findChild<QLineEdit *>(QStringLiteral("nameEdit"));
  auto *password = d.findChild<QLineEdit *>(QStringLiteral("lineEditPassword"));
  name->setText(QStringLiteral("vpn/office"));
  password->setText(QStringLiteral("s3cret"));
  okButton(d)->click();

  QCOMPARE(d.result(), int(QDialog::Accepted));
  QCOMPARE(d.entryPath(), QStringLiteral("work/vpn/office"));
  QCOMPARE(pass.inserted, QStringLiteral("work/vpn/office"));
  QVERIFY2(!pass.insertedOverwrite, "a new entry is not an overwrite");
  QVERIFY2(QDir(store.path()).exists(QStringLiteral("work/vpn")),
           "the folder the name asked for was created");
  QVERIFY(d.windowTitle().endsWith(QStringLiteral("work/vpn/office")));
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
