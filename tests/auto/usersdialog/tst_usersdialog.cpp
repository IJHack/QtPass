// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>
#ifndef Q_OS_WIN
#include <unistd.h>
#endif

#include "../../../src/appsettings.h"
#include "../../../src/gpgidgeneration.h"
#include "../../../src/importkeydialog.h"
#include "../../../src/pass.h"
#include "../../../src/qtpasssettings.h"
#include "../../../src/usersdialog.h"
#include "../testpass.h"
#include "../testsettings.h"

namespace {
/**
 * A Pass that records Init() calls. listKeys() is not virtual, so the key
 * list comes from a stand-in gpg script that prints a fixed --with-colons
 * listing; that keeps the test off the real keyring.
 */
class RecordingPass : public NullPass {
public:
  explicit RecordingPass(const AppSettings &s) { init(s); }

  void Init(QString path, const QList<UserInfo> &users) override {
    initCalls << qMakePair(path, users);
  }

  QList<QPair<QString, QList<UserInfo>>> initCalls;
};

} // namespace

class tst_usersdialog : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void newStoreStartsWithNothingSelected();
  void existingStorePreselectsItsRecipients();
  void withSigningATamperedListPreselectsNothing();
  void withSigningAnOlderVerifiedListIsPreselectedWithAWarning();
  void withSigningAListForAnotherFolderPreselectsNothing();
  void withSigningAHeaderlessListBelowTheRecordPreselectsNothing();
  void withSigningAMalformedHeaderPreselectsNothing();
  void withSigningAConflictingListPreselectsNothing();
  void withSigningAnUnreadableRecordPreselectsNothing();
  void folderOutsideTheStoreDoesNotInheritItsRecipients();
  void acceptRunsInitByDefault();
  void acceptWithoutSelectionDoesNothing();
  void acceptWithoutInitOnlyCollectsTheSelection();
  void togglingAFilteredRowEnablesTheRightKey();
  void selectionSurvivesFilteringAndEscapeClearsTheFilter();
  void emptyKeyringRejectsTheDialogWithACriticalBox();
  void unknownRecipientIsListedAsNotFoundAndTicked();
  void unusableKeysAreHiddenUntilAskedForAndThenBadged();
  void rowWithAnOutOfRangeIndexChangesNoSelection();
  void ownKeysAreBoldAndInLinkColour();
  void importingAKeyRefreshesTheListAndSelectsIt();
  void cancellingTheImportLeavesTheListAlone();

private:
  /// A stand-in gpg at m_dir/@p name: "#!/bin/sh" followed by @p body.
  /// Empty when it could not be written.
  auto writeGpg(const QString &name, const QString &body) -> QString;
  /// A gpg like the one from initTestCase() that also answers --verify: with
  /// VALIDSIG by kSigner when the bytes on stdin equal the file at
  /// @p signedBytes, and with failure otherwise. A signature bound to bytes,
  /// in a shell script.
  auto writeVerifyingGpg(const QString &signedBytes) -> QString;
  /// A signed pair: @p list as `.gpg-id` at @p gpgIdFile with a `.sig`
  /// beside it, and settings whose gpg verifies exactly those bytes by
  /// kSigner. Empty gpgExecutable when something could not be written.
  auto signedPair(const QString &gpgIdFile, const QByteArray &list,
                  const QString &store) -> AppSettings;

  QTemporaryDir m_dir;
  AppSettings m_settings;
};

static const QString kSigner =
    QStringLiteral("13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9");

auto tst_usersdialog::writeVerifyingGpg(const QString &signedBytes) -> QString {
  const QString gpg =
      QDir(m_dir.path()).filePath(QStringLiteral("gpg-verifying"));
  QFile script(gpg);
  if (!script.open(QIODevice::WriteOnly | QIODevice::Text |
                   QIODevice::Truncate))
    return {};
  QTextStream out(&script);
  out << "#!/bin/sh\n"
      << "case \"$*\" in\n"
      << "  *--verify*) cat > \"$0.stdin\"; if cmp -s \"$0.stdin\" '"
      << signedBytes << "'; then printf '[GNUPG:] VALIDSIG " << kSigner
      << " 2026-09-21 1758400000 0 4 0 1 10 00 " << kSigner
      << "\\n'; exit 0; else exit 1; fi ;;\n"
      << "  *--list-secret-keys*) exit 0 ;;\n"
      << "esac\n"
      << "all() { cat <<'LISTING'\n"
      << kColonListing << "LISTING\n}\n"
      << "case \"$*\" in\n"
      << "*31850CF72D9CDDE9*693A0AF3FA364E76*|*693A0AF3FA364E76*"
         "31850CF72D9CDDE9*) "
         "all ;;\n"
      << "*31850CF72D9CDDE9*) all | head -3 ;;\n"
      << "*693A0AF3FA364E76*) all | tail -3 ;;\n"
      << "*) all ;;\nesac\nexit 0\n";
  out.flush();
  script.close();
  if (!script.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                             QFile::ExeOwner))
    return {};
  return gpg;
}

auto tst_usersdialog::signedPair(const QString &gpgIdFile,
                                 const QByteArray &list, const QString &store)
    -> AppSettings {
  AppSettings s = m_settings;
  s.gpgExecutable.clear();
  const QString signedCopy =
      QDir(m_dir.path())
          .filePath(
              QStringLiteral("signed-bytes-") +
              QString::number(QFileInfo(gpgIdFile).absoluteFilePath().size()) +
              QString::number(list.size()));
  for (const QString &path : {gpgIdFile, signedCopy}) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        f.write(list) != list.size())
      return s;
  }
  QFile sig(gpgIdFile + QStringLiteral(".sig"));
  if (!sig.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
      sig.write("sig") != 3)
    return s;
  s.gpgExecutable = writeVerifyingGpg(signedCopy);
  s.passSigningKey = kSigner;
  s.passStore = store + QLatin1Char('/');
  return s;
}

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
 * @brief With a signing key, the dialog preselects only what verifies. The
 *        store's signed list names Alice; someone appends Bob to the file
 *        without being able to re-sign it. Opening Users must not tick Bob
 *        (one OK would sign him in), must tick nobody, and must say why.
 */
void tst_usersdialog::withSigningATamperedListPreselectsNothing() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QString gpgIdFile =
      QDir(store.path()).filePath(QStringLiteral(".gpg-id"));
  const QString signedCopy =
      QDir(m_dir.path()).filePath(QStringLiteral("signed-bytes"));
  const QByteArray signedList =
      GpgIdGeneration::withHeader(1, QStringLiteral("."), "31850CF72D9CDDE9\n");
  for (const QString &path : {gpgIdFile, signedCopy}) {
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(signedList);
  }
  {
    QFile sig(gpgIdFile + QStringLiteral(".sig"));
    QVERIFY(sig.open(QIODevice::WriteOnly));
    sig.write("sig");
  }
  AppSettings s = m_settings;
  s.gpgExecutable = writeVerifyingGpg(signedCopy);
  QVERIFY(!s.gpgExecutable.isEmpty());
  s.passSigningKey = kSigner;
  s.passStore = store.path() + QLatin1Char('/');
  // Untouched, the verified list is preselected and nothing is said.
  {
    RecordingPass pass(s);
    UsersDialog dialog(&pass, s, s.passStore);
    auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
    QVERIFY(list != nullptr);
    QCOMPARE(checkedNames(list), QStringList{QStringLiteral("Alice")});
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("recipientWarning")) ==
            nullptr);
  }
  // Tampered: Bob appended, signature left as it was.
  {
    QFile f(gpgIdFile);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(signedList + "693A0AF3FA364E76\n");
  }
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QVERIFY2(checkedNames(list).isEmpty(),
           qPrintable("preselected: " + checkedNames(list).join(", ")));
  auto *banner = dialog.findChild<QLabel *>(QStringLiteral("recipientWarning"));
  QVERIFY2(banner != nullptr, "the dialog must say why nothing is selected");
  QVERIFY2(banner->text().contains(QStringLiteral("does not verify")),
           qPrintable(banner->text()));
  // Without a signing key the same file is taken as it is, as before.
  s.passSigningKey.clear();
  RecordingPass plain(s);
  UsersDialog plainDialog(&plain, s, s.passStore);
  QCOMPARE(checkedNames(plainDialog.findChild<QListWidget *>(
               QStringLiteral("listWidget"))),
           (QStringList{QStringLiteral("Alice"), QStringLiteral("Bob")}));
}

/**
 * @brief A verified list that the generation record says is older is
 *        authentic, so it is preselected (the recovery from a rollback goes
 *        through this dialog), but the reason is shown so the user reviews
 *        who is ticked before saving.
 */
void tst_usersdialog::
    withSigningAnOlderVerifiedListIsPreselectedWithAWarning() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QString gpgIdFile =
      QDir(store.path()).filePath(QStringLiteral(".gpg-id"));
  const QString signedCopy =
      QDir(m_dir.path()).filePath(QStringLiteral("signed-bytes-old"));
  const QByteArray oldList = GpgIdGeneration::withHeader(
      1, QStringLiteral("."), "31850CF72D9CDDE9\n693A0AF3FA364E76\n");
  for (const QString &path : {gpgIdFile, signedCopy}) {
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(oldList);
  }
  {
    QFile sig(gpgIdFile + QStringLiteral(".sig"));
    QVERIFY(sig.open(QIODevice::WriteOnly));
    sig.write("sig");
  }
  // This device has accepted generation 2 of that list before.
  QCOMPARE(
      GpgIdGeneration::accept(gpgIdFile,
                              GpgIdGeneration::withHeader(
                                  2, QStringLiteral("."), "31850CF72D9CDDE9\n"),
                              store.path()),
      GpgIdGeneration::Verdict::Accepted);
  AppSettings s = m_settings;
  s.gpgExecutable = writeVerifyingGpg(signedCopy);
  QVERIFY(!s.gpgExecutable.isEmpty());
  s.passSigningKey = kSigner;
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QCOMPARE(checkedNames(list),
           (QStringList{QStringLiteral("Alice"), QStringLiteral("Bob")}));
  auto *banner = dialog.findChild<QLabel *>(QStringLiteral("recipientWarning"));
  QVERIFY(banner != nullptr);
  QVERIFY2(banner->text().contains(QStringLiteral("generation 1")) &&
               banner->text().contains(QStringLiteral("generation 2")) &&
               banner->text().contains(QStringLiteral("preselected")),
           qPrintable(banner->text()));
}

/**
 * @brief A signed list that verifies but was written for another folder is
 *        not this folder's: nothing is preselected (saving would sign it in
 *        here) and the banner says so. Only the authentic rollback keeps its
 *        recipients ticked.
 */
void tst_usersdialog::withSigningAListForAnotherFolderPreselectsNothing() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  QVERIFY(QDir(store.path()).mkpath(QStringLiteral("team")));
  const QString gpgIdFile =
      QDir(store.path()).filePath(QStringLiteral("team/.gpg-id"));
  const QString signedCopy =
      QDir(m_dir.path()).filePath(QStringLiteral("signed-bytes-root"));
  const QByteArray rootList = GpgIdGeneration::withHeader(
      1, QStringLiteral("."), "31850CF72D9CDDE9\n693A0AF3FA364E76\n");
  for (const QString &path : {gpgIdFile, signedCopy}) {
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(rootList);
  }
  {
    QFile sig(gpgIdFile + QStringLiteral(".sig"));
    QVERIFY(sig.open(QIODevice::WriteOnly));
    sig.write("sig");
  }
  AppSettings s = m_settings;
  s.gpgExecutable = writeVerifyingGpg(signedCopy);
  QVERIFY(!s.gpgExecutable.isEmpty());
  s.passSigningKey = kSigner;
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore + QStringLiteral("team/"));
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QVERIFY2(checkedNames(list).isEmpty(),
           qPrintable("preselected: " + checkedNames(list).join(", ")));
  auto *banner = dialog.findChild<QLabel *>(QStringLiteral("recipientWarning"));
  QVERIFY(banner != nullptr);
  QVERIFY2(
      banner->text().contains(QStringLiteral("\"team\"")) &&
          banner->text().contains(QStringLiteral("Nothing is preselected")),
      qPrintable(banner->text()));
}

/**
 * @brief A signed list without a header (pass, or QtPass before 2.0, wrote
 *        it) in a folder where this device has accepted a generation is not
 *        the authentic rollback the dialog recovers: with no folder line it
 *        may be any folder's old pair, planted from history. Nothing is
 *        preselected and the banner says so.
 */
void tst_usersdialog::
    withSigningAHeaderlessListBelowTheRecordPreselectsNothing() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QString gpgIdFile =
      QDir(store.path()).filePath(QStringLiteral(".gpg-id"));
  const QString signedCopy =
      QDir(m_dir.path()).filePath(QStringLiteral("signed-bytes-headerless"));
  const QByteArray headerless = "31850CF72D9CDDE9\n693A0AF3FA364E76\n";
  for (const QString &path : {gpgIdFile, signedCopy}) {
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(headerless);
  }
  {
    QFile sig(gpgIdFile + QStringLiteral(".sig"));
    QVERIFY(sig.open(QIODevice::WriteOnly));
    sig.write("sig");
  }
  // This device has accepted generation 2 of this folder's list before.
  QCOMPARE(
      GpgIdGeneration::accept(gpgIdFile,
                              GpgIdGeneration::withHeader(
                                  2, QStringLiteral("."), "31850CF72D9CDDE9\n"),
                              store.path()),
      GpgIdGeneration::Verdict::Accepted);
  AppSettings s = m_settings;
  s.gpgExecutable = writeVerifyingGpg(signedCopy);
  QVERIFY(!s.gpgExecutable.isEmpty());
  s.passSigningKey = kSigner;
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QVERIFY2(checkedNames(list).isEmpty(),
           qPrintable("preselected: " + checkedNames(list).join(", ")));
  auto *banner = dialog.findChild<QLabel *>(QStringLiteral("recipientWarning"));
  QVERIFY(banner != nullptr);
  QVERIFY2(
      banner->text().contains(QStringLiteral("no generation line")) &&
          banner->text().contains(QStringLiteral("generation 2")) &&
          banner->text().contains(QStringLiteral("Nothing is preselected")),
      qPrintable(banner->text()));
}

/**
 * @brief A signed list of the generation this device accepted last, with
 *        other bytes (two devices saved at once, or a swap): not the list
 *        this device knows, so nothing is preselected and the banner says
 *        why.
 */
void tst_usersdialog::withSigningAConflictingListPreselectsNothing() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QString gpgIdFile =
      QDir(store.path()).filePath(QStringLiteral(".gpg-id"));
  // This device accepted generation 2 with Alice alone.
  QCOMPARE(
      GpgIdGeneration::accept(gpgIdFile,
                              GpgIdGeneration::withHeader(
                                  2, QStringLiteral("."), "31850CF72D9CDDE9\n"),
                              store.path()),
      GpgIdGeneration::Verdict::Accepted);
  const AppSettings s = signedPair(
      gpgIdFile,
      GpgIdGeneration::withHeader(2, QStringLiteral("."),
                                  "31850CF72D9CDDE9\n693A0AF3FA364E76\n"),
      store.path());
  QVERIFY(!s.gpgExecutable.isEmpty());
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QVERIFY2(checkedNames(list).isEmpty(),
           qPrintable("preselected: " + checkedNames(list).join(", ")));
  auto *banner = dialog.findChild<QLabel *>(QStringLiteral("recipientWarning"));
  QVERIFY(banner != nullptr);
  QVERIFY2(
      banner->text().contains(QStringLiteral("same generation")) &&
          banner->text().contains(QStringLiteral("Nothing is preselected")),
      qPrintable(banner->text()));
}

/**
 * @brief A signed list whose header does not parse (a generation line that
 *        is not a number) is not a list to trust, signature or not: nothing
 *        is preselected and the banner says so.
 */
void tst_usersdialog::withSigningAMalformedHeaderPreselectsNothing() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QString gpgIdFile =
      QDir(store.path()).filePath(QStringLiteral(".gpg-id"));
  const AppSettings s =
      signedPair(gpgIdFile,
                 "# QtPass-GpgId-Generation: x\n# QtPass-GpgId-Folder: .\n"
                 "31850CF72D9CDDE9\n693A0AF3FA364E76\n",
                 store.path());
  QVERIFY(!s.gpgExecutable.isEmpty());
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QVERIFY2(checkedNames(list).isEmpty(),
           qPrintable("preselected: " + checkedNames(list).join(", ")));
  auto *banner = dialog.findChild<QLabel *>(QStringLiteral("recipientWarning"));
  QVERIFY(banner != nullptr);
  QVERIFY2(banner->text().contains(QStringLiteral("Nothing is preselected")) &&
               banner->text().contains(gpgIdFile),
           qPrintable(banner->text()));
}

/**
 * @brief With the generation record unreadable there is nothing to judge a
 *        signed list by, and a list that cannot be judged is not preselected
 *        (it would be signed in): nothing ticked, the record named.
 */
void tst_usersdialog::withSigningAnUnreadableRecordPreselectsNothing() {
#ifdef Q_OS_WIN
  QSKIP("permission bits do not stop reads on Windows");
#else
  if (::geteuid() == 0) {
    QSKIP("root reads anywhere");
  }
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QString gpgIdFile =
      QDir(store.path()).filePath(QStringLiteral(".gpg-id"));
  const AppSettings s = signedPair(
      gpgIdFile,
      GpgIdGeneration::withHeader(3, QStringLiteral("."),
                                  "31850CF72D9CDDE9\n693A0AF3FA364E76\n"),
      store.path());
  QVERIFY(!s.gpgExecutable.isEmpty());
  // Bring the record into being, then take it away.
  QCOMPARE(GpgIdGeneration::remembered(gpgIdFile), std::optional<qint64>(0));
  const QString recordFile = GpgIdGeneration::recordFile();
  const QString recordDir = QFileInfo(recordFile).absolutePath();
  QVERIFY(QDir().mkpath(recordDir));
  const QFile::Permissions was = QFile::permissions(recordDir);
  QVERIFY(QFile::setPermissions(recordDir, QFile::Permissions()));
  const auto restore =
      qScopeGuard([&] { QFile::setPermissions(recordDir, was); });
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QVERIFY2(checkedNames(list).isEmpty(),
           qPrintable("preselected: " + checkedNames(list).join(", ")));
  auto *banner = dialog.findChild<QLabel *>(QStringLiteral("recipientWarning"));
  QVERIFY(banner != nullptr);
  QVERIFY2(banner->text().contains(QStringLiteral("Nothing is preselected")) &&
               banner->text().contains(recordFile),
           qPrintable(banner->text()));
#endif
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

/// The one row whose text contains @p needle; the list is sorted, so rows
/// are found by name rather than by position.
auto itemWithText(QListWidget *list, const QString &needle)
    -> QListWidgetItem * {
  QListWidgetItem *found = nullptr;
  for (int i = 0; i < list->count(); ++i) {
    if (list->item(i)->text().contains(needle)) {
      if (found != nullptr) {
        return nullptr;
      }
      found = list->item(i);
    }
  }
  return found;
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

auto tst_usersdialog::writeGpg(const QString &name, const QString &body)
    -> QString {
  const QString gpg = QDir(m_dir.path()).filePath(name);
  QFile script(gpg);
  if (!script.open(QIODevice::WriteOnly | QIODevice::Text |
                   QIODevice::Truncate))
    return {};
  QTextStream out(&script);
  out << "#!/bin/sh\n" << body;
  out.flush();
  script.close();
  if (!script.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                             QFile::ExeOwner))
    return {};
  return gpg;
}

/**
 * @brief With no key in the keyring there is nothing to pick from: the
 *        dialog reports it in a critical box and rejects itself instead of
 *        opening on an empty list whose OK would write an empty .gpg-id.
 */
void tst_usersdialog::emptyKeyringRejectsTheDialogWithACriticalBox() {
  AppSettings s = m_settings;
  s.gpgExecutable =
      writeGpg(QStringLiteral("gpg-empty"), QStringLiteral("exit 0\n"));
  QVERIFY(!s.gpgExecutable.isEmpty());
  RecordingPass pass(s);

  int boxes = 0;
  QString title;
  QString text;
  QTimer poker;
  poker.setInterval(20);
  QObject::connect(&poker, &QTimer::timeout, [&]() {
    auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
    if (box == nullptr || box->property("tst_driven").toBool()) {
      return;
    }
    box->setProperty("tst_driven", true);
    ++boxes;
    title = box->windowTitle();
    text = box->text();
    box->accept();
  });
  poker.start();
  UsersDialog dialog(&pass, s, s.passStore);
  poker.stop();

  QCOMPARE(boxes, 1);
#ifndef Q_OS_MACOS
  // QMessageBox shows no window title on macOS and Qt leaves it empty there.
  QCOMPARE(title, QStringLiteral("Keylist missing"));
#endif
  QVERIFY2(text.contains(QStringLiteral("Could not fetch list")),
           qPrintable(text));
  QCOMPARE(dialog.result(), static_cast<int>(QDialog::Rejected));
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QCOMPARE(list->count(), 0);
  QVERIFY2(!dialog.hasSelection(), "an empty keyring selects nothing");
  dialog.accept();
  QVERIFY2(pass.initCalls.isEmpty(), "OK must not run Init on an empty list");
}

/**
 * @brief A recipient in .gpg-id that gpg does not know must not silently
 *        drop out of the list (saving would then re-encrypt the store
 *        without them): it is listed as a ticked placeholder that says the
 *        key is missing. Being unusable it hides behind "Show unusable
 *        keys" and carries the [INVALID] badge once shown.
 */
void tst_usersdialog::unknownRecipientIsListedAsNotFoundAndTicked() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
  gpgId.write("13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9\nDEADBEEFDEADBEEF\n");
  gpgId.close();
  AppSettings s = m_settings;
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  auto *unusable = dialog.findChild<QCheckBox *>(QStringLiteral("checkBox"));
  QVERIFY(list != nullptr && unusable != nullptr);
  QVERIFY(!unusable->isChecked());

  // Hidden while unusable keys are hidden, but part of the selection.
  QCOMPARE(list->count(), 2);
  QCOMPARE(checkedNames(list), QStringList{QStringLiteral("Alice")});
  QCOMPARE(
      enabledIds(dialog.selectedUsers()),
      (QStringList{QStringLiteral("13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9"),
                   QStringLiteral("DEADBEEFDEADBEEF")}));

  unusable->click();
  QVERIFY(unusable->isChecked());
  QCOMPARE(list->count(), 3);
  QListWidgetItem *placeholder =
      itemWithText(list, QStringLiteral("DEADBEEFDEADBEEF"));
  QVERIFY2(placeholder != nullptr, "the unknown recipient must be listed");
  QVERIFY2(placeholder->text().startsWith(QStringLiteral("[INVALID] ")),
           qPrintable(placeholder->text()));
  QVERIFY2(
      placeholder->text().contains(QStringLiteral("Key not found in keyring")),
      qPrintable(placeholder->text()));
  QVERIFY2(placeholder->text().contains(QStringLiteral("DEADBEEFDEADBEEF")),
           qPrintable(placeholder->text()));
  QCOMPARE(placeholder->checkState(), Qt::Checked);
  QCOMPARE(placeholder->background().color(), QColor(Qt::darkRed));

  // Unticking it is how the user drops the lost key from the store.
  placeholder->setCheckState(Qt::Unchecked);
  QCOMPARE(
      enabledIds(dialog.selectedUsers()),
      QStringList{QStringLiteral("13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9")});
}

namespace {
/// Three keys as gpg --with-colons prints them: fully valid but past its
/// expiry (f, 2023), marginally valid (m) and revoked (r). The fingerprints
/// are the key ids padded to 40 hex digits so the parser adopts them.
const char kMixedListing[] =
    "pub:f:4096:1:AAAAAAAAAAAAAAA1:1600000000:1700000000::f:::escarESCA::::::"
    "23::0:\n"
    "fpr:::::::::000000000000000000000000AAAAAAAAAAAAAAA1:\n"
    "uid:f::::1600000000::X::Expired <expired@example.org>::::::::::0:\n"
    "pub:m:4096:1:BBBBBBBBBBBBBBB2:1600000000:::m:::escarESCA::::::23::0:\n"
    "fpr:::::::::000000000000000000000000BBBBBBBBBBBBBBB2:\n"
    "uid:m::::1600000000::X::Marginal <marginal@example.org>::::::::::0:\n"
    "pub:r:4096:1:CCCCCCCCCCCCCCC3:1600000000:::r:::escarESCA::::::23::0:\n"
    "fpr:::::::::000000000000000000000000CCCCCCCCCCCCCCC3:\n"
    "uid:r::::1600000000::X::Revoked <revoked@example.org>::::::::::0:\n";
} // namespace

/**
 * @brief gpg refuses to encrypt to expired and revoked keys, so the dialog
 *        hides them until "Show unusable keys" is ticked and then marks
 *        them [EXPIRED] / [INVALID] in white on dark red, with the expiry
 *        date in the row; a marginally valid key is always listed but
 *        badged [PARTIAL] on dark yellow.
 */
void tst_usersdialog::unusableKeysAreHiddenUntilAskedForAndThenBadged() {
  AppSettings s = m_settings;
  s.gpgExecutable = writeGpg(
      QStringLiteral("gpg-mixed"),
      QStringLiteral("case \"$*\" in *--list-secret-keys*) exit 0 ;; esac\n"
                     "cat <<'LISTING'\n") +
          QString::fromLatin1(kMixedListing) +
          QStringLiteral("LISTING\nexit 0\n"));
  QVERIFY(!s.gpgExecutable.isEmpty());
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  auto *unusable = dialog.findChild<QCheckBox *>(QStringLiteral("checkBox"));
  QVERIFY(list != nullptr && unusable != nullptr);

  QCOMPARE(list->count(), 1);
  QVERIFY2(
      list->item(0)->text().startsWith(QStringLiteral("[PARTIAL] Marginal")),
      qPrintable(list->item(0)->text()));
  QCOMPARE(list->item(0)->background().color(), QColor(Qt::darkYellow));
  QCOMPARE(list->item(0)->foreground().color(), QColor(Qt::white));

  unusable->click();
  QCOMPARE(list->count(), 3);
  QListWidgetItem *expired = itemWithText(list, QStringLiteral("Expired"));
  QVERIFY(expired != nullptr);
  QVERIFY2(expired->text().startsWith(QStringLiteral("[EXPIRED] Expired")),
           qPrintable(expired->text()));
  QVERIFY2(expired->text().contains(QStringLiteral("expires")),
           qPrintable(expired->text()));
  QVERIFY2(expired->text().contains(QStringLiteral("created")),
           qPrintable(expired->text()));
  QCOMPARE(expired->background().color(), QColor(Qt::darkRed));
  QCOMPARE(expired->foreground().color(), QColor(Qt::white));
  QListWidgetItem *revoked = itemWithText(list, QStringLiteral("Revoked"));
  QVERIFY(revoked != nullptr);
  QVERIFY2(revoked->text().startsWith(QStringLiteral("[INVALID] Revoked")),
           qPrintable(revoked->text()));
  QVERIFY2(!revoked->text().contains(QStringLiteral("expires")),
           "a key without expiry shows none");
  QCOMPARE(revoked->background().color(), QColor(Qt::darkRed));

  // Unticking hides them again, and the filter still applies on top.
  unusable->click();
  QCOMPARE(list->count(), 1);
  auto *filter = dialog.findChild<QLineEdit *>(QStringLiteral("lineEdit"));
  QVERIFY(filter != nullptr);
  filter->setText(QStringLiteral("expired"));
  QCOMPARE(list->count(), 0);
  unusable->click();
  QCOMPARE(list->count(), 1);
  QVERIFY2(list->item(0)->text().contains(QStringLiteral("Expired")),
           qPrintable(list->item(0)->text()));
}

/**
 * @brief Rows carry their m_userList index in Qt::UserRole. A row whose
 *        index points outside the list (a stale row from a rebuild) must
 *        not write through to some other key or crash: it is logged and
 *        ignored, and the selection stays what it was.
 */
void tst_usersdialog::rowWithAnOutOfRangeIndexChangesNoSelection() {
  RecordingPass pass(m_settings);
  UsersDialog dialog(&pass, m_settings, m_settings.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  auto *box = dialog.findChild<QDialogButtonBox *>(QStringLiteral("buttonBox"));
  QVERIFY(list != nullptr && box != nullptr);
  QCOMPARE(list->count(), 2);

  // Built detached so that only the tick below reaches itemChange(); the
  // list takes ownership on addItem(). Exactly one warning is expected.
  auto *stale = new QListWidgetItem(QStringLiteral("Stale"));
  stale->setData(Qt::UserRole, QVariant::fromValue(42));
  stale->setCheckState(Qt::Unchecked);
  list->addItem(stale);
  QCOMPARE(list->count(), 3);
  QTest::ignoreMessage(
      QtWarningMsg, QRegularExpression(QStringLiteral(
                        "user index out of range: 42 valid range is \\[0, 1")));
  stale->setCheckState(Qt::Checked);
  QVERIFY2(!dialog.hasSelection(), "a stale row must not tick any key");
  QVERIFY2(!box->button(QDialogButtonBox::Ok)->isEnabled(),
           "OK stays disabled: nothing real is selected");
  QVERIFY2(enabledIds(dialog.selectedUsers()).isEmpty(),
           qPrintable(enabledIds(dialog.selectedUsers()).join(", ")));

  // A real row still works next to it.
  list->item(0)->setCheckState(Qt::Checked);
  QVERIFY(dialog.hasSelection());
  QVERIFY(box->button(QDialogButtonBox::Ok)->isEnabled());
}

/**
 * @brief Keys whose secret half is in the keyring (the ones this user can
 *        decrypt with) are marked: bold, and in the palette's link colour when
 *        no status badge already claims the foreground. A key without a secret
 *        half is left in the plain font and colour.
 */
void tst_usersdialog::ownKeysAreBoldAndInLinkColour() {
  AppSettings s = m_settings;
  // --list-secret-keys answers with Alice alone; --list-keys with both.
  s.gpgExecutable =
      writeGpg(QStringLiteral("gpg-secret"),
               QStringLiteral("all() { cat <<'LISTING'\n") +
                   QString::fromLatin1(kColonListing) +
                   QStringLiteral("LISTING\n}\n"
                                  "case \"$*\" in\n"
                                  "  *--list-secret-keys*) all | head -3 ;;\n"
                                  "  *) all ;;\n"
                                  "esac\nexit 0\n"));
  QVERIFY(!s.gpgExecutable.isEmpty());
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QCOMPARE(list->count(), 2);

  QListWidgetItem *alice = itemWithText(list, QStringLiteral("Alice"));
  QListWidgetItem *bob = itemWithText(list, QStringLiteral("Bob"));
  QVERIFY(alice != nullptr && bob != nullptr);
  QVERIFY2(alice->font().bold(), "an own key is shown bold");
  QCOMPARE(alice->foreground().color(),
           QApplication::palette().color(QPalette::Link));
  QVERIFY2(!alice->text().startsWith(QLatin1Char('[')),
           qPrintable(alice->text() + " must carry no badge"));
  QVERIFY2(!bob->font().bold(), "a key without its secret half is not bold");
  QVERIFY2(bob->foreground().color() !=
               QApplication::palette().color(QPalette::Link),
           "a key without its secret half keeps the plain colour");
  QCOMPARE(bob->foreground().style(), Qt::NoBrush);

  // The marker is presentation only: it ticks nothing.
  QVERIFY2(checkedNames(list).isEmpty(),
           qPrintable(checkedNames(list).join(", ")));
  const QList<UserInfo> users = dialog.selectedUsers();
  int secret = 0;
  for (const UserInfo &u : users) {
    if (u.have_secret) {
      ++secret;
      QCOMPARE(u.key_id,
               QStringLiteral("13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9"));
    }
  }
  QCOMPARE(secret, 1);
}

namespace {
/// The stand-in gpg for the import tests: Alice alone until --import has
/// been run, Alice and Bob afterwards; --import reports Bob's fingerprint.
const char kImportingGpg[] =
    "case \"$*\" in\n"
    "  *--import*) cat > /dev/null; touch \"$0.imported\"; "
    "printf '[GNUPG:] IMPORT_OK 1 "
    "4EF2550F79F4E9E68B09F71D693A0AF3FA364E76\\n'; "
    "exit 0 ;;\n"
    "  *--list-secret-keys*) exit 0 ;;\n"
    "esac\n"
    "all() { cat <<'LISTING'\n";
const char kImportingGpgTail[] =
    "LISTING\n}\n"
    "if [ -f \"$0.imported\" ]; then all; else all | head -3; fi\n"
    "exit 0\n";

/**
 * @brief Drives the ImportKeyDialog that importKeyButton opens: pastes
 *        @p armored and clicks Import (or cancels when @p armored is empty),
 *        then accepts the message box the import shows.
 * @return How many import dialogs were seen; -1 without an Import button.
 */
auto driveImport(UsersDialog *dialog, const QString &armored) -> int {
  auto *button =
      dialog->findChild<QPushButton *>(QStringLiteral("importKeyButton"));
  if (button == nullptr) {
    return -1;
  }
  int dialogs = 0;
  QTimer poker;
  poker.setInterval(20);
  QObject::connect(&poker, &QTimer::timeout, [&]() {
    QWidget *modal = QApplication::activeModalWidget();
    if (modal == nullptr || modal->property("tst_driven").toBool()) {
      return;
    }
    if (auto *import = qobject_cast<ImportKeyDialog *>(modal)) {
      modal->setProperty("tst_driven", true);
      ++dialogs;
      if (armored.isEmpty()) {
        import->reject();
        return;
      }
      auto *input =
          import->findChild<QPlainTextEdit *>(QStringLiteral("inputTextEdit"));
      auto *go =
          import->findChild<QPushButton *>(QStringLiteral("importButton"));
      if (input == nullptr || go == nullptr) {
        import->reject();
        return;
      }
      input->setPlainText(armored);
      // Not from inside this timeout: a nested exec() in here would keep
      // this timer from firing again, and the import's message box would
      // never be accepted.
      QTimer::singleShot(0, go, &QPushButton::click);
      return;
    }
    if (auto *box = qobject_cast<QMessageBox *>(modal)) {
      modal->setProperty("tst_driven", true);
      box->accept();
    }
  });
  poker.start();
  // click() runs the slot, and thus the nested modal loops, synchronously.
  button->click();
  poker.stop();
  return dialogs;
}
} // namespace

/**
 * @brief After a key is imported through the Import button the list is
 *        reloaded from gpg, the filter is cleared so the new key is
 *        visible, and the new key becomes the current row (matched on its
 *        stored key id, whether gpg reported a long id or a fingerprint).
 */
void tst_usersdialog::importingAKeyRefreshesTheListAndSelectsIt() {
  AppSettings s = m_settings;
  s.gpgExecutable = writeGpg(QStringLiteral("gpg-importing"),
                             QString::fromLatin1(kImportingGpg) +
                                 QString::fromLatin1(kColonListing) +
                                 QString::fromLatin1(kImportingGpgTail));
  QVERIFY(!s.gpgExecutable.isEmpty());
  QFile::remove(s.gpgExecutable + QStringLiteral(".imported"));
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  auto *filter = dialog.findChild<QLineEdit *>(QStringLiteral("lineEdit"));
  QVERIFY(list != nullptr && filter != nullptr);
  QCOMPARE(list->count(), 1);
  QVERIFY(list->item(0)->text().startsWith(QStringLiteral("Alice")));
  // The reload after an import rebuilds m_userList from gpg, so ticks made
  // before importing do not survive it. Alice is left unticked here so the
  // test pins the #1167 behaviour (refresh, cleared filter, current row) and
  // not that loss.
  filter->setText(QStringLiteral("nobody"));
  QCOMPARE(list->count(), 0);

  QCOMPARE(driveImport(&dialog, QStringLiteral(
                                    "-----BEGIN PGP PUBLIC KEY BLOCK-----\n"
                                    "bob\n-----END PGP PUBLIC KEY BLOCK-----")),
           1);

  QVERIFY2(filter->text().isEmpty(), qPrintable(filter->text()));
  QCOMPARE(list->count(), 2);
  QVERIFY2(list->currentItem() != nullptr, "the imported key must be current");
  QVERIFY2(list->currentItem()->text().startsWith(QStringLiteral("Bob")),
           qPrintable(list->currentItem()->text()));
  QVERIFY2(list->currentItem()->checkState() == Qt::Unchecked,
           "importing does not tick the key; the user decides");
  QVERIFY2(enabledIds(dialog.selectedUsers()).isEmpty(),
           qPrintable(enabledIds(dialog.selectedUsers()).join(", ")));
  QCOMPARE(dialog.selectedUsers().size(), 2);
  QVERIFY2(QFile::exists(s.gpgExecutable + QStringLiteral(".imported")),
           "gpg --import must have run");
}

/**
 * @brief Cancelling the import dialog leaves the list, the filter and the
 *        selection exactly as they were: no reload, no cleared filter.
 */
void tst_usersdialog::cancellingTheImportLeavesTheListAlone() {
  RecordingPass pass(m_settings);
  UsersDialog dialog(&pass, m_settings, m_settings.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  auto *filter = dialog.findChild<QLineEdit *>(QStringLiteral("lineEdit"));
  QVERIFY(list != nullptr && filter != nullptr);
  filter->setText(QStringLiteral("bob"));
  QCOMPARE(list->count(), 1);
  list->item(0)->setCheckState(Qt::Checked);
  QListWidgetItem *const bob = list->item(0);
  const QListWidgetItem *const currentBefore = list->currentItem();

  QCOMPARE(driveImport(&dialog, QString()), 1);

  QCOMPARE(filter->text(), QStringLiteral("bob"));
  QCOMPARE(list->count(), 1);
  QVERIFY2(list->item(0) == bob, "the list must not have been rebuilt");
  QCOMPARE(list->item(0)->checkState(), Qt::Checked);
  QVERIFY2(list->currentItem() == currentBefore,
           "nothing was imported, so the current row is untouched");
  QCOMPARE(
      enabledIds(dialog.selectedUsers()),
      QStringList{QStringLiteral("4EF2550F79F4E9E68B09F71D693A0AF3FA364E76")});
}

QTEST_MAIN(tst_usersdialog)
#include "tst_usersdialog.moc"
