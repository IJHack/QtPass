// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
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

/**
 * A Pass whose Init() runs @p beforeWrite (a pull landing late) and then
 * the point-of-write check, as the backends do.
 */
class LateChangePass : public RecordingPass {
public:
  using RecordingPass::RecordingPass;

  void Init(QString path, const QList<UserInfo> &users) override {
    if (beforeWrite) {
      beforeWrite();
    }
    if (refuseChangedList()) {
      return;
    }
    RecordingPass::Init(path, users);
  }

  std::function<void()> beforeWrite;
};

} // namespace

class tst_usersdialog : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void newStoreStartsWithNothingSelected();
  void existingStorePreselectsItsRecipients();
  void foldersBelowWithTheirOwnListAreNamed();
  void aWarningQuotingLongFolderNamesDoesNotSizeTheDialog();
  void foldersBelowThatCannotBeReadAreNamed();
  void withSigningATamperedListPreselectsNothing();
  void withSigningAnOlderVerifiedListIsPreselectedWithAWarning();
  void withSigningAListForAnotherFolderPreselectsNothing();
  void withSigningAHeaderlessListBelowTheRecordPreselectsNothing();
  void withSigningAMalformedHeaderPreselectsNothing();
  void withSigningAConflictingListPreselectsNothing();
  void withSigningAnUnreadableRecordPreselectsNothing();
  void folderOutsideTheStoreDoesNotInheritItsRecipients();
  void acceptRefusesAListChangedMeanwhile();
  void acceptRunsInitByDefault();
  void acceptWithoutSelectionDoesNothing();
  void acceptWithoutInitOnlyCollectsTheSelection();
  void togglingAFilteredRowEnablesTheRightKey();
  void selectionSurvivesFilteringAndEscapeClearsTheFilter();
  void searchListsUserIdsWithASlash();
  void aUserIdCannotDrawALineOfItsOwn();
  void anUnknownRecipientCannotDrawALineOfItsOwn();
  void aFolderNameCannotTurnTheWarningAround();
  void emptyKeyringRejectsTheDialogWithACriticalBox();
  void unknownRecipientIsListedAsNotFoundAndTicked();
  void anOverlongRecipientIsShownShortAndKeptWhole();
  void manyUnknownRecipientsOpenAtOnceAndSorted();
  void unreadableRecipientLineLeavesTheOthersTicked();
  void unreadableLineStaysMissingWhenAResolvedUidContainsIt();
  void fallbackAsksAgainForTheReadableLinesInOneCall();
  void preselectsWhatEncryptionReachesNotWhatTheSearchFinds();
  void aSharedSubkeyTicksOnlyTheKeyTheLineNames();
  void aListWithAGoneKeyStillTicksOnlyWhatEachLineReaches();
  void perLineProbesTakeAddressesFirstAndKeepTheRest();
  void aLineNamingASubkeyOrAnotherUidTicksItsKey();
  void unusualLinesPastTheLookupCapAreListedUnticked();
  void recipientByShortOrLongIdIsNotListedAsNotFound();
  void v6FingerprintMatchesHeadIdsButNotTailIds();
  void unusableKeysAreHiddenUntilAskedForAndThenBadged();
  void unusableRecipientsAreShownAndCanBeRemoved();
  void rowWithAnOutOfRangeIndexChangesNoSelection();
  void ownKeysAreBoldAndInLinkColour();
  void importingAKeyRefreshesTheListAndSelectsIt();
  void importingAV6KeyRefreshesTheListAndSelectsIt();
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
  // A key the keyring does not have: gpg lists nothing for it.
  script.write("*DEADBEEFDEADBEEF*) ;;\n");
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
 * @brief The rows ticked for a folder are the keys gpg's -r reaches for its
 *        list, not every key a keyring search finds for it: a colleague's
 *        key with a user ID that contains a recipient's address is found by
 *        the search but not ticked, so OK does not add it (scan finding
 *        ANT-2026-8Z9SUHAD).
 */
void tst_usersdialog::preselectsWhatEncryptionReachesNotWhatTheSearchFinds() {
  // Mallory's key carries a second user ID "Alice <alice@example.org>": the
  // search for the line finds both keys; the probe reaches Alice's alone.
  const QString listing = QStringLiteral(
      "pub:u:4096:1:31850CF72D9CDDE9:1774947438:::u:::escarESCA::::::23::0:\n"
      "fpr:::::::::13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9:\n"
      "uid:u::::1774947438::CBF23008234AA5F88824CE76140F482FAE34923E::Alice "
      "<alice@example.org>::::::::::0:\n"
      "pub:f:4096:1:693A0AF3FA364E76:1775005968:::f:::escarESCA::::::23::0:\n"
      "fpr:::::::::4EF2550F79F4E9E68B09F71D693A0AF3FA364E76:\n"
      "uid:f::::1775005968::8AA011711F27F6E08DF71653718C299A13B323A0::Mallory "
      "<mallory@example.org>::::::::::0:\n"
      "uid:f::::1775005968::9AA011711F27F6E08DF71653718C299A13B323A0::Alice "
      "<alice@example.org>::::::::::0:\n");
  AppSettings s = m_settings;
  s.gpgExecutable = writeGpg(
      QStringLiteral("gpg-lookalike"),
      QStringLiteral("case \"$*\" in\n"
                     "  *--list-secret-keys*) exit 0 ;;\n"
                     "  *--encrypt*) cat >/dev/null; exit 0 ;;\n"
                     "  *--list-only*) echo '[GNUPG:] ENC_TO 31850CF72D9CDDE9 "
                     "1 0'; exit 0 ;;\n"
                     "esac\n"
                     "cat <<'LISTING'\n") +
          listing + QStringLiteral("LISTING\nexit 0\n"));
  QVERIFY(!s.gpgExecutable.isEmpty());
  QTemporaryDir store;
  QVERIFY(store.isValid());
  QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
  gpgId.write("alice@example.org\n");
  gpgId.close();
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QCOMPARE(list->count(), 2);
  QCOMPARE(checkedNames(list), QStringList{QStringLiteral("Alice")});
}

/**
 * @brief A subkey bound to two keys (Mallory binding Alice's encryption
 *        subkey to her own key) is reached by the probe, which cannot tell
 *        which key gpg chose. A line naming Alice's key by fingerprint
 *        ticks Alice alone; an address line is kept as written, with
 *        neither key ticked (scan finding ANT-2026-8Z9SUHAD).
 */
void tst_usersdialog::aSharedSubkeyTicksOnlyTheKeyTheLineNames() {
  const QString listing = QStringLiteral(
      "pub:u:4096:1:31850CF72D9CDDE9:1774947438:::u:::escarESCA::::::23::0:\n"
      "fpr:::::::::13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9:\n"
      "uid:u::::1774947438::CBF23008234AA5F88824CE76140F482FAE34923E::Alice "
      "<alice@example.org>::::::::::0:\n"
      "sub:u:4096:1:1111222233334444:1774947438::::::e::::::23:\n"
      "fpr:::::::::AAAABBBBCCCCDDDDEEEEFFFF1111222233334444:\n"
      "pub:f:4096:1:693A0AF3FA364E76:1775005968:::f:::escarESCA::::::23::0:\n"
      "fpr:::::::::4EF2550F79F4E9E68B09F71D693A0AF3FA364E76:\n"
      "uid:f::::1775005968::8AA011711F27F6E08DF71653718C299A13B323A0::Mallory "
      "<mallory@example.org>::::::::::0:\n"
      "sub:u:4096:1:1111222233334444:1774947438::::::e::::::23:\n"
      "fpr:::::::::AAAABBBBCCCCDDDDEEEEFFFF1111222233334444:\n");
  const auto open = [&](const QByteArray &line, QStringList *checked,
                        QStringList *enabled) {
    AppSettings s = m_settings;
    s.gpgExecutable =
        writeGpg(QStringLiteral("gpg-shared-subkey"),
                 QStringLiteral("case \"$*\" in\n"
                                "  *--list-secret-keys*) exit 0 ;;\n"
                                "  *--encrypt*) cat >/dev/null; exit 0 ;;\n"
                                "  *--list-only*) echo '[GNUPG:] ENC_TO "
                                "1111222233334444 1 0'; exit 0 ;;\n"
                                "esac\n"
                                "cat <<'LISTING'\n") +
                     listing + QStringLiteral("LISTING\nexit 0\n"));
    QTemporaryDir store;
    QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
    if (s.gpgExecutable.isEmpty() || !store.isValid() ||
        !gpgId.open(QIODevice::WriteOnly | QIODevice::Text)) {
      return false;
    }
    gpgId.write(line + '\n');
    gpgId.close();
    s.passStore = store.path() + QLatin1Char('/');
    RecordingPass pass(s);
    UsersDialog dialog(&pass, s, s.passStore);
    auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
    if (list == nullptr) {
      return false;
    }
    *checked = checkedNames(list);
    enabled->clear();
    for (const UserInfo &user : dialog.selectedUsers()) {
      if (user.enabled) {
        *enabled << user.key_id;
      }
    }
    return true;
  };
  QStringList checked;
  QStringList enabled;
  QVERIFY(open("13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9", &checked, &enabled));
  QCOMPARE(checked, QStringList{QStringLiteral("Alice")});
  QCOMPARE(enabled, QStringList{QStringLiteral(
                        "13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9")});
  QVERIFY(open("alice@example.org", &checked, &enabled));
  QVERIFY2(!checked.contains(QStringLiteral("Alice")) &&
               !checked.contains(QStringLiteral("Mallory")),
           qPrintable(checked.join(',')));
  QCOMPARE(enabled, QStringList{QStringLiteral("alice@example.org")});
}

/**
 * @brief A list gpg cannot encrypt to as a whole, because one line names a
 *        key that is gone (a list edited after someone left), is probed line
 *        by line: the address line ticks the key it reaches and not
 *        Mallory's look-alike, and the gone line is kept as written (scan
 *        finding ANT-2026-8Z9SUHAD, fallback).
 */
void tst_usersdialog::aListWithAGoneKeyStillTicksOnlyWhatEachLineReaches() {
  const QString listing = QStringLiteral(
      "pub:u:4096:1:31850CF72D9CDDE9:1774947438:::u:::escarESCA::::::23::0:\n"
      "fpr:::::::::13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9:\n"
      "uid:u::::1774947438::CBF23008234AA5F88824CE76140F482FAE34923E::Alice "
      "<alice@example.org>::::::::::0:\n"
      "pub:f:4096:1:693A0AF3FA364E76:1775005968:::f:::escarESCA::::::23::0:\n"
      "fpr:::::::::4EF2550F79F4E9E68B09F71D693A0AF3FA364E76:\n"
      "uid:f::::1775005968::8AA011711F27F6E08DF71653718C299A13B323A0::Mallory "
      "<mallory@example.org>::::::::::0:\n"
      "uid:f::::1775005968::9AA011711F27F6E08DF71653718C299A13B323A0::Alice "
      "<alice@example.org>::::::::::0:\n");
  const QString gone = QStringLiteral("DEAD") + QString(36, QLatin1Char('0'));
  AppSettings s = m_settings;
  // gpg cannot encrypt to the gone key, so neither to the whole list; the
  // address line alone reaches Alice's key.
  s.gpgExecutable = writeGpg(
      QStringLiteral("gpg-gone-key"),
      QStringLiteral("case \"$*\" in\n"
                     "  *--list-secret-keys*) exit 0 ;;\n"
                     "  *--encrypt*DEAD*) exit 2 ;;\n"
                     "  *--encrypt*) cat >/dev/null; exit 0 ;;\n"
                     "  *--list-only*) echo '[GNUPG:] ENC_TO 31850CF72D9CDDE9 "
                     "1 0'; exit 0 ;;\n"
                     "esac\n"
                     "cat <<'LISTING'\n") +
          listing + QStringLiteral("LISTING\nexit 0\n"));
  QVERIFY(!s.gpgExecutable.isEmpty());
  QTemporaryDir store;
  QVERIFY(store.isValid());
  QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
  gpgId.write("alice@example.org\n" + gone.toLatin1() + '\n');
  gpgId.close();
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  const QStringList checked = checkedNames(list);
  QVERIFY2(checked.contains(QStringLiteral("Alice")) &&
               !checked.contains(QStringLiteral("Mallory")),
           qPrintable(checked.join(',')));
  QStringList enabled;
  for (const UserInfo &user : dialog.selectedUsers()) {
    if (user.enabled) {
      enabled << user.key_id;
    }
  }
  enabled.sort();
  QCOMPARE(
      enabled,
      QStringList(
          {QStringLiteral("13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9"), gone}));
}

/**
 * @brief When gpg refuses the whole list, the per-line probes go to
 *        address and name lines first, where look-alike keys come from, and
 *        a line past the ten probed is kept as written rather than answered
 *        by the keyring search (scan finding ANT-2026-8Z9SUHAD, fallback).
 */
void tst_usersdialog::perLineProbesTakeAddressesFirstAndKeepTheRest() {
  const QString listing = QStringLiteral(
      "pub:u:4096:1:31850CF72D9CDDE9:1774947438:::u:::escarESCA::::::23::0:\n"
      "fpr:::::::::13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9:\n"
      "uid:u::::1774947438::CBF23008234AA5F88824CE76140F482FAE34923E::Alice "
      "<alice@example.org>::::::::::0:\n"
      "pub:f:4096:1:693A0AF3FA364E76:1775005968:::f:::escarESCA::::::23::0:\n"
      "fpr:::::::::4EF2550F79F4E9E68B09F71D693A0AF3FA364E76:\n"
      "uid:f::::1775005968::8AA011711F27F6E08DF71653718C299A13B323A0::Mallory "
      "<mallory@example.org>::::::::::0:\n"
      "uid:f::::1775005968::9AA011711F27F6E08DF71653718C299A13B323A0::Alice "
      "<alice@example.org>::::::::::0:\n");
  AppSettings s = m_settings;
  // Eleven keys that are gone, then the address line: gpg encrypts to the
  // address alone and to nothing that names a gone key; the search finds
  // both keys for anything.
  s.gpgExecutable = writeGpg(
      QStringLiteral("gpg-many-gone"),
      QStringLiteral("case \"$*\" in\n"
                     "  *--list-secret-keys*) exit 0 ;;\n"
                     "  *--encrypt*DEAD*) exit 2 ;;\n"
                     "  *--encrypt*) cat >/dev/null; exit 0 ;;\n"
                     "  *--list-only*) echo '[GNUPG:] ENC_TO 31850CF72D9CDDE9 "
                     "1 0'; exit 0 ;;\n"
                     "esac\n"
                     "cat <<'LISTING'\n") +
          listing + QStringLiteral("LISTING\nexit 0\n"));
  QVERIFY(!s.gpgExecutable.isEmpty());
  QStringList gone;
  QByteArray body;
  for (int i = 1; i <= 11; ++i) {
    gone << QStringLiteral("DEAD%1").arg(i, 36, 16, QLatin1Char('0')).toUpper();
    body += gone.last().toLatin1() + '\n';
  }
  body += "alice@example.org\n";
  QTemporaryDir store;
  QVERIFY(store.isValid());
  QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
  gpgId.write(body);
  gpgId.close();
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  const QStringList checked = checkedNames(list);
  QVERIFY2(checked.contains(QStringLiteral("Alice")) &&
               !checked.contains(QStringLiteral("Mallory")),
           qPrintable(checked.join(',')));
  QStringList enabled;
  for (const UserInfo &user : dialog.selectedUsers()) {
    if (user.enabled) {
      enabled << user.key_id;
    }
  }
  QStringList expected = gone;
  expected << QStringLiteral("13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9");
  enabled.sort();
  expected.sort();
  QCOMPARE(enabled, expected);
}

/**
 * @brief Saving a folder's recipients changes its own list only. A folder
 *        below with a list of its own keeps its recipients, someone removed
 *        here included, so the dialog names those folders. Hidden folders
 *        are not walked (re-encryption does not walk them either), and a
 *        folder with nothing below says nothing.
 */
void tst_usersdialog::foldersBelowWithTheirOwnListAreNamed() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QDir root(store.path());
  for (const QString &folder :
       {QStringLiteral("."), QStringLiteral("work"),
        QStringLiteral("work/bank"), QStringLiteral(".hidden"),
        QStringLiteral("plain/inner")}) {
    QVERIFY(root.mkpath(folder));
    if (folder == QLatin1String("plain/inner")) {
      continue;
    }
    QFile gpgId(
        QDir(root.filePath(folder)).filePath(QStringLiteral(".gpg-id")));
    QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
    gpgId.write("31850CF72D9CDDE9\n");
  }
  AppSettings s = m_settings;
  s.passStore = store.path() + QLatin1Char('/');
  const auto banner = [](const UsersDialog &dialog) {
    auto *label =
        dialog.findChild<QLabel *>(QStringLiteral("nestedListsWarning"));
    return label != nullptr ? label->text() : QString();
  };

  {
    RecordingPass pass(s);
    UsersDialog dialog(&pass, s, s.passStore);
    const QString text = banner(dialog);
    QVERIFY2(text.startsWith(QStringLiteral("2 folder")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("work, ")) &&
                 text.contains(
                     QDir::toNativeSeparators(QStringLiteral("work/bank"))),
             qPrintable(text));
    QVERIFY2(!text.contains(QStringLiteral("hidden")), qPrintable(text));
    QVERIFY2(!text.contains(QStringLiteral("plain")), qPrintable(text));
    // The list itself is not a warning about the folder's own recipients.
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("recipientWarning")) ==
            nullptr);
  }
  {
    RecordingPass pass(s);
    UsersDialog dialog(&pass, s, root.filePath(QStringLiteral("work")));
    const QString text = banner(dialog);
    QVERIFY2(text.startsWith(QStringLiteral("1 folder")) &&
                 text.endsWith(QStringLiteral(": bank")),
             qPrintable(text));
  }
  {
    RecordingPass pass(s);
    UsersDialog dialog(&pass, s, root.filePath(QStringLiteral("plain")));
    QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("nestedListsWarning")) ==
            nullptr);
  }
}

/**
 * @brief Folder names a co-writer chose, long, of many short words and with
 * line breaks, do not size the dialog by the warning that quotes them: it
 * scrolls in a bounded area, its explanation whole (scan finding
 * ANT-2026-MRD4LUFB).
 */
void tst_usersdialog::aWarningQuotingLongFolderNamesDoesNotSizeTheDialog() {
#ifdef Q_OS_WIN
  QSKIP("folder names with line breaks need a POSIX file system");
#else
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QDir root(store.path());
  const QString words = QStringLiteral("w ").repeated(20);
  QStringList folders{QStringLiteral(".")};
  for (int i = 0; i < 5; ++i) {
    folders << QStringLiteral("%1%2").arg(i).arg(words) +
                   QString(200, QLatin1Char('\n')) + QStringLiteral("end");
  }
  for (const QString &folder : std::as_const(folders)) {
    QVERIFY(root.mkpath(folder));
    QFile gpgId(
        QDir(root.filePath(folder)).filePath(QStringLiteral(".gpg-id")));
    QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
    gpgId.write("31850CF72D9CDDE9\n");
  }
  AppSettings s = m_settings;
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *label =
      dialog.findChild<QLabel *>(QStringLiteral("nestedListsWarning"));
  QVERIFY(label != nullptr);
  QVERIFY2(label->text().contains(QStringLiteral("have their own recipients")),
           qPrintable(label->text()));
  dialog.show();
  QVERIFY(QTest::qWaitForWindowExposed(&dialog));
  QVERIFY2(
      dialog.width() < 2000 && dialog.height() < 2000,
      qPrintable(
          QStringLiteral("%1x%2").arg(dialog.width()).arg(dialog.height())));
#endif
}

/**
 * @brief A folder below that cannot be read may hold a list of its own that
 *        the dialog cannot see, so it is named as unknown rather than left
 *        out.
 */
void tst_usersdialog::foldersBelowThatCannotBeReadAreNamed() {
#ifdef Q_OS_WIN
  QSKIP("relies on Unix directory permissions");
#else
  if (::geteuid() == 0) {
    QSKIP("root reads anywhere");
  }
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QDir root(store.path());
  QVERIFY(root.mkpath(QStringLiteral("locked/inner")));
  {
    QFile gpgId(root.filePath(QStringLiteral(".gpg-id")));
    QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
    gpgId.write("31850CF72D9CDDE9\n");
  }
  const QString locked = root.filePath(QStringLiteral("locked"));
  QVERIFY(QFile::setPermissions(locked, QFile::WriteOwner));
  const auto restore = qScopeGuard([&locked] {
    QFile::setPermissions(locked, QFile::ReadOwner | QFile::WriteOwner |
                                      QFile::ExeOwner);
  });
  AppSettings s = m_settings;
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *label =
      dialog.findChild<QLabel *>(QStringLiteral("nestedListsWarning"));
  QVERIFY(label != nullptr);
  QVERIFY2(label->text().startsWith(QStringLiteral("1 folder")) &&
               label->text().contains(QStringLiteral("could not be read")) &&
               label->text().endsWith(QStringLiteral(": locked")),
           qPrintable(label->text()));
#endif
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

/**
 * @brief OK writes nothing when the folder's list changed while the dialog
 *        was open (a pull landing under it): the selection was ticked from
 *        the old list, and writing it brought back someone just removed, as
 *        the next generation. Unchanged, OK goes ahead. Scan
 *        ANT-2026-W7W7B1M9.
 */
void tst_usersdialog::acceptRefusesAListChangedMeanwhile() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QString gpgIdFile =
      QDir(store.path()).filePath(QStringLiteral(".gpg-id"));
  const auto writeList = [&gpgIdFile](const QByteArray &list) {
    QFile f(gpgIdFile);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      return false;
    }
    return f.write(list) == list.size();
  };
  QVERIFY(writeList("31850CF72D9CDDE9\n"));
  AppSettings s = m_settings;
  s.passStore = store.path() + QLatin1Char('/');
  s.passSigningKey.clear();
  RecordingPass pass(s);

  QStringList texts;
  QTimer poker;
  poker.setInterval(20);
  QObject::connect(&poker, &QTimer::timeout, [&texts]() {
    auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
    if (box == nullptr || box->property("tst_driven").toBool()) {
      return;
    }
    box->setProperty("tst_driven", true);
    texts << box->text();
    box->accept();
  });
  poker.start();

  {
    UsersDialog dialog(&pass, s, s.passStore);
    auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
    QVERIFY(list != nullptr);
    QCOMPARE(checkedNames(list), QStringList{QStringLiteral("Alice")});
    // Alice is replaced by Bob elsewhere, and the pull lands under the
    // dialog.
    QVERIFY(writeList("693A0AF3FA364E76\n"));
    dialog.accept();
    QVERIFY(pass.initCalls.isEmpty());
    QVERIFY2(dialog.result() != QDialog::Accepted, "the dialog stays open");
    QCOMPARE(texts.size(), 1);
    QVERIFY2(texts.first().contains(QStringLiteral("changed while this "
                                                   "dialog was open")),
             qPrintable(texts.first()));
  }

  UsersDialog fresh(&pass, s, s.passStore);
  auto *list = fresh.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QCOMPARE(checkedNames(list), QStringList{QStringLiteral("Bob")});
  fresh.accept();
  QCOMPARE(pass.initCalls.size(), 1);
  QCOMPARE(texts.size(), 1);

  // The list changes after OK's own check, before the backend writes: the
  // backend refuses, and the dialog stays open with its selection.
  LateChangePass late(s);
  QSignalSpy critical(&late, &Pass::critical);
  UsersDialog dialog(&late, s, s.passStore);
  list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QCOMPARE(checkedNames(list), QStringList{QStringLiteral("Bob")});
  late.beforeWrite = [&writeList] { writeList("31850CF72D9CDDE9\n"); };
  dialog.accept();
  QVERIFY(late.initCalls.isEmpty());
  QCOMPARE(critical.count(), 1);
  QVERIFY2(dialog.result() != QDialog::Accepted, "the dialog stays open");
  QCOMPARE(checkedNames(list), QStringList{QStringLiteral("Bob")});
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

/**
 * @brief A user ID with a line separator (gpg escapes only ASCII) cannot
 *        draw a line of its own: the row is the name on one line and the key
 *        ID on the next, so a fake fingerprint line cannot push the real one
 *        out of view.
 */
void tst_usersdialog::aUserIdCannotDrawALineOfItsOwn() {
  const QString uid = QStringLiteral("Alice") + QChar(0x2028) +
                      QStringLiteral("0000000000000000 created 1/1/20");
  const QString listing =
      QStringLiteral(
          "pub:u:4096:1:AAAAAAAAAAAAAAA1:1700000000:::u:::escarESCA::::::23::"
          "0:\n"
          "fpr:::::::::1111111111111111111111111111111111111111:\n"
          "uid:u::::1700000000::H1::") +
      uid + QStringLiteral(" <alice@example.org>::::::::::0:\n");
  AppSettings s = m_settings;
  s.gpgExecutable = writeGpg(
      QStringLiteral("gpg-separator-uid"),
      QStringLiteral("case \"$*\" in *--list-secret-keys*) exit 0 ;; esac\n"
                     "cat <<'LISTING'\n") +
          listing + QStringLiteral("LISTING\nexit 0\n"));
  QVERIFY(!s.gpgExecutable.isEmpty());
  QTemporaryDir store;
  QVERIFY(store.isValid());
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore, UsersDialog::Preselect::Nothing);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QCOMPARE(list->count(), 1);
  const QString row = list->item(0)->text();
  QVERIFY2(!row.contains(QChar(0x2028)), qPrintable(row));
  QCOMPARE(row.count(QLatin1Char('\n')), 1);
  QVERIFY(row.section(QLatin1Char('\n'), 0, 0)
              .contains(QChar(QChar::ReplacementCharacter)));
  QVERIFY(row.section(QLatin1Char('\n'), 1)
              .startsWith(QStringLiteral("AAAAAAAAAAAAAAA1")));
}

/**
 * @brief A recipient no key answers to is listed by its .gpg-id line as
 *        written; a line separator in it cannot draw a line of its own.
 */
void tst_usersdialog::anUnknownRecipientCannotDrawALineOfItsOwn() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
  gpgId.write((QStringLiteral("DEADBEEFDEADBEEF") + QChar(0x2028) +
               QStringLiteral("0000000000000000\n"))
                  .toUtf8());
  gpgId.close();
  AppSettings s = m_settings;
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  bool seen = false;
  for (int r = 0; r < list->count(); ++r) {
    const QString text = list->item(r)->text();
    if (text.contains(QStringLiteral("DEADBEEFDEADBEEF"))) {
      seen = true;
      QVERIFY2(!text.contains(QChar(0x2028)), qPrintable(text));
      QCOMPARE(text.count(QLatin1Char('\n')), 1);
    }
  }
  QVERIFY2(seen, "the unknown recipient is listed");
}

/**
 * @brief A folder name with a bidi override cannot turn the warning that
 *        names it around: the override is shown as U+FFFD.
 */
void tst_usersdialog::aFolderNameCannotTurnTheWarningAround() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  const QDir root(store.path());
  const QString name =
      QStringLiteral("work") + QChar(0x202E) + QStringLiteral("knab");
  QVERIFY(root.mkpath(name));
  for (const QString &folder : {QStringLiteral("."), name}) {
    QFile gpgId(
        QDir(root.filePath(folder)).filePath(QStringLiteral(".gpg-id")));
    QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
    gpgId.write("31850CF72D9CDDE9\n");
  }
  AppSettings s = m_settings;
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *banner =
      dialog.findChild<QLabel *>(QStringLiteral("nestedListsWarning"));
  QVERIFY(banner != nullptr);
  QVERIFY2(!banner->text().contains(QChar(0x202E)), qPrintable(banner->text()));
  QVERIFY(banner->text().contains(QStringLiteral("work") +
                                  QChar(QChar::ReplacementCharacter) +
                                  QStringLiteral("knab")));
}

/**
 * @brief The search matches user IDs, not file names: "*" and "?" match a
 *        "/" too. A key whose user ID has a slash must be listed when its
 *        name is typed; otherwise unticking the listed rows leaves it ticked
 *        unseen, and OK keeps it as a recipient.
 */
void tst_usersdialog::searchListsUserIdsWithASlash() {
  const QString listing = QStringLiteral(
      "pub:u:4096:1:AAAAAAAAAAAAAAA1:1700000000:::u:::escarESCA::::::23::0:\n"
      "fpr:::::::::1111111111111111111111111111111111111111:\n"
      "uid:u::::1700000000::H1::Mallory (dev-ops) "
      "<mallory@example.org>::::::::::0:\n"
      "pub:u:4096:1:BBBBBBBBBBBBBBB2:1700000000:::u:::escarESCA::::::23::0:\n"
      "fpr:::::::::2222222222222222222222222222222222222222:\n"
      "uid:u::::1700000000::H2::Mallory (dev/ops) "
      "<mallory@example.org>::::::::::0:\n"
      "pub:u:4096:1:CCCCCCCCCCCCCCC3:1700000000:::u:::escarESCA::::::23::0:\n"
      "fpr:::::::::3333333333333333333333333333333333333333:\n"
      "uid:u::::1700000000::H3::Trent <trent@example.org>::::::::::0:\n");
  AppSettings s = m_settings;
  s.gpgExecutable = writeGpg(
      QStringLiteral("gpg-slash-uid"),
      QStringLiteral("case \"$*\" in *--list-secret-keys*) exit 0 ;; esac\n"
                     "cat <<'LISTING'\n") +
          listing + QStringLiteral("LISTING\nexit 0\n"));
  QVERIFY(!s.gpgExecutable.isEmpty());

  QTemporaryDir store;
  QVERIFY(store.isValid());
  QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
  gpgId.write("AAAAAAAAAAAAAAA1\nBBBBBBBBBBBBBBB2\nCCCCCCCCCCCCCCC3\n");
  gpgId.close();
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  auto *filter = dialog.findChild<QLineEdit *>(QStringLiteral("lineEdit"));
  QVERIFY(list != nullptr && filter != nullptr);
  QCOMPARE(list->count(), 3);

  // Typed wildcards that have to cross the slash match both keys too.
  for (const QString &typed :
       {QStringLiteral("dev?ops"), QStringLiteral("mallory*ops")}) {
    filter->setText(typed);
    QVERIFY2(list->count() == 2, qPrintable(typed));
  }

  // Remove Mallory by name: untick every row the search lists.
  filter->setText(QStringLiteral("mallory"));
  QCOMPARE(list->count(), 2);
  for (int i = 0; i < list->count(); ++i) {
    list->item(i)->setCheckState(Qt::Unchecked);
  }

  // Only Trent stays: neither Mallory key, the slash one included.
  dialog.accept();
  QCOMPARE(pass.initCalls.size(), 1);
  QCOMPARE(enabledIds(pass.initCalls.first().second),
           QStringList{QStringLiteral("CCCCCCCCCCCCCCC3")});
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
 * @brief A .gpg-id may name a key by its long or short ID (0x-prefixed or
 *        not), its email or part of its UID rather than its fingerprint; gpg
 *        resolves it, and the dialog must not also list it as "Key not found
 *        in keyring".
 */
void tst_usersdialog::recipientByShortOrLongIdIsNotListedAsNotFound() {
  // One recipient per list: the stand-in gpg answers by substring.
  for (const QByteArray &line :
       {QByteArrayLiteral("31850CF72D9CDDE9"), QByteArrayLiteral("0x2d9cdde9"),
        QByteArrayLiteral("alice@example.org"),
        QByteArrayLiteral("<alice@example.org>"), QByteArrayLiteral("Alice")}) {
    QTemporaryDir store;
    QVERIFY(store.isValid());
    QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
    QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
    gpgId.write(line + '\n');
    gpgId.close();
    AppSettings s = m_settings;
    s.passStore = store.path() + QLatin1Char('/');
    RecordingPass pass(s);
    UsersDialog dialog(&pass, s, s.passStore);
    auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
    auto *unusable = dialog.findChild<QCheckBox *>(QStringLiteral("checkBox"));
    QVERIFY(list != nullptr && unusable != nullptr);
    unusable->click();
    for (int i = 0; i < list->count(); ++i) {
      QVERIFY2(
          !list->item(i)->text().contains(
              QStringLiteral("Key not found in keyring")),
          qPrintable(QString::fromLatin1(line) + ": " + list->item(i)->text()));
    }
    QVERIFY2(checkedNames(list).contains(QStringLiteral("Alice")),
             qPrintable(QString::fromLatin1(line)));
  }
}

/**
 * @brief A v6 fingerprint uses its first 16 hex digits as the long key ID
 *        and the next eight as the short ID. Its tail must not match either.
 */
void tst_usersdialog::v6FingerprintMatchesHeadIdsButNotTailIds() {
  const QString fingerprint =
      QStringLiteral("0123456789ABCDEF") + QString(48, QLatin1Char('A'));
  const QString listing =
      QStringLiteral("pub:u:255:22:0123456789ABCDEF:1774947438:::u::::\n"
                     "fpr:::::::::%1:\n"
                     "uid:u::::1774947438::H::V6 key <v6@example.org>::::\n")
          .arg(fingerprint);
  AppSettings s = m_settings;
  s.gpgExecutable = writeGpg(
      QStringLiteral("gpg-v6-recipient"),
      QStringLiteral("case \"$*\" in *--list-secret-keys*) exit 0 ;; esac\n"
                     "all() { cat <<'LISTING'\n") +
          listing +
          QStringLiteral("LISTING\n}\n"
                         "case \"$*\" in\n"
                         "  *0123456789ABCDEF*|*89ABCDEF*) all ;;\n"
                         "  *AAAAAAAAAAAAAAAA*|*DEADBEEF*) exit 0 ;;\n"
                         "  *) all ;;\n"
                         "esac\nexit 0\n"));
  QVERIFY(!s.gpgExecutable.isEmpty());

  for (const QString &recipient :
       {QStringLiteral("0123456789ABCDEF"), QStringLiteral("89ABCDEF"),
        QStringLiteral("AAAAAAAAAAAAAAAA")}) {
    QTemporaryDir store;
    QVERIFY(store.isValid());
    QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
    QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
    gpgId.write(recipient.toLatin1() + '\n');
    gpgId.close();
    s.passStore = store.path() + QLatin1Char('/');
    RecordingPass pass(s);
    UsersDialog dialog(&pass, s, s.passStore);
    auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
    QVERIFY(list != nullptr);
    const QList<UserInfo> selected = dialog.selectedUsers();
    QCOMPARE(selected.size(),
             recipient == QStringLiteral("AAAAAAAAAAAAAAAA") ? 2 : 1);
    QCOMPARE(selected.first().key_id, fingerprint);
    QCOMPARE(selected.first().enabled,
             recipient != QStringLiteral("AAAAAAAAAAAAAAAA"));
    QListWidgetItem *missing =
        itemWithText(list, QStringLiteral("Key not found in keyring"));
    if (recipient == QStringLiteral("AAAAAAAAAAAAAAAA")) {
      QVERIFY(missing != nullptr);
    } else {
      QCOMPARE(missing, nullptr);
    }
  }

  // After ten lines that name no key, a v6 head ID and short ID still match
  // the listed fingerprint without a gpg call of their own.
  QTemporaryDir store;
  QVERIFY(store.isValid());
  QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
  for (int i = 1; i <= 10; ++i) {
    gpgId.write(QStringLiteral("DEADBEEF%1\n")
                    .arg(i, 8, 16, QLatin1Char('0'))
                    .toLatin1());
  }
  gpgId.write("0123456789ABCDEF\n");
  gpgId.write("89ABCDEF\n");
  gpgId.write("AAAAAAAA\n");
  gpgId.close();
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  const QList<UserInfo> selected = dialog.selectedUsers();
  QCOMPARE(selected.size(), 12);
  QCOMPARE(selected.first().key_id, fingerprint);
  QVERIFY(selected.first().enabled);
  QVERIFY(itemWithText(list,
                       QStringLiteral("Key not found in keyring\nAAAAAAAA")) !=
          nullptr);
  QCOMPARE(
      itemWithText(list, QStringLiteral("Key not found in keyring\n89ABCDEF")),
      nullptr);
}

/**
 * @brief A recipient in .gpg-id that gpg does not know must not silently
 *        drop out of the list (saving would then re-encrypt the store
 *        without them): it is listed as a ticked placeholder that says the
 *        key is missing, badged [INVALID]. It is shown without "Show
 *        unusable keys": hidden, it could neither be seen nor unticked.
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

  // Listed and ticked although unusable keys are hidden: it is a recipient.
  QCOMPARE(list->count(), 3);
  QCOMPARE(
      enabledIds(dialog.selectedUsers()),
      (QStringList{QStringLiteral("13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9"),
                   QStringLiteral("DEADBEEFDEADBEEF")}));
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

/**
 * @brief A .gpg-id line no key answers to is shown cut short, and kept whole
 * in what OK writes back. Qt elided the whole line on every paint, in time
 * quadratic in its tabs and script changes: a co-writer's line froze the
 * dialog for minutes (scan finding ANT-2026-DAFRPZ0K).
 */
void tst_usersdialog::anOverlongRecipientIsShownShortAndKeptWhole() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  // "aα" crosses a script boundary every character; a surrogate pair sits
  // where the row is cut. The stand-in gpg knows no key for DEADBEEF.
  const QString line =
      QString(255, QLatin1Char('k')) + QStringLiteral("\U0001F511") +
      QStringLiteral("a\u03B1").repeated(30000) +
      QString(20000, QLatin1Char('\t')) + QStringLiteral("DEADBEEFDEADBEEF");
  QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
  gpgId.write("13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9\n" + line.toUtf8() +
              "\n");
  gpgId.close();
  AppSettings s = m_settings;
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QListWidgetItem *row = nullptr;
  for (int i = 0; i < list->count(); ++i) {
    if (list->item(i)->text().contains(QStringLiteral("kkkk"))) {
      row = list->item(i);
    }
  }
  QVERIFY2(row != nullptr, "the unknown recipient is listed");
  const QString shown = row->text();
  QVERIFY2(shown.size() < 1000, qPrintable(QString::number(shown.size())));
  QVERIFY(shown.contains(QString(255, QLatin1Char('k')) + QChar(0x2026)));

  dialog.resize(400, 300);
  QElapsedTimer timer;
  timer.start();
  (void)list->viewport()->grab();
  QVERIFY2(timer.elapsed() < 2000,
           qPrintable(QString::number(timer.elapsed())));

  QVERIFY2(enabledIds(dialog.selectedUsers()).contains(line),
           "OK keeps the line as written");
}

/**
 * @brief A .gpg-id of thousands of recipients no key answers to opens at
 * once, its rows still sorted: each [INVALID] badge re-sorted every row
 * added so far, so a co-writer's list kept the dialog from opening for
 * minutes (scan finding ANT-2026-6KB4IRXS).
 */
void tst_usersdialog::manyUnknownRecipientsOpenAtOnceAndSorted() {
  const auto writeList = [](const QTemporaryDir &store,
                            const QStringList &lines) {
    QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
    if (!gpgId.open(QIODevice::WriteOnly | QIODevice::Text)) {
      return false;
    }
    gpgId.write((lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toLatin1());
    return true;
  };
  const auto sorted = [](const QListWidget *list) {
    for (int row = 1; row < list->count(); ++row) {
      if (*list->item(row) < *list->item(row - 1)) {
        return false;
      }
    }
    return true;
  };

  // In order, as a co-writer would list them: each badge put its row out of
  // place and Qt sorted all rows again (6.8 s for these, before).
  QTemporaryDir store;
  QStringList many;
  for (int i = 0; i < 6000; ++i) {
    many << QStringLiteral("k%1").arg(i, 6, 10, QLatin1Char('0'));
  }
  QVERIFY(writeList(store, many));
  AppSettings s = m_settings;
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  QElapsedTimer timer;
  timer.start();
  UsersDialog dialog(&pass, s, s.passStore);
  const qint64 elapsed = timer.elapsed();
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QVERIFY2(list->count() > 3000, qPrintable(QString::number(list->count())));
  QVERIFY2(elapsed < 3000, qPrintable(QString::number(elapsed)));
  QVERIFY(sorted(list));

  // Listed backwards: the rows still come out sorted, and stay so.
  QTemporaryDir backwards;
  QVERIFY(writeList(backwards, {QStringLiteral("k3"), QStringLiteral("k2"),
                                QStringLiteral("k1")}));
  s.passStore = backwards.path() + QLatin1Char('/');
  RecordingPass otherPass(s);
  UsersDialog few(&otherPass, s, s.passStore);
  list = few.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);
  QVERIFY(sorted(list));
  QVERIFY2(list->isSortingEnabled(), "a later search keeps the rows sorted");
}

/**
 * @brief One line gpg cannot read as a key search ("0x", a fingerprint one
 *        digit short) aborts the batched --list-keys call, so none of the
 *        other recipients' keys resolve from it. The dialog must still tick
 *        the keyring keys the other lines name, from a per-line fallback, and
 *        report only the unreadable line as missing. Otherwise OK writes the
 *        bad line alone over .gpg-id and re-encryption drops every recipient.
 */
void tst_usersdialog::unreadableRecipientLineLeavesTheOthersTicked() {
  // Real gpg exits 2 and lists nothing for the whole call when any one term
  // is unreadable, wherever it stands. The stand-in does the same: exit 2 for
  // any argument list that contains "0x", otherwise list by substring as the
  // default stand-in gpg does.
  AppSettings s = m_settings;
  s.gpgExecutable = writeGpg(
      QStringLiteral("gpg-unreadable"),
      QStringLiteral("case \"$*\" in *--list-secret-keys*) exit 0 ;; esac\n"
                     "case \"$*\" in *0x*) echo 'gpg: error reading key: "
                     "Invalid user ID' >&2; exit 2 ;; esac\n"
                     "all() { cat <<'LISTING'\n") +
          QString::fromLatin1(kColonListing) +
          QStringLiteral("LISTING\n}\n"
                         "case \"$*\" in\n"
                         "*31850CF72D9CDDE9*693A0AF3FA364E76*) all ;;\n"
                         "*31850CF72D9CDDE9*) all | head -3 ;;\n"
                         "*693A0AF3FA364E76*) all | tail -3 ;;\n"
                         "*) all ;;\nesac\nexit 0\n"));
  QVERIFY(!s.gpgExecutable.isEmpty());

  QTemporaryDir store;
  QVERIFY(store.isValid());
  QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
  gpgId.write("13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9\n"
              "4EF2550F79F4E9E68B09F71D693A0AF3FA364E76\n"
              "0x\n");
  gpgId.close();
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);

  // Both keyring recipients stay ticked although the batched lookup failed;
  // only "0x" is reported missing, as an unknown key already is. OK writes the
  // same three lines back rather than dropping the two keys.
  const QStringList enabled = enabledIds(dialog.selectedUsers());
  QVERIFY2(enabled.contains(
               QStringLiteral("13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9")),
           qPrintable(enabled.join(QLatin1Char(','))));
  QVERIFY2(enabled.contains(
               QStringLiteral("4EF2550F79F4E9E68B09F71D693A0AF3FA364E76")),
           qPrintable(enabled.join(QLatin1Char(','))));
  QListWidgetItem *placeholder = itemWithText(list, QStringLiteral("0x"));
  QVERIFY2(placeholder != nullptr, "the unreadable line must be listed");
  QVERIFY2(
      placeholder->text().contains(QStringLiteral("Key not found in keyring")),
      qPrintable(placeholder->text()));
  QCOMPARE(placeholder->checkState(), Qt::Checked);
}

/**
 * @brief The cached per-line gpg verdict decides a line even when the
 *        heuristic answersTo() would match it. An unreadable "0x" line stays
 *        reported missing although a resolved key's UID contains "0x".
 */
void tst_usersdialog::unreadableLineStaysMissingWhenAResolvedUidContainsIt() {
  // A key whose UID contains "0x", listed for its fingerprint; the whole call
  // and a bare "0x" both abort with exit 2, as real gpg does.
  const QString listing = QStringLiteral(
      "pub:u:4096:1:AAAABBBBCCCCDDDD:1700000000:::u:::escarESCA::::::23::0:\n"
      "fpr:::::::::AAAABBBBCCCCDDDDAAAABBBBAAAABBBBCCCCDDDD:\n"
      "uid:u::::1700000000::HASH::Max 0xface <max@example.org>::::::::::0:\n");
  AppSettings s = m_settings;
  s.gpgExecutable = writeGpg(
      QStringLiteral("gpg-uid-0x"),
      QStringLiteral("case \"$*\" in *--list-secret-keys*) exit 0 ;; esac\n"
                     "case \"$*\" in *0x*) echo 'gpg: error reading key: "
                     "Invalid user ID' >&2; exit 2 ;; esac\n"
                     "cat <<'LISTING'\n") +
          listing + QStringLiteral("LISTING\nexit 0\n"));
  QVERIFY(!s.gpgExecutable.isEmpty());

  QTemporaryDir store;
  QVERIFY(store.isValid());
  QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
  gpgId.write("AAAABBBBCCCCDDDDAAAABBBBAAAABBBBCCCCDDDD\n0x\n");
  gpgId.close();
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);

  // Both the placeholder and the "Max 0xface" row carry "0x", so the
  // placeholder is found by the text only it has.
  QListWidgetItem *placeholder =
      itemWithText(list, QStringLiteral("Key not found in keyring"));
  QVERIFY2(placeholder != nullptr,
           "the unreadable 0x line must still be reported missing");
  QVERIFY2(placeholder->text().contains(QStringLiteral("0x")),
           qPrintable(placeholder->text()));
  QCOMPARE(placeholder->checkState(), Qt::Checked);
}

/**
 * @brief The fallback for a batched call that one unreadable line aborted
 *        must stay bounded. A fresh clone or a team store can have every
 *        recipient's key missing from the keyring. The dialog asks again for
 *        all lines it can judge in one call, and only the unreadable line
 *        gets a call of its own, however many lines there are.
 */
void tst_usersdialog::fallbackAsksAgainForTheReadableLinesInOneCall() {
  // Build a store whose .gpg-id holds @p missing unknown fingerprints and one
  // unreadable "0x" line, open the Users dialog on it, and count the gpg calls
  // that carried a recipient (a fingerprint or "0x"); the whole-keyring and
  // secret-key calls carry neither and are not counted.
  const auto callsFor = [this](int missing) -> int {
    const QString counter =
        QDir(m_dir.path()).filePath(QStringLiteral("lookups-%1").arg(missing));
    QFile::remove(counter);
    AppSettings s = m_settings;
    s.gpgExecutable = writeGpg(
        QStringLiteral("gpg-counting-%1").arg(missing),
        // The probe of the whole list fails, as gpg's does for a key it
        // lacks; a probe of one line passes without naming anyone, so every
        // line goes to the search, which is what is counted.
        QStringLiteral("case \"$*\" in *--list-secret-keys*) exit 0 ;; esac\n"
                       "case \"$*\" in *--encrypt*-r*-r*) exit 2 ;; esac\n"
                       "case \"$*\" in *--encrypt*) exit 0 ;; esac\n"
                       "case \"$*\" in *--list-only*) exit 0 ;; esac\n"
                       "case \"$*\" in *FACE*|*0x*) echo x >> '") +
            counter +
            QStringLiteral(
                "' ;; esac\n"
                "case \"$*\" in *0x*) echo 'gpg: error reading key: Invalid "
                "user ID' >&2; exit 2 ;; esac\n"
                // An unknown fingerprint: gpg lists nothing and exits 0.
                "case \"$*\" in *FACE*) exit 0 ;; esac\n"
                "cat <<'LISTING'\n") +
            QString::fromLatin1(kColonListing) +
            QStringLiteral("LISTING\nexit 0\n"));
    if (s.gpgExecutable.isEmpty()) {
      return -1;
    }
    QTemporaryDir store;
    if (!store.isValid()) {
      return -1;
    }
    QByteArray body;
    for (int i = 1; i <= missing; ++i) {
      // "FACE" + 36 hex digits = a 40-char fingerprint not in the keyring.
      body += QStringLiteral("FACE%1\n")
                  .arg(i, 36, 16, QLatin1Char('0'))
                  .toLatin1();
    }
    body += "0x\n";
    QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
    if (!gpgId.open(QIODevice::WriteOnly | QIODevice::Text)) {
      return -1;
    }
    gpgId.write(body);
    gpgId.close();
    s.passStore = store.path() + QLatin1Char('/');
    RecordingPass pass(s);
    UsersDialog dialog(&pass, s, s.passStore);
    QFile log(counter);
    if (!log.open(QIODevice::ReadOnly | QIODevice::Text)) {
      return 0;
    }
    return static_cast<int>(log.readAll().count('x'));
  };

  // The batched call fails on "0x", the readable lines are asked again in
  // one call, and "0x" alone once: three calls for five lines or fifteen.
  // The per-line fallback this replaced made one call per line, up to ten.
  QCOMPARE(callsFor(5), 3);
  QCOMPARE(callsFor(15), 3);
}

/// Alice from kColonListing, and Mallory with an encryption subkey and a
/// second user ID, as gpg --with-colons lists them.
const char kSubkeyListing[] =
    "pub:u:255:22:D3DC2DCEA20CE159:1795499446:::u:::scESC::::::23::0:\n"
    "fpr:::::::::149D30615789F7CD996A6581D3DC2DCEA20CE159:\n"
    "uid:u::::1795499446::H1::Mallory <mallory@example.org>::::::::::0:\n"
    "uid:u::::1795499446::H2::Mallory Work <mal-work@example.org>::::::::::0:"
    "\n"
    "sub:u:255:18:EBBFA1F57F2E64CD:1795499446::::::e:::::cv25519::\n"
    "fpr:::::::::4B9D410DA3140B35BAC7A28EEBBFA1F57F2E64CD:\n";

/**
 * @brief Scan ANT-2026-G4QELVPH: a .gpg-id line may name a key by its
 *        subkey's fingerprint, by a fingerprint with spaces (as gpg prints
 *        it) or between colons, or by a user ID other than the one shown.
 *        Such a line must tick that key and not also be listed as a ticked
 *        "Key not found" row, or unticking the key leaves the line in the
 *        list and the store stays encrypted to it. Ten lines naming no
 *        key used to spend the lookups that recognised such a line.
 */
void tst_usersdialog::aLineNamingASubkeyOrAnotherUidTicksItsKey() {
  const QString counter =
      QDir(m_dir.path()).filePath(QStringLiteral("subkey-lookups"));
  QFile::remove(counter);
  // Like gpg: a call with recipients lists the keys they name (Alice and
  // Mallory), and an unknown fingerprint adds nothing. Each such call is
  // counted.
  AppSettings s = m_settings;
  s.gpgExecutable = writeGpg(
      QStringLiteral("gpg-subkey"),
      QStringLiteral("case \"$*\" in *--list-secret-keys*) exit 0 ;; esac\n"
                     "keyring() { cat <<'LISTING'\n") +
          QString::fromLatin1(kColonListing) +
          QStringLiteral("LISTING\n}\nmallory() { cat <<'LISTING'\n") +
          QString::fromLatin1(kSubkeyListing) +
          QStringLiteral("LISTING\n}\n"
                         "case \"$*\" in\n"
                         "  *--list-keys\\ ?*) echo x >> '") +
          counter +
          QStringLiteral("'; keyring | head -3; mallory ;;\n"
                         "  *) keyring; mallory ;;\nesac\nexit 0\n"));
  QVERIFY(!s.gpgExecutable.isEmpty());

  QTemporaryDir store;
  QVERIFY(store.isValid());
  QByteArray body = "13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9\n";
  for (int i = 1; i <= 10; ++i) {
    body +=
        QStringLiteral("FACE%1\n").arg(i, 36, 16, QLatin1Char('0')).toLatin1();
  }
  // A line listed twice is one row.
  body +=
      QStringLiteral("FACE%1\n").arg(1, 36, 16, QLatin1Char('0')).toLatin1();
  body += "4B9D410DA3140B35BAC7A28EEBBFA1F57F2E64CD\n"
          "0x7F2E64CD!\n"
          "mal-work@example.org\n"
          "4B9D 410D A314 0B35 BAC7  A28E EBBF A1F5 7F2E 64CD\n"
          ":149D30615789F7CD996A6581D3DC2DCEA20CE159:\n";
  QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
  gpgId.write(body);
  gpgId.close();
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);

  const QString mallory =
      QStringLiteral("149D30615789F7CD996A6581D3DC2DCEA20CE159");
  QVERIFY2(enabledIds(dialog.selectedUsers()).contains(mallory),
           qPrintable(enabledIds(dialog.selectedUsers()).join(u',')));
  for (const QString &line :
       {QStringLiteral("4B9D410DA3140B35BAC7A28EEBBFA1F57F2E64CD"),
        QStringLiteral("0x7F2E64CD!"), QStringLiteral("mal-work@example.org"),
        QStringLiteral("4B9D 410D A314 0B35 BAC7  A28E EBBF A1F5 7F2E 64CD"),
        QStringLiteral(":149D30615789F7CD996A6581D3DC2DCEA20CE159:")}) {
    QVERIFY2(itemWithText(list, line) == nullptr, qPrintable(line));
  }
  // The ten unknown fingerprints are still listed missing, without a gpg
  // call each: one batched call answers for every line. Only the
  // fingerprints written with spaces or between colons get a call each.
  for (int i : {1, 10}) {
    QVERIFY2(itemWithText(list, QStringLiteral("FACE%1").arg(
                                    i, 36, 16, QLatin1Char('0'))) != nullptr,
             "listed once");
  }
  QFile log(counter);
  QVERIFY(log.open(QIODevice::ReadOnly | QIODevice::Text));
  QCOMPARE(log.readAll().count('x'), 3);

  // Unticking Mallory leaves no line that still names him.
  QListWidgetItem *row = itemWithText(list, QStringLiteral("Mallory"));
  QVERIFY(row != nullptr);
  row->setCheckState(Qt::Unchecked);
  const QStringList kept = enabledIds(dialog.selectedUsers());
  QVERIFY2(!kept.contains(mallory), qPrintable(kept.join(u',')));
  QVERIFY2(
      kept.contains(QStringLiteral("13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9")),
      qPrintable(kept.join(u',')));
  QCOMPARE(kept.size(), 11);
}

/**
 * @brief A line in a form the dialog cannot judge itself (here a keygrip)
 *        needs a gpg call of its own. Past ten such calls, a line must not
 *        stay ticked unseen as "Key not found": it is listed unticked as not
 *        looked up, so OK leaves it out unless the user ticks it.
 */
void tst_usersdialog::unusualLinesPastTheLookupCapAreListedUnticked() {
  const QString counter =
      QDir(m_dir.path()).filePath(QStringLiteral("keygrip-lookups"));
  QFile::remove(counter);
  AppSettings s = m_settings;
  s.gpgExecutable = writeGpg(
      QStringLiteral("gpg-keygrip"),
      QStringLiteral("case \"$*\" in *--list-secret-keys*) exit 0 ;; esac\n"
                     "case \"$*\" in *--list-keys\\ \\&*) echo x >> '") +
          counter + QStringLiteral("'; exit 0 ;; esac\ncat <<'LISTING'\n") +
          QString::fromLatin1(kColonListing) +
          QStringLiteral("LISTING\nexit 0\n"));
  QVERIFY(!s.gpgExecutable.isEmpty());

  QTemporaryDir store;
  QVERIFY(store.isValid());
  QByteArray body = "13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9\n";
  for (int i = 1; i <= 11; ++i) {
    body += QStringLiteral("&%1\n").arg(i, 40, 16, QLatin1Char('0')).toLatin1();
  }
  QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly | QIODevice::Text));
  gpgId.write(body);
  gpgId.close();
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  QVERIFY(list != nullptr);

  QFile log(counter);
  QVERIFY(log.open(QIODevice::ReadOnly | QIODevice::Text));
  QCOMPARE(log.readAll().count('x'), 10);
  for (int i = 1; i <= 10; ++i) {
    QListWidgetItem *row =
        itemWithText(list, QStringLiteral("Key not found in keyring\n&%1")
                               .arg(i, 40, 16, QLatin1Char('0')));
    QVERIFY2(row != nullptr, qPrintable(QString::number(i)));
    QCOMPARE(row->checkState(), Qt::Checked);
  }
  QListWidgetItem *last =
      itemWithText(list, QStringLiteral("Not looked up in keyring\n&%1")
                             .arg(11, 40, 16, QLatin1Char('0')));
  QVERIFY(last != nullptr);
  QCOMPARE(last->checkState(), Qt::Unchecked);
  QVERIFY(
      !enabledIds(dialog.selectedUsers())
           .contains(QStringLiteral("&%1").arg(11, 40, 16, QLatin1Char('0'))));
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
 * @brief The keys a folder is encrypted to are shown whatever their state,
 *        with "Show unusable keys" unticked: an expired, revoked or missing
 *        recipient used to stay in .gpg-id unseen, could not be unticked,
 *        and made every save in the folder fail. A key gpg itself marks
 *        expired (validity e) is badged [EXPIRED], not [INVALID].
 */
void tst_usersdialog::unusableRecipientsAreShownAndCanBeRemoved() {
  QTemporaryDir store;
  QVERIFY(store.isValid());
  {
    QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
    QVERIFY(gpgId.open(QIODevice::WriteOnly));
    gpgId.write("AAAAAAAAAAAAAAA1\nCCCCCCCCCCCCCCC3\nEEEEEEEEEEEEEEE5\n"
                "BBBBBBBBBBBBBBB2\nDDDDDDDDDDDDDDD4\n");
  }
  // The one-call listing answers for every recipient; a lookup of the
  // missing key alone finds nothing.
  const QString gpgExpired = QStringLiteral(
      "pub:e:4096:1:EEEEEEEEEEEEEEE5:1400000000:1500000000::e:::escarESCA::::"
      "::23::0:\n"
      "fpr:::::::::000000000000000000000000EEEEEEEEEEEEEEE5:\n"
      "uid:e::::1400000000::X::Old Gmail <old@example.org>::::::::::0:\n");
  AppSettings s = m_settings;
  s.gpgExecutable =
      writeGpg(QStringLiteral("gpg-recipients"),
               QStringLiteral("case \"$*\" in *--list-secret-keys*) exit 0 ;;\n"
                              "  *AAAAAAAAAAAAAAA1*) ;;\n"
                              "  *DDDDDDDDDDDDDDD4*) exit 2 ;;\n"
                              "esac\n"
                              "cat <<'LISTING'\n") +
                   QString::fromLatin1(kMixedListing) + gpgExpired +
                   QStringLiteral("LISTING\nexit 0\n"));
  QVERIFY(!s.gpgExecutable.isEmpty());
  s.passStore = store.path() + QLatin1Char('/');
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  auto *unusable = dialog.findChild<QCheckBox *>(QStringLiteral("checkBox"));
  QVERIFY(list != nullptr && unusable != nullptr);
  QVERIFY(!unusable->isChecked());

  QListWidgetItem *expired = itemWithText(list, QStringLiteral("Expired"));
  QListWidgetItem *revoked = itemWithText(list, QStringLiteral("Revoked"));
  QListWidgetItem *gmail = itemWithText(list, QStringLiteral("Old Gmail"));
  QListWidgetItem *missing =
      itemWithText(list, QStringLiteral("DDDDDDDDDDDDDDD4"));
  QVERIFY2(expired && revoked && gmail && missing,
           "every recipient must be listed without Show unusable keys");
  for (QListWidgetItem *item : {expired, revoked, gmail, missing}) {
    QCOMPARE(item->checkState(), Qt::Checked);
  }
  QVERIFY2(gmail->text().startsWith(QStringLiteral("[EXPIRED] ")),
           qPrintable(gmail->text()));
  QVERIFY2(expired->text().startsWith(QStringLiteral("[EXPIRED] ")),
           qPrintable(expired->text()));
  QVERIFY2(revoked->text().startsWith(QStringLiteral("[INVALID] ")),
           qPrintable(revoked->text()));

  for (QListWidgetItem *item : {expired, revoked, gmail, missing}) {
    item->setCheckState(Qt::Unchecked);
  }
  QCOMPARE(
      enabledIds(dialog.selectedUsers()),
      QStringList{QStringLiteral("000000000000000000000000BBBBBBBBBBBBBBB2")});
  // Unticked, they are candidates again, and those only show on request.
  auto *filter = dialog.findChild<QLineEdit *>(QStringLiteral("lineEdit"));
  QVERIFY(filter != nullptr);
  filter->setText(QStringLiteral("e"));
  filter->clear();
  QVERIFY(itemWithText(list, QStringLiteral("Old Gmail")) == nullptr);
  unusable->click();
  QVERIFY(itemWithText(list, QStringLiteral("Old Gmail")) != nullptr);
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
    "  *show-only*) cat > /dev/null; "
    "echo 'pub:-:255:22:693A0AF3FA364E76:1795454642:::-:::scSC::::::::0:'; "
    "exit 0 ;;\n"
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
  // Tick Alice before importing: the reload rebuilds m_userList from gpg, but
  // the selection made before the import must survive it, so an import does not
  // silently drop the other recipients.
  list->item(0)->setCheckState(Qt::Checked);
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

  // Alice's tick survived the import; Bob was not ticked by it.
  QListWidgetItem *alice = nullptr;
  for (int i = 0; i < list->count(); ++i) {
    if (list->item(i)->text().startsWith(QStringLiteral("Alice"))) {
      alice = list->item(i);
    }
  }
  QVERIFY2(alice != nullptr, "Alice must still be listed after the import");
  QVERIFY2(alice->checkState() == Qt::Checked,
           "Alice's selection must survive importing another key");
  QCOMPARE(enabledIds(dialog.selectedUsers()).size(), 1);
  QCOMPARE(dialog.selectedUsers().size(), 2);
  QVERIFY2(QFile::exists(s.gpgExecutable + QStringLiteral(".imported")),
           "gpg --import must have run");
}

/**
 * @brief After an OpenPGP v6 key is imported and gpg reports its 16-hex
 *        head key ID on an IMPORTED line, the key is selected by matching
 *        its stored 64-hex fingerprint.
 */
void tst_usersdialog::importingAV6KeyRefreshesTheListAndSelectsIt() {
  const QString v6Key =
      QStringLiteral(
          "pub:u:255:22:A1B2C3D4E5F60718:1774947439:::u::::\n"
          "fpr:::::::::%1:\n"
          "uid:u::::1774947439::H::Bob <bob@example.org>::::::::::0:\n")
          .arg(QStringLiteral("A1B2C3D4E5F60718") +
               QString(48, QLatin1Char('B')));
  AppSettings s = m_settings;
  s.gpgExecutable = writeGpg(
      QStringLiteral("gpg-importing-v6"),
      QStringLiteral("case \"$*\" in\n"
                     "  *show-only*) cat > /dev/null; "
                     "echo 'pub:-:255:22:A1B2C3D4E5F60718:1774947439::::::'; "
                     "exit 0 ;;\n"
                     "  *--import*) cat > /dev/null; touch \"$0.imported\"; "
                     "printf '[GNUPG:] IMPORTED A1B2C3D4E5F60718 Bob\\n'; "
                     "exit 0 ;;\n"
                     "  *--list-secret-keys*) exit 0 ;;\n"
                     "esac\n"
                     "all() { cat <<'LISTING'\n") +
          QString::fromLatin1(kColonListing)
              .split(QLatin1Char('\n'))
              .mid(0, 3)
              .join(QLatin1Char('\n')) +
          QLatin1Char('\n') + v6Key + QString::fromLatin1(kImportingGpgTail));
  QVERIFY(!s.gpgExecutable.isEmpty());
  QFile::remove(s.gpgExecutable + QStringLiteral(".imported"));
  RecordingPass pass(s);
  UsersDialog dialog(&pass, s, s.passStore);
  auto *list = dialog.findChild<QListWidget *>(QStringLiteral("listWidget"));
  auto *filter = dialog.findChild<QLineEdit *>(QStringLiteral("lineEdit"));
  QVERIFY(list != nullptr && filter != nullptr);
  QCOMPARE(list->count(), 1);
  QVERIFY(list->item(0)->text().startsWith(QStringLiteral("Alice")));
  // Tick Alice before importing: the reload rebuilds m_userList from gpg, but
  // the selection made before the import must survive it, so an import does not
  // silently drop the other recipients.
  list->item(0)->setCheckState(Qt::Checked);
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

  // Alice's tick survived the import; Bob was not ticked by it.
  QListWidgetItem *alice = nullptr;
  for (int i = 0; i < list->count(); ++i) {
    if (list->item(i)->text().startsWith(QStringLiteral("Alice"))) {
      alice = list->item(i);
    }
  }
  QVERIFY2(alice != nullptr, "Alice must still be listed after the import");
  QVERIFY2(alice->checkState() == Qt::Checked,
           "Alice's selection must survive importing another key");
  QCOMPARE(enabledIds(dialog.selectedUsers()).size(), 1);
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
