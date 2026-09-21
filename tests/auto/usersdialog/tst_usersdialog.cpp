// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "../../../src/appsettings.h"
#include "../../../src/pass.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/usersdialog.h"
#include "../testsettings.h"

namespace {
/**
 * A Pass that records Init() calls. listKeys() is not virtual, so the key
 * list comes from a stand-in gpg script that prints a fixed --with-colons
 * listing; that keeps the test off the real keyring.
 */
class RecordingPass : public Pass {
public:
  explicit RecordingPass(const AppSettings &s) { init(s); }

  void GitInit() override {}
  void GitPull() override {}
  void GitPull_b() override {}
  void GitPush() override {}
  void Show(QString) override {}
  void Insert(QString, QString, bool) override {}
  void Remove(QString, bool) override {}
  void Move(const QString, const QString, const bool) override {}
  void Copy(const QString, const QString, const bool) override {}
  void Init(QString path, const QList<UserInfo> &users) override {
    initCalls << qMakePair(path, users);
  }
  void Grep(QString, bool) override {}

  QList<QPair<QString, QList<UserInfo>>> initCalls;
};

const char kColonListing[] =
    "pub:u:4096:1:31850CF72D9CDDE9:1774947438:::u:::escarESCA::::::23::0:\n"
    "fpr:::::::::13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9:\n"
    "uid:u::::1774947438::CBF23008234AA5F88824CE76140F482FAE34923E::Alice "
    "<alice@example.org>::::::::::0:\n"
    "pub:f:4096:1:693A0AF3FA364E76:1775005968:::f:::escarESCA::::::23::0:\n"
    "fpr:::::::::4EF2550F79F4E9E68B09F71D693A0AF3FA364E76:\n"
    "uid:f::::1775005968::8AA011711F27F6E08DF71653718C299A13B323A0::Bob "
    "<bob@example.org>::::::::::0:\n";
} // namespace

class tst_usersdialog : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void newStoreStartsWithNothingSelected();
  void existingStorePreselectsItsRecipients();
  void folderOutsideTheStoreDoesNotInheritItsRecipients();
  void acceptRunsInitByDefault();
  void acceptWithoutSelectionDoesNothing();
  void acceptWithoutInitOnlyCollectsTheSelection();
  void togglingAFilteredRowEnablesTheRightKey();
  void selectionSurvivesFilteringAndEscapeClearsTheFilter();

private:
  QTemporaryDir m_dir;
  AppSettings m_settings;
};

void tst_usersdialog::initTestCase() {
  isolateTestSettings();
#ifdef Q_OS_WIN
  QSKIP("the stand-in gpg is a shell script");
#endif
  QVERIFY(m_dir.isValid());
  const QString gpg = QDir(m_dir.path()).filePath(QStringLiteral("gpg"));
  QFile script(gpg);
  QVERIFY(script.open(QIODevice::WriteOnly | QIODevice::Text));
  // Like gpg, print only the keys matching a key id given after
  // --list-keys, and everything when none is given.
  script.write("#!/bin/sh\n");
  script.write("case \"$*\" in *--list-secret-keys*) exit 0 ;; esac\n");
  script.write("all() { cat <<'LISTING'\n");
  script.write(kColonListing);
  script.write("LISTING\n}\n");
  script.write("case \"$*\" in\n");
  script.write("*31850CF72D9CDDE9*) all | head -3 ;;\n");
  script.write("*693A0AF3FA364E76*) all | tail -3 ;;\n");
  script.write("*) all ;;\nesac\nexit 0\n");
  script.close();
  QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                                QFile::ExeOwner));
  m_settings = QtPassSettings::load();
  m_settings.gpgExecutable = gpg;
  m_settings.passStore = QDir(m_dir.path()).filePath(QStringLiteral("store/"));
  QVERIFY(QDir().mkpath(m_settings.passStore));
  QtPassSettings::save(m_settings);
}

namespace {
auto checkedNames(QListWidget *list) -> QStringList {
  QStringList names;
  for (int i = 0; i < list->count(); ++i) {
    if (list->item(i)->checkState() == Qt::Checked) {
      names << list->item(i)->text().section(QLatin1Char(' '), 0, 0);
    }
  }
  return names;
}
} // namespace

/**
 * @brief A folder without .gpg-id must not come up with the entire keyring
 *        preselected: listKeys() with an empty filter lists every key.
 */
void tst_usersdialog::newStoreStartsWithNothingSelected() {
  RecordingPass pass(m_settings);
  UsersDialog dialog(&pass, m_settings, m_settings.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QCOMPARE(list->count(), 2);
  for (int i = 0; i < list->count(); ++i) {
    QVERIFY2(list->item(i)->checkState() == Qt::Unchecked,
             qPrintable(list->item(i)->text() + " must start unchecked"));
  }
}

/**
 * @brief Sanity check for the stand-in gpg: with a .gpg-id in place the
 *        listed recipient comes up ticked.
 */
void tst_usersdialog::existingStorePreselectsItsRecipients() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
  gpgId.write("31850CF72D9CDDE9\n");
  gpgId.close();
  AppSettings s = m_settings;
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QCOMPARE(checkedNames(list), QStringList{QStringLiteral("Alice")});
}

/**
 * @brief A new profile lives outside the active store. Pass::getGpgIdPath()
 *        falls back to <store>/.gpg-id for such a folder, so a dialog that
 *        still carried the active store as its store came up with the active
 *        store's recipients ticked — and this branch writes exactly the
 *        ticked keys into the profile. The caller hands the profile as the
 *        store; nothing may be preselected then.
 */
void tst_usersdialog::folderOutsideTheStoreDoesNotInheritItsRecipients() {
  QTemporaryDir active;
  QTemporaryDir profile;
  QVERIFY(active.isValid() && profile.isValid());
  QFile gpgId(QDir(active.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
  gpgId.write("31850CF72D9CDDE9\n");
  gpgId.close();

  AppSettings activeSettings = m_settings;
  activeSettings.passStore = active.path() + QLatin1Char('/');
  RecordingPass pass(activeSettings);
  {
    // What initializeNewProfiles() used to do: the active store's settings
    // with the profile as the folder.
    UsersDialog wrong(&pass, activeSettings, profile.path() + QLatin1Char('/'));
    auto *list = wrong.findChild<QListWidget *>(QStringLiteral("listWidget"));
    QVERIFY(list != nullptr);
    QCOMPARE(checkedNames(list), QStringList{QStringLiteral("Alice")});
  }
  AppSettings profileSettings = activeSettings;
  profileSettings.passStore = profile.path() + QLatin1Char('/');
  UsersDialog right(&pass, profileSettings, profileSettings.passStore);
  auto *list = right.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QVERIFY2(checkedNames(list).isEmpty(),
           qPrintable("nothing may be preselected for a new profile, got: " +
                      checkedNames(list).join(QLatin1String(", "))));
}

void tst_usersdialog::acceptRunsInitByDefault() {
  RecordingPass pass(m_settings);
  UsersDialog dialog(&pass, m_settings, m_settings.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QCOMPARE(list->count(), 2);
  list->item(0)->setCheckState(Qt::Checked);

  dialog.accept();
  QCOMPARE(pass.initCalls.size(), 1);
  QCOMPARE(pass.initCalls.first().first, m_settings.passStore);
}

/**
 * @brief OK is greyed out until a key is ticked, and accept() refuses an
 *        empty selection anyway: an empty .gpg-id makes every insert fail
 *        and the wizard never shows this dialog again once the file exists.
 */
void tst_usersdialog::acceptWithoutSelectionDoesNothing() {
  RecordingPass pass(m_settings);
  UsersDialog dialog(&pass, m_settings, m_settings.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  auto *box = dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
  QVERIFY(list != nullptr && box != nullptr);
  QVERIFY2(!box->button(QDialogButtonBox::Ok)->isEnabled(),
           "OK must be disabled while nothing is selected");

  dialog.accept();
  QVERIFY(pass.initCalls.isEmpty());
  QVERIFY2(dialog.result() != QDialog::Accepted,
           "the dialog must stay open without a recipient");

  list->item(0)->setCheckState(Qt::Checked);
  QVERIFY2(box->button(QDialogButtonBox::Ok)->isEnabled(),
           "OK must be enabled once a key is ticked");
  list->item(0)->setCheckState(Qt::Unchecked);
  QVERIFY2(!box->button(QDialogButtonBox::Ok)->isEnabled(),
           "OK must be disabled again when the last key is unticked");
}

/**
 * @brief New profiles must not be initialised through the active store's
 *        backend (#1774): the dialog hands the selection back instead.
 */
void tst_usersdialog::acceptWithoutInitOnlyCollectsTheSelection() {
  RecordingPass pass(m_settings);
  UsersDialog dialog(&pass, m_settings, m_settings.passStore);
  dialog.setInitOnAccept(false);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QCOMPARE(list->count(), 2);
  list->item(1)->setCheckState(Qt::Checked);

  dialog.accept();
  QVERIFY2(pass.initCalls.isEmpty(), "Pass::Init must not run in this mode");
  const QList<UserInfo> users = dialog.selectedUsers();
  QCOMPARE(users.size(), 2);
  int enabled = 0;
  QString enabledId;
  for (const UserInfo &u : users) {
    if (u.enabled) {
      ++enabled;
      enabledId = u.key_id;
    }
  }
  QCOMPARE(enabled, 1);
  // The item's UserRole is its index into the user list.
  const int index = list->item(1)->data(Qt::UserRole).toInt();
  QCOMPARE(enabledId, users.at(index).key_id);
  QVERIFY(!enabledId.isEmpty());
}

namespace {
auto enabledIds(const QList<UserInfo> &users) -> QStringList {
  QStringList ids;
  for (const UserInfo &u : users) {
    if (u.enabled) {
      ids << u.key_id;
    }
  }
  ids.sort();
  return ids;
}
} // namespace

/**
 * @brief The list is rebuilt on every filter change and rows carry the
 *        index into m_userList in Qt::UserRole. Ticking row 0 of a filtered
 *        list must enable the key that row shows, not the first key
 *        overall — this mapping decides which keys the store is
 *        re-encrypted to.
 */
void tst_usersdialog::togglingAFilteredRowEnablesTheRightKey() {
  RecordingPass pass(m_settings);
  UsersDialog dialog(&pass, m_settings, m_settings.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  auto *filter = dialog.findChild<QLineEdit *>(QStringLiteral("lineEdit"));
  QVERIFY(list != nullptr && filter != nullptr);

  filter->setText(QStringLiteral("bob"));
  QCOMPARE(list->count(), 1);
  QVERIFY(list->item(0)->text().startsWith(QStringLiteral("Bob")));
  list->item(0)->setCheckState(Qt::Checked);

  dialog.accept();
  QCOMPARE(pass.initCalls.size(), 1);
  QCOMPARE(pass.initCalls.first().first, m_settings.passStore);
  QCOMPARE(
      enabledIds(pass.initCalls.first().second),
      QStringList{QStringLiteral("4EF2550F79F4E9E68B09F71D693A0AF3FA364E76")});
}

/**
 * @brief Ticks live in m_userList, not in the widgets, so a key ticked
 *        before filtering it out of view is still enabled afterwards;
 *        Escape clears the filter.
 */
void tst_usersdialog::selectionSurvivesFilteringAndEscapeClearsTheFilter() {
  RecordingPass pass(m_settings);
  UsersDialog dialog(&pass, m_settings, m_settings.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  auto *filter = dialog.findChild<QLineEdit *>(QStringLiteral("lineEdit"));
  QVERIFY(list != nullptr && filter != nullptr);

  QVERIFY(list->item(0)->text().startsWith(QStringLiteral("Alice")));
  list->item(0)->setCheckState(Qt::Checked);
  filter->setText(QStringLiteral("bob"));
  QCOMPARE(list->count(), 1);
  QVERIFY2(list->item(0)->checkState() == Qt::Unchecked,
           "Bob was never ticked");

  QTest::keyClick(&dialog, Qt::Key_Escape);
  QVERIFY2(filter->text().isEmpty(), "Escape must clear the filter");
  QCOMPARE(list->count(), 2);
  QVERIFY2(list->item(0)->checkState() == Qt::Checked,
           "Alice must come back ticked");

  dialog.accept();
  QCOMPARE(pass.initCalls.size(), 1);
  QCOMPARE(
      enabledIds(pass.initCalls.first().second),
      QStringList{QStringLiteral("13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9")});
}

QTEST_MAIN(tst_usersdialog)
#include "tst_usersdialog.moc"
