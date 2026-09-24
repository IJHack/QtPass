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
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QtTest>

#include <functional>

#include "../../../src/fieldlabel.h"
#include "../../../src/pass.h"
#include "../../../src/passworddialog.h"
#include "../../../src/qtpasssettings.h"
#include "../testpass.h"
#include "../testsettings.h"

namespace {

/**
 * @brief Minimal Pass stub with no-op operations.
 *
 * Show() never touches the executor, so finishedShow/processErrorExit are
 * only emitted when the test calls deliverShow()/deliverError() – making the
 * asynchronous decrypt deterministic to test.
 */
class FakePass : public NullPass {
public:
  FakePass() { init(QtPassSettings::load()); }

  void Show(QString file) override { shown = file; }
  void Insert(QString file, QString content, bool overwrite) override {
    inserted = file;
    insertedContent = content;
    insertedOverwrite = overwrite;
  }
  QString inserted;
  QString insertedContent;
  bool insertedOverwrite = false;

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
  void fieldLabelDoubleClickStartsTheEdit();
  void fieldLabelContextMenuRenamesAndRemoves();
  void fieldLabelEditorTakesTheFormCell();
  void generateButtonFillsThePasswordField();
  void generateButtonIgnoresAnUnknownCharset();
  void okBeforeTheContentLoadedWritesNothing();
  void okWritesTheEntryBackWithATrailingNewline();
  void cancelClearsTheFields();
  void newEntryNameCannotEndInASlash();
  void acceptRefusesAnInvalidNameAndAnUncreatableFolder();
  void unknownDefaultTemplateFallsBackToTheFirst();
  void reloadAndTemplateChangeDropOldFieldRows();
  void otpFieldFlagsAnInvalidSecret();
  void otpWarningSurvivesAReloadOnlyOnce();
  void otpUriIsNormalisedOnLeavingTheField();
  void otpUntouchedValueIsWrittenBackVerbatim();
  void otpPopulatedFieldWinsOverTheEmptyTemplateOne();
  void renamingTheOtpFieldAwayDropsItsValidation();
  void typingInARenamedAwayOtpFieldLeavesTheCurrentOneAlone();
  void removingTheFocusedOtpFieldRewritesNoOtherField();
  void removingAFieldWhileRenamingItClosesTheEditor();
  void destroyingAShownDialogWithATypedOtpFieldIsSafe();
  void destroyingAShownDialogWithAnOpenRenameEditorIsSafe();
  void reloadingTheEntryForgetsWhatTheUserTyped();
  void theHookedFieldStaysTheOtpFieldWhileTheUserTypes();
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

namespace {
/**
 * @brief Run @p drive on the QMenu that @p owner is about to exec(): the
 *        menu blocks the caller, so the choice has to come from the event
 *        loop the menu runs. @p drive must close the menu.
 */
void whenContextMenuOpens(QWidget *owner,
                          const std::function<void(QMenu *)> &drive) {
  auto *poll = new QTimer(owner);
  poll->setInterval(0);
  QObject::connect(poll, &QTimer::timeout, poll, [owner, drive, poll] {
    auto *menu = owner->findChild<QMenu *>();
    if (menu == nullptr || !menu->isVisible()) {
      return;
    }
    poll->stop();
    poll->deleteLater();
    drive(menu);
  });
  poll->start();
}

/**
 * @brief Deliver a mouse-triggered QContextMenuEvent at the centre of
 *        @p owner, the way a right click would.
 */
void openContextMenu(QWidget *owner) {
  const QPoint pos = owner->rect().center();
  QContextMenuEvent ev(QContextMenuEvent::Mouse, pos, owner->mapToGlobal(pos));
  QCoreApplication::sendEvent(owner, &ev);
}
} // namespace

/**
 * @brief A left double-click on the label opens the editor, a right one is
 *        left to QLabel; a second startEdit() reuses the editor instead of
 *        opening another.
 */
void tst_passworddialog::fieldLabelDoubleClickStartsTheEdit() {
  FakePass pass;
  PasswordDialog d(&pass, allFieldsSettings(), QStringLiteral("entry.gpg"),
                   false);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  pass.deliverShow(QStringLiteral("secret\nlogin: bob\n"));

  FieldLabel *label = fieldLabel(d, QStringLiteral("login"));
  QVERIFY(label != nullptr);
  QTest::mouseDClick(label, Qt::RightButton);
  QVERIFY2(d.findChild<QLineEdit *>(QStringLiteral("fieldNameEditor")) ==
               nullptr,
           "a right double-click must not start the edit");

  QTest::mouseDClick(label, Qt::LeftButton);
  QPointer<QLineEdit> editor =
      d.findChild<QLineEdit *>(QStringLiteral("fieldNameEditor"));
  QVERIFY2(!editor.isNull(), "a left double-click must open the editor");
  QCOMPARE(editor->text(), QStringLiteral("login"));

  label->startEdit();
  QVERIFY2(!editor.isNull(),
           "a second startEdit() must keep the editor already open");
  QCOMPARE(
      d.findChildren<QLineEdit *>(QStringLiteral("fieldNameEditor")).size(), 1);
  QVERIFY2(d.focusWidget() == editor,
           "a second startEdit() must only focus the existing editor");
}

/**
 * @brief The context menu offers Rename field and Remove field; choosing
 *        Rename opens the editor on that field, choosing Remove drops the
 *        row, with the label (the menu's parent, still inside exec()) and
 *        the line deleted only once control is back in the event loop.
 */
void tst_passworddialog::fieldLabelContextMenuRenamesAndRemoves() {
  FakePass pass;
  PasswordDialog d(&pass, allFieldsSettings(), QStringLiteral("entry.gpg"),
                   false);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  pass.deliverShow(QStringLiteral("secret\nlogin: bob\nurl: example.com\n"));

  FieldLabel *label = fieldLabel(d, QStringLiteral("login"));
  QVERIFY(label != nullptr);
  QStringList offered;
  whenContextMenuOpens(label, [&offered](QMenu *menu) {
    for (QAction *action : menu->actions()) {
      offered.append(action->text());
    }
    menu->setActiveAction(menu->actions().first());
    QTest::keyClick(menu, Qt::Key_Return);
  });
  openContextMenu(label);
  QCOMPARE(offered.size(), 2);
  QVERIFY2(offered.at(0).startsWith(QStringLiteral("Rename field")),
           qPrintable(offered.join(QStringLiteral(" | "))));
  QCOMPARE(offered.at(1), QStringLiteral("Remove field"));

  auto *editor = d.findChild<QLineEdit *>(QStringLiteral("fieldNameEditor"));
  QVERIFY2(editor != nullptr, "Rename field must open the editor");
  QCOMPARE(editor->text(), QStringLiteral("login"));
  editor->setText(QStringLiteral("user"));
  QTest::keyClick(editor, Qt::Key_Return);
  QCOMPARE(label->text(), QStringLiteral("user"));
  QVERIFY(d.getPassword().contains(QStringLiteral("user: bob\n")));
  QVERIFY2(!d.getPassword().contains(QStringLiteral("login")),
           "the field must be written under the new name only");

  // Remove field, from the menu: the row goes, nothing is written back, and
  // the widgets outlive the menu's exec() that asked for their removal.
  QPointer<FieldLabel> gone(label);
  QPointer<QLineEdit> line = d.findChild<QLineEdit *>(QStringLiteral("user"));
  QVERIFY(line != nullptr);
  whenContextMenuOpens(label, [](QMenu *menu) {
    menu->setActiveAction(menu->actions().at(1));
    QTest::keyClick(menu, Qt::Key_Return);
  });
  openContextMenu(label);
  QVERIFY2(!gone.isNull() && !line.isNull(),
           "the row's widgets must not be deleted from inside the menu");
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QVERIFY2(gone.isNull() && line.isNull(),
           "the row's widgets must be deleted once the stack has unwound");
  const QString written = d.getPassword();
  QVERIFY2(!written.contains(QStringLiteral("user")),
           qPrintable("a removed field must not be written back:\n" + written));
  QVERIFY(written.contains(QStringLiteral("url: example.com\n")));
}

/**
 * @brief When the label sits directly in a QFormLayout, the editor takes its
 *        cell (so the label column widens for it) and the label gets the
 *        cell back once the edit ends; the rename is reported as a signal.
 */
void tst_passworddialog::fieldLabelEditorTakesTheFormCell() {
  QWidget w;
  auto *form = new QFormLayout(&w);
  auto *label = new FieldLabel(QStringLiteral("login"), &w);
  auto *value = new QLineEdit(&w);
  form->addRow(label, value);
  w.show();
  QVERIFY(QTest::qWaitForWindowExposed(&w));
  QSignalSpy renamed(label, &FieldLabel::renamed);

  label->startEdit();
  auto *editor = w.findChild<QLineEdit *>(QStringLiteral("fieldNameEditor"));
  QVERIFY(editor != nullptr);
  int row = -1;
  QFormLayout::ItemRole role{};
  form->getWidgetPosition(editor, &row, &role);
  QCOMPARE(row, 0);
  QCOMPARE(role, QFormLayout::LabelRole);
  QVERIFY2(label->isHidden(), "the label must give its cell to the editor");
  form->getWidgetPosition(label, &row, &role);
  QCOMPARE(row, -1);

  editor->setText(QStringLiteral("user"));
  QTest::keyClick(editor, Qt::Key_Return);
  QCOMPARE(renamed.size(), 1);
  QCOMPARE(renamed.first().at(0).toString(), QStringLiteral("login"));
  QCOMPARE(renamed.first().at(1).toString(), QStringLiteral("user"));
  form->getWidgetPosition(label, &row, &role);
  QCOMPARE(row, 0);
  QCOMPARE(role, QFormLayout::LabelRole);
  QVERIFY2(!label->isHidden(), "the label must be back in its cell");
  QCOMPARE(label->text(), QStringLiteral("login"));
}

/**
 * @brief The generate button fills the password field with a password of the
 *        configured length and leaves the editor enabled afterwards.
 */
void tst_passworddialog::generateButtonFillsThePasswordField() {
  FakePass pass;
  PasswordDialog d(&pass, QtPassSettings::load(), QStringLiteral("new.gpg"),
                   true);
  d.setLength(12);
  d.setPasswordCharTemplate(0);
  auto *generate =
      d.findChild<QToolButton *>(QStringLiteral("createPasswordButton"));
  auto *pw = d.findChild<QLineEdit *>(QStringLiteral("lineEditPassword"));
  auto *editor = d.findChild<QWidget *>(QStringLiteral("widget"));
  QVERIFY(generate != nullptr && pw != nullptr && editor != nullptr);
  QVERIFY(pw->text().isEmpty());

  generate->click();
  QCOMPARE(pw->text().size(), 12);
  QVERIFY2(editor->isEnabled(), "the editor must be re-enabled afterwards");
  const QString first = pw->text();
  generate->click();
  QVERIFY2(pw->text() != first, "each click must generate a new password");
}

/**
 * @brief Without a valid character set selected there is nothing to
 *        generate from: the field is left alone and the editor unlocked.
 */
void tst_passworddialog::generateButtonIgnoresAnUnknownCharset() {
  FakePass pass;
  PasswordDialog d(&pass, QtPassSettings::load(), QStringLiteral("new.gpg"),
                   true);
  d.setPasswordCharTemplate(-1);
  auto *generate =
      d.findChild<QToolButton *>(QStringLiteral("createPasswordButton"));
  auto *pw = d.findChild<QLineEdit *>(QStringLiteral("lineEditPassword"));
  auto *editor = d.findChild<QWidget *>(QStringLiteral("widget"));
  QVERIFY(generate != nullptr && pw != nullptr && editor != nullptr);
  pw->setText(QStringLiteral("keep"));

  generate->click();
  QCOMPARE(pw->text(), QStringLiteral("keep"));
  QVERIFY2(editor->isEnabled(),
           "the editor must not stay locked when nothing was generated");
}

/**
 * @brief Accepting an existing entry before its decrypt arrived must not
 *        write the empty fields over it.
 */
void tst_passworddialog::okBeforeTheContentLoadedWritesNothing() {
  FakePass pass;
  PasswordDialog d(&pass, QtPassSettings::load(), QStringLiteral("entry.gpg"),
                   false);
  QSignalSpy accepted(&d, &QDialog::accepted);
  d.accept();
  QCOMPARE(accepted.size(), 1);
  QVERIFY2(pass.inserted.isEmpty(),
           "nothing may be inserted before the content has loaded");
}

/**
 * @brief OK on a loaded entry hands Insert the joined fields as an overwrite,
 *        terminated by a newline even when the notes lack one.
 */
void tst_passworddialog::okWritesTheEntryBackWithATrailingNewline() {
  FakePass pass;
  PasswordDialog d(&pass, QtPassSettings::load(), QStringLiteral("entry.gpg"),
                   false);
  pass.deliverShow(QStringLiteral("secret\nnote"));
  auto *notes = d.findChild<QPlainTextEdit *>();
  QVERIFY(notes != nullptr);
  QCOMPARE(notes->toPlainText(), QStringLiteral("note"));

  okButton(d)->click();
  QCOMPARE(pass.inserted, QStringLiteral("entry.gpg"));
  QVERIFY2(pass.insertedOverwrite, "an existing entry is overwritten");
  QCOMPARE(pass.insertedContent, QStringLiteral("secret\nnote\n"));
}

/**
 * @brief Cancel empties the dialog: the password and every field row go, so
 *        nothing lingers in a dialog that is reused or inspected later.
 */
void tst_passworddialog::cancelClearsTheFields() {
  FakePass pass;
  PasswordDialog d(&pass, allFieldsSettings(), QStringLiteral("entry.gpg"),
                   false);
  pass.deliverShow(QStringLiteral("secret\nlogin: bob\nnotes\n"));
  QVERIFY(d.findChild<QLineEdit *>(QStringLiteral("login")) != nullptr);

  d.reject();
  QCOMPARE(d.result(), int(QDialog::Rejected));
  QVERIFY(pass.inserted.isEmpty());
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QVERIFY2(d.findChild<QLineEdit *>(QStringLiteral("login")) == nullptr,
           "the field rows must be gone");
  QCOMPARE(d.getPassword(), QStringLiteral("\n"));
}

/**
 * @brief A name ending in / names a folder, not an entry: OK stays off and
 *        the status line says why.
 */
void tst_passworddialog::newEntryNameCannotEndInASlash() {
  QTemporaryDir store;
  FakePass pass;
  PasswordDialog d(&pass, QtPassSettings::load(), QString(), true);
  d.setNewEntryLocation(store.path(), {QString()}, QString());
  auto *name = d.findChild<QLineEdit *>(QStringLiteral("nameEdit"));
  auto *status = d.findChild<QLabel *>(QStringLiteral("statusLabel"));
  QVERIFY(name != nullptr && status != nullptr);

  name->setText(QStringLiteral("work/"));
  QVERIFY2(!okButton(d)->isEnabled(), "a trailing slash is not a name");
  QVERIFY2(status->text().contains(QStringLiteral("end in /")),
           qPrintable("the status must explain: " + status->text()));
}

/**
 * @brief accept() re-checks the name itself (Enter can bypass the disabled
 *        OK) and refuses when the folder the name asks for cannot be made.
 */
void tst_passworddialog::acceptRefusesAnInvalidNameAndAnUncreatableFolder() {
  QTemporaryDir store;
  QFile blocker(QDir(store.path()).filePath(QStringLiteral("blocker")));
  QVERIFY(blocker.open(QIODevice::WriteOnly));
  blocker.close();

  FakePass pass;
  PasswordDialog d(&pass, QtPassSettings::load(), QString(), true);
  d.setNewEntryLocation(store.path(), {QString()}, QString());
  auto *name = d.findChild<QLineEdit *>(QStringLiteral("nameEdit"));
  auto *status = d.findChild<QLabel *>(QStringLiteral("statusLabel"));
  QVERIFY(name != nullptr && status != nullptr);
  QSignalSpy accepted(&d, &QDialog::accepted);

  name->setText(QStringLiteral("  "));
  d.accept();
  QVERIFY2(accepted.isEmpty(), "an empty name must not be accepted");
  QVERIFY(status->text().contains(QStringLiteral("name")));
  QVERIFY(pass.inserted.isEmpty());

  // "blocker" is a file, so the folder blocker/ can never be created.
  name->setText(QStringLiteral("blocker/vpn"));
  QVERIFY2(okButton(d)->isEnabled(), "nothing wrong with the name as typed");
  okButton(d)->click();
  QVERIFY2(accepted.isEmpty(), "an uncreatable folder must block accepting");
  QVERIFY2(status->text().contains(QStringLiteral("Could not create")),
           qPrintable("the status must name the failure: " + status->text()));
  QVERIFY(pass.inserted.isEmpty());
  QVERIFY(d.entryPath().isEmpty());
}

/**
 * @brief A default template that the .templates file no longer has falls
 *        back to the first name, so the dialog never shows no template.
 */
void tst_passworddialog::unknownDefaultTemplateFallsBackToTheFirst() {
  FakePass pass;
  PasswordDialog d(&pass, QtPassSettings::load(), QStringLiteral("new.gpg"),
                   true);
  d.setAvailableTemplates(twoTemplates(), QStringLiteral("gone"));
  auto *box = d.findChild<QComboBox *>(QStringLiteral("templateBox"));
  QVERIFY(box != nullptr);
  QCOMPARE(box->currentText(), QStringLiteral("login"));
  QVERIFY2(d.findChild<QLineEdit *>(QStringLiteral("username")) != nullptr,
           "the fallback template must be applied");
}

/**
 * @brief Re-populating the dialog (a second decrypt) and switching template
 *        both discard the previous entry-defined rows instead of stacking
 *        duplicates that would be written back twice.
 */
void tst_passworddialog::reloadAndTemplateChangeDropOldFieldRows() {
  FakePass pass;
  PasswordDialog d(&pass, allFieldsSettings(), QStringLiteral("entry.gpg"),
                   false);
  pass.deliverShow(QStringLiteral("secret\nlogin: bob\n"));
  pass.deliverShow(QStringLiteral("secret\nlogin: alice\n"));
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  const auto logins = d.findChildren<QLineEdit *>(QStringLiteral("login"));
  QCOMPARE(logins.size(), 1);
  QCOMPARE(logins.first()->text(), QStringLiteral("alice"));
  QCOMPARE(d.getPassword().count(QStringLiteral("login:")), 1);

  d.setAvailableTemplates(twoTemplates(), QStringLiteral("wifi"));
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QVERIFY2(d.findChild<QLineEdit *>(QStringLiteral("login")) == nullptr,
           "applying a template must drop the entry-defined rows");
  QVERIFY(d.findChild<QLineEdit *>(QStringLiteral("ssid")) != nullptr);
  QVERIFY(!d.getPassword().contains(QStringLiteral("login:")));
}

/**
 * @brief The OTP template field explains what it takes and flags a value
 *        that is not a usable secret with a trailing icon and tooltip, which
 *        both go away once the value is fixed or cleared.
 */
void tst_passworddialog::otpFieldFlagsAnInvalidSecret() {
  FakePass pass;
  AppSettings s = QtPassSettings::load();
  s.useTemplate = true;
  s.passTemplate = QStringLiteral("OTP");
  PasswordDialog d(&pass, s, QStringLiteral("new.gpg"), true);
  auto *otp = d.findChild<QLineEdit *>(QStringLiteral("OTP"));
  QVERIFY(otp != nullptr);
  QVERIFY2(!otp->placeholderText().isEmpty(),
           "the OTP field must say what it takes");
  const int plainActions = otp->actions().size();
  QVERIFY(otp->toolTip().isEmpty());

  otp->setText(QStringLiteral("otpauth://broken"));
  QCOMPARE(otp->actions().size(), plainActions + 1);
  QCOMPARE(otp->toolTip(), QStringLiteral("Invalid OTP secret"));

  otp->setText(QStringLiteral("otpauth://still broken"));
  QVERIFY2(otp->actions().size() == plainActions + 1,
           "a second bad value must not add a second icon");

  otp->setText(QStringLiteral("JBSWY3DPEHPK3PXP"));
  QCOMPARE(otp->actions().size(), plainActions);
  QVERIFY2(otp->toolTip().isEmpty(), "a valid secret clears the warning");

  otp->setText(QStringLiteral("otpauth://broken"));
  otp->clear();
  QCOMPARE(otp->actions().size(), plainActions);
  QVERIFY(otp->toolTip().isEmpty());
}

/**
 * @brief A template OTP widget survives setPassword(); its warning icon must
 *        be removed when the field is re-hooked, not stacked or leaked.
 */
void tst_passworddialog::otpWarningSurvivesAReloadOnlyOnce() {
  FakePass pass;
  AppSettings s = QtPassSettings::load();
  s.useTemplate = true;
  s.passTemplate = QStringLiteral("OTP");
  PasswordDialog d(&pass, s, QStringLiteral("entry.gpg"), false);
  auto *otp = d.findChild<QLineEdit *>(QStringLiteral("OTP"));
  QVERIFY(otp != nullptr);
  const int plainActions = otp->actions().size();

  otp->setText(QStringLiteral("otpauth://broken"));
  QCOMPARE(otp->actions().size(), plainActions + 1);

  pass.deliverShow(QStringLiteral("secret\nOTP: otpauth://also broken\n"));
  QCOMPARE(otp->text(), QStringLiteral("otpauth://also broken"));
  QCOMPARE(otp->actions().size(), plainActions + 1);
  QCOMPARE(otp->toolTip(), QStringLiteral("Invalid OTP secret"));

  pass.deliverShow(QStringLiteral("secret\n"));
  QVERIFY(otp->text().isEmpty());
  QCOMPARE(otp->actions().size(), plainActions);
  QVERIFY(otp->toolTip().isEmpty());
}

/**
 * @brief An otpauth:// URI entered in the field is canonicalised (label from
 *        the entry name, explicit digits and period) as soon as the field is
 *        left, so what is saved is what was shown; so is a bare base32 secret
 *        the user typed; a broken URI is left as typed for the user to fix.
 */
void tst_passworddialog::otpUriIsNormalisedOnLeavingTheField() {
  FakePass pass;
  AppSettings s = QtPassSettings::load();
  s.useTemplate = true;
  s.passTemplate = QStringLiteral("OTP");
  PasswordDialog d(&pass, s, QStringLiteral("github.com"), true);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  auto *otp = d.findChild<QLineEdit *>(QStringLiteral("OTP"));
  QVERIFY(otp != nullptr);

  QTest::keyClicks(otp,
                   QStringLiteral("otpauth://totp/?secret=JBSWY3DPEHPK3PXP"));
  QTest::keyClick(otp, Qt::Key_Return);
  QVERIFY2(
      otp->text().startsWith(QStringLiteral("otpauth://totp/github.com?")),
      qPrintable("the URI must be labelled with the entry: " + otp->text()));
  QVERIFY(otp->text().contains(QStringLiteral("secret=JBSWY3DPEHPK3PXP")));
  QVERIFY2(otp->text().contains(QStringLiteral("digits=6")),
           qPrintable("the URI must be made explicit: " + otp->text()));

  // A bare secret the user typed becomes a URI too; only a loaded one is
  // kept as it was (otpUntouchedValueIsWrittenBackVerbatim). A fresh dialog,
  // so the keystrokes here are what marks the field as typed in.
  {
    FakePass bare;
    PasswordDialog e(&bare, s, QStringLiteral("github.com"), true);
    e.show();
    QVERIFY(QTest::qWaitForWindowExposed(&e));
    auto *field = e.findChild<QLineEdit *>(QStringLiteral("OTP"));
    QVERIFY(field != nullptr);
    QTest::keyClicks(field, QStringLiteral("JBSWY3DPEHPK3PXP"));
    QTest::keyClick(field, Qt::Key_Return);
    QVERIFY2(
        field->text().startsWith(QStringLiteral("otpauth://totp/github.com?")),
        qPrintable("a typed bare secret must become a URI: " + field->text()));
    QVERIFY(field->text().contains(QStringLiteral("secret=JBSWY3DPEHPK3PXP")));
  }

  // The typed value is invalid: the field is left as typed for the user to fix.
  otp->clear();
  QTest::keyClicks(otp, QStringLiteral("otpauth://broken"));
  QTest::keyClick(otp, Qt::Key_Return);
  QCOMPARE(otp->text(), QStringLiteral("otpauth://broken"));

  // An empty field has nothing to normalise and stays empty.
  otp->clear();
  QTest::keyClick(otp, Qt::Key_Return);
  QVERIFY(otp->text().isEmpty());
}

/**
 * @brief A value that came from the entry and was not touched is written
 *        back byte-for-byte (a backup code like 12345678 looks like base32),
 *        while an otpauth:// URI is canonicalised on save.
 */
void tst_passworddialog::otpUntouchedValueIsWrittenBackVerbatim() {
  FakePass pass;
  AppSettings s = QtPassSettings::load();
  s.useTemplate = true;
  s.passTemplate = QStringLiteral("OTP");
  PasswordDialog d(&pass, s, QStringLiteral("entry.gpg"), false);
  pass.deliverShow(QStringLiteral("secret\nOTP: 12345678\n"));
  okButton(d)->click();
  QVERIFY2(
      pass.insertedContent.contains(QStringLiteral("OTP: 12345678\n")),
      qPrintable("an untouched value must be kept:\n" + pass.insertedContent));

  FakePass uriPass;
  PasswordDialog e(&uriPass, s, QStringLiteral("entry.gpg"), false);
  uriPass.deliverShow(QStringLiteral(
      "secret\nOTP: otpauth://totp/x?secret=jbswy3dpehpk3pxp\n"));
  okButton(e)->click();
  QVERIFY2(
      uriPass.insertedContent.contains(QStringLiteral(
          "OTP: otpauth://totp/x?secret=jbswy3dpehpk3pxp&digits=6")),
      qPrintable("a URI must be canonicalised:\n" + uriPass.insertedContent));
  QVERIFY(uriPass.insertedContent.contains(QStringLiteral("period=30")));
}

/**
 * @brief An entry storing its secret under `totp:` leaves the template's
 *        `OTP` widget empty; validation must follow the populated field.
 */
void tst_passworddialog::otpPopulatedFieldWinsOverTheEmptyTemplateOne() {
  FakePass pass;
  AppSettings s = allFieldsSettings();
  s.passTemplate = QStringLiteral("OTP");
  PasswordDialog d(&pass, s, QStringLiteral("entry.gpg"), false);
  pass.deliverShow(QStringLiteral("secret\ntotp: otpauth://broken\n"));
  auto *templ = d.findChild<QLineEdit *>(QStringLiteral("OTP"));
  auto *stored = d.findChild<QLineEdit *>(QStringLiteral("totp"));
  QVERIFY(templ != nullptr && stored != nullptr);
  QVERIFY(templ->text().isEmpty());

  QVERIFY2(!stored->placeholderText().isEmpty(),
           "the populated field must be the hooked one");
  QCOMPARE(stored->toolTip(), QStringLiteral("Invalid OTP secret"));
  QVERIFY2(templ->toolTip().isEmpty(),
           "the empty template widget must not carry the warning");
}

/**
 * @brief Renaming the only OTP field to a plain name makes it an ordinary
 *        field: the warning icon goes with the OTP role, and editing the value
 *        afterwards (its textChanged hook is still wired) must neither flag it
 *        again nor crash for want of an OTP field.
 */
void tst_passworddialog::renamingTheOtpFieldAwayDropsItsValidation() {
  FakePass pass;
  PasswordDialog d(&pass, allFieldsSettings(), QStringLiteral("entry.gpg"),
                   false);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  pass.deliverShow(QStringLiteral("secret\ntotp: otpauth://broken\n"));
  auto *line = d.findChild<QLineEdit *>(QStringLiteral("totp"));
  QVERIFY(line != nullptr);
  QCOMPARE(line->toolTip(), QStringLiteral("Invalid OTP secret"));
  const int flagged = line->actions().size();

  FieldLabel *label = fieldLabel(d, QStringLiteral("totp"));
  QVERIFY(label != nullptr);
  label->startEdit();
  auto *editor = d.findChild<QLineEdit *>(QStringLiteral("fieldNameEditor"));
  QVERIFY(editor != nullptr);
  editor->setText(QStringLiteral("backup"));
  QTest::keyClick(editor, Qt::Key_Return);
  QCOMPARE(line->objectName(), QStringLiteral("backup"));
  QCOMPARE(line->actions().size(), flagged - 1);

  line->setText(QStringLiteral("otpauth://also broken"));
  QVERIFY2(line->actions().size() == flagged - 1,
           "a field that is no longer the OTP one must not be flagged");
  QVERIFY2(d.getPassword().contains(
               QStringLiteral("backup: otpauth://also broken\n")),
           qPrintable(d.getPassword()));
}

/**
 * @brief The "typed in" mark belongs to the field, not to the dialog: typing
 *        into a field that used to be the OTP one (renamed away) says nothing
 *        about the field that is the OTP one now, whose loaded value (a backup
 *        code that happens to look like base32) is written back as it was.
 */
void tst_passworddialog::
    typingInARenamedAwayOtpFieldLeavesTheCurrentOneAlone() {
  FakePass pass;
  PasswordDialog d(&pass, allFieldsSettings(), QStringLiteral("entry.gpg"),
                   false);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  pass.deliverShow(
      QStringLiteral("secret\ntotp: JBSWY3DPEHPK3PXP\notp: 12345678\n"));
  auto *totp = d.findChild<QLineEdit *>(QStringLiteral("totp"));
  auto *otp = d.findChild<QLineEdit *>(QStringLiteral("otp"));
  QVERIFY(totp != nullptr && otp != nullptr);

  d.activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(&d));
  // The rows were added to the form after the dialog was shown; setFocus()
  // on a widget that is not visible yet does nothing.
  QTRY_VERIFY(totp->isVisible() && otp->isVisible());
  FieldLabel *label = fieldLabel(d, QStringLiteral("totp"));
  QVERIFY(label != nullptr);
  label->startEdit();
  auto *editor = d.findChild<QLineEdit *>(QStringLiteral("fieldNameEditor"));
  QVERIFY(editor != nullptr);
  QTRY_VERIFY(editor->hasFocus());
  // Typed, not setText(): a programmatic change leaves the editor
  // unedited, and an unedited line edit emits no editingFinished when it
  // loses the focus. Leaving it commits the name; Return would too, but it
  // also reaches the dialog's default button and accepts the whole dialog,
  // and the rest of this test needs a dialog the user is still working in.
  QTest::keyClicks(editor, QStringLiteral("seed"));
  otp->setFocus();
  QCOMPARE(totp->objectName(), QStringLiteral("seed"));
  QVERIFY(d.isVisible());

  // Type in the renamed field, then leave it: its editingFinished is the
  // stale hook, and it must not reach the field that is the OTP one now.
  QSignalSpy leftTheRenamedField(totp, &QLineEdit::editingFinished);
  totp->setFocus();
  QTRY_VERIFY(totp->hasFocus());
  QTest::keyClicks(totp, QStringLiteral("x"));
  otp->setFocus();
  QTRY_COMPARE(leftTheRenamedField.count(), 1);
  QCOMPARE(otp->text(), QStringLiteral("12345678"));
  // And leaving the OTP field itself keeps its loaded value.
  totp->setFocus();
  QCOMPARE(otp->text(), QStringLiteral("12345678"));
  okButton(d)->click();
  QVERIFY2(pass.insertedContent.contains(QStringLiteral("otp: 12345678\n")),
           qPrintable(pass.insertedContent));
  QVERIFY(pass.insertedContent.contains(
      QStringLiteral("seed: JBSWY3DPEHPK3PXPx\n")));
}

/**
 * @brief Removing the OTP field the user typed in, while it has the focus,
 *        must not rewrite the field that becomes the OTP one: hiding the
 *        removed line emits editingFinished(), which used to run the
 *        normalisation on whatever otpLineEdit() found next.
 */
void tst_passworddialog::removingTheFocusedOtpFieldRewritesNoOtherField() {
  FakePass pass;
  PasswordDialog d(&pass, allFieldsSettings(), QStringLiteral("entry.gpg"),
                   false);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  pass.deliverShow(
      QStringLiteral("secret\ntotp: JBSWY3DPEHPK3PXP\notp: 12345678\n"));
  QPointer<QLineEdit> totp = d.findChild<QLineEdit *>(QStringLiteral("totp"));
  auto *otp = d.findChild<QLineEdit *>(QStringLiteral("otp"));
  QVERIFY(totp != nullptr && otp != nullptr);
  d.activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(&d));
  totp->setFocus();
  QTest::keyClicks(totp, QStringLiteral("A"));
  QTRY_VERIFY(totp->hasFocus());
  auto *remove =
      totp->findChild<QAction *>(QStringLiteral("removeFieldAction"));
  QVERIFY(remove != nullptr);
  remove->trigger();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QVERIFY(totp.isNull());
  QCOMPARE(otp->text(), QStringLiteral("12345678"));
  okButton(d)->click();
  QVERIFY2(pass.insertedContent.contains(QStringLiteral("otp: 12345678\n")),
           qPrintable(pass.insertedContent));
  QVERIFY(!pass.insertedContent.contains(QStringLiteral("totp")));
}

/**
 * @brief Removing a field while its name is being edited takes the editor
 *        with it: the editor is a sibling of the label, not a child, and
 *        stayed behind as a focused widget holding the old name.
 */
void tst_passworddialog::removingAFieldWhileRenamingItClosesTheEditor() {
  FakePass pass;
  PasswordDialog d(&pass, allFieldsSettings(), QStringLiteral("entry.gpg"),
                   false);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  pass.deliverShow(QStringLiteral("secret\nlogin: bob\nurl: example.com\n"));
  FieldLabel *label = fieldLabel(d, QStringLiteral("login"));
  QVERIFY(label != nullptr);
  label->startEdit();
  QPointer<QLineEdit> editor =
      d.findChild<QLineEdit *>(QStringLiteral("fieldNameEditor"));
  QVERIFY(editor != nullptr);
  editor->setText(QStringLiteral("changed"));
  auto *line = d.findChild<QLineEdit *>(QStringLiteral("login"));
  QVERIFY(line != nullptr);
  line->findChild<QAction *>(QStringLiteral("removeFieldAction"))->trigger();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QVERIFY2(editor.isNull(), "the editor must go with its label");
  QVERIFY(d.findChild<QLineEdit *>(QStringLiteral("fieldNameEditor")) ==
          nullptr);
  QVERIFY(!d.getPassword().contains(QStringLiteral("login")));
}

/**
 * @brief A shown dialog whose focused OTP field was typed in can be destroyed:
 *        QDialog's destructor hides the window, the focus leaves the field,
 *        editingFinished() fires, and the slot it reached belonged to the
 *        PasswordDialog part that was already gone (a debug build asserted).
 */
void tst_passworddialog::destroyingAShownDialogWithATypedOtpFieldIsSafe() {
  FakePass pass;
  AppSettings s = QtPassSettings::load();
  s.useTemplate = true;
  s.passTemplate = QStringLiteral("OTP");
  auto *d = new PasswordDialog(&pass, s, QStringLiteral("entry.gpg"), false);
  d->show();
  QVERIFY(QTest::qWaitForWindowExposed(d));
  pass.deliverShow(QStringLiteral("secret\nOTP: JBSWY3DPEHPK3PXP\n"));
  auto *otp = d->findChild<QLineEdit *>(QStringLiteral("OTP"));
  QVERIFY(otp != nullptr);
  d->activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(d));
  otp->setFocus();
  QTest::keyClicks(otp, QStringLiteral("A"));
  QTRY_VERIFY(otp->hasFocus());
  delete d;
  QVERIFY(pass.insertedContent.isEmpty());
}

/**
 * @brief A shown dialog whose field name is being edited can be destroyed:
 *        hiding it takes the focus from the editor, whose editingFinished()
 *        commits the rename into a slot of the dialog — which must still be
 *        whole when that runs.
 */
void tst_passworddialog::destroyingAShownDialogWithAnOpenRenameEditorIsSafe() {
  FakePass pass;
  auto *d = new PasswordDialog(&pass, allFieldsSettings(),
                               QStringLiteral("entry.gpg"), false);
  d->show();
  QVERIFY(QTest::qWaitForWindowExposed(d));
  d->activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(d));
  pass.deliverShow(QStringLiteral("secret\nlogin: bob\nurl: example.com\n"));
  FieldLabel *label = fieldLabel(*d, QStringLiteral("login"));
  QVERIFY(label != nullptr);
  // The row was added to the form after the dialog was shown; startEdit()
  // on a label that is not visible yet focuses nothing.
  QTRY_VERIFY(label->isVisible());
  label->startEdit();
  auto *editor = d->findChild<QLineEdit *>(QStringLiteral("fieldNameEditor"));
  QVERIFY(editor != nullptr);
  QTRY_VERIFY(editor->hasFocus());
  QTest::keyClicks(editor, QStringLiteral("x"));
  delete d;
  QVERIFY(pass.insertedContent.isEmpty());
}

/**
 * @brief The entry is shown again (a second decrypt for the same file lands
 *        while the dialog is open): the fields hold the entry's values once
 *        more, so what the user typed before is forgotten and a loaded value
 *        that looks like base32 is not rewritten on save.
 */
void tst_passworddialog::reloadingTheEntryForgetsWhatTheUserTyped() {
  FakePass pass;
  AppSettings s = QtPassSettings::load();
  s.useTemplate = true;
  s.passTemplate = QStringLiteral("OTP");
  PasswordDialog d(&pass, s, QStringLiteral("entry.gpg"), false);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  d.activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(&d));
  pass.deliverShow(QStringLiteral("secret\nOTP: 12345678\n"));
  auto *otp = d.findChild<QLineEdit *>(QStringLiteral("OTP"));
  QVERIFY(otp != nullptr);
  otp->setFocus();
  QTRY_VERIFY(otp->hasFocus());
  QTest::keyClicks(otp, QStringLiteral("A"));

  pass.deliverShow(QStringLiteral("secret\nOTP: 12345678\n"));
  QCOMPARE(otp->text(), QStringLiteral("12345678"));
  okButton(d)->click();
  QVERIFY2(pass.insertedContent.contains(QStringLiteral("OTP: 12345678\n")),
           qPrintable(pass.insertedContent));
}

/**
 * @brief Which field is the OTP one is settled when the fields change, not
 *        by what the user types: an entry keeping its secret under `totp`
 *        leaves the template's empty `OTP` widget an ordinary field, and
 *        typing a secret into it neither flags it nor rewrites it, while
 *        `totp` stays the field that is validated and canonicalised.
 */
void tst_passworddialog::theHookedFieldStaysTheOtpFieldWhileTheUserTypes() {
  FakePass pass;
  AppSettings s = QtPassSettings::load();
  s.useTemplate = true;
  s.passTemplate = QStringLiteral("OTP");
  s.templateAllFields = true;
  PasswordDialog d(&pass, s, QStringLiteral("entry.gpg"), false);
  d.show();
  QVERIFY(QTest::qWaitForWindowExposed(&d));
  d.activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(&d));
  pass.deliverShow(QStringLiteral("secret\ntotp: JBSWY3DPEHPK3PXP\n"));
  auto *tmpl = d.findChild<QLineEdit *>(QStringLiteral("OTP"));
  auto *totp = d.findChild<QLineEdit *>(QStringLiteral("totp"));
  QVERIFY(tmpl != nullptr && totp != nullptr);
  QTRY_VERIFY(tmpl->isVisible() && totp->isVisible());
  QVERIFY2(tmpl->text().isEmpty(), qPrintable(tmpl->text()));

  // Something that is not a secret, typed into the field that is not the
  // OTP one: left alone, not flagged.
  tmpl->setFocus();
  QTRY_VERIFY(tmpl->hasFocus());
  QTest::keyClicks(tmpl, QStringLiteral("not a secret"));
  totp->setFocus();
  QCOMPARE(tmpl->text(), QStringLiteral("not a secret"));
  QVERIFY2(tmpl->actions().isEmpty(), "an ordinary field is not flagged");
  QVERIFY(tmpl->toolTip().isEmpty());

  // The hooked field is still the one that gets canonicalised.
  QTRY_VERIFY(totp->hasFocus());
  totp->clear();
  QTest::keyClicks(totp, QStringLiteral("JBSWY3DPEHPK3PXP"));
  tmpl->setFocus();
  QVERIFY2(totp->text().startsWith(QStringLiteral("otpauth://totp/entry.gpg?")),
           qPrintable(totp->text()));
}

QTEST_MAIN(tst_passworddialog)
#include "tst_passworddialog.moc"
