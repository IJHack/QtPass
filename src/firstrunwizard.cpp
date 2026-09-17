// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "firstrunwizard.h"
#include "executor.h"
#include "gpgkeystate.h"
#include "imitatepass.h"
#include "keygendialog.h"
#include "passbackendfactory.h"
#include "profileinit.h"
#include "qtpasssettings.h"
#include "util.h"
#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QToolButton>
#include <QVBoxLayout>
#include <functional>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

/// A line edit with a "…" button that opens a file or folder picker.
auto pathRow(QLineEdit *edit, QWidget *parent,
             const std::function<QString()> &pick) -> QWidget * {
  auto *row = new QWidget(parent);
  auto *layout = new QHBoxLayout(row);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(edit);
  auto *browse = new QToolButton(row);
  browse->setText(QStringLiteral("…"));
  browse->setToolTip(FirstRunWizard::tr("Browse"));
  layout->addWidget(browse);
  QObject::connect(browse, &QToolButton::clicked, edit, [edit, pick]() {
    const QString chosen = pick();
    if (!chosen.isEmpty()) {
      edit->setText(chosen);
      emit edit->textEdited(chosen);
    }
  });
  return row;
}

auto runnable(const QString &exe) -> bool {
  return exe.startsWith(QStringLiteral("wsl ")) ||
         (!exe.isEmpty() && QFileInfo(exe).isExecutable());
}

} // namespace

// --------------------------------------------------------------------------

FirstRunWizard::FirstRunWizard(QWidget *parent)
    : QWizard(parent), m_settings(QtPassSettings::load()) {
  setWindowTitle(tr("Welcome to QtPass"));
  setWizardStyle(QWizard::ModernStyle);
  setOption(QWizard::NoBackButtonOnStartPage);
  setOption(QWizard::HaveHelpButton, false);

  auto *intro = new QWizardPage(this);
  intro->setTitle(tr("Welcome to QtPass"));
  auto *introLayout = new QVBoxLayout(intro);
  auto *introText = new QLabel(
      tr("QtPass is a graphical front-end for <i>pass</i>, the standard Unix "
         "password manager: every password is a file encrypted with GnuPG, "
         "kept in a folder you can put under Git.<br><br>The next pages find "
         "GnuPG, make sure you have a key to encrypt to and pick the folder "
         "your passwords live in. Nothing is written until you press "
         "Finish."),
      intro);
  introText->setWordWrap(true);
  introText->setTextFormat(Qt::RichText);
  introLayout->addWidget(introText);
  setPage(IntroPage, intro);
  setPage(ProgramsPage, new ProgramsWizardPage(this));
  setPage(KeyPage, new KeyWizardPage(this));
  setPage(StorePage, new StoreWizardPage(this));
  setPage(DonePage, new DoneWizardPage(this));
  setStartId(IntroPage);
  resize(640, 480);
}

auto FirstRunWizard::secretKeys(const QString &gpgExecutable)
    -> QList<UserInfo> {
  if (!runnable(gpgExecutable)) {
    return {};
  }
  QString out;
  const int rc = Executor::executeBlocking(
      gpgExecutable,
      {QStringLiteral("--no-tty"), QStringLiteral("--with-colons"),
       QStringLiteral("--with-fingerprint"),
       QStringLiteral("--list-secret-keys")},
      &out);
  if (rc != 0) {
    return {};
  }
  return parseGpgColonOutput(out, true);
}

auto FirstRunWizard::isStore(const QString &path) -> bool {
  return !path.isEmpty() &&
         QFile::exists(QDir(path).filePath(QStringLiteral(".gpg-id")));
}

void FirstRunWizard::accept() {
  // The Finish button validates the page before calling this; a programmatic
  // accept() gets the same treatment so the last page's choices are in.
  if (currentPage() != nullptr && !currentPage()->validatePage()) {
    return;
  }
  const QString store = QDir::cleanPath(m_settings.passStore);
  QString note;
  if (!isStore(store)) {
    QList<UserInfo> recipients;
    for (const UserInfo &key : std::as_const(m_keys)) {
      if (key.enabled) {
        recipients.append(key);
      }
    }
    const bool created = !QDir(store).exists();
    if (created && !QDir().mkpath(store)) {
      QMessageBox::warning(
          this, tr("Error"),
          tr("Failed to create password-store at: %1").arg(store));
      return;
    }
#ifdef Q_OS_WIN
    // Only a folder made here is hidden, like ~/.password-store on Unix; a
    // folder the user picked keeps its attributes.
    if (created) {
      SetFileAttributes(store.toStdWString().c_str(), FILE_ATTRIBUTE_HIDDEN);
    }
#endif
    if (!ProfileInit::initialise(store, recipients, m_settings,
                                 m_settings.useGit, &note)) {
      // ProfileInit writes .gpg-id before it signs and commits; take it back
      // so a second Finish runs the whole initialisation again instead of
      // finding a store that looks finished.
      QFile::remove(QDir(store).filePath(QStringLiteral(".gpg-id")));
      QFile::remove(QDir(store).filePath(QStringLiteral(".gpg-id.sig")));
      QMessageBox::warning(this, tr("Password store not initialised"), note);
      return;
    }
    if (!note.isEmpty()) {
      // ProfileInit's own note talks of switching profiles; on the first run
      // the folder simply becomes the store.
      QMessageBox::information(
          this, tr("Password store"),
          tr("%1 already contains encrypted files; they were not "
             "re-encrypted to the ticked keys. Open Users after the start "
             "to do that.")
              .arg(QDir::toNativeSeparators(store)));
    }
  } else if (m_settings.useGit && !QDir(store).exists(QStringLiteral(".git"))) {
    if (!ProfileInit::initGit(store, m_settings, &note)) {
      // Take the repository back too, so a second Finish retries the whole
      // thing instead of skipping a .git that never got its first commit.
      QDir(QDir(store).filePath(QStringLiteral(".git"))).removeRecursively();
      QMessageBox::warning(this, tr("Password store not initialised"), note);
      return;
    }
  }
  m_settings.passStore = Util::normalizeFolderPath(store);
  QtPassSettings::save(m_settings);
  // The backend cached the old paths at construction.
  PassBackendFactory::invalidate();
  QWizard::accept();
}

// --------------------------------------------------------------------------

ProgramsWizardPage::ProgramsWizardPage(FirstRunWizard *wizard)
    : QWizardPage(wizard), m_wizard(wizard), m_gpg(new QLineEdit(this)),
      m_git(new QLineEdit(this)), m_pass(new QLineEdit(this)),
      m_usePass(new QCheckBox(tr("Use the pass command-line tool"), this)),
      m_gpgStatus(new QLabel(this)) {
  setTitle(tr("Programs"));
  setSubTitle(tr("GnuPG does the encrypting. pass and Git are optional; "
                 "QtPass can do their work itself."));
  auto *layout = new QFormLayout(this);
  auto pickExe = [this]() {
    return QFileDialog::getOpenFileName(this, tr("Select executable"),
                                        QString(), tr("All files (*)"));
  };
  layout->addRow(tr("GnuPG"), pathRow(m_gpg, this, pickExe));
  m_gpgStatus->setWordWrap(true);
  layout->addRow(QString(), m_gpgStatus);
  layout->addRow(tr("Git"), pathRow(m_git, this, pickExe));
  layout->addRow(tr("pass"), pathRow(m_pass, this, pickExe));
  m_usePass->setToolTip(tr("Run the pass script for every operation instead "
                           "of calling gpg and git directly"));
  layout->addRow(QString(), m_usePass);
  for (QLineEdit *edit : {m_gpg, m_git, m_pass}) {
    connect(edit, &QLineEdit::textEdited, this,
            &ProgramsWizardPage::updateStatus);
  }
}

void ProgramsWizardPage::initializePage() {
  AppSettings &s = m_wizard->m_settings;
  // QtPassSettings::initExecutables() fills empty paths from PATH only; a
  // stored path whose binary has moved would be shown as an error here, so
  // look it up again in that case.
  auto detect = [](QString &stored, const QStringList &names) {
    if (runnable(stored)) {
      return;
    }
    for (const QString &name : names) {
      const QString found = Util::findBinaryInPath(name);
      if (!found.isEmpty()) {
        stored = found;
        return;
      }
    }
  };
  detect(s.gpgExecutable, {QStringLiteral("gpg2"), QStringLiteral("gpg")});
  detect(s.gitExecutable, {QStringLiteral("git")});
  detect(s.passExecutable, {QStringLiteral("pass")});
  detect(s.pwgenExecutable, {QStringLiteral("pwgen")});
  m_gpg->setText(s.gpgExecutable);
  m_git->setText(s.gitExecutable);
  m_pass->setText(s.passExecutable);
  m_usePass->setChecked(s.usePass || runnable(s.passExecutable));
  updateStatus();
}

void ProgramsWizardPage::updateStatus() {
  const QString gpg = m_gpg->text().trimmed();
  if (gpg.isEmpty()) {
    m_gpgStatus->setText(tr("GnuPG was not found. Install it (gpg or gpg2) "
                            "and enter its location here."));
  } else if (!runnable(gpg)) {
    m_gpgStatus->setText(tr("%1 is not an executable file.").arg(gpg));
  } else {
    m_gpgStatus->setText(QString());
  }
  m_usePass->setEnabled(runnable(m_pass->text().trimmed()));
  if (!m_usePass->isEnabled()) {
    m_usePass->setChecked(false);
  }
  emit completeChanged();
}

auto ProgramsWizardPage::isComplete() const -> bool {
  return runnable(m_gpg->text().trimmed());
}

auto ProgramsWizardPage::validatePage() -> bool {
  AppSettings &s = m_wizard->m_settings;
  s.gpgExecutable = m_gpg->text().trimmed();
  s.gitExecutable = m_git->text().trimmed();
  s.passExecutable = m_pass->text().trimmed();
  s.usePass = m_usePass->isChecked();
  return true;
}

// --------------------------------------------------------------------------

KeyWizardPage::KeyWizardPage(FirstRunWizard *wizard)
    : QWizardPage(wizard), m_wizard(wizard), m_list(new QListWidget(this)),
      m_generate(new QPushButton(tr("Generate a new key pair…"), this)),
      m_status(new QLabel(this)) {
  setTitle(tr("Your key"));
  setSubTitle(tr("Passwords are encrypted to GnuPG keys. Tick the keys that "
                 "should be able to open a new store; you need at least one "
                 "with its secret half on this machine."));
  auto *layout = new QVBoxLayout(this);
  layout->addWidget(m_list);
  m_status->setWordWrap(true);
  layout->addWidget(m_status);
  auto *buttons = new QHBoxLayout;
  buttons->addStretch();
  buttons->addWidget(m_generate);
  layout->addLayout(buttons);
  connect(m_generate, &QPushButton::clicked, this, &KeyWizardPage::generate);
  connect(m_list, &QListWidget::itemChanged, this,
          [this]() { emit completeChanged(); });
}

void KeyWizardPage::initializePage() { reload(); }

void KeyWizardPage::reload() {
  m_wizard->m_keys =
      FirstRunWizard::secretKeys(m_wizard->m_settings.gpgExecutable);
  m_list->clear();
  const QDateTime now = QDateTime::currentDateTime();
  for (UserInfo &key : m_wizard->m_keys) {
    // An expired or otherwise unusable key would make every encryption to
    // the new store fail; list it, but do not tick it.
    QString why;
    if (key.expiry.isValid() && key.expiry < now) {
      why = tr("expired");
    } else if (!key.isValid()) {
      why = tr("not usable");
    }
    key.enabled = why.isEmpty();
    QString label = QStringLiteral("%1\n%2").arg(key.name, key.key_id);
    if (!why.isEmpty()) {
      label += QStringLiteral(" (%1)").arg(why);
    }
    auto *item = new QListWidgetItem(label, m_list);
    item->setFlags(key.enabled ? (item->flags() | Qt::ItemIsUserCheckable)
                               : (item->flags() & ~Qt::ItemIsEnabled));
    item->setCheckState(key.enabled ? Qt::Checked : Qt::Unchecked);
  }
  m_status->setText(m_wizard->m_keys.isEmpty()
                        ? tr("GnuPG has no secret key yet. Generate one here, "
                             "or import your existing key with gpg first.")
                        : QString());
  emit completeChanged();
}

void KeyWizardPage::generate() {
  // A backend of the wizard's own, set up from its copy of the settings, so
  // the chosen gpg is used and nothing is written before Finish. Each Pass
  // owns its Executor; the shared backend is not involved.
  ImitatePass pass;
  pass.init(m_wizard->m_settings);
  KeygenDialog d(m_wizard->m_settings.gpgExecutable, &pass, this);
  d.exec();
  reload();
}

auto KeyWizardPage::isComplete() const -> bool { return true; }

auto KeyWizardPage::validatePage() -> bool {
  for (int i = 0; i < m_list->count() && i < m_wizard->m_keys.size(); ++i) {
    m_wizard->m_keys[i].enabled = m_list->item(i)->checkState() == Qt::Checked;
  }
  return true;
}

// --------------------------------------------------------------------------

StoreWizardPage::StoreWizardPage(FirstRunWizard *wizard)
    : QWizardPage(wizard), m_wizard(wizard), m_path(new QLineEdit(this)),
      m_useGit(new QCheckBox(tr("Keep the store under Git"), this)),
      m_status(new QLabel(this)) {
  setTitle(tr("Password store"));
  setSubTitle(tr("The folder your passwords live in. An existing store is "
                 "used as it is; an empty or missing folder is set up for "
                 "the keys you ticked."));
  auto *layout = new QFormLayout(this);
  layout->addRow(tr("Folder"), pathRow(m_path, this, [this]() {
                   return QFileDialog::getExistingDirectory(
                       this, tr("Choose the password store folder"),
                       m_path->text());
                 }));
  m_status->setWordWrap(true);
  layout->addRow(QString(), m_status);
  layout->addRow(QString(), m_useGit);
  connect(m_path, &QLineEdit::textEdited, this, &StoreWizardPage::updateStatus);
}

void StoreWizardPage::initializePage() {
  QString path = m_wizard->m_settings.passStore;
  if (path.isEmpty()) {
    path = Util::findPasswordStore();
  }
  m_path->setText(QDir::toNativeSeparators(QDir::cleanPath(path)));
  const QString clean = QDir::cleanPath(path);
  const bool repository = QDir(clean).exists(QStringLiteral(".git"));
  bool haveGit = runnable(m_wizard->m_settings.gitExecutable);
  // A first commit needs a name and an e-mail; without them Finish would
  // fail at the very end. Ask git now and say what to do instead.
  const bool identity =
      haveGit && (repository || ProfileInit::gitIdentityConfigured(
                                    QDir::homePath(), m_wizard->m_settings));
  m_useGit->setToolTip(
      haveGit && !identity
          ? tr("Git has no name and e-mail to commit with yet. Run\n"
               "git config --global user.name \"Your Name\"\n"
               "git config --global user.email you@example.org\n"
               "and turn Git on in Settings afterwards.")
          : tr("Every change becomes a commit; a folder that is no "
               "repository yet gets one"));
  haveGit = haveGit && identity;
  m_useGit->setEnabled(haveGit);
  // On for a repository or a store still to be made, off for an existing
  // store that is no repository: turning Git on there would make every
  // remove fail in a folder git knows nothing about.
  m_useGit->setChecked(haveGit && (m_wizard->m_settings.useGit || repository ||
                                   !FirstRunWizard::isStore(clean)));
  updateStatus();
}

auto StoreWizardPage::recipients() const -> int {
  int ticked = 0;
  for (const UserInfo &key : std::as_const(m_wizard->m_keys)) {
    ticked += key.enabled ? 1 : 0;
  }
  return ticked;
}

void StoreWizardPage::updateStatus() {
  const QString path = QDir::cleanPath(m_path->text().trimmed());
  QString text;
  if (path.isEmpty()) {
    text = tr("Enter a folder.");
  } else if (FirstRunWizard::isStore(path)) {
    int entries = 0;
    QDirIterator it(path, {QStringLiteral("*.gpg")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
      it.next();
      ++entries;
    }
    text = tr("An existing password store with %n entries.", nullptr, entries);
  } else if (QDir(path).exists()) {
    text = QDir(path).isEmpty()
               ? tr("An empty folder; it will be set up as a password store.")
               : tr("This folder is not a password store yet; a .gpg-id for "
                    "the ticked keys will be written into it.");
  } else {
    text = tr("The folder does not exist yet; it will be created.");
  }
  if (!path.isEmpty() && !FirstRunWizard::isStore(path) && recipients() == 0) {
    text += QLatin1Char(' ') +
            tr("Go back and tick at least one key to encrypt it to.");
  }
  m_status->setText(text);
  emit completeChanged();
}

auto StoreWizardPage::isComplete() const -> bool {
  const QString path = QDir::cleanPath(m_path->text().trimmed());
  if (path.isEmpty()) {
    return false;
  }
  return FirstRunWizard::isStore(path) || recipients() > 0;
}

auto StoreWizardPage::validatePage() -> bool {
  m_wizard->m_settings.passStore =
      QDir::fromNativeSeparators(m_path->text().trimmed());
  m_wizard->m_settings.useGit = m_useGit->isEnabled() && m_useGit->isChecked();
  return true;
}

// --------------------------------------------------------------------------

DoneWizardPage::DoneWizardPage(FirstRunWizard *wizard)
    : QWizardPage(wizard), m_wizard(wizard), m_summary(new QLabel(this)),
      m_hidePassword(new QCheckBox(tr("Hide passwords until asked"), this)),
      m_trayIcon(new QCheckBox(tr("Show an icon in the system tray"), this)) {
  setTitle(tr("Ready"));
  setSubTitle(tr("Everything else can be changed later in Settings."));
  auto *layout = new QVBoxLayout(this);
  m_summary->setWordWrap(true);
  m_summary->setTextFormat(Qt::RichText);
  layout->addWidget(m_summary);
  layout->addSpacing(12);
  m_hidePassword->setToolTip(
      tr("Show the password line as dots; Show password reveals it"));
  layout->addWidget(m_hidePassword);
  layout->addWidget(m_trayIcon);
  layout->addStretch();
  setFinalPage(true);
}

void DoneWizardPage::initializePage() {
  const AppSettings &s = m_wizard->m_settings;
  const QString store = QDir::toNativeSeparators(QDir::cleanPath(s.passStore));
  QStringList lines;
  lines << tr("Store: %1").arg(store.toHtmlEscaped());
  lines << (FirstRunWizard::isStore(s.passStore)
                ? tr("It is already a password store and is used as it is.")
                : tr("It will be set up for the ticked keys%1.")
                      .arg(s.useGit ? tr(" and put under Git") : QString()));
  lines << tr("GnuPG: %1").arg(s.gpgExecutable.toHtmlEscaped());
  lines << (s.usePass ? tr("Operations run through pass.")
                      : tr("Operations run through gpg and git directly."));
  m_summary->setText(lines.join(QStringLiteral("<br>")));
  m_hidePassword->setChecked(true);
  const bool tray = QSystemTrayIcon::isSystemTrayAvailable();
  m_trayIcon->setVisible(tray);
  m_trayIcon->setChecked(tray && s.useTrayIcon);
}

auto DoneWizardPage::validatePage() -> bool {
  m_wizard->m_settings.hidePassword = m_hidePassword->isChecked();
  m_wizard->m_settings.useTrayIcon =
      m_trayIcon->isVisible() && m_trayIcon->isChecked();
  return true;
}
