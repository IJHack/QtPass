// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QtTest>
#include <algorithm>
#include <functional>

#include "../../../src/firstrunwizard.h"
#include "../../../src/qtpasssettings.h"
#include "../testsettings.h"

/**
 * @brief The first-run wizard against a scripted gpg: the pages gate on a
 *        runnable gpg, a ticked secret key and a store path, and Finish
 *        writes the settings and initialises a store that needs it.
 */
class tst_firstrunwizard : public QObject {
  Q_OBJECT

  QTemporaryDir m_tmp;
  QString m_gpg;

  /// The fingerprint: parseGpgColonOutput() prefers the fpr record over the
  /// long key ID, and that is what lands in .gpg-id.
  static constexpr const char *kKeyId =
      "13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9";

  auto lineEdit(QWizardPage *page, int index) -> QLineEdit * {
    const QList<QLineEdit *> edits = page->findChildren<QLineEdit *>();
    return index < edits.size() ? edits.at(index) : nullptr;
  }

  /// What the modals opened during driveModals() were and said.
  struct ModalRun {
    QStringList classes;
    QString messageText;
  };

  /// Run @p trigger while a 20 ms timer drives every modal it opens: a
  /// QFileDialog picks @p pick (a file or, in directory mode, a folder) or
  /// is cancelled when @p pick is empty, a QMessageBox has its text captured
  /// and is accepted, any other dialog is rejected. A modal that is still
  /// up five seconds after being driven is closed and its class marked
  /// "(stuck)", so a no-op accept fails the test instead of hanging it.
  auto driveModals(const std::function<void()> &trigger,
                   const QString &pick = QString()) -> ModalRun;

  /// Write an executable shell script at tools/@p name and return its path.
  auto script(const QString &name, const QByteArray &body) -> QString;

  /// Walk the wizard from the intro to the Done page with Next.
  static void walkToDone(FirstRunWizard &w);

private slots:
  void initTestCase();
  void init();
  void secretKeysNeedARunnableGpg();
  void programsPageWaitsForGpg();
  void keyPageListsAndTicksTheSecretKeys();
  void storePageDescribesThePath();
  void gitDefaultsFollowTheFolder();
  void finishCreatesAndInitialisesTheStore();
  void finishLeavesAnExistingStoreAlone();
  void secretKeysAreEmptyWhenGpgFails();
  void programsPageLooksUpAMovedGpgOnPath();
  void browseButtonFillsTheProgramField();
  void browseButtonPicksTheStoreFolder();
  void keyPageListsUnusableKeysUnticked();
  void generateOpensTheKeygenDialogAndReloads();
  void donePageAnnouncesGit();
  void finishReportsAFolderItCannotCreate();
  void finishTakesTheGpgIdBackWhenGitFails();
  void finishWarnsAboutEncryptedFilesAlreadyThere();
  void finishTakesTheRepositoryBackWhenCommitFails();
  void finishPutsAnExistingStoreUnderGit();
};

auto tst_firstrunwizard::driveModals(const std::function<void()> &trigger,
                                     const QString &pick) -> ModalRun {
  ModalRun run;
  QTimer poker;
  poker.setInterval(20);
  QPointer<QWidget> driven;
  int ticksOnDriven = 0;
  QObject::connect(&poker, &QTimer::timeout, [&]() {
    QWidget *modal = QApplication::activeModalWidget();
    if (modal == nullptr) {
      return;
    }
    if (modal == driven) {
      // Driven already and still up: the accept or reject did nothing (a
      // native dialog, say). Close it so the trigger returns and the class
      // list tells the test what happened.
      if (++ticksOnDriven == 250) {
        run.classes.last() += QStringLiteral(" (stuck)");
        modal->close();
      }
      return;
    }
    driven = modal;
    ticksOnDriven = 0;
    run.classes << QString::fromLatin1(modal->metaObject()->className());
    if (auto *fileDialog = qobject_cast<QFileDialog *>(modal)) {
      if (pick.isEmpty()) {
        fileDialog->reject();
        return;
      }
      // The widget-based dialog lists the folder from a worker thread, so
      // selectFile() may find no row yet and leave the name field empty,
      // and accept() then does nothing. Type the path the way a user would:
      // an absolute path in the name field is taken as it is.
      if (auto *name = fileDialog->findChild<QLineEdit *>(
              QStringLiteral("fileNameEdit"))) {
        name->setText(pick);
      } else {
        fileDialog->selectFile(pick);
      }
      // QFileDialog re-declares accept() protected; the QDialog view of it
      // is public and still dispatches virtually to the QFileDialog logic.
      static_cast<QDialog *>(fileDialog)->accept();
      return;
    }
    if (auto *box = qobject_cast<QMessageBox *>(modal)) {
      run.messageText = box->text();
      box->accept();
      return;
    }
    if (auto *dialog = qobject_cast<QDialog *>(modal)) {
      dialog->reject();
    }
  });
  poker.start();
  // The trigger runs the nested modal loops synchronously.
  trigger();
  poker.stop();
  return run;
}

auto tst_firstrunwizard::script(const QString &name, const QByteArray &body)
    -> QString {
  const QString path = m_tmp.filePath(QStringLiteral("tools/") + name);
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return {};
  }
  file.write("#!/bin/sh\n" + body);
  file.close();
  if (!QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner |
                                       QFile::ExeOwner)) {
    return {};
  }
  return path;
}

void tst_firstrunwizard::walkToDone(FirstRunWizard &w) {
  w.restart();
  for (int i = 0; i < 4; ++i) {
    QVERIFY2(w.currentPage()->isComplete(),
             qPrintable(QStringLiteral("page %1 blocks Next").arg(i)));
    w.next();
  }
  QCOMPARE(w.currentId(), int(FirstRunWizard::DonePage));
}

void tst_firstrunwizard::initTestCase() {
  isolateTestSettings();
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as the gpg stand-in");
#endif
  QVERIFY(m_tmp.isValid());
  // The Programs page looks binaries up on PATH when a stored path does not
  // run; keep that deterministic: nothing on PATH, the stand-in off it.
  QVERIFY(QDir(m_tmp.path()).mkpath(QStringLiteral("empty")));
  QVERIFY(QDir(m_tmp.path()).mkpath(QStringLiteral("tools")));
  qputenv("PATH", m_tmp.filePath(QStringLiteral("empty")).toUtf8());
  m_gpg = m_tmp.filePath(QStringLiteral("tools/gpg"));
  QFile script(m_gpg);
  QVERIFY(script.open(QIODevice::WriteOnly));
  // echo, not printf: PATH is empty here, and printf is not a builtin of
  // every /bin/sh (OpenBSD's ksh has echo only).
  script.write(
      "#!/bin/sh\n"
      "case \"$*\" in\n"
      "*--list-secret-keys*)\n"
      "echo "
      "'sec:u:4096:1:31850CF72D9CDDE9:1774947438:::u:::escarESCA:::+:::23::0:'"
      "\n"
      "echo 'fpr:::::::::13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9:'\n"
      "echo "
      "'uid:u::::1774947438::CBF23008234AA5F88824CE76140F482FAE34923E::Anne "
      "Jan Brouwer <anne@example.org>::::::::::0:'\n"
      ";;\n"
      "esac\n"
      "exit 0\n");
  script.close();
  QVERIFY(QFile::setPermissions(m_gpg, QFile::ReadOwner | QFile::WriteOwner |
                                           QFile::ExeOwner));
}

void tst_firstrunwizard::init() {
  QtPassSettings::getInstance()->clear();
  AppSettings s = QtPassSettings::load();
  s.gpgExecutable = m_gpg;
  s.gitExecutable.clear(); // no git: keeps ProfileInit to the .gpg-id
  s.passExecutable.clear();
  s.useGit = false;
  s.usePass = false;
  QtPassSettings::save(s);
}

void tst_firstrunwizard::secretKeysNeedARunnableGpg() {
  QVERIFY(FirstRunWizard::secretKeys(QString()).isEmpty());
  QVERIFY(FirstRunWizard::secretKeys(m_tmp.filePath("tools/nope")).isEmpty());
  const QList<UserInfo> keys = FirstRunWizard::secretKeys(m_gpg);
  QCOMPARE(keys.size(), 1);
  QCOMPARE(keys.first().key_id, QString::fromLatin1(kKeyId));
  QVERIFY(keys.first().have_secret);
}

void tst_firstrunwizard::programsPageWaitsForGpg() {
  AppSettings s = QtPassSettings::load();
  s.gpgExecutable.clear();
  QtPassSettings::save(s);

  FirstRunWizard w;
  w.setStartId(FirstRunWizard::ProgramsPage);
  w.restart();
  QWizardPage *page = w.currentPage();
  QVERIFY(page != nullptr);
  QVERIFY2(!page->isComplete(), "no gpg, no Next");

  QLineEdit *gpg = lineEdit(page, 0);
  QVERIFY(gpg != nullptr);
  gpg->setText(m_tmp.filePath("tools/nope"));
  emit gpg->textEdited(gpg->text());
  QVERIFY2(!page->isComplete(), "a path that is not executable does not do");
  gpg->setText(m_gpg);
  emit gpg->textEdited(gpg->text());
  QVERIFY(page->isComplete());

  auto *useGit = page->findChild<QCheckBox *>();
  QVERIFY(useGit != nullptr);
  QVERIFY2(!useGit->isEnabled() && !useGit->isChecked(),
           "without git the Git box is off and greyed");

  QVERIFY(page->validatePage());
  QCOMPARE(w.settings().gpgExecutable, m_gpg);
  QVERIFY(!w.settings().useGit);
}

void tst_firstrunwizard::keyPageListsAndTicksTheSecretKeys() {
  FirstRunWizard w;
  w.setStartId(FirstRunWizard::KeyPage);
  w.restart();
  QWizardPage *page = w.currentPage();
  auto *list = page->findChild<QListWidget *>();
  QVERIFY(list != nullptr);
  QCOMPARE(list->count(), 1);
  QVERIFY(list->item(0)->text().contains(QStringLiteral("Anne Jan Brouwer")));
  QCOMPARE(list->item(0)->checkState(), Qt::Checked);
  QVERIFY(page->isComplete());

  // Unticking everything is allowed here (an existing store needs no
  // recipients); the store page decides whether that is enough.
  list->item(0)->setCheckState(Qt::Unchecked);
  QVERIFY(page->isComplete());
  QVERIFY(page->validatePage());
  w.next();
  QCOMPARE(w.currentId(), int(FirstRunWizard::StorePage));
  QWizardPage *store = w.currentPage();
  QLineEdit *path = lineEdit(store, 0);
  QVERIFY(path != nullptr);
  QTemporaryDir fresh;
  path->setText(fresh.filePath(QStringLiteral("new")));
  emit path->textEdited(path->text());
  QVERIFY2(!store->isComplete(), "a new store needs at least one recipient");
  QVERIFY(store->findChild<QLabel *>()->text().contains(
      QStringLiteral("tick at least one key")));
  QFile gpgId(QDir(fresh.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly));
  gpgId.write("X\n");
  gpgId.close();
  path->setText(fresh.path());
  emit path->textEdited(path->text());
  QVERIFY2(store->isComplete(), "an existing store needs no ticked key");
}

void tst_firstrunwizard::storePageDescribesThePath() {
  QTemporaryDir store;
  FirstRunWizard w;
  w.setStartId(FirstRunWizard::KeyPage); // loads and ticks the key
  w.restart();
  w.next();
  QCOMPARE(w.currentId(), int(FirstRunWizard::StorePage));
  QWizardPage *page = w.currentPage();
  QLineEdit *path = lineEdit(page, 0);
  QVERIFY(path != nullptr);

  path->setText(QString());
  emit path->textEdited(path->text());
  QVERIFY(!page->isComplete());

  path->setText(store.filePath(QStringLiteral("new")));
  emit path->textEdited(path->text());
  QVERIFY(page->isComplete());
  auto *useGit = page->findChild<QCheckBox *>();
  QVERIFY(useGit != nullptr);
  QVERIFY2(!useGit->isEnabled(), "no git configured: the box is greyed");
  QVERIFY(page->validatePage());
  QCOMPARE(w.settings().passStore, store.filePath(QStringLiteral("new")));
  QVERIFY(!w.settings().useGit);
}

void tst_firstrunwizard::gitDefaultsFollowTheFolder() {
  const QString git = m_tmp.filePath(QStringLiteral("tools/git"));
  {
    QFile script(git);
    QVERIFY(script.open(QIODevice::WriteOnly));
    // Answers "config --get user.*" like a configured git; everything else
    // succeeds silently.
    script.write("#!/bin/sh\ncase \"$*\" in *config*) echo someone;; esac\n"
                 "exit 0\n");
    script.close();
    QVERIFY(QFile::setPermissions(git, QFile::ReadOwner | QFile::WriteOwner |
                                           QFile::ExeOwner));
  }
  AppSettings s = QtPassSettings::load();
  s.gitExecutable = git;
  QtPassSettings::save(s);

  auto boxFor = [&](const QString &path) {
    s.passStore = path;
    QtPassSettings::save(s);
    FirstRunWizard w;
    w.setStartId(FirstRunWizard::StorePage);
    w.restart();
    auto *box = w.currentPage()->findChild<QCheckBox *>();
    return box != nullptr && box->isEnabled() && box->isChecked();
  };
  QTemporaryDir plain;
  QFile gpgId(QDir(plain.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly));
  gpgId.write("X\n");
  gpgId.close();
  QVERIFY2(!boxFor(plain.path()),
           "an existing store that is no repository stays off Git");
  QVERIFY(QDir(plain.path()).mkdir(QStringLiteral(".git")));
  QVERIFY2(boxFor(plain.path()), "a repository is on");
  QVERIFY2(boxFor(m_tmp.filePath(QStringLiteral("does-not-exist"))),
           "a store still to be made is on when git is there");

  // A git without user.name/user.email cannot commit: the box is greyed
  // with the instructions, for a new store as well.
  {
    QFile script(git);
    QVERIFY(script.open(QIODevice::WriteOnly | QIODevice::Truncate));
    script.write("#!/bin/sh\ncase \"$*\" in *config*) exit 1;; esac\nexit 0\n");
  }
  s.passStore = m_tmp.filePath(QStringLiteral("does-not-exist"));
  QtPassSettings::save(s);
  FirstRunWizard w;
  w.setStartId(FirstRunWizard::StorePage);
  w.restart();
  auto *box = w.currentPage()->findChild<QCheckBox *>();
  QVERIFY(box != nullptr);
  QVERIFY(!box->isEnabled());
  QVERIFY(box->toolTip().contains(QStringLiteral("user.email")));
}

void tst_firstrunwizard::finishCreatesAndInitialisesTheStore() {
  QTemporaryDir scratch;
  const QString store = scratch.filePath(QStringLiteral("store"));
  {
    AppSettings s = QtPassSettings::load();
    s.passStore = store;
    QtPassSettings::save(s);
  }

  FirstRunWizard w;
  w.restart();
  // Walk every page the way Next does, checking each one lets us through.
  for (int i = 0; i < 4; ++i) {
    QVERIFY2(w.currentPage()->isComplete(),
             qPrintable(QStringLiteral("page %1 blocks Next").arg(i)));
    w.next();
  }
  QCOMPARE(w.currentId(), int(FirstRunWizard::DonePage));
  QVERIFY(w.currentPage()->isComplete());
  w.accept();

  QCOMPARE(w.result(), int(QDialog::Accepted));
  QVERIFY2(QDir(store).exists(), "the store folder was created");
  QFile gpgId(QDir(store).filePath(QStringLiteral(".gpg-id")));
  QVERIFY2(gpgId.open(QIODevice::ReadOnly), ".gpg-id was written");
  QCOMPARE(QString::fromUtf8(gpgId.readAll()).trimmed(),
           QString::fromLatin1(kKeyId));

  const AppSettings saved = QtPassSettings::load();
  QCOMPARE(QDir::cleanPath(saved.passStore), QDir::cleanPath(store));
  QVERIFY2(saved.passStore.endsWith(QLatin1Char('/')),
           "the store path is normalised with its trailing separator");
  QCOMPARE(saved.gpgExecutable, m_gpg);
  QVERIFY(saved.hidePassword);
  QVERIFY(!saved.usePass);
  QVERIFY(!saved.useGit);
}

void tst_firstrunwizard::finishLeavesAnExistingStoreAlone() {
  QTemporaryDir store;
  QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly));
  gpgId.write("AAAABBBBCCCCDDDD\n");
  gpgId.close();
  {
    AppSettings s = QtPassSettings::load();
    s.passStore = store.path();
    QtPassSettings::save(s);
  }

  FirstRunWizard w;
  w.restart();
  for (int i = 0; i < 4; ++i) {
    QVERIFY(w.currentPage()->isComplete());
    w.next();
  }
  w.accept();
  QCOMPARE(w.result(), int(QDialog::Accepted));

  QVERIFY(gpgId.open(QIODevice::ReadOnly));
  QCOMPARE(QString::fromUtf8(gpgId.readAll()),
           QStringLiteral("AAAABBBBCCCCDDDD\n"));
  QCOMPARE(QDir::cleanPath(QtPassSettings::load().passStore),
           QDir::cleanPath(store.path()));
}

/**
 * @brief Pins secretKeys() on a gpg that runs but fails: a non-zero exit
 *        yields no keys rather than whatever landed on stdout, and the key
 *        page then shows the "no secret key yet" hint.
 */
void tst_firstrunwizard::secretKeysAreEmptyWhenGpgFails() {
  const QString failing = script(QStringLiteral("gpg-fail"),
                                 "echo 'sec:u:4096:1:31850CF72D9CDDE9:1::::'\n"
                                 "exit 2\n");
  QVERIFY(!failing.isEmpty());
  QVERIFY2(FirstRunWizard::secretKeys(failing).isEmpty(),
           "a failing gpg lists no keys, whatever it printed");

  AppSettings s = QtPassSettings::load();
  s.gpgExecutable = failing;
  QtPassSettings::save(s);
  FirstRunWizard w;
  w.setStartId(FirstRunWizard::KeyPage);
  w.restart();
  QWizardPage *page = w.currentPage();
  auto *list = page->findChild<QListWidget *>();
  QVERIFY(list != nullptr);
  QCOMPARE(list->count(), 0);
  auto *status = page->findChild<QLabel *>();
  QVERIFY(status != nullptr);
  QVERIFY2(status->text().contains(QStringLiteral("no secret key")),
           qPrintable(status->text()));
}

/**
 * @brief Pins the Programs page's re-detection: a stored gpg path whose
 *        binary is gone is replaced by the gpg found on PATH instead of
 *        being shown as an error.
 */
void tst_firstrunwizard::programsPageLooksUpAMovedGpgOnPath() {
  // Util caches the environment on first use, so PATH cannot be changed
  // here; put a gpg into the (so far empty) PATH directory instead.
  const QString onPath = m_tmp.filePath(QStringLiteral("empty/gpg"));
  QVERIFY(QFile::copy(m_gpg, onPath));
  const auto restore = qScopeGuard([&]() { QFile::remove(onPath); });
  AppSettings s = QtPassSettings::load();
  s.gpgExecutable = m_tmp.filePath(QStringLiteral("tools/moved-away"));
  QtPassSettings::save(s);

  FirstRunWizard w;
  w.setStartId(FirstRunWizard::ProgramsPage);
  w.restart();
  QWizardPage *page = w.currentPage();
  QLineEdit *gpg = lineEdit(page, 0);
  QVERIFY(gpg != nullptr);
  QCOMPARE(gpg->text(), onPath);
  QVERIFY2(page->isComplete(), "the gpg found on PATH lets Next through");
  QVERIFY(page->validatePage());
  QCOMPARE(w.settings().gpgExecutable, onPath);
}

/**
 * @brief Pins the "…" button next to a program field: the file picked in
 *        the dialog lands in the field and counts as typed (the status and
 *        completeness follow), while a cancelled dialog leaves it alone.
 */
void tst_firstrunwizard::browseButtonFillsTheProgramField() {
  AppSettings s = QtPassSettings::load();
  s.gpgExecutable.clear();
  QtPassSettings::save(s);

  FirstRunWizard w;
  w.setStartId(FirstRunWizard::ProgramsPage);
  w.restart();
  QWizardPage *page = w.currentPage();
  QLineEdit *gpg = lineEdit(page, 0);
  QVERIFY(gpg != nullptr);
  QVERIFY(gpg->text().isEmpty());
  QVERIFY(!page->isComplete());
  // The status label is the one telling the user gpg is missing; the form's
  // row labels are QLabels too.
  const QList<QLabel *> labels = page->findChildren<QLabel *>();
  const auto statusIt =
      std::find_if(labels.cbegin(), labels.cend(), [](const QLabel *label) {
        return label->text().contains(QStringLiteral("not found"));
      });
  QVERIFY2(statusIt != labels.cend(), "the status says GnuPG was not found");
  QLabel *status = *statusIt;
  const QList<QToolButton *> browse = page->findChildren<QToolButton *>();
  QCOMPARE(browse.size(), 3);

  ModalRun cancelled = driveModals([&]() { browse.first()->click(); });
  QCOMPARE(cancelled.classes, QStringList{QStringLiteral("QFileDialog")});
  QVERIFY2(gpg->text().isEmpty(), "a cancelled picker changes nothing");
  QVERIFY(!page->isComplete());
  QVERIFY2(status->text().contains(QStringLiteral("not found")),
           "a cancelled picker leaves the status alone");

  ModalRun picked = driveModals([&]() { browse.first()->click(); }, m_gpg);
  QCOMPARE(picked.classes, QStringList{QStringLiteral("QFileDialog")});
  QCOMPARE(QFileInfo(gpg->text()).canonicalFilePath(),
           QFileInfo(m_gpg).canonicalFilePath());
  QVERIFY2(page->isComplete(), "the picked gpg counts as typed");
  QVERIFY2(status->text().isEmpty(),
           qPrintable("the status follows the picked gpg: " + status->text()));
}

/**
 * @brief Pins the "…" button on the store page: the folder chosen in the
 *        directory dialog becomes the store path and the description
 *        follows it.
 */
void tst_firstrunwizard::browseButtonPicksTheStoreFolder() {
  QTemporaryDir chosen;
  QTemporaryDir scratch;
  QVERIFY(chosen.isValid() && scratch.isValid());
  FirstRunWizard w;
  w.setStartId(FirstRunWizard::KeyPage); // loads and ticks the key
  w.restart();
  w.next();
  QCOMPARE(w.currentId(), int(FirstRunWizard::StorePage));
  QWizardPage *page = w.currentPage();
  QLineEdit *path = lineEdit(page, 0);
  QVERIFY(path != nullptr);
  auto *browse = page->findChild<QToolButton *>();
  QVERIFY(browse != nullptr);
  auto *status = page->findChild<QLabel *>();
  QVERIFY(status != nullptr);
  // The picker opens at the field's folder, which initializePage() took
  // from the settings (~/.password-store by default): point it at a folder
  // of ours that does not exist yet, so the dialog lists scratch and the
  // description has something to change from.
  path->setText(scratch.filePath(QStringLiteral("elsewhere")));
  emit path->textEdited(path->text());
  QVERIFY2(status->text().contains(QStringLiteral("does not exist yet")),
           qPrintable(status->text()));

  ModalRun run = driveModals([&]() { browse->click(); }, chosen.path());
  QCOMPARE(run.classes, QStringList{QStringLiteral("QFileDialog")});
  QCOMPARE(QFileInfo(path->text()).canonicalFilePath(),
           QFileInfo(chosen.path()).canonicalFilePath());
  QVERIFY2(status->text().contains(QStringLiteral("empty folder")),
           qPrintable("the description follows the picked folder: " +
                      status->text()));
  QVERIFY(page->isComplete());
}

/**
 * @brief Pins the key page's handling of keys that cannot encrypt: an
 *        expired key and one without validity are listed with the reason,
 *        greyed and unticked, so only the usable key becomes a recipient.
 */
void tst_firstrunwizard::keyPageListsUnusableKeysUnticked() {
  const QString mixed = script(
      QStringLiteral("gpg-mixed"),
      "case \"$*\" in\n"
      "*--list-secret-keys*)\n"
      "printf '%s\\n' "
      "'sec:e:4096:1:AAAAAAAAAAAAAAAA:1500000000:1600000000::e:::sc:::+:::23::"
      "0:' "
      "'fpr:::::::::AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA:' "
      "'uid:e::::1500000000::X::Old Key <old@example.org>::::::::::0:' "
      "'sec:-:4096:1:BBBBBBBBBBBBBBBB:1774947438:::-:::sc:::+:::23::0:' "
      "'fpr:::::::::BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB:' "
      "'uid:-::::1774947438::Y::Untrusted <untrusted@example.org>::::::::::0:' "
      "'sec:u:4096:1:31850CF72D9CDDE9:1774947438:::u:::escarESCA:::+:::23::0:' "
      "'fpr:::::::::13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9:' "
      "'uid:u::::1774947438::Z::Anne Jan Brouwer "
      "<anne@example.org>::::::::::0:'\n"
      ";;\n"
      "esac\n"
      "exit 0\n");
  QVERIFY(!mixed.isEmpty());
  AppSettings s = QtPassSettings::load();
  s.gpgExecutable = mixed;
  QtPassSettings::save(s);

  FirstRunWizard w;
  w.setStartId(FirstRunWizard::KeyPage);
  w.restart();
  QWizardPage *page = w.currentPage();
  auto *list = page->findChild<QListWidget *>();
  QVERIFY(list != nullptr);
  QCOMPARE(list->count(), 3);

  QListWidgetItem *expired = list->item(0);
  QVERIFY2(expired->text().contains(QStringLiteral("(expired)")),
           qPrintable(expired->text()));
  QVERIFY2(!(expired->flags() & Qt::ItemIsEnabled), "an expired key is greyed");
  QCOMPARE(expired->checkState(), Qt::Unchecked);

  QListWidgetItem *untrusted = list->item(1);
  QVERIFY2(untrusted->text().contains(QStringLiteral("(not usable)")),
           qPrintable(untrusted->text()));
  QVERIFY2(!(untrusted->flags() & Qt::ItemIsEnabled),
           "a key without validity is greyed");
  QCOMPARE(untrusted->checkState(), Qt::Unchecked);

  QListWidgetItem *good = list->item(2);
  QVERIFY(!good->text().contains(QLatin1Char('(')));
  QVERIFY(good->flags() & Qt::ItemIsEnabled);
  QVERIFY(good->flags() & Qt::ItemIsUserCheckable);
  QCOMPARE(good->checkState(), Qt::Checked);

  // Only the usable key counts as a recipient for a new store.
  w.next();
  QCOMPARE(w.currentId(), int(FirstRunWizard::StorePage));
  QTemporaryDir fresh;
  QLineEdit *path = lineEdit(w.currentPage(), 0);
  QVERIFY(path != nullptr);
  path->setText(fresh.filePath(QStringLiteral("new")));
  emit path->textEdited(path->text());
  QVERIFY2(w.currentPage()->isComplete(), "the usable key is the recipient");
  good->setCheckState(Qt::Unchecked);
  w.back();
  QVERIFY(w.currentPage()->validatePage());
  w.next();
  // Next re-initialises the store page from the settings; point it at the
  // new folder again so only the recipients decide.
  path = lineEdit(w.currentPage(), 0);
  QVERIFY(path != nullptr);
  path->setText(fresh.filePath(QStringLiteral("new")));
  emit path->textEdited(path->text());
  QVERIFY2(!w.currentPage()->isComplete(),
           "unticking the only usable key leaves no recipient");
  QVERIFY(w.currentPage()->findChild<QLabel *>()->text().contains(
      QStringLiteral("tick at least one key")));
}

/**
 * @brief Pins the Generate button: it opens the key generation dialog for
 *        the wizard's gpg and, once that closes, lists the keys again.
 */
void tst_firstrunwizard::generateOpensTheKeygenDialogAndReloads() {
  FirstRunWizard w;
  w.setStartId(FirstRunWizard::KeyPage);
  w.restart();
  QWizardPage *page = w.currentPage();
  auto *list = page->findChild<QListWidget *>();
  QVERIFY(list != nullptr);
  QCOMPARE(list->count(), 1);
  list->item(0)->setCheckState(Qt::Unchecked);
  auto *generate = page->findChild<QPushButton *>();
  QVERIFY(generate != nullptr);
  QVERIFY(generate->text().startsWith(QStringLiteral("Generate")));

  ModalRun run = driveModals([&]() { generate->click(); });
  QCOMPARE(run.classes, QStringList{QStringLiteral("KeygenDialog")});
  QCOMPARE(list->count(), 1);
  // Unticked before the dialog, ticked after: the list was reloaded from
  // gpg once the dialog closed, not left as it was.
  QVERIFY2(list->item(0)->checkState() == Qt::Checked,
           "the list was reloaded from gpg after the dialog closed");
}

/**
 * @brief Pins the Done page summary for a new store under Git: it says so,
 *        which is what the user is about to confirm with Finish.
 */
void tst_firstrunwizard::donePageAnnouncesGit() {
  const QString git = script(QStringLiteral("git-ok"),
                             "case \"$*\" in *config*) echo someone;; esac\n"
                             "exit 0\n");
  QVERIFY(!git.isEmpty());
  QTemporaryDir scratch;
  AppSettings s = QtPassSettings::load();
  s.gitExecutable = git;
  s.passStore = scratch.filePath(QStringLiteral("store"));
  QtPassSettings::save(s);

  FirstRunWizard w;
  walkToDone(w);
  QVERIFY2(w.settings().useGit, "a new store with a usable git is under Git");
  auto *summary = w.currentPage()->findChild<QLabel *>();
  QVERIFY(summary != nullptr);
  QVERIFY2(summary->text().contains(QStringLiteral("put under Git")),
           qPrintable(summary->text()));
  QVERIFY(summary->text().contains(QStringLiteral("gpg and git directly")));
}

/**
 * @brief Pins Finish on a store folder that cannot be made (a regular file
 *        stands where a parent folder should be): a warning names the path,
 *        the wizard stays open and nothing is saved.
 */
void tst_firstrunwizard::finishReportsAFolderItCannotCreate() {
  QTemporaryDir scratch;
  const QString blocker = scratch.filePath(QStringLiteral("blocker"));
  {
    QFile file(blocker);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not a folder\n");
  }
  const QString store = QDir(blocker).filePath(QStringLiteral("store"));
  AppSettings s = QtPassSettings::load();
  s.passStore = store;
  // The Done page ticks "hide passwords"; a Finish that got as far as
  // saving would turn this on.
  s.hidePassword = false;
  QtPassSettings::save(s);

  FirstRunWizard w;
  walkToDone(w);
  ModalRun run = driveModals([&]() { w.accept(); });
  QCOMPARE(run.classes, QStringList{QStringLiteral("QMessageBox")});
  QVERIFY2(run.messageText.contains(QStringLiteral("Failed to create")),
           qPrintable(run.messageText));
  QVERIFY(run.messageText.contains(store));
  QVERIFY2(w.result() != int(QDialog::Accepted), "the wizard stays open");
  QVERIFY2(!QtPassSettings::load().hidePassword, "nothing was saved");
}

/**
 * @brief Pins Finish when ProfileInit fails after writing .gpg-id (here git
 *        init fails): the .gpg-id is taken back so a second Finish runs the
 *        whole initialisation again, and the warning carries git's reason.
 */
void tst_firstrunwizard::finishTakesTheGpgIdBackWhenGitFails() {
  const QString git =
      script(QStringLiteral("git-init-fails"),
             "case \"$*\" in\n*config*) echo someone;;\n"
             "init*) echo 'no repository for you' >&2; exit 1;;\nesac\n"
             "exit 0\n");
  QVERIFY(!git.isEmpty());
  QTemporaryDir scratch;
  const QString store = scratch.filePath(QStringLiteral("store"));
  AppSettings s = QtPassSettings::load();
  s.gitExecutable = git;
  s.passStore = store;
  QtPassSettings::save(s);

  FirstRunWizard w;
  walkToDone(w);
  QVERIFY(w.settings().useGit);
  ModalRun run = driveModals([&]() { w.accept(); });
  QCOMPARE(run.classes, QStringList{QStringLiteral("QMessageBox")});
  QVERIFY2(run.messageText.contains(QStringLiteral("git init failed")),
           qPrintable(run.messageText));
  QVERIFY(run.messageText.contains(QStringLiteral("no repository for you")));
  QVERIFY(w.result() != int(QDialog::Accepted));
  QVERIFY2(QDir(store).exists(), "the folder itself stays");
  QVERIFY2(!QFile::exists(QDir(store).filePath(QStringLiteral(".gpg-id"))),
           ".gpg-id was taken back so the next Finish starts over");
}

/**
 * @brief Pins Finish on a folder that holds .gpg files but no .gpg-id: the
 *        store is initialised, and an information box says the files were
 *        not re-encrypted, in the wizard's own words rather than
 *        ProfileInit's talk of switching profiles.
 */
void tst_firstrunwizard::finishWarnsAboutEncryptedFilesAlreadyThere() {
  QTemporaryDir store;
  {
    QFile file(QDir(store.path()).filePath(QStringLiteral("old.gpg")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("ciphertext\n");
  }
  AppSettings s = QtPassSettings::load();
  s.passStore = store.path();
  QtPassSettings::save(s);

  FirstRunWizard w;
  walkToDone(w);
  ModalRun run = driveModals([&]() { w.accept(); });
  QCOMPARE(run.classes, QStringList{QStringLiteral("QMessageBox")});
  QVERIFY2(run.messageText.contains(
               QStringLiteral("already contains encrypted files")),
           qPrintable(run.messageText));
  QVERIFY2(!run.messageText.contains(QStringLiteral("profile")),
           "the first-run wording, not ProfileInit's");
  QCOMPARE(w.result(), int(QDialog::Accepted));
  QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY2(gpgId.open(QIODevice::ReadOnly), ".gpg-id was written");
  QCOMPARE(QString::fromUtf8(gpgId.readAll()).trimmed(),
           QString::fromLatin1(kKeyId));
  QCOMPARE(QDir::cleanPath(QtPassSettings::load().passStore),
           QDir::cleanPath(store.path()));
}

/**
 * @brief Pins Finish on an existing store that is no repository yet, with
 *        Git on: when the first commit fails the fresh .git is removed so a
 *        second Finish retries instead of skipping a half-made repository,
 *        and the store's .gpg-id is left as it was.
 */
void tst_firstrunwizard::finishTakesTheRepositoryBackWhenCommitFails() {
  // init makes .git the way git would and leaves a marker outside the
  // store, so ".git is gone" afterwards means removed, not never made;
  // commit refuses. The suite's PATH is empty (initTestCase), and mkdir is
  // no shell builtin: give it a PATH of its own.
  QTemporaryDir scratch;
  const QString marker = scratch.filePath(QStringLiteral("init-ran"));
  const QString git =
      script(QStringLiteral("git-commit-fails"),
             "case \"$*\" in\n*config*) echo someone;;\n"
             "init*) PATH=/bin:/usr/bin mkdir .git && echo made > '" +
                 QDir::cleanPath(marker).toUtf8() +
                 "';;\n"
                 "commit*) echo 'nothing to commit with' >&2; exit 1;;\nesac\n"
                 "exit 0\n");
  QVERIFY(!git.isEmpty());
  QTemporaryDir store;
  QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly));
  gpgId.write("AAAABBBBCCCCDDDD\n");
  gpgId.close();
  AppSettings s = QtPassSettings::load();
  s.gitExecutable = git;
  s.useGit = true;
  s.passStore = store.path();
  QtPassSettings::save(s);

  FirstRunWizard w;
  walkToDone(w);
  QVERIFY2(w.settings().useGit, "Git stays on for an existing store when set");
  ModalRun run = driveModals([&]() { w.accept(); });
  QCOMPARE(run.classes, QStringList{QStringLiteral("QMessageBox")});
  QVERIFY2(run.messageText.contains(QStringLiteral("git commit failed")),
           qPrintable(run.messageText));
  QVERIFY(w.result() != int(QDialog::Accepted));
  QVERIFY2(QFile::exists(marker), "the fake git init ran and made .git");
  QVERIFY2(!QDir(store.path()).exists(QStringLiteral(".git")),
           "the half-made repository was removed");
  QVERIFY(gpgId.open(QIODevice::ReadOnly));
  QCOMPARE(QString::fromUtf8(gpgId.readAll()),
           QStringLiteral("AAAABBBBCCCCDDDD\n"));
}

/**
 * @brief Pins the happy path of the same branch: an existing store gets its
 *        repository (init, add, commit run in the store) and Finish saves
 *        Git as on.
 */
void tst_firstrunwizard::finishPutsAnExistingStoreUnderGit() {
  QTemporaryDir store;
  QTemporaryDir scratch;
  const QString log = scratch.filePath(QStringLiteral("git.log"));
  // The identity probe (git config in the home folder) is answered without
  // being logged; everything else records where it ran.
  const QString git =
      script(QStringLiteral("git-logs"),
             "case \"$*\" in *config*) echo someone; exit 0;; esac\n"
             "echo \"$PWD $1\" >> '" +
                 QDir::cleanPath(log).toUtf8() + "'\nexit 0\n");
  QVERIFY(!git.isEmpty());
  QFile gpgId(QDir(store.path()).filePath(QStringLiteral(".gpg-id")));
  QVERIFY(gpgId.open(QIODevice::WriteOnly));
  gpgId.write("AAAABBBBCCCCDDDD\n");
  gpgId.close();
  AppSettings s = QtPassSettings::load();
  s.gitExecutable = git;
  s.useGit = true;
  s.passStore = store.path();
  QtPassSettings::save(s);

  FirstRunWizard w;
  walkToDone(w);
  ModalRun run = driveModals([&]() { w.accept(); });
  QVERIFY2(run.classes.isEmpty(), "no message box on success");
  QCOMPARE(w.result(), int(QDialog::Accepted));
  QFile logFile(log);
  QVERIFY2(logFile.open(QIODevice::ReadOnly), "git was run");
  const QStringList calls = QString::fromUtf8(logFile.readAll())
                                .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  QStringList verbs;
  for (const QString &call : calls) {
    QVERIFY2(call.startsWith(QFileInfo(store.path()).canonicalFilePath() +
                             QLatin1Char(' ')),
             qPrintable("git ran outside the store: " + call));
    verbs << call.section(QLatin1Char(' '), 1);
  }
  QCOMPARE(verbs, (QStringList{QStringLiteral("init"), QStringLiteral("add"),
                               QStringLiteral("commit")}));
  const AppSettings saved = QtPassSettings::load();
  QVERIFY2(saved.useGit, "Git is saved as on");
  QCOMPARE(QDir::cleanPath(saved.passStore), QDir::cleanPath(store.path()));
}

QTEST_MAIN(tst_firstrunwizard)
#include "tst_firstrunwizard.moc"
