// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "configdialog.h"
#include "appsettings.h"
#include "keygendialog.h"
#include "profileinit.h"
#include "qtpasssettings.h"
#include "sshauthsock.h"
#include "ui_configdialog.h"
#include "usersdialog.h"
#include "util.h"
#include "windowstatestore.h"
#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSystemTrayIcon>
#include <algorithm>
#include <utility>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "qtpasslogging.h"

ConfigDialog::ConfigDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::ConfigDialog) {
  ui->setupUi(this);

  WindowStateStore::attach(*this, QStringLiteral("configDialog"));

  // setSpecialValueText() in retranslateUi() drops the size hint without
  // telling the layout, which then sizes the box for "0" and shows "ver".
  ui->spinBoxAutoclearSeconds->updateGeometry();
  ui->spinBoxAutoclearPanelSeconds->updateGeometry();

  // "Seconds" says nothing next to "Never" (the spin box's 0).
  connect(ui->spinBoxAutoclearSeconds, &QSpinBox::valueChanged, this,
          [this](int value) { ui->labelSeconds->setVisible(value > 0); });
  connect(ui->spinBoxAutoclearPanelSeconds, &QSpinBox::valueChanged, this,
          [this](int value) { ui->labelPanelSeconds->setVisible(value > 0); });
  connect(
      ui->comboBoxFields, &QComboBox::currentIndexChanged, this,
      [this](int index) { ui->plainTextEditTemplate->setEnabled(index > 0); });

  const AppSettings s = QtPassSettings::load();
  applySettings(s);

  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    ui->checkBoxUseTrayIcon->setEnabled(false);
    ui->checkBoxUseTrayIcon->setToolTip(tr("System tray is not available"));
    ui->checkBoxHideOnClose->setEnabled(false);
    ui->checkBoxStartMinimized->setEnabled(false);
  }

  // Wayland has no client-side "keep above" — Qt::WindowStaysOnTopHint is
  // silently ignored there (stacking belongs to the compositor). Say so
  // instead of offering a checkbox that does nothing.
  if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
    ui->checkBoxAlwaysOnTop->setEnabled(false);
    ui->checkBoxAlwaysOnTop->setToolTip(
        tr("Not available on Wayland; use your compositor's "
           "\"keep above\" window rule instead"));
  }

#if defined(Q_OS_WIN)
  // checkBoxUseOtp stays visible: OTP is generated in-process, not by the
  // Unix-only pass-otp extension.
  ui->checkBoxUseQrencode->hide();
#endif

  if (!isQrencodeAvailable(s.qrencodeExecutable)) {
    ui->checkBoxUseQrencode->setEnabled(false);
    ui->checkBoxUseQrencode->setToolTip(tr("qrencode needs to be installed"));
  }

  connect(ui->profileList, &QListWidget::currentRowChanged, this,
          &ConfigDialog::onProfileSelected);
  connect(ui->profileName, &QLineEdit::textEdited, this,
          &ConfigDialog::onProfileNameEdited);
  connect(ui->profilePath, &QLineEdit::textEdited, this,
          &ConfigDialog::onProfilePathEdited);
  connect(ui->profileSigningKey, &QLineEdit::textEdited, this,
          &ConfigDialog::onProfileSigningKeyEdited);
  for (QCheckBox *box :
       {ui->profileUseGit, ui->profileAutoPush, ui->profileAutoPull}) {
    connect(box, &QCheckBox::clicked, this, &ConfigDialog::onProfileGitToggled);
  }
  m_nameTip = ui->profileName->toolTip();
  m_pathTip = ui->profilePath->toolTip();
  m_keyTip = ui->profileSigningKey->toolTip();
  setProfiles(QtPassSettings::getProfiles(), QtPassSettings::getProfile());

  ui->label->setText(ui->label->text() + VERSION);
  ui->comboBoxClipboard->clear();

  ui->comboBoxClipboard->addItem(tr("No clipboard"));
  ui->comboBoxClipboard->addItem(tr("Always copy to clipboard"));
  ui->comboBoxClipboard->addItem(tr("On-demand copy to clipboard"));

  int currentIndex = static_cast<int>(s.clipBoardType);
  ui->comboBoxClipboard->setCurrentIndex(currentIndex);
  on_comboBoxClipboard_activated(currentIndex);

  QClipboard *clip = QApplication::clipboard();
  if (!clip->supportsSelection()) {
    useSelection(false);
    ui->checkBoxSelection->setVisible(false);
  } else {
    useSelection(s.useSelection);
  }

  connect(ui->pageList, &QListWidget::currentRowChanged, ui->pages,
          &QStackedWidget::setCurrentIndex);
  ui->pageList->setCurrentRow(
      Util::configIsValid(s)
          ? 0
          // Programs is what needs fixing when the configuration is not valid.
          : ui->pages->indexOf(ui->pagePrograms));

  connect(this, &ConfigDialog::accepted, this, &ConfigDialog::on_accepted);
}

void ConfigDialog::applySettings(const AppSettings &settings) {
  ui->passPath->setText(settings.passExecutable);
  setGitPath(settings.gitExecutable);
  ui->gpgPath->setText(settings.gpgExecutable);
  ui->storePath->setText(settings.passStore);
  ui->sshAuthSockOverride->setText(settings.sshAuthSockOverride);

  // One control each: 0 is "Never", so off and a delay are one setting.
  ui->spinBoxAutoclearSeconds->setValue(
      settings.useAutoclear ? settings.autoclearSeconds : 0);
  ui->labelSeconds->setVisible(ui->spinBoxAutoclearSeconds->value() > 0);
  ui->spinBoxAutoclearPanelSeconds->setValue(
      settings.useAutoclearPanel ? settings.autoclearPanelSeconds : 0);
  ui->labelPanelSeconds->setVisible(ui->spinBoxAutoclearPanelSeconds->value() >
                                    0);
  ui->checkBoxHidePassword->setChecked(settings.hidePassword);
  ui->checkBoxHideContent->setChecked(settings.hideContent);
  ui->checkBoxUseMonospace->setChecked(settings.useMonospace);
  ui->checkBoxDisplayAsIs->setChecked(settings.displayAsIs);
  ui->checkBoxNoLineWrapping->setChecked(settings.noLineWrapping);
  ui->checkBoxAddGPGId->setChecked(settings.addGPGId);
  ui->checkBoxHideOnClose->setChecked(settings.hideOnClose);
  ui->checkBoxStartMinimized->setChecked(settings.startMinimized);

  ui->checkBoxAvoidCapitals->setChecked(settings.avoidCapitals);
  ui->checkBoxAvoidNumbers->setChecked(settings.avoidNumbers);
  ui->checkBoxLessRandom->setChecked(settings.lessRandom);
  ui->checkBoxUseSymbols->setChecked(settings.useSymbols);
  ui->plainTextEditTemplate->setPlainText(settings.passTemplate);
  // All fields only applies while templating is on (#1766), so the two
  // switches are one choice of three.
  ui->comboBoxFields->setCurrentIndex(
      !settings.useTemplate ? 0 : (settings.templateAllFields ? 2 : 1));
  ui->plainTextEditTemplate->setEnabled(settings.useTemplate);
  ui->checkBoxAutoPull->setChecked(settings.autoPull);
  ui->checkBoxAutoPush->setChecked(settings.autoPush);
  ui->checkBoxAlwaysOnTop->setChecked(settings.alwaysOnTop);

  // Dependent helpers: set after the plain values above so the enable/disable
  // logic they trigger sees the final widget states.
  usePass(settings.usePass);
  useTrayIcon(settings.useTrayIcon);
  useGit(settings.useGit);
  useOtp(settings.useOtp);
  useGrepSearch(settings.useGrepSearch);
  useQrencode(settings.useQrencode);
  // Before usePwgen (gated on the pwgen path); otherwise readSettings() writes
  // the .ui defaults over the saved configuration on every OK.
  setPwgenPath(settings.pwgenExecutable);
  setPasswordConfiguration(settings.passwordConfiguration);
  usePwgen(settings.usePwgen);
}

auto ConfigDialog::readSettings() -> AppSettings {
  // Start from the persisted settings so keys this dialog does not own
  // (window geometry, profiles, etc.) survive the save.
  AppSettings settings = QtPassSettings::load();

  settings.passExecutable = ui->passPath->text();
  settings.gitExecutable = ui->gitPath->text();
  settings.gpgExecutable = ui->gpgPath->text();
  settings.sshAuthSockOverride = ui->sshAuthSockOverride->text().trimmed();
  settings.passStore = Util::normalizeFolderPath(ui->storePath->text());
  settings.usePass = ui->radioButtonPass->isChecked();
  settings.clipBoardType =
      static_cast<Enums::ClipBoardType>(ui->comboBoxClipboard->currentIndex());
  settings.useSelection = ui->checkBoxSelection->isChecked();
  settings.autoclearSeconds = ui->spinBoxAutoclearSeconds->value();
  settings.useAutoclear = settings.autoclearSeconds > 0;
  settings.autoclearPanelSeconds = ui->spinBoxAutoclearPanelSeconds->value();
  settings.useAutoclearPanel = settings.autoclearPanelSeconds > 0;
  settings.hidePassword = ui->checkBoxHidePassword->isChecked();
  settings.hideContent = ui->checkBoxHideContent->isChecked();
  settings.useMonospace = ui->checkBoxUseMonospace->isChecked();
  settings.displayAsIs = ui->checkBoxDisplayAsIs->isChecked();
  settings.noLineWrapping = ui->checkBoxNoLineWrapping->isChecked();
  settings.addGPGId = ui->checkBoxAddGPGId->isChecked();
  // A disabled control (e.g. no system tray) keeps the persisted value
  // instead of clobbering it to false.
  if (ui->checkBoxUseTrayIcon->isEnabled()) {
    settings.useTrayIcon = ui->checkBoxUseTrayIcon->isChecked();
  }
  if (ui->checkBoxHideOnClose->isEnabled()) {
    settings.hideOnClose = ui->checkBoxHideOnClose->isChecked();
  }
  if (ui->checkBoxStartMinimized->isEnabled()) {
    settings.startMinimized = ui->checkBoxStartMinimized->isChecked();
  }
  settings.useGit = ui->checkBoxUseGit->isChecked();
  // The backends and the auto-pull-on-start read the global keys; #1140
  // started storing these per profile as well and stopped writing the
  // globals, which left both checkboxes without effect.
  settings.autoPush = ui->checkBoxAutoPush->isChecked();
  settings.autoPull = ui->checkBoxAutoPull->isChecked();
  settings.useOtp = ui->checkBoxUseOtp->isChecked();
  settings.useGrepSearch = ui->checkBoxUseGrepSearch->isChecked();
  settings.useQrencode = ui->checkBoxUseQrencode->isChecked();
  settings.pwgenExecutable = ui->pwgenPath->text();
  settings.usePwgen = ui->checkBoxUsePwgen->isChecked();
  settings.avoidCapitals = ui->checkBoxAvoidCapitals->isChecked();
  settings.avoidNumbers = ui->checkBoxAvoidNumbers->isChecked();
  settings.lessRandom = ui->checkBoxLessRandom->isChecked();
  settings.useSymbols = ui->checkBoxUseSymbols->isChecked();
  settings.passwordConfiguration = getPasswordConfiguration();
  settings.useTemplate = ui->comboBoxFields->currentIndex() > 0;
  settings.passTemplate = ui->plainTextEditTemplate->toPlainText();
  settings.templateAllFields = ui->comboBoxFields->currentIndex() == 2;
  settings.alwaysOnTop = ui->checkBoxAlwaysOnTop->isChecked();
  settings.version = VERSION;

  return settings;
}

ConfigDialog::~ConfigDialog() = default;

void ConfigDialog::setGitPath(const QString &path) {
  ui->gitPath->setText(path);
  ui->checkBoxUseGit->setEnabled(!path.isEmpty());
  if (path.isEmpty()) {
    useGit(false);
  }
}

void ConfigDialog::usePass(bool usePass) {
  ui->radioButtonNative->setChecked(!usePass);
  ui->radioButtonPass->setChecked(usePass);
  setGroupBoxState();
}

auto ConfigDialog::isFingerprintList(const QString &setting) -> bool {
  // 40 hex characters for OpenPGP v4 keys, 64 for v5/v6 (gpg 2.5+).
  static const QRegularExpression fingerprint(
      QStringLiteral("^(?:[0-9A-Fa-f]{40}|[0-9A-Fa-f]{64})$"));
  const QStringList keys = setting.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  return std::all_of(keys.cbegin(), keys.cend(), [](const QString &key) {
    return fingerprint.match(key).hasMatch();
  });
}

// Profile names must be unique: the settings key profiles by name, so a
// duplicate would silently overwrite the other.
auto ConfigDialog::problemsOf(const ProfileEntry &entry,
                              const QHash<QString, int> &names) const
    -> ProfileProblems {
  ProfileProblems problems;
  if (entry.name.isEmpty()) {
    problems.name = tr("This field is required");
  } else if (names.value(entry.name) > 1) {
    problems.name = tr("Another profile already has this name");
  }
  if (entry.profile.path.isEmpty()) {
    problems.path = tr("This field is required");
  }
  // pass compares PASSWORD_STORE_SIGNING_KEY against the fingerprints in
  // gpg's VALIDSIG line, so anything shorter than a fingerprint would sign
  // and then never verify. Refuse it here instead of failing at first use.
  if (!isFingerprintList(entry.profile.signingKey)) {
    problems.key = tr("Full key fingerprints only (40 or 64 hexadecimal "
                      "characters), separated by spaces");
  }
  return problems;
}

void ConfigDialog::showProblems(int row, const ProfileProblems &problems) {
  if (QListWidgetItem *item = ui->profileList->item(row)) {
    item->setBackground(problems.none() ? QBrush() : QBrush(Qt::red));
    item->setToolTip(problems.first());
  }
  if (row != m_currentEntry) {
    return;
  }
  const auto tipOr = [](const QString &problem, const QString &tip) {
    return problem.isEmpty() ? tip : problem;
  };
  ui->profileName->setToolTip(tipOr(problems.name, m_nameTip));
  ui->profilePath->setToolTip(tipOr(problems.path, m_pathTip));
  ui->profileSigningKey->setToolTip(tipOr(problems.key, m_keyTip));
}

void ConfigDialog::validate() {
  QHash<QString, int> names;
  for (const ProfileEntry &entry : std::as_const(m_entries)) {
    names[entry.name]++;
  }
  bool status = true;
  for (int row = 0; row < m_entries.size(); ++row) {
    const ProfileProblems problems = problemsOf(m_entries.at(row), names);
    showProblems(row, problems);
    status = status && problems.none();
  }
  ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(status);
}

void ConfigDialog::warnAboutSshAuthSockOverride() {
  const QString override = ui->sshAuthSockOverride->text().trimmed();
  if (override.isEmpty()) {
    return;
  }
  QString reason;
  switch (SshAuthSock::overrideStatus(override)) {
  case SshAuthSock::OverrideStatus::Valid:
    return;
  case SshAuthSock::OverrideStatus::DoesNotExist:
    reason = tr("The path does not exist.");
    break;
  case SshAuthSock::OverrideStatus::NotReadable:
    reason = tr("The path is not readable.");
    break;
  case SshAuthSock::OverrideStatus::NotUnixDomainSocket:
    reason = tr("The path is not a Unix domain socket.");
    break;
  }
  QMessageBox::warning(
      this, tr("Potentially invalid SSH_AUTH_SOCK override"),
      tr("The SSH_AUTH_SOCK override value may be invalid.\n\n%1\n\n"
         "The value will still be saved as entered.")
          .arg(reason));
}

void ConfigDialog::followActiveProfile(const Profiles &profiles) {
  // As MainWindow::on_profileBox_currentTextChanged does when switching.
  AppSettings s = QtPassSettings::load();
  const QString wasActive = s.activeProfile;
  QString nowActive;
  for (const ProfileEntry &entry : std::as_const(m_entries)) {
    if (!wasActive.isEmpty() && entry.originalName == wasActive) {
      nowActive = entry.name;
    }
  }
  s.activeProfile = nowActive;
  const auto active = profiles.constFind(nowActive);
  if (active != profiles.constEnd()) {
    s.useGit = active->useGit.value_or(s.useGit);
    s.autoPush = active->autoPush.value_or(s.autoPush);
    s.autoPull = active->autoPull.value_or(s.autoPull);
  }
  QtPassSettings::save(s);
}

void ConfigDialog::on_accepted() {
  warnAboutSshAuthSockOverride();
  const Profiles existingProfiles = QtPassSettings::getProfiles();
  // Persist via the facade, which also invalidates the cached Pass backend so
  // a changed "use pass" mode takes effect.
  QtPassSettings::save(readSettings());
  // Profiles are not part of AppSettings yet, so persist them separately.
  const Profiles profiles = getProfiles();
  QtPassSettings::setProfiles(profiles);
  followActiveProfile(profiles);
  initializeNewProfiles(existingProfiles);
}

void ConfigDialog::on_autodetectButton_clicked() {
  QString pass = Util::findBinaryInPath("pass");
  if (!pass.isEmpty()) {
    ui->passPath->setText(pass);
  }
  usePass(!pass.isEmpty());
  QString gpg = Util::findBinaryInPath("gpg2");
  if (gpg.isEmpty()) {
    gpg = Util::findBinaryInPath("gpg");
  }
  if (!gpg.isEmpty()) {
    ui->gpgPath->setText(gpg);
  }
  QString git = Util::findBinaryInPath("git");
  if (!git.isEmpty()) {
    ui->gitPath->setText(git);
  }
  QString pwgen = Util::findBinaryInPath("pwgen");
  if (!pwgen.isEmpty()) {
    ui->pwgenPath->setText(pwgen);
  }
}

void ConfigDialog::on_radioButtonNative_clicked() { setGroupBoxState(); }

void ConfigDialog::on_radioButtonPass_clicked() { setGroupBoxState(); }

void ConfigDialog::setGroupBoxState() {
  bool state = ui->radioButtonPass->isChecked();
  ui->groupBoxNative->setEnabled(!state);
  ui->groupBoxPass->setEnabled(state);
  if (state) {
    ui->spinBoxPasswordLength->setEnabled(false);
    ui->checkBoxUsePwgen->setEnabled(false);
    ui->checkBoxAvoidCapitals->setEnabled(false);
    ui->checkBoxUseSymbols->setEnabled(false);
    ui->checkBoxLessRandom->setEnabled(false);
    ui->checkBoxAvoidNumbers->setEnabled(false);
    ui->labelPasswordChars->setEnabled(false);
    ui->passwordCharTemplateSelector->setEnabled(false);
    ui->lineEditPasswordChars->setEnabled(false);
  } else {
    // native mode: restore pwgen/charset state from existing handlers
    ui->spinBoxPasswordLength->setEnabled(true);
    ui->checkBoxUsePwgen->setEnabled(!ui->pwgenPath->text().isEmpty());
    on_checkBoxUsePwgen_clicked();
  }
}

auto ConfigDialog::selectExecutable() -> QString {
  QFileDialog dialog(this);
  dialog.setFileMode(QFileDialog::ExistingFile);
  dialog.setOption(QFileDialog::ReadOnly);
  if (dialog.exec()) {
    return dialog.selectedFiles().constFirst();
  }

  return {};
}

auto ConfigDialog::selectFolder() -> QString {
  QFileDialog dialog(this);
  dialog.setFileMode(QFileDialog::Directory);
  dialog.setFilter(QDir::NoFilter);
  dialog.setOption(QFileDialog::ShowDirsOnly);
  if (dialog.exec()) {
    return dialog.selectedFiles().constFirst();
  }

  return {};
}

void ConfigDialog::on_toolButtonGit_clicked() {
  QString git = selectExecutable();
  bool state = !git.isEmpty();
  if (state) {
    ui->gitPath->setText(git);
  } else {
    useGit(false);
  }

  ui->checkBoxUseGit->setEnabled(state);
}

void ConfigDialog::on_toolButtonGpg_clicked() {
  QString gpg = selectExecutable();
  if (!gpg.isEmpty()) {
    ui->gpgPath->setText(gpg);
  }
}

void ConfigDialog::on_pushButtonGenerateKey_clicked() {
  KeygenDialog d(ui->gpgPath->text(), QtPassSettings::getPass(), this);
  d.exec();
}

void ConfigDialog::on_toolButtonPass_clicked() {
  QString pass = selectExecutable();
  if (!pass.isEmpty()) {
    ui->passPath->setText(pass);
  }
}

void ConfigDialog::on_toolButtonStore_clicked() {
  QString store = selectFolder();
  if (!store.isEmpty()) {
    ui->storePath->setText(store);
  }
}

// index 0 is "No clipboard".
void ConfigDialog::on_comboBoxClipboard_activated(int index) {
  bool state = index > 0;

  ui->checkBoxSelection->setEnabled(state);
  ui->checkBoxHidePassword->setEnabled(state);
  ui->checkBoxHideContent->setEnabled(state);
  ui->labelAutoclear->setEnabled(state);
  ui->spinBoxAutoclearSeconds->setEnabled(state);
  ui->labelSeconds->setEnabled(state);
}

void ConfigDialog::useSelection(bool useSelection) {
  ui->checkBoxSelection->setChecked(useSelection);
  on_checkBoxSelection_clicked();
}

void ConfigDialog::on_checkBoxSelection_clicked() {
  on_comboBoxClipboard_activated(ui->comboBoxClipboard->currentIndex());
}

void ConfigDialog::setProfiles(Profiles profiles,
                               const QString &currentProfile) {
  // remove weird "" key value pairs
  profiles.remove(QString());

  m_entries.clear();
  ui->profileList->clear();
  int currentRow = -1;
  for (auto i = profiles.cbegin(); i != profiles.cend(); ++i) {
    if (i.key() == currentProfile) {
      currentRow = m_entries.size();
    }
    m_entries.append({i.key(), i.value(), i.key()});
    ui->profileList->addItem(i.key());
  }
  ui->profileList->setCurrentRow(currentRow);
  if (currentRow < 0) {
    loadProfileForm(-1);
  }
  validate();
}

auto ConfigDialog::currentEntry() -> ProfileEntry * {
  if (m_currentEntry < 0 || m_currentEntry >= m_entries.size()) {
    return nullptr;
  }
  return &m_entries[m_currentEntry];
}

// The Git boxes show the profile's own flags, else the global ones; only a
// click on a box makes the value the profile's own.
void ConfigDialog::loadProfileForm(int row) {
  m_currentEntry = (row >= 0 && row < m_entries.size()) ? row : -1;
  const ProfileEntry *entry = currentEntry();
  m_loadingForm = true;
  ui->profileForm->setEnabled(entry != nullptr);
  ui->deleteButton->setEnabled(entry != nullptr);
  ui->profileName->setText(entry ? entry->name : QString());
  ui->profilePath->setText(entry ? entry->profile.path : QString());
  ui->profileSigningKey->setText(entry ? entry->profile.signingKey : QString());
  const bool useGit =
      entry && entry->profile.useGit.value_or(ui->checkBoxUseGit->isChecked());
  ui->profileUseGit->setChecked(useGit);
  ui->profileAutoPush->setChecked(
      entry &&
      entry->profile.autoPush.value_or(ui->checkBoxAutoPush->isChecked()));
  ui->profileAutoPull->setChecked(
      entry &&
      entry->profile.autoPull.value_or(ui->checkBoxAutoPull->isChecked()));
  ui->profileAutoPush->setEnabled(useGit);
  ui->profileAutoPull->setEnabled(useGit);
  m_loadingForm = false;
  updateProfileStatus();
  validate();
}

auto ConfigDialog::getProfiles() -> Profiles {
  Profiles profiles;
  for (const ProfileEntry &entry : std::as_const(m_entries)) {
    profiles.insert(entry.name, entry.profile);
  }
  return profiles;
}

auto ConfigDialog::ensureProfileFolder(const QString &path) -> bool {
  if (QDir(path).exists()) {
    return true;
  }
  if (QMessageBox::question(
          this, tr("Create profile directory?"),
          tr("Would you like to create a password store at %1?").arg(path),
          QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
    return false;
  }
  if (!QDir().mkpath(path)) {
    QMessageBox::warning(
        this, tr("Error"),
        tr("Could not create profile directory: %1").arg(path));
    return false;
  }
  return true;
}

void ConfigDialog::initialiseProfileStore(const QString &name,
                                          const Profile &profile,
                                          const QString &path) {
  // Only the recipients come through the active backend. passStore is the
  // profile, or getGpgIdPath() would pre-tick the active store's recipients;
  // signing keys are per profile, exactly as the profile switch applies them
  // (MainWindow::on_profileBox_currentTextChanged).
  const AppSettings settings = QtPassSettings::load();
  AppSettings profileSettings = settings;
  profileSettings.passStore = Util::normalizeFolderPath(path);
  profileSettings.passSigningKey = profile.signingKey;
  UsersDialog usersDialog(QtPassSettings::getPass(), profileSettings,
                          profileSettings.passStore, this);
  usersDialog.setInitOnAccept(false);
  usersDialog.setWindowTitle(tr("Select recipients for %1").arg(name));
  if (usersDialog.exec() != QDialog::Accepted) {
    return;
  }
  QString note;
  const bool ok = ProfileInit::initialise(
      path, usersDialog.selectedUsers(), profileSettings,
      profile.useGit.value_or(settings.useGit), &note);
  if (!ok) {
    QMessageBox::warning(this, tr("Could not initialise profile %1").arg(name),
                         note);
  } else if (!note.isEmpty()) {
    QMessageBox::information(this, tr("Profile %1").arg(name), note);
  }
}

void ConfigDialog::initializeNewProfiles(const Profiles &existingProfiles) {
  const Profiles newProfiles = getProfiles();
  // QMap iterates in name order.
  for (auto it = newProfiles.cbegin(); it != newProfiles.cend(); ++it) {
    const QString path = QDir::cleanPath(it.value().path);
    // A store that was already a profile when the dialog opened, under this
    // or another name: a rename is not a new store.
    const bool known = std::any_of(
        existingProfiles.cbegin(), existingProfiles.cend(),
        [&it](const Profile &old) { return old.path == it.value().path; });
    // needsInit is false for a missing folder, so it is created first.
    if (!known && ensureProfileFolder(path) && ProfileInit::needsInit(path)) {
      initialiseProfileStore(it.key(), it.value(), path);
    }
  }
}

void ConfigDialog::on_addButton_clicked() {
  ProfileEntry entry;
  entry.name = tr("New profile");
  entry.profile.path = ui->storePath->text();
  m_entries.append(entry);
  ui->profileList->addItem(entry.name);
  ui->profileList->setCurrentRow(m_entries.size() - 1);
  ui->profileName->setFocus();
  ui->profileName->selectAll();
  validate();
}

void ConfigDialog::on_profilePathBrowse_clicked() {
  const QString dir = selectFolder();
  if (!dir.isEmpty()) {
    ui->profilePath->setText(dir);
    onProfilePathEdited(dir);
  }
}

void ConfigDialog::on_deleteButton_clicked() {
  const int row = ui->profileList->currentRow();
  if (row < 0 || row >= m_entries.size()) {
    QMessageBox::warning(this, tr("No profile selected"),
                         tr("No profile selected to delete"));
    return;
  }
  m_entries.removeAt(row);
  delete ui->profileList->takeItem(row);
  // currentRowChanged has re-pointed the form at the neighbour (or nothing).
  validate();
}

void ConfigDialog::criticalMessage(const QString &title, const QString &text) {
  QMessageBox::critical(this, title, text, QMessageBox::Ok, QMessageBox::Ok);
}

// Looks on Util's PATH (with the macOS and Windows additions) without a
// subprocess; a hit is stored for the QR display, a miss leaves the setting.
auto ConfigDialog::isQrencodeAvailable(const QString &configuredPath) -> bool {
#ifdef Q_OS_WIN
  Q_UNUSED(configuredPath);
  return false;
#else
  if (!configuredPath.isEmpty()) {
    const QFileInfo configured(configuredPath);
    if (configured.isFile() && configured.isExecutable()) {
      return true;
    }
  }
  const QString found = Util::findBinaryInPath(QStringLiteral("qrencode"));
  if (found.isEmpty()) {
    return false;
  }
  QtPassSettings::setQrencodeExecutable(found);
  return true;
#endif
}

void ConfigDialog::useTrayIcon(bool useSystray) {
  if (QSystemTrayIcon::isSystemTrayAvailable()) {
    ui->checkBoxUseTrayIcon->setChecked(useSystray);
    ui->checkBoxHideOnClose->setEnabled(useSystray);
    ui->checkBoxStartMinimized->setEnabled(useSystray);

    if (!useSystray) {
      ui->checkBoxHideOnClose->setChecked(false);
      ui->checkBoxStartMinimized->setChecked(false);
    }
  }
}

void ConfigDialog::on_checkBoxUseTrayIcon_clicked() {
  bool state = ui->checkBoxUseTrayIcon->isChecked();
  ui->checkBoxHideOnClose->setEnabled(state);
  ui->checkBoxStartMinimized->setEnabled(state);
}

void ConfigDialog::useGit(bool useGit) {
  ui->checkBoxUseGit->setChecked(useGit);
  on_checkBoxUseGit_clicked();
}

void ConfigDialog::useOtp(bool useOtp) {
  ui->checkBoxUseOtp->setChecked(useOtp);
}

void ConfigDialog::useGrepSearch(bool useGrepSearch) {
  ui->checkBoxUseGrepSearch->setChecked(useGrepSearch);
}

void ConfigDialog::useQrencode(bool useQrencode) {
  ui->checkBoxUseQrencode->setChecked(useQrencode);
}

void ConfigDialog::on_checkBoxUseGit_clicked() {
  ui->checkBoxAddGPGId->setEnabled(ui->checkBoxUseGit->isChecked());
  ui->checkBoxAutoPull->setEnabled(ui->checkBoxUseGit->isChecked());
  ui->checkBoxAutoPush->setEnabled(ui->checkBoxUseGit->isChecked());
}

void ConfigDialog::on_toolButtonPwgen_clicked() {
  QString pwgen = selectExecutable();
  if (!pwgen.isEmpty()) {
    ui->pwgenPath->setText(pwgen);
    ui->checkBoxUsePwgen->setEnabled(true);
  } else {
    ui->checkBoxUsePwgen->setEnabled(false);
    ui->checkBoxUsePwgen->setChecked(false);
  }
}

void ConfigDialog::setPwgenPath(const QString &pwgen) {
  ui->pwgenPath->setText(pwgen);
  if (pwgen.isEmpty()) {
    ui->checkBoxUsePwgen->setChecked(false);
    ui->checkBoxUsePwgen->setEnabled(false);
  }
  on_checkBoxUsePwgen_clicked();
}

void ConfigDialog::on_checkBoxUsePwgen_clicked() {
  if (ui->radioButtonPass->isChecked())
    return;
  bool usePwgen = ui->checkBoxUsePwgen->isChecked();
  ui->checkBoxAvoidCapitals->setEnabled(usePwgen);
  ui->checkBoxAvoidNumbers->setEnabled(usePwgen);
  ui->checkBoxLessRandom->setEnabled(usePwgen);
  ui->checkBoxUseSymbols->setEnabled(usePwgen);
  ui->lineEditPasswordChars->setEnabled(!usePwgen);
  ui->labelPasswordChars->setEnabled(!usePwgen);
  ui->passwordCharTemplateSelector->setEnabled(!usePwgen);
}

void ConfigDialog::usePwgen(bool usePwgen) {
  if (ui->pwgenPath->text().isEmpty()) {
    usePwgen = false;
  }
  ui->checkBoxUsePwgen->setChecked(usePwgen);
  on_checkBoxUsePwgen_clicked();
}

void ConfigDialog::setPasswordConfiguration(
    const PasswordConfiguration &config) {
  // Retain the custom charset so it survives even when a builtin set is the
  // active selection (the line edit then shows the builtin's characters).
  m_customPasswordChars = config.Characters[PasswordConfiguration::CUSTOM];
  ui->spinBoxPasswordLength->setValue(config.length);
  ui->passwordCharTemplateSelector->setCurrentIndex(config.selected);
  if (config.selected != PasswordConfiguration::CUSTOM) {
    ui->lineEditPasswordChars->setEnabled(false);
  }
  ui->lineEditPasswordChars->setText(config.Characters[config.selected]);
}

auto ConfigDialog::getPasswordConfiguration() -> PasswordConfiguration {
  PasswordConfiguration config;
  config.length = ui->spinBoxPasswordLength->value();
  config.selected = static_cast<PasswordConfiguration::CharacterSet>(
      ui->passwordCharTemplateSelector->currentIndex());
  // For a builtin selection the line edit shows the builtin's characters;
  // reading it would overwrite the saved custom charset.
  if (config.selected == PasswordConfiguration::CUSTOM) {
    config.Characters[PasswordConfiguration::CUSTOM] =
        ui->lineEditPasswordChars->text();
  } else {
    config.Characters[PasswordConfiguration::CUSTOM] = m_customPasswordChars;
  }
  return config;
}

void ConfigDialog::on_passwordCharTemplateSelector_activated(int index) {
  ui->lineEditPasswordChars->setText(
      QtPassSettings::getPasswordConfiguration().Characters[index]);
  if (index == PasswordConfiguration::CUSTOM) {
    ui->lineEditPasswordChars->setEnabled(true);
  } else {
    ui->lineEditPasswordChars->setEnabled(false);
  }
}

void ConfigDialog::onProfileSelected(int row) { loadProfileForm(row); }

void ConfigDialog::onProfileNameEdited(const QString &name) {
  ProfileEntry *entry = currentEntry();
  if (m_loadingForm || entry == nullptr) {
    return;
  }
  entry->name = name;
  if (QListWidgetItem *item = ui->profileList->item(m_currentEntry)) {
    item->setText(name);
  }
  validate();
  updateProfileStatus();
}

void ConfigDialog::onProfilePathEdited(const QString &path) {
  ProfileEntry *entry = currentEntry();
  if (m_loadingForm || entry == nullptr) {
    return;
  }
  entry->profile.path = path;
  validate();
  updateProfileStatus();
}

void ConfigDialog::onProfileSigningKeyEdited(const QString &key) {
  ProfileEntry *entry = currentEntry();
  if (m_loadingForm || entry == nullptr) {
    return;
  }
  entry->profile.signingKey = key;
  validate();
}

// A Git box was clicked: the profile now has its own flags.
void ConfigDialog::onProfileGitToggled() {
  ProfileEntry *entry = currentEntry();
  if (m_loadingForm || entry == nullptr) {
    return;
  }
  entry->profile.useGit = ui->profileUseGit->isChecked();
  entry->profile.autoPush = ui->profileAutoPush->isChecked();
  entry->profile.autoPull = ui->profileAutoPull->isChecked();
  ui->profileAutoPush->setEnabled(ui->profileUseGit->isChecked());
  ui->profileAutoPull->setEnabled(ui->profileUseGit->isChecked());
}

void ConfigDialog::updateProfileStatus() {
  const ProfileEntry *entry = currentEntry();
  if (entry == nullptr) {
    ui->statusLabel->setText(QString());
    return;
  }

  QString statusMessage;
  if (!entry->name.isEmpty() && !entry->profile.path.isEmpty()) {
    const QString path = QDir::cleanPath(entry->profile.path);
    if (!QDir(path).exists()) {
      statusMessage = tr("New profile: %1 at %2").arg(entry->name, path);
    } else {
      statusMessage = tr("Profile: %1 at %2").arg(entry->name, path);
    }
  } else {
    statusMessage = tr("Fill in all required fields");
  }

  ui->statusLabel->setText(statusMessage);
}
