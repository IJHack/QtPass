// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QListWidget>
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
  void acceptRunsInitByDefault();
  void acceptWithoutInitOnlyCollectsTheSelection();

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
  script.write("#!/bin/sh\n");
  script.write("case \"$*\" in\n*--list-secret-keys*) exit 0 ;;\n");
  script.write("*--list-keys*) cat <<'LISTING'\n");
  script.write(kColonListing);
  script.write("LISTING\n;;\nesac\nexit 0\n");
  script.close();
  QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                                QFile::ExeOwner));
  m_settings = QtPassSettings::load();
  m_settings.gpgExecutable = gpg;
  m_settings.passStore = QDir(m_dir.path()).filePath(QStringLiteral("store/"));
  QVERIFY(QDir().mkpath(m_settings.passStore));
  QtPassSettings::save(m_settings);
}

/**
 * @brief A folder without .gpg-id must not come up with the entire keyring
 *        pre-selected: listKeys() with an empty filter lists every key.
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

QTEST_MAIN(tst_usersdialog)
#include "tst_usersdialog.moc"
