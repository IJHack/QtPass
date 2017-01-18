#include "configdialog.h"
#include "debughelper.h"
#include "debughelper.h"
#include "keygendialog.h"
#include "mainwindow.h"
#include "qtpasssettings.h"
#include "ui_configdialog.h"
#include <QDir>
#include <QMessageBox>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

/**
 * @brief ConfigDialog::ConfigDialog this sets up the configuration screen.
 * @param parent
 */
ConfigDialog::ConfigDialog(MainWindow *parent)
    : QDialog(parent), ui(new Ui::ConfigDialog) {
  mainWindow = parent;
  ui->setupUi(this);
  ui->profileTable->verticalHeader()->hide();
  ui->profileTable->horizontalHeader()->setStretchLastSection(true);
  ui->label->setText(ui->label->text() + VERSION);
  ui->comboBoxClipboard->clear();

  ui->comboBoxClipboard->addItem(tr("No Clipboard"));
  ui->comboBoxClipboard->addItem(tr("Always copy to clipboard"));
  ui->comboBoxClipboard->addItem(tr("On-demand copy to clipboard"));
  ui->comboBoxClipboard->setCurrentIndex(0);
}

/**
 * @brief ConfigDialog::~ConfigDialog config destructor, makes sure the
 * mainWindow knows about git, gpg and pass executables.
 */
ConfigDialog::~ConfigDialog() {
  QtPassSettings::setGitExecutable(ui->gitPath->text());
  QtPassSettings::setGpgExecutable(ui->gpgPath->text());
  QtPassSettings::setPassExecutable(ui->passPath->text());
}

/**
 * @brief ConfigDialog::setPassPath set the pass executable path.
 * @param path
 */
void ConfigDialog::setPassPath(QString path) { ui->passPath->setText(path); }

/**
 * @brief ConfigDialog::setGitPath set the git executable path.
 * Make sure the checkBoxUseGit is updated.
 * @param path
 */
void ConfigDialog::setGitPath(QString path) {
  ui->gitPath->setText(path);
  if (path.isEmpty()) {
    useGit(false);
    ui->checkBoxUseGit->setEnabled(false);
  } else {
    ui->checkBoxUseGit->setEnabled(true);
  }
}

/**
 * @brief ConfigDialog::setGpgPath set the gpg executable path.
 * @param path
 */
void ConfigDialog::setGpgPath(QString path) { ui->gpgPath->setText(path); }

/**
 * @brief ConfigDialog::setStorePath set the .password-store folder path.
 * @param path
 */
void ConfigDialog::setStorePath(QString path) { ui->storePath->setText(path); }

/**
 * @brief ConfigDialog::getPassPath return path to pass.
 * @return
 */
QString ConfigDialog::getPassPath() { return ui->passPath->text(); }

/**
 * @brief ConfigDialog::getGitPath return path to git.
 * @return
 */
QString ConfigDialog::getGitPath() { return ui->gitPath->text(); }

/**
 * @brief ConfigDialog::getGpgPath return path to gpg.
 * @return
 */
QString ConfigDialog::getGpgPath() { return ui->gpgPath->text(); }

/**
 * @brief ConfigDialog::getStorePath return path to .password-store.
 * @return
 */
QString ConfigDialog::getStorePath() { return ui->storePath->text(); }

/**
 * @brief ConfigDialog::usePass return wether or not we want to use pass (or
 * native gpg+git etc).
 * @return
 */
bool ConfigDialog::usePass() { return ui->radioButtonPass->isChecked(); }

/**
 * @brief ConfigDialog::usePass set wether or not we want to use pass.
 * Update radio buttons accordingly.
 * @param usePass
 */
void ConfigDialog::usePass(bool usePass) {
  if (usePass) {
    ui->radioButtonNative->setChecked(false);
    ui->radioButtonPass->setChecked(true);
  } else {
    ui->radioButtonNative->setChecked(true);
    ui->radioButtonPass->setChecked(false);
  }
  setGroupBoxState();
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
 * @brief ConfigDialog::setGroupBoxState update checkboxes.
 */
void ConfigDialog::setGroupBoxState() {
  if (ui->radioButtonPass->isChecked()) {
    ui->groupBoxNative->setEnabled(false);
    ui->groupBoxPass->setEnabled(true);
  } else {
    ui->groupBoxNative->setEnabled(true);
    ui->groupBoxPass->setEnabled(false);
  }
}

/**
 * @brief ConfigDialog::selectExecutable pop-up to choose an executable.
 * @return
 */
QString ConfigDialog::selectExecutable() {
  QFileDialog dialog(this);
  dialog.setFileMode(QFileDialog::ExistingFile);
  dialog.setOption(QFileDialog::ReadOnly);
  if (dialog.exec())
    return dialog.selectedFiles().first();
  else
    return "";
}

/**
 * @brief ConfigDialog::selectFolder pop-up to choose a folder.
 * @return
 */
QString ConfigDialog::selectFolder() {
  QFileDialog dialog(this);
  dialog.setFileMode(QFileDialog::Directory);
  dialog.setFilter(QDir::NoFilter);
  dialog.setOption(QFileDialog::ShowDirsOnly);
  if (dialog.exec())
    return dialog.selectedFiles().first();
  else
    return "";
}

/**
 * @brief ConfigDialog::on_toolButtonGit_clicked get git application.
 * Enable checkboxes if found.
 */
void ConfigDialog::on_toolButtonGit_clicked() {
  QString git = selectExecutable();
  if (!git.isEmpty()) {
    ui->gitPath->setText(git);
    ui->checkBoxUseGit->setEnabled(true);
  } else {
    useGit(false);
    ui->checkBoxUseGit->setEnabled(false);
  }
}

/**
 * @brief ConfigDialog::on_toolButtonGpg_clicked get gpg application.
 */
void ConfigDialog::on_toolButtonGpg_clicked() {
  QString gpg = selectExecutable();
  if (!gpg.isEmpty())
    ui->gpgPath->setText(gpg);
}

/**
 * @brief ConfigDialog::on_toolButtonPass_clicked get pass application.
 */
void ConfigDialog::on_toolButtonPass_clicked() {
  QString pass = selectExecutable();
  if (!pass.isEmpty())
    ui->passPath->setText(pass);
}

/**
 * @brief ConfigDialog::on_toolButtonStore_clicked get .password-store
 * location.s
 */
void ConfigDialog::on_toolButtonStore_clicked() {
  QString store = selectFolder();
  if (!store.isEmpty()) // TODO(annejan) call check
    ui->storePath->setText(store);
}

/**
 * @brief ConfigDialog::on_comboBoxClipboard_activated show and hide options.
 * @param index of selectbox (0 = no clipboard).
 */
void ConfigDialog::on_comboBoxClipboard_activated(int index) {
  if (index > 0) {
    ui->checkBoxSelection->setEnabled(true);
    ui->checkBoxAutoclear->setEnabled(true);
    ui->checkBoxHidePassword->setEnabled(true);
    ui->checkBoxHideContent->setEnabled(true);
    if (ui->checkBoxAutoclear->isChecked()) {
      ui->spinBoxAutoclearSeconds->setEnabled(true);
      ui->labelSeconds->setEnabled(true);
    } else {
      ui->spinBoxAutoclearSeconds->setEnabled(false);
      ui->labelSeconds->setEnabled(false);
    }
  } else {
    ui->checkBoxSelection->setEnabled(false);
    ui->checkBoxAutoclear->setEnabled(false);
    ui->spinBoxAutoclearSeconds->setEnabled(false);
    ui->labelSeconds->setEnabled(false);
    ui->checkBoxHidePassword->setEnabled(false);
    ui->checkBoxHideContent->setEnabled(false);
  }
}

/**
 * @brief ConfigDialog::on_checkBoxAutoclearPanel_clicked enable and disable
 * options based on autoclear use.
 */
void ConfigDialog::on_checkBoxAutoclearPanel_clicked() {
  if (ui->checkBoxAutoclearPanel->isChecked()) {
    ui->spinBoxAutoclearPanelSeconds->setEnabled(true);
    ui->labelPanelSeconds->setEnabled(true);
  } else {
    ui->spinBoxAutoclearPanelSeconds->setEnabled(false);
    ui->labelPanelSeconds->setEnabled(false);
  }
}

/**
 * @brief ConfigDialog::useClipboard set the clipboard use from MainWindow.
 */
void ConfigDialog::useClipboard(Enums::clipBoardType useClipboard) {
  ui->comboBoxClipboard->setCurrentIndex(static_cast<int>(useClipboard));
  on_comboBoxClipboard_activated(static_cast<int>(useClipboard));
}

/**
 * @brief ConfigDialog::useSelection set the clipboard autoclear use from
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
 * @brief ConfigDialog::setAutoclear set the clipboard autoclear timout from
 * MainWindow.
 * @param seconds
 */
void ConfigDialog::setAutoclear(int seconds) {
  ui->spinBoxAutoclearSeconds->setValue(seconds);
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
 * @brief ConfigDialog::setAutoclearPanel set the panel autoclear timout from
 * MainWindow.
 * @param seconds
 */
void ConfigDialog::setAutoclearPanel(int seconds) {
  ui->spinBoxAutoclearPanelSeconds->setValue(seconds);
}

/**
 * @brief ConfigDialog::useClipboard set the use of clipboard from MainWindow.
 * @return
 */
Enums::clipBoardType ConfigDialog::useClipboard() {
  return static_cast<Enums::clipBoardType>(
      ui->comboBoxClipboard->currentIndex());
}

/**
 * @brief ConfigDialog::useSelection return the use of clipboard autoclear.
 * @return
 */
bool ConfigDialog::useSelection() { return ui->checkBoxSelection->isChecked(); }

/**
 * @brief ConfigDialog::on_checkBoxSelection_clicked checkbox clicked, update
 * state via ConfigDialog::on_comboBoxClipboard_activated
 */
void ConfigDialog::on_checkBoxSelection_clicked() {
  on_comboBoxClipboard_activated(1);
}

/**
 * @brief ConfigDialog::useAutoclear return the use of clipboard autoclear.
 * @return
 */
bool ConfigDialog::useAutoclear() { return ui->checkBoxAutoclear->isChecked(); }

/**
 * @brief ConfigDialog::getAutoclear return the clipboard autoclear timout.
 * @return
 */
int ConfigDialog::getAutoclear() {
  return ui->spinBoxAutoclearSeconds->value();
}

/**
 * @brief ConfigDialog::on_checkBoxAutoclear_clicked checkbox clicked, update
 * state via ConfigDialog::on_comboBoxClipboard_activated
 */
void ConfigDialog::on_checkBoxAutoclear_clicked() {
  on_comboBoxClipboard_activated(1);
}

/**
 * @brief ConfigDialog::useAutoclearPanel return panel autoclear usage.
 * @return
 */
bool ConfigDialog::useAutoclearPanel() {
  return ui->checkBoxAutoclearPanel->isChecked();
}

/**
 * @brief ConfigDialog::getAutoclearPanel return panel autoclear timeout.
 * @return
 */
int ConfigDialog::getAutoclearPanel() {
  return ui->spinBoxAutoclearPanelSeconds->value();
}

/**
 * @brief ConfigDialog::hidePassword return preference for hiding passwords from
 * shoulder-surfers.
 * @return
 */
bool ConfigDialog::hidePassword() {
  return ui->checkBoxHidePassword->isChecked();
}

/**
 * @brief ConfigDialog::hideContent return preference for hiding all information
 * from shoulder-surfers.
 * @return
 */
bool ConfigDialog::hideContent() {
  return ui->checkBoxHideContent->isChecked();
}

/**
 * @brief ConfigDialog::hidePassword set preference for hiding passwords from
 * shoulder-surfers.
 * @param hidePassword
 */
void ConfigDialog::hidePassword(bool hidePassword) {
  ui->checkBoxHidePassword->setChecked(hidePassword);
}

/**
 * @brief ConfigDialog::hideContent set preference for hiding all content from
 * shoulder-surfers.
 * @param hideContent
 */
void ConfigDialog::hideContent(bool hideContent) {
  ui->checkBoxHideContent->setChecked(hideContent);
}

/**
 * @brief ConfigDialog::addGPGId return preference for always adding gpg-id
 * changes to git.
 * @return
 */
bool ConfigDialog::addGPGId() { return ui->checkBoxAddGPGId->isChecked(); }

/**
 * @brief ConfigDialog::addGPGId set preference for always adding gpg-id changes
 * to git.
 * @param addGPGId
 */
void ConfigDialog::addGPGId(bool addGPGId) {
  ui->checkBoxAddGPGId->setChecked(addGPGId);
}

/**
 * @brief ConfigDialog::genKey tunnel function to make MainWindow generate a gpg
 * key pair.
 * @todo refactor the process to not be entangled so much.
 * @param batch
 * @param dialog
 */
void ConfigDialog::genKey(QString batch, QDialog *dialog) {
  mainWindow->generateKeyPair(batch, dialog);
}

/**
 * @brief ConfigDialog::setProfiles set the profiles and chosen profile from
 * MainWindow.
 * @param profiles
 * @param profile
 */
void ConfigDialog::setProfiles(QHash<QString, QString> profiles,
                               QString profile) {
  // dbg()<< profiles;
  if (profiles.contains("")) {
    profiles.remove("");
    // remove weird "" key value pairs
  }

  ui->profileTable->setRowCount(profiles.count());
  QHashIterator<QString, QString> i(profiles);
  int n = 0;
  while (i.hasNext()) {
    i.next();
    if (!i.value().isEmpty() && !i.key().isEmpty()) {
      ui->profileTable->setItem(n, 0, new QTableWidgetItem(i.key()));
      ui->profileTable->setItem(n, 1, new QTableWidgetItem(i.value()));
      // dbg()<< "naam:" + i.key();
      if (i.key() == profile)
        ui->profileTable->selectRow(n);
    }
    ++n;
  }
}

/**
 * @brief ConfigDialog::getProfiles return profile list.
 * @return
 */
QHash<QString, QString> ConfigDialog::getProfiles() {
  QHash<QString, QString> profiles;
  // Check?
  for (int i = 0; i < ui->profileTable->rowCount(); ++i) {
    QTableWidgetItem *pathItem = ui->profileTable->item(i, 1);
    if (0 != pathItem) {
      QTableWidgetItem *item = ui->profileTable->item(i, 0);
      if (item == 0) {
        dbg() << "empty name, should fix in frontend";
        continue;
      }
      profiles.insert(item->text(), pathItem->text());
    }
  }
  return profiles;
}

/**
 * @brief ConfigDialog::on_addButton_clicked add a profile row.
 */
void ConfigDialog::on_addButton_clicked() {
  int n = ui->profileTable->rowCount();
  ui->profileTable->insertRow(n);
  ui->profileTable->setItem(n, 1, new QTableWidgetItem(ui->storePath->text()));
  ui->profileTable->selectRow(n);
  ui->deleteButton->setEnabled(true);
}

/**
 * @brief ConfigDialog::on_deleteButton_clicked remove a profile row.
 */
void ConfigDialog::on_deleteButton_clicked() {
  QSet<int> selectedRows; // we use a set to prevent doubles
  QList<QTableWidgetItem *> itemList = ui->profileTable->selectedItems();
  if (itemList.count() == 0) {
    QMessageBox::warning(this, tr("No profile selected"),
                         tr("No profile selected to delete"));
    return;
  }
  QTableWidgetItem *item;
  foreach (item, itemList)
    selectedRows.insert(item->row());
  // get a list, and sort it big to small
  QList<int> rows = selectedRows.toList();
  qSort(rows.begin(), rows.end());
  // now actually do the removing:
  foreach (int row, rows)
    ui->profileTable->removeRow(row);
  if (ui->profileTable->rowCount() < 1)
    ui->deleteButton->setEnabled(false);
}

/**
 * @brief ConfigDialog::criticalMessage weapper for showing critical messages in
 * a popup.
 * @param title
 * @param text
 */
void ConfigDialog::criticalMessage(const QString &title, const QString &text) {
  QMessageBox::critical(this, title, text, QMessageBox::Ok, QMessageBox::Ok);
}

/**
 * @brief ConfigDialog::wizard first-time use wizard.
 * @todo make this thing more reliable.
 */
void ConfigDialog::wizard() {
  // mainWindow->checkConfig();
  bool clean = false;

  QString gpg = ui->gpgPath->text();
  // QString gpg = mainWindow->getGpgExecutable();
  if (!QFile(gpg).exists()) {
    criticalMessage(
        tr("GnuPG not found"),
        tr("Please install GnuPG on your system.<br>Install "
           "<strong>gpg</strong> using your favorite package manager<br>or <a "
           "href=\"https://www.gnupg.org/download/#sec-1-2\">download</a> it "
           "from GnuPG.org"));
    clean = true;
  }

  QStringList names = mainWindow->getSecretKeys();
  dbg() << names;
  if (QFile(gpg).exists() && names.empty()) {
    KeygenDialog d(this);
    if (!d.exec())
      return;
  }

  QString passStore = ui->storePath->text();

  if (!QFile(passStore).exists()) {
    // TODO(annejan) pass version?
    if (QMessageBox::question(
            this, tr("Create password-store?"),
            tr("Would you like to create a password-store at %1?")
                .arg(passStore),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      QDir().mkdir(passStore);
#ifdef Q_OS_WIN
      SetFileAttributes(passStore.toStdWString().c_str(),
                        FILE_ATTRIBUTE_HIDDEN);
#endif
      if (useGit())
        mainWindow->executePassGitInit();
      mainWindow->userDialog(passStore);
    }
  }

  if (!QFile(QDir(passStore).filePath(".gpg-id")).exists()) {
    dbg() << ".gpg-id file does not exist";

    if (!clean) {
      criticalMessage(tr("Password store not initialised"),
                      tr("The folder %1 doesn't seem to be a password store or "
                         "is not yet initialised.")
                          .arg(passStore));
    }
    while (!QFile(passStore).exists()) {
      on_toolButtonStore_clicked();
      // allow user to cancel
      if (passStore == ui->storePath->text())
        return;
      passStore = ui->storePath->text();
    }
    if (!QFile(passStore + ".gpg-id").exists()) {
      dbg() << ".gpg-id file still does not exist :/";
      // appears not to be store
      // init yes / no ?
      mainWindow->userDialog(passStore);
    }
  }
}

/**
 * @brief ConfigDialog::useTrayIcon return preference for using a (system) tray
 * icon.
 * @return
 */
bool ConfigDialog::useTrayIcon() {
  return ui->checkBoxUseTrayIcon->isChecked();
}

/**
 * @brief ConfigDialog::hideOnClose return preference for hiding instead of
 * closing (quitting) application.
 * @return
 */
bool ConfigDialog::hideOnClose() {
  return ui->checkBoxHideOnClose->isEnabled() &&
         ui->checkBoxHideOnClose->isChecked();
}

/**
 * @brief ConfigDialog::useTrayIcon set preference for using trayicon.
 * Enable or disable related checkboxes accordingly.
 * @param useSystray
 */
void ConfigDialog::useTrayIcon(bool useSystray) {
  ui->checkBoxUseTrayIcon->setChecked(useSystray);
  ui->checkBoxHideOnClose->setEnabled(useSystray);
  ui->checkBoxStartMinimized->setEnabled(useSystray);
  if (!useSystray) {
    ui->checkBoxHideOnClose->setChecked(false);
    ui->checkBoxStartMinimized->setChecked(false);
  }
}

/**
 * @brief ConfigDialog::hideOnClose set preference for hiding instead of closing
 * (quitting) application.
 * @param hideOnClose
 */
void ConfigDialog::hideOnClose(bool hideOnClose) {
  ui->checkBoxHideOnClose->setChecked(hideOnClose);
}

/**
 * @brief ConfigDialog::on_checkBoxUseTrayIcon_clicked enable and disable
 * related checkboxes.
 */
void ConfigDialog::on_checkBoxUseTrayIcon_clicked() {
  if (ui->checkBoxUseTrayIcon->isChecked()) {
    ui->checkBoxHideOnClose->setEnabled(true);
    ui->checkBoxStartMinimized->setEnabled(true);
  } else {
    ui->checkBoxStartMinimized->setEnabled(false);
    ui->checkBoxHideOnClose->setEnabled(false);
  }
}

/**
 * @brief ConfigDialog::closeEvent close this window.
 * @param event
 */
void ConfigDialog::closeEvent(QCloseEvent *event) {
  // TODO(annejan) save window size or something?
  event->accept();
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
 * @brief ConfigDialog::useGit retrun preference for using git.
 * @return
 */
bool ConfigDialog::useGit() { return ui->checkBoxUseGit->isChecked(); }

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
 * @brief ConfigDialog::getPwgenPath return pwgen executable path.
 * @return
 */
QString ConfigDialog::getPwgenPath() { return ui->pwgenPath->text(); }

/**
 * @brief ConfigDialog::setPwgenPath set pwgen executable path.
 * Enable or disable related options in the interface.
 * @param pwgen
 */
void ConfigDialog::setPwgenPath(QString pwgen) {
  ui->pwgenPath->setText(pwgen);
  if (pwgen.isEmpty()) {
    ui->checkBoxUsePwgen->setChecked(false);
    ui->checkBoxUsePwgen->setEnabled(false);
  }
  on_checkBoxUsePwgen_clicked();
}

/**
 * @brief ConfigDialog::on_checkBoxUsPwgen_clicked enable or disable related
 * options in the interface.
 */
void ConfigDialog::on_checkBoxUsePwgen_clicked() {
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
 * overruled buy empty pwgenPath).
 * enable or disable related options in the interface via
 * ConfigDialog::on_checkBoxUsePwgen_clicked
 * @param usePwgen
 */
void ConfigDialog::usePwgen(bool usePwgen) {
  if (ui->pwgenPath->text().isEmpty())
    usePwgen = false;
  ui->checkBoxUsePwgen->setChecked(usePwgen);
  on_checkBoxUsePwgen_clicked();
}

/**
 * @brief ConfigDialog::avoidCapitals set preference for avoiding uppercase
 * letters using pwgen.
 * @param avoidCapitals
 */
void ConfigDialog::avoidCapitals(bool avoidCapitals) {
  ui->checkBoxAvoidCapitals->setChecked(avoidCapitals);
}

/**
 * @brief ConfigDialog::avoidNumbers set preference for using numbers in pwgen
 * generated password.
 * @param avoidNumbers
 */
void ConfigDialog::avoidNumbers(bool avoidNumbers) {
  ui->checkBoxAvoidNumbers->setChecked(avoidNumbers);
}

/**
 * @brief ConfigDialog::lessRandom set preference for using less random
 * passwords.
 * @param lessRandom
 */
void ConfigDialog::lessRandom(bool lessRandom) {
  ui->checkBoxLessRandom->setChecked(lessRandom);
}

/**
 * @brief ConfigDialog::useSymbols set preference for using special characters
 * in pwgen.
 * @param useSymbols
 */
void ConfigDialog::useSymbols(bool useSymbols) {
  ui->checkBoxUseSymbols->setChecked(useSymbols);
}

/**
 * @brief ConfigDialog::setPasswordLength set the length of desired (generated)
 * passwords.
 * @param pwLen
 */
void ConfigDialog::setPasswordLength(int pwLen) {
  ui->spinBoxPasswordLength->setValue(pwLen);
}

/**
 * @brief ConfigDialog::setPasswordChars use these charcters to generate
 * password (non-pwgen option).
 * @param pwChars
 */
void ConfigDialog::setPasswordChars(QString pwChars) {
  ui->lineEditPasswordChars->setText(pwChars);
}

/**
 * @brief ConfigDialog::usePwgen return preference for using pwgen.
 * @return
 */
bool ConfigDialog::usePwgen() { return ui->checkBoxUsePwgen->isChecked(); }

/**
 * @brief ConfigDialog::avoidCapitals return preference for avoiding uppercase
 * letters using pwgen.
 * @return
 */
bool ConfigDialog::avoidCapitals() {
  return ui->checkBoxAvoidCapitals->isChecked();
}

/**
 * @brief ConfigDialog::avoidNumbers return preference for using numbers in
 * generated password using pwgen.
 * @return
 */
bool ConfigDialog::avoidNumbers() {
  return ui->checkBoxAvoidNumbers->isChecked();
}

/**
 * @brief ConfigDialog::lessRandom return preference for using less random
 * passwords in pwgen.
 * @return
 */
bool ConfigDialog::lessRandom() { return ui->checkBoxLessRandom->isChecked(); }

/**
 * @brief ConfigDialog::useSymbols return preference for using special
 * characters with pwgen.
 * @return
 */
bool ConfigDialog::useSymbols() { return ui->checkBoxUseSymbols->isChecked(); }

/**
 * @brief ConfigDialog::getPasswordLength return desired length for generated
 * passwords.
 * @return
 */
int ConfigDialog::getPasswordLength() {
  return ui->spinBoxPasswordLength->value();
}

/**
 * @brief ConfigDialog::getPasswordChars return characters to use in password
 * generation (non-pwgen).
 * @return
 */
QString ConfigDialog::getPasswordChars() {
  return ui->lineEditPasswordChars->text();
}

/**
 * @brief ConfigDialog::setPwdTemplateSelector sets the current index of the
 * password characters template combobox
 * @return
 */
void ConfigDialog::setPwdTemplateSelector(int selection) {
  ui->passwordCharTemplateSelector->setCurrentIndex(selection);
}

/**
 * @brief ConfigDialog::getPwdTemplateSelector returns the selection of the
 * password characters template combobox
 * @return
 */
int ConfigDialog::getPwdTemplateSelector() {
  return ui->passwordCharTemplateSelector->currentIndex();
}

/**
 * @brief ConfigDialog::on_passwordCharTemplateSelector_activated sets the
 * passwordChar Template
 * combo box to the desired entry
 * @param entry of
 */
void ConfigDialog::on_passwordCharTemplateSelector_activated(int index) {
  ui->lineEditPasswordChars->setText(mainWindow->pwdConfig.Characters[index]);
  if (index == 3) {
    ui->lineEditPasswordChars->setEnabled(true);
  } else {
    ui->lineEditPasswordChars->setEnabled(false);
  }
}

/**
 * @brief ConfigDialog::setLineEditEnabled enabling/disabling the textbox with
 * the
 * password characters
 * @param b enable/disable
 */
void ConfigDialog::setLineEditEnabled(bool b) {
  ui->lineEditPasswordChars->setEnabled(b);
}

/**
 * @brief ConfigDialog::startMinimized return preference for starting
 * application minimized (tray icon).
 * @return
 */
bool ConfigDialog::startMinimized() {
  return ui->checkBoxStartMinimized->isChecked();
}

/**
 * @brief ConfigDialog::startMinimized set preference for starting application
 * minimized (tray icon).
 * @param startMinimized
 */
void ConfigDialog::startMinimized(bool startMinimized) {
  ui->checkBoxStartMinimized->setChecked(startMinimized);
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

/**
 * @brief ConfigDialog::useTemplate set preference for using templates.
 * @param useTemplate
 */
void ConfigDialog::useTemplate(bool useTemplate) {
  ui->checkBoxUseTemplate->setChecked(useTemplate);
  on_checkBoxUseTemplate_clicked();
}

/**
 * @brief ConfigDialog::useTemplate return preference for using templates.
 * @return
 */
bool ConfigDialog::useTemplate() {
  return ui->checkBoxUseTemplate->isChecked();
}

/**
 * @brief ConfigDialog::setTemplate set the desired template.
 * @param passTemplate
 */
void ConfigDialog::setTemplate(QString passTemplate) {
  ui->plainTextEditTemplate->setPlainText(passTemplate);
}

/**
 * @brief ConfigDialog::getTemplate return the desired template.
 * @return
 */
QString ConfigDialog::getTemplate() {
  return ui->plainTextEditTemplate->toPlainText();
}

/**
 * @brief ConfigDialog::autoPull set preference for automatically pulling from
 * git
 * @param autoPull
 */
void ConfigDialog::autoPull(bool autoPull) {
  ui->checkBoxAutoPull->setChecked(autoPull);
}

/**
 * @brief ConfigDialog::autoPush set preference for automatically pushing to git
 * @param autoPush
 */
void ConfigDialog::autoPush(bool autoPush) {
  ui->checkBoxAutoPush->setChecked(autoPush);
}

/**
 * @brief ConfigDialog::autoPull return preference for automatically pulling
 * from git
 * @return
 */
bool ConfigDialog::autoPull() { return ui->checkBoxAutoPull->isChecked(); }

/**
 * @brief ConfigDialog::autoPush return preference for automatically pushing to
 * git
 * @return
 */
bool ConfigDialog::autoPush() { return ui->checkBoxAutoPush->isChecked(); }

/**
 * @brief ConfigDialog::templateAllFields return preference for templating all
 * tokenisable fields
 * @return
 */
bool ConfigDialog::templateAllFields() {
  return ui->checkBoxTemplateAllFields->isChecked();
}

/**
 * @brief ConfigDialog::templateAllFields set preference for templating all
 * tokenisable fields
 * @param templateAll
 */
void ConfigDialog::templateAllFields(bool templateAll) {
  ui->checkBoxTemplateAllFields->setChecked(templateAll);
}

/**
 * @brief ConfigDialog::alwaysOnTop set preference for running application on
 * top of others
 * @param alwaysOnTop
 */
void ConfigDialog::alwaysOnTop(bool alwaysOnTop) {
  ui->checkBoxAlwaysOnTop->setChecked(alwaysOnTop);
}

/**
 * @brief ConfigDialog::alwaysOnTop return preference for running application on
 * top of others.
 * @return
 */
bool ConfigDialog::alwaysOnTop() {
  return ui->checkBoxAlwaysOnTop->isChecked();
}
