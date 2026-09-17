// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QTemporaryDir>
#include <QtTest>

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

private slots:
  void initTestCase();
  void init();
  void secretKeysNeedARunnableGpg();
  void programsPageWaitsForGpg();
  void keyPageListsAndTicksTheSecretKeys();
  void storePageDescribesThePath();
  void finishCreatesAndInitialisesTheStore();
  void finishLeavesAnExistingStoreAlone();
};

void tst_firstrunwizard::initTestCase() {
  isolateTestSettings();
#ifdef Q_OS_WIN
  QSKIP("uses a shell script as the gpg stand-in");
#endif
  QVERIFY(m_tmp.isValid());
  m_gpg = m_tmp.filePath(QStringLiteral("gpg"));
  QFile script(m_gpg);
  QVERIFY(script.open(QIODevice::WriteOnly));
  script.write(
      "#!/bin/sh\n"
      "case \"$*\" in\n"
      "*--list-secret-keys*)\n"
      "printf '%s\\n' "
      "'sec:u:4096:1:31850CF72D9CDDE9:1774947438:::u:::escarESCA:::+:::23::0:' "
      "'fpr:::::::::13A47CCE2B3DA3AC340A274A31850CF72D9CDDE9:' "
      "'uid:u::::1774947438::CBF23008234AA5F88824CE76140F482FAE34923E::Anne "
      "Jan Brouwer <henk@annejan.com>::::::::::0:'\n"
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
  QVERIFY(FirstRunWizard::secretKeys(m_tmp.filePath("nope")).isEmpty());
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
  gpg->setText(m_tmp.filePath("nope"));
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

  list->item(0)->setCheckState(Qt::Unchecked);
  QVERIFY2(!page->isComplete(), "a store needs at least one recipient");
  list->item(0)->setCheckState(Qt::Checked);
  QVERIFY(page->isComplete());
}

void tst_firstrunwizard::storePageDescribesThePath() {
  QTemporaryDir store;
  FirstRunWizard w;
  w.setStartId(FirstRunWizard::StorePage);
  w.restart();
  QWizardPage *page = w.currentPage();
  QLineEdit *path = lineEdit(page, 0);
  QVERIFY(path != nullptr);

  path->setText(QString());
  emit path->textEdited(path->text());
  QVERIFY(!page->isComplete());

  path->setText(store.filePath(QStringLiteral("new")));
  emit path->textEdited(path->text());
  QVERIFY(page->isComplete());
  QVERIFY(page->validatePage());
  QCOMPARE(w.settings().passStore, store.filePath(QStringLiteral("new")));
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

QTEST_MAIN(tst_firstrunwizard)
#include "tst_firstrunwizard.moc"
