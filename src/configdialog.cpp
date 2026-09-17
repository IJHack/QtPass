// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "configdialog.h"
#include "appsettings.h"
#include "keygendialog.h"
#include "passbackendfactory.h"
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
#include <QSystemTrayIcon>
#include <utility>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "qtpasslogging.h"

/**
 * @brief ConfigDialog::ConfigDialog this sets up the configuration screen.
 * @param parent
 */
ConfigDialog::ConfigDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::ConfigDialog) {
  ui->setupUi(this);

  WindowStateStore::attach(*this, QStringLiteral("configDialog"));

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
  // checkBoxUseOtp is deliberately not hidden here any more: one-time
  // passwords are generated in-process, so the feature no longer depends on
  // the Unix-only pass-otp extension.
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
  setProfiles(QtPassSettings::getProfiles(), QtPassSettings::getProfile());

  ui->label->setText(ui->label->text() + VERSION);
  ui->comboBoxClipboard->clear();

  ui->comboBoxClipboard->addItem(tr("No Clipboard"));
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

  if (!Util::configIsValid(s)) {
    // Show Programs tab, which is likely
    // what the user needs to fix now.
    ui->tabWidget->setCurrentIndex(1);
  }

  connect(this, &ConfigDialog::accepted, this, &ConfigDialog::on_accepted);
}

void ConfigDialog::applySettings(const AppSettings &settings) {
  ui->passPath->setText(settings.passExecutable);
  setGitPath(settings.gitExecutable);
  ui->gpgPath->setText(settings.gpgExecutable);
  ui->storePath->setText(settings.passStore);
  ui->sshAuthSockOverride->setText(settings.sshAuthSockOverride);

  ui->spinBoxAutoclearSeconds->setValue(settings.autoclearSeconds);
  ui->spinBoxAutoclearPanelSeconds->setValue(settings.autoclearPanelSeconds);
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
  ui->checkBoxTemplateAllFields->setChecked(settings.templateAllFields);
  ui->checkBoxShowProcessOutput->setChecked(settings.showProcessOutput);
  ui->checkBoxAutoPull->setChecked(settings.autoPull);
  ui->checkBoxAutoPush->setChecked(settings.autoPush);
  ui->checkBoxAlwaysOnTop->setChecked(settings.alwaysOnTop);

  // Dependent helpers: set after the plain values above so the enable/disable
  // logic they trigger sees the final widget states.
  usePass(settings.usePass);
  useAutoclear(settings.useAutoclear);
  useAutoclearPanel(settings.useAutoclearPanel);
  useTrayIcon(settings.useTrayIcon);
  useGit(settings.useGit);
  useOtp(settings.useOtp);
  useGrepSearch(settings.useGrepSearch);
  useQrencode(settings.useQrencode);
  // Restore the persisted pwgen path and password-generation settings before
  // toggling usePwgen (which is gated on a non-empty pwgen path). Without this
  // the Password-tab widgets keep their .ui defaults and readSettings() writes
  // those defaults back over the user's saved configuration on every OK.
  setPwgenPath(settings.pwgenExecutable);
  setPasswordConfiguration(settings.passwordConfiguration);
  usePwgen(settings.usePwgen);
  useTemplate(settings.useTemplate);
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
      static_cast<Enums::clipBoardType>(ui->comboBoxClipboard->currentIndex());
  settings.useSelection = ui->checkBoxSelection->isChecked();
  settings.useAutoclear = ui->checkBoxAutoclear->isChecked();
  settings.autoclearSeconds = ui->spinBoxAutoclearSeconds->value();
  settings.useAutoclearPanel = ui->checkBoxAutoclearPanel->isChecked();
  settings.autoclearPanelSeconds = ui->spinBoxAutoclearPanelSeconds->value();
  settings.hidePassword = ui->checkBoxHidePassword->isChecked();
  settings.hideContent = ui->checkBoxHideContent->isChecked();
  settings.useMonospace = ui->checkBoxUseMonospace->isChecked();
  settings.displayAsIs = ui->checkBoxDisplayAsIs->isChecked();
  settings.noLineWrapping = ui->checkBoxNoLineWrapping->isChecked();
  settings.addGPGId = ui->checkBoxAddGPGId->isChecked();
  // Only overwrite these environment-gated preferences when their control is
  // enabled. When disabled (e.g. no system tray available) keep the persisted
  // value loaded above rather than clobbering it to false, so the preference
  // survives until the user can change it on a capable environment.
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
  settings.useTemplate = ui->checkBoxUseTemplate->isChecked();
  settings.passTemplate = ui->plainTextEditTemplate->toPlainText();
  settings.templateAllFields = ui->checkBoxTemplateAllFields->isChecked();
  settings.showProcessOutput = ui->checkBoxShowProcessOutput->isChecked();
  settings.alwaysOnTop = ui->checkBoxAlwaysOnTop->isChecked();
  settings.version = VERSION;

  return settings;
}

/**
 * @brief ConfigDialog::~ConfigDialog config destructor.
 */
ConfigDialog::~ConfigDialog() = default;

/**
 * @brief ConfigDialog::setGitPath set the git executable path.
 * Make sure the checkBoxUseGit is updated.
 * @param path
 */
void ConfigDialog::setGitPath(const QString &path) {
  ui->gitPath->setText(path);
  ui->checkBoxUseGit->setEnabled(!path.isEmpty());
  if (path.isEmpty()) {
    useGit(false);
  }
}

/**
 * @brief ConfigDialog::usePass set whether or not we want to use pass.
 * Update radio buttons accordingly.
 * @param usePass
 */
void ConfigDialog::usePass(bool usePass) {
  ui->radioButtonNative->setChecked(!usePass);
  ui->radioButtonPass->setChecked(usePass);
  setGroupBoxState();
}

/**
 * @brief Mark the profiles that cannot be saved and gate OK on all of them
 * being fine: every profile needs a name and a path, and no two profiles may
 * share a name (the settings key them by it, so a duplicate would silently
 * overwrite the other). The offending rows are painted in the list and the
 * form field of the current one carries the reason as its tooltip.
 */
void ConfigDialog::validate() {
  bool status = true;
  QHash<QString, int> names;
  for (const ProfileEntry &entry : std::as_const(m_entries)) {
    names[entry.name]++;
  }
  for (int row = 0; row < m_entries.size(); ++row) {
    const ProfileEntry &entry = m_entries.at(row);
    QString nameProblem;
    if (entry.name.isEmpty()) {
      nameProblem = tr("This field is required");
    } else if (names.value(entry.name) > 1) {
      nameProblem = tr("Another profile already has this name");
    }
    const QString pathProblem =
        entry.profile.path.isEmpty() ? tr("This field is required") : QString();
    QListWidgetItem *item = ui->profileList->item(row);
    if (item != nullptr) {
      const bool bad = !nameProblem.isEmpty() || !pathProblem.isEmpty();
      item->setBackground(bad ? QBrush(Qt::red) : QBrush());
      item->setToolTip(!nameProblem.isEmpty() ? nameProblem : pathProblem);
    }
    if (row == m_currentEntry) {
      ui->profileName->setToolTip(nameProblem);
      ui->profilePath->setToolTip(pathProblem);
    }
    status = status && nameProblem.isEmpty() && pathProblem.isEmpty();
  }
  ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(status);
}

/**
 * @brief Saves the configuration dialog settings to persistent application
 * settings.
 * @example
 * ConfigDialog dialog;
 * dialog.on_accepted();
 * // Expected output: All UI-selected configuration values are stored via
 * QtPassSettings.
 *
 * @return void - This method does not return a value.
 */
void ConfigDialog::on_accepted() {
  const QString sshAuthSockOverride = ui->sshAuthSockOverride->text().trimmed();
  if (!sshAuthSockOverride.isEmpty()) {
    QString reason;
    switch (SshAuthSock::overrideStatus(sshAuthSockOverride)) {
    case SshAuthSock::OverrideStatus::Valid:
      break;
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
    if (!reason.isEmpty()) {
      QMessageBox::warning(
          this, tr("Potentially invalid SSH_AUTH_SOCK override"),
          tr("The SSH_AUTH_SOCK override value may be invalid.\n\n%1\n\n"
             "The value will still be saved as entered.")
              .arg(reason));
    }
  }

  const Profiles existingProfiles = QtPassSettings::getProfiles();

  // Persist via the facade, which also invalidates the cached Pass backend so
  // a changed "use pass" mode takes effect.
  QtPassSettings::save(readSettings());

  // Profiles are not part of AppSettings yet, so persist them separately.
  const Profiles profiles = getProfiles();
  QtPassSettings::setProfiles(profiles);

  // The active profile's own Git flags replace the global ones, as they do
  // when switching to it (MainWindow::on_profileBox_currentTextChanged).
  const auto active = profiles.constFind(QtPassSettings::getProfile());
  if (active != profiles.constEnd() &&
      (active->useGit.has_value() || active->autoPush.has_value() ||
       active->autoPull.has_value())) {
    AppSettings s = QtPassSettings::load();
    s.useGit = active->useGit.value_or(s.useGit);
    s.autoPush = active->autoPush.value_or(s.autoPush);
    s.autoPull = active->autoPull.value_or(s.autoPull);
    QtPassSettings::save(s);
  }

  // Initialize new profiles that need pass/git initialization
  initializeNewProfiles(existingProfiles);
}

/**
 * @brief Automatically detects required external binaries in the system PATH
 * and updates the dialog fields.
 * @example
 * ConfigDialog configDialog;
 * configDialog.on_autodetectButton_clicked();
 *
 * @return void - This function does not return a value.
 */
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

/**
 * @brief ConfigDialog::on_radioButtonNative_clicked wrapper for
 * ConfigDialog::setGroupBoxState()
 */
void ConfigDialog::on_radioButtonNative_clicked() { setGroupBoxState(); }

/**
 * @brief ConfigDialog::on_radioButtonPass_clicked wrapper for
 * ConfigDialog::setGroupBoxState()
 */
void ConfigDialog::on_radioButtonPass_clicked() { setGroupBoxState(); }

/**
 * @brief ConfigDialog::getSecretKeys get list of secret/private keys
 * @return QStringList keys
 */
auto ConfigDialog::getSecretKeys() -> QStringList {
  const QList<UserInfo> keys = QtPassSettings::getPass()->listKeys("", true);
  QStringList names;

  if (keys.empty()) {
    return names;
  }

  for (const UserInfo &sec : keys)
    names << sec.name;

  return names;
}

/**
 * @brief ConfigDialog::setGroupBoxState update checkboxes.
 */
void ConfigDialog::setGroupBoxState() {
  bool state = ui->radioButtonPass->isChecked();
  ui->groupBoxNative->setEnabled(!state);
  ui->groupBoxPass->setEnabled(state);
  if (state) {
    // pass mode: disable all password generation controls
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

/**
 * @brief ConfigDialog::selectExecutable pop-up to choose an executable.
 * @return
 */
auto ConfigDialog::selectExecutable() -> QString {
  QFileDialog dialog(this);
  dialog.setFileMode(QFileDialog::ExistingFile);
  dialog.setOption(QFileDialog::ReadOnly);
  if (dialog.exec()) {
    return dialog.selectedFiles().constFirst();
  }

  return {};
}

/**
 * @brief ConfigDialog::selectFolder pop-up to choose a folder.
 * @return
 */
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

/**
 * @brief ConfigDialog::on_toolButtonGit_clicked get git application.
 * Enable checkboxes if found.
 */
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

/**
 * @brief ConfigDialog::on_toolButtonGpg_clicked get gpg application.
 */
void ConfigDialog::on_toolButtonGpg_clicked() {
  QString gpg = selectExecutable();
  if (!gpg.isEmpty()) {
    ui->gpgPath->setText(gpg);
  }
}

/**
 * @brief ConfigDialog::on_pushButtonGenerateKey_clicked open keygen dialog.
 */
void ConfigDialog::on_pushButtonGenerateKey_clicked() {
  KeygenDialog d(ui->gpgPath->text(), QtPassSettings::getPass(), this);
  d.exec();
}

/**
 * @brief ConfigDialog::on_toolButtonPass_clicked get pass application.
 */
void ConfigDialog::on_toolButtonPass_clicked() {
  QString pass = selectExecutable();
  if (!pass.isEmpty()) {
    ui->passPath->setText(pass);
  }
}

/**
 * @brief ConfigDialog::on_toolButtonStore_clicked get .password-store
 * location.
 */
void ConfigDialog::on_toolButtonStore_clicked() {
  QString store = selectFolder();
  if (!store.isEmpty()) {
    ui->storePath->setText(store);
  }
}

/**
 * @brief ConfigDialog::on_comboBoxClipboard_activated show and hide options.
 * @param index of selectbox (0 = no clipboard).
 */
void ConfigDialog::on_comboBoxClipboard_activated(int index) {
  bool state = index > 0;

  ui->checkBoxSelection->setEnabled(state);
  ui->checkBoxAutoclear->setEnabled(state);
  ui->checkBoxHidePassword->setEnabled(state);
  ui->checkBoxHideContent->setEnabled(state);
  if (state) {
    ui->spinBoxAutoclearSeconds->setEnabled(ui->checkBoxAutoclear->isChecked());
    ui->labelSeconds->setEnabled(ui->checkBoxAutoclear->isChecked());
  } else {
    ui->spinBoxAutoclearSeconds->setEnabled(false);
    ui->labelSeconds->setEnabled(false);
  }
}

/**
 * @brief ConfigDialog::on_checkBoxAutoclearPanel_clicked enable and disable
 * options based on autoclear use.
 */
void ConfigDialog::on_checkBoxAutoclearPanel_clicked() {
  bool state = ui->checkBoxAutoclearPanel->isChecked();
  ui->spinBoxAutoclearPanelSeconds->setEnabled(state);
  ui->labelPanelSeconds->setEnabled(state);
}

/**
 * @brief ConfigDialog::useSelection set the clipboard type use from
 * MainWindow.
 * @param useSelection
 */
void ConfigDialog::useSelection(bool useSelection) {
  ui->checkBoxSelection->setChecked(useSelection);
  on_checkBoxSelection_clicked();
}

/**
 * @brief ConfigDialog::useAutoclear set the clipboard autoclear use from
 * MainWindow.
 * @param useAutoclear
 */
void ConfigDialog::useAutoclear(bool useAutoclear) {
  ui->checkBoxAutoclear->setChecked(useAutoclear);
  on_checkBoxAutoclear_clicked();
}

/**
 * @brief ConfigDialog::useAutoclearPanel set the panel autoclear use from
 * MainWindow.
 * @param useAutoclearPanel
 */
void ConfigDialog::useAutoclearPanel(bool useAutoclearPanel) {
  ui->checkBoxAutoclearPanel->setChecked(useAutoclearPanel);
  on_checkBoxAutoclearPanel_clicked();
}

/**
 * @brief ConfigDialog::on_checkBoxSelection_clicked checkbox clicked, update
 * state via ConfigDialog::on_comboBoxClipboard_activated
 */
void ConfigDialog::on_checkBoxSelection_clicked() {
  on_comboBoxClipboard_activated(ui->comboBoxClipboard->currentIndex());
}

/**
 * @brief ConfigDialog::on_checkBoxAutoclear_clicked checkbox clicked, update
 * state via ConfigDialog::on_comboBoxClipboard_activated
 */
void ConfigDialog::on_checkBoxAutoclear_clicked() {
  on_comboBoxClipboard_activated(ui->comboBoxClipboard->currentIndex());
}

/**
 * @brief ConfigDialog::setProfiles set the profiles and chosen profile from
 * MainWindow.
 * @param profiles
 * @param currentProfile
 */
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
    m_entries.append({i.key(), i.value()});
    ui->profileList->addItem(i.key());
  }
  ui->profileList->setCurrentRow(currentRow);
  if (currentRow < 0) {
    loadProfileForm(-1);
  }
  validate();
}

/**
 * @brief The entry the form is showing.
 * @return The entry, or nullptr when nothing is selected.
 */
auto ConfigDialog::currentEntry() -> ProfileEntry * {
  if (m_currentEntry < 0 || m_currentEntry >= m_entries.size()) {
    return nullptr;
  }
  return &m_entries[m_currentEntry];
}

/**
 * @brief Show one profile in the form, or clear and disable the form.
 *
 * The Git boxes show the profile's own flags when it has them and the
 * global ones from the Settings tab otherwise; only a click on a box turns
 * the value into the profile's own.
 * @param row Index into m_entries, or -1 for none.
 */
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
}

/**
 * @brief ConfigDialog::getProfiles return profile list.
 * @return The profiles as edited, keyed by name.
 */
auto ConfigDialog::getProfiles() -> Profiles {
  Profiles profiles;
  for (const ProfileEntry &entry : std::as_const(m_entries)) {
    profiles.insert(entry.name, entry.profile);
  }
  return profiles;
}

/**
 * @brief Initialize new profiles that need pass/git initialization.
 * @param existingProfiles The profiles that existed before the dialog was
 * opened.
 */
void ConfigDialog::initializeNewProfiles(const Profiles &existingProfiles) {
  const Profiles newProfiles = getProfiles();

  // Collect keys and sort for deterministic iteration
  // QMap iterates in name order.
  for (auto it = newProfiles.cbegin(); it != newProfiles.cend(); ++it) {
    const QString &name = it.key();
    const Profile &profile = it.value();
    const QString &path = profile.path;

    // Skip if already existed before (check by name and path)
    if (const auto old = existingProfiles.constFind(name);
        old != existingProfiles.constEnd() && old->path == path) {
      continue;
    }

    // This is a new profile - create directory if needed and initialize
    // Note: needsInit returns false for non-existent directories, so we
    // must create the directory first.
    QString cleanPath = QDir::cleanPath(path);
    QDir dir(cleanPath);
    if (!dir.exists()) {
      if (QMessageBox::question(
              this, tr("Create profile directory?"),
              tr("Would you like to create a password store at %1?")
                  .arg(cleanPath),
              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        continue;
      }
      if (!QDir().mkpath(cleanPath)) {
        QMessageBox::warning(
            this, tr("Error"),
            tr("Could not create profile directory: %1").arg(cleanPath));
        continue;
      }
    }

    // Now check if initialization is needed (directory exists with no .gpg-id)
    if (!ProfileInit::needsInit(cleanPath)) {
      continue;
    }

    // The dialog only collects the recipients here (it lists keys through
    // the active backend, which is fine); the folder is initialised without
    // that backend, whose settings snapshot, PASSWORD_STORE_DIR and process
    // queue all belong to the active store (#1774). It gets the profile as
    // its store: Pass::getGpgIdPath() falls back to <store>/.gpg-id for a
    // folder outside the store, which would pre-tick the active store's
    // recipients for the new profile.
    const AppSettings settings = QtPassSettings::load();
    AppSettings profileSettings = settings;
    profileSettings.passStore = Util::normalizeFolderPath(cleanPath);
    // Signing keys are per profile, exactly as the profile switch applies
    // them (MainWindow::on_profileBox_currentTextChanged).
    profileSettings.passSigningKey = profile.signingKey;
    UsersDialog usersDialog(QtPassSettings::getPass(), profileSettings,
                            profileSettings.passStore, this);
    usersDialog.setInitOnAccept(false);
    usersDialog.setWindowTitle(tr("Select recipients for %1").arg(name));
    if (usersDialog.exec() != QDialog::Accepted) {
      continue;
    }

    // Use per-profile useGit setting, falling back to global if not set
    const bool useGit = profile.useGit.value_or(settings.useGit);

    QString note;
    const bool ok = ProfileInit::initialise(
        cleanPath, usersDialog.selectedUsers(), profileSettings, useGit, &note);
    if (!ok) {
      QMessageBox::warning(
          this, tr("Could not initialise profile %1").arg(name), note);
    } else if (!note.isEmpty()) {
      QMessageBox::information(this, tr("Profile %1").arg(name), note);
    }
  }
}

/**
 * @brief ConfigDialog::on_addButton_clicked add a profile and start editing
 * its name.
 */
void ConfigDialog::on_addButton_clicked() {
  ProfileEntry entry;
  entry.name = tr("New Profile");
  entry.profile.path = ui->storePath->text();
  m_entries.append(entry);
  ui->profileList->addItem(entry.name);
  ui->profileList->setCurrentRow(m_entries.size() - 1);
  ui->profileName->setFocus();
  ui->profileName->selectAll();
  validate();
}

/**
 * @brief Pick the store folder for the profile in the form.
 */
void ConfigDialog::on_profilePathBrowse_clicked() {
  const QString dir = selectFolder();
  if (!dir.isEmpty()) {
    ui->profilePath->setText(dir);
    onProfilePathEdited(dir);
  }
}

/**
 * @brief ConfigDialog::on_deleteButton_clicked forget the selected profile.
 */
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

/**
 * @brief ConfigDialog::criticalMessage wrapper for showing critical messages
 * in a popup.
 * @param title
 * @param text
 */
void ConfigDialog::criticalMessage(const QString &title, const QString &text) {
  QMessageBox::critical(this, title, text, QMessageBox::Ok, QMessageBox::Ok);
}

/**
 * @brief Checks whether the qrencode executable is available on the system.
 *
 * A configured path that points at an executable file is accepted as-is.
 * Otherwise qrencode is looked up on Util's PATH (which carries the macOS and
 * Windows additions) without spawning a subprocess; a hit is stored so the
 * QR display can use it, a miss leaves the stored path untouched.
 *
 * @param configuredPath The qrencode path currently stored in the settings.
 * @return bool - True if qrencode is available; otherwise false. On Windows,
 * always returns false.
 */
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

/**
 * @brief ConfigDialog::wizard first-time use wizard.
 */
void ConfigDialog::wizard() {
  (void)Util::configIsValid(QtPassSettings::load());
  on_autodetectButton_clicked();

  if (!checkGpgExistence()) {
    return;
  }
  if (!checkSecretKeys()) {
    return;
  }
  if (!checkPasswordStore()) {
    return;
  }
  handleGpgIdFile();

  ui->checkBoxHidePassword->setCheckState(Qt::Checked);
}

/**
 * @brief Checks whether the configured GnuPG executable exists.
 * @example
 * bool result = ConfigDialog::checkGpgExistence();
 * std::cout << result << std::endl; // Expected output: true if GnuPG is found,
 * false otherwise
 *
 * @return bool - True if the GnuPG path is valid or uses a WSL command prefix;
 * false if the executable cannot be found and an error message is shown.
 */
auto ConfigDialog::checkGpgExistence() -> bool {
  QString gpg = ui->gpgPath->text();
  if (!gpg.startsWith("wsl ") && !QFile(gpg).exists()) {
    criticalMessage(
        tr("GnuPG not found"),
#ifdef Q_OS_WIN
        tr("Please install GnuPG on your system.<br>Install "
           "<strong>Ubuntu</strong> from the Microsoft Store<br>or <a "
           "href=\"https://www.gnupg.org/download/#sec-1-2\">download</a> it "
           "from GnuPG.org")
#else
        tr("Please install GnuPG on your system.<br>Install "
           "<strong>gpg</strong> using your favorite package manager<br>or "
           "<a "
           "href=\"https://www.gnupg.org/download/#sec-1-2\">download</a> it "
           "from GnuPG.org")
#endif
    );
    return false;
  }
  return true;
}

/**
 * @brief Checks whether secret keys are available and, if needed, prompts the
 * user to generate them.
 * @example
 * bool result = ConfigDialog.checkSecretKeys();
 * std::cout << result << std::endl; // Expected output: true if keys are
 * present or key generation dialog is accepted, false otherwise
 *
 * @return bool - Returns true when secret keys are already available or the key
 * generation dialog is accepted; false if the dialog is rejected.
 */
auto ConfigDialog::checkSecretKeys() -> bool {
  QString gpg = ui->gpgPath->text();
  QStringList names = getSecretKeys();

  qCDebug(lcQtPass) << names;

  if ((gpg.startsWith("wsl ") || QFile(gpg).exists()) && names.empty()) {
    KeygenDialog d(gpg, QtPassSettings::getPass(), this);
    return d.exec() == QDialog::Accepted;
  }
  return true;
}

/**
 * @brief Checks whether the password-store path exists and prompts the user to
 * create it if it does not.
 * @example
 * bool result = ConfigDialog::checkPasswordStore();
 * std::cout << std::boolalpha << result << std::endl; // Expected output: true
 * if the store exists or is created successfully, false if creation fails
 *
 * @return bool - True if the password-store exists or is successfully created,
 * false if creation fails.
 */
auto ConfigDialog::checkPasswordStore() -> bool {
  QString passStore = ui->storePath->text();

  if (!QFile(passStore).exists()) {
    if (QMessageBox::question(
            this, tr("Create password-store?"),
            tr("Would you like to create a password-store at %1?")
                .arg(passStore),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      if (!QDir().mkdir(passStore)) {
        QMessageBox::warning(
            this, tr("Error"),
            tr("Failed to create password-store at: %1").arg(passStore));
        return false;
      }
#ifdef Q_OS_WIN
      SetFileAttributes(passStore.toStdWString().c_str(),
                        FILE_ATTRIBUTE_HIDDEN);
#endif
      selectRecipients(passStore, ui->checkBoxUseGit->isChecked());
    }
  }
  return true;
}

/**
 * @brief Handles selection and validation of the password store's .gpg-id file.
 * @example
 * ConfigDialog dialog;
 * dialog.handleGpgIdFile();
 *
 * @return void - This method does not return a value; it updates the UI flow
 * and may prompt the user to choose a valid password store.
 */
void ConfigDialog::handleGpgIdFile() {
  QString passStore = ui->storePath->text();
  if (!QFile(QDir(passStore).filePath(".gpg-id")).exists()) {
    qCDebug(lcQtPass) << ".gpg-id file does not exist";
    criticalMessage(tr("Password store not initialised"),
                    tr("The folder %1 doesn't seem to be a password store or "
                       "is not yet initialised.")
                        .arg(passStore));

    while (!QFile(passStore).exists()) {
      on_toolButtonStore_clicked();
      if (passStore == ui->storePath->text()) {
        return;
      }
      passStore = ui->storePath->text();
    }
    if (!QFile(QDir(passStore).filePath(".gpg-id")).exists()) {
      qCDebug(lcQtPass) << ".gpg-id file still does not exist :/";
      selectRecipients(passStore, false);
    }
  }
}

/**
 * @brief Let the user pick the recipients for a store that has no .gpg-id yet.
 *
 * First-run wizard only. The store path is saved and the backend
 * re-initialised before use: Pass caches its settings at init(), so
 * RealPass::Init() would otherwise make `--path` relative to the previous
 * store and updateEnv() would export that one. The saved path is put back
 * if the dialog is not accepted; after OK the wizard saves this same path
 * anyway. Git is initialised first so that the .gpg-id written by pass init
 * lands in the first commit.
 * @param storePath Directory that becomes the password store.
 * @param gitInit Whether to run `git init` there before selecting recipients.
 */
void ConfigDialog::selectRecipients(const QString &storePath, bool gitInit) {
  // ImitatePass::Init() and Pass::getRecipientList() append ".gpg-id" to the
  // folder, so it has to keep its trailing separator.
  const QString store = Util::normalizeFolderPath(QDir::cleanPath(storePath));
  const QString prevStore = QtPassSettings::getPassStore();
  QtPassSettings::setPassStore(store);
  PassBackendFactory::invalidate();
  Pass *pass = QtPassSettings::getPass();
  // init() only records the settings; PASSWORD_STORE_DIR, which `pass`
  // prefers over its working directory, is exported by updateEnv().
  pass->updateEnv();
  if (gitInit) {
    pass->GitInit();
  }
  UsersDialog d(pass, QtPassSettings::load(), store, this);
  if (d.exec() != QDialog::Accepted) {
    // Only the saved key goes back; the backend keeps the new path so a git
    // init already queued still runs where it was meant to.
    QtPassSettings::setPassStore(prevStore);
  }
}

/**
 * @brief ConfigDialog::useTrayIcon set preference for using trayicon.
 * Enable or disable related checkboxes accordingly.
 * @param useSystray
 */
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

/**
 * @brief ConfigDialog::on_checkBoxUseTrayIcon_clicked enable and disable
 * related checkboxes.
 */
void ConfigDialog::on_checkBoxUseTrayIcon_clicked() {
  bool state = ui->checkBoxUseTrayIcon->isChecked();
  ui->checkBoxHideOnClose->setEnabled(state);
  ui->checkBoxStartMinimized->setEnabled(state);
}

/**
 * @brief ConfigDialog::useGit set preference for using git.
 * @param useGit
 */
void ConfigDialog::useGit(bool useGit) {
  ui->checkBoxUseGit->setChecked(useGit);
  on_checkBoxUseGit_clicked();
}

/**
 * @brief ConfigDialog::useOtp set preference for using otp plugin.
 * @param useOtp
 */
void ConfigDialog::useOtp(bool useOtp) {
  ui->checkBoxUseOtp->setChecked(useOtp);
}

void ConfigDialog::useGrepSearch(bool useGrepSearch) {
  ui->checkBoxUseGrepSearch->setChecked(useGrepSearch);
}

/**
 * @brief ConfigDialog::useQrencode set preference for using qrencode plugin.
 * @param useQrencode
 */
void ConfigDialog::useQrencode(bool useQrencode) {
  ui->checkBoxUseQrencode->setChecked(useQrencode);
}

/**
 * @brief ConfigDialog::on_checkBoxUseGit_clicked enable or disable related
 * checkboxes.
 */
void ConfigDialog::on_checkBoxUseGit_clicked() {
  ui->checkBoxAddGPGId->setEnabled(ui->checkBoxUseGit->isChecked());
  ui->checkBoxAutoPull->setEnabled(ui->checkBoxUseGit->isChecked());
  ui->checkBoxAutoPush->setEnabled(ui->checkBoxUseGit->isChecked());
}

/**
 * @brief ConfigDialog::on_toolButtonPwgen_clicked enable or disable related
 * options in the interface.
 */
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

/**
 * @brief ConfigDialog::setPwgenPath set pwgen executable path.
 * Enable or disable related options in the interface.
 * @param pwgen
 */
void ConfigDialog::setPwgenPath(const QString &pwgen) {
  ui->pwgenPath->setText(pwgen);
  if (pwgen.isEmpty()) {
    ui->checkBoxUsePwgen->setChecked(false);
    ui->checkBoxUsePwgen->setEnabled(false);
  }
  on_checkBoxUsePwgen_clicked();
}

/**
 * @brief ConfigDialog::on_checkBoxUsePwgen_clicked enable or disable related
 * options in the interface.
 */
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

/**
 * @brief ConfigDialog::usePwgen set preference for using pwgen (can be
 * overruled by empty pwgenPath).
 * enable or disable related options in the interface via
 * ConfigDialog::on_checkBoxUsePwgen_clicked
 * @param usePwgen
 */
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
  config.selected = static_cast<PasswordConfiguration::characterSet>(
      ui->passwordCharTemplateSelector->currentIndex());
  // The line edit only holds the user's custom charset while CUSTOM is
  // selected; for a builtin selection it shows that builtin's characters.
  // Reading it unconditionally would overwrite the saved custom charset with a
  // builtin string, so fall back to the retained custom value in that case.
  if (config.selected == PasswordConfiguration::CUSTOM) {
    config.Characters[PasswordConfiguration::CUSTOM] =
        ui->lineEditPasswordChars->text();
  } else {
    config.Characters[PasswordConfiguration::CUSTOM] = m_customPasswordChars;
  }
  return config;
}

/**
 * @brief ConfigDialog::on_passwordCharTemplateSelector_activated sets the
 * passwordChar Template
 * combo box to the desired entry
 * @param entry of
 */
void ConfigDialog::on_passwordCharTemplateSelector_activated(int index) {
  ui->lineEditPasswordChars->setText(
      QtPassSettings::getPasswordConfiguration().Characters[index]);
  if (index == PasswordConfiguration::CUSTOM) {
    ui->lineEditPasswordChars->setEnabled(true);
  } else {
    ui->lineEditPasswordChars->setEnabled(false);
  }
}

/**
 * @brief ConfigDialog::on_checkBoxUseTemplate_clicked enable or disable the
 * template field and options.
 */
void ConfigDialog::on_checkBoxUseTemplate_clicked() {
  ui->plainTextEditTemplate->setEnabled(ui->checkBoxUseTemplate->isChecked());
  ui->checkBoxTemplateAllFields->setEnabled(
      ui->checkBoxUseTemplate->isChecked());
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
}

/**
 * @brief A Git box was clicked: the profile now has its own flags.
 */
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

/**
 * @brief Update the status line with a preview of the profile in the form.
 */
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

/**
 * @brief ConfigDialog::useTemplate set preference for using templates.
 * @param useTemplate
 */
void ConfigDialog::useTemplate(bool useTemplate) {
  ui->checkBoxUseTemplate->setChecked(useTemplate);
  on_checkBoxUseTemplate_clicked();
}
