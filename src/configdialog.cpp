#include "configdialog.h"
#include "keygendialog.h"
#include "mainwindow.h"
#include "qtpasssettings.h"
#include "ui_configdialog.h"
#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QTableWidgetItem>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

/**
 * @brief ConfigDialog::ConfigDialog this sets up the configuration screen.
 * @param parent
 */
ConfigDialog::ConfigDialog(MainWindow *parent)
    : QDialog(parent), ui(new Ui::ConfigDialog) {
  mainWindow = parent;
  ui->setupUi(this);

  ui->passPath->setText(QtPassSettings::getPassExecutable());
  setGitPath(QtPassSettings::getGitExecutable());
  ui->gpgPath->setText(QtPassSettings::getGpgExecutable());
  ui->storePath->setText(QtPassSettings::getPassStore());

  ui->spinBoxAutoclearSeconds->setValue(QtPassSettings::getAutoclearSeconds());
  ui->spinBoxAutoclearPanelSeconds->setValue(
      QtPassSettings::getAutoclearPanelSeconds());
  ui->checkBoxHidePassword->setChecked(QtPassSettings::isHidePassword());
  ui->checkBoxHideContent->setChecked(QtPassSettings::isHideContent());
  ui->checkBoxAddGPGId->setChecked(QtPassSettings::isAddGPGId(true));

  if (QSystemTrayIcon::isSystemTrayAvailable() == true) {
    ui->checkBoxHideOnClose->setChecked(QtPassSettings::isHideOnClose());
    ui->checkBoxStartMinimized->setChecked(QtPassSettings::isStartMinimized());
  } else {
    ui->checkBoxUseTrayIcon->setEnabled(false);
    ui->checkBoxUseTrayIcon->setToolTip(tr("System tray is not available"));
    ui->checkBoxHideOnClose->setEnabled(false);
    ui->checkBoxStartMinimized->setEnabled(false);
  }

  ui->checkBoxAvoidCapitals->setChecked(QtPassSettings::isAvoidCapitals());
  ui->checkBoxAvoidNumbers->setChecked(QtPassSettings::isAvoidNumbers());
  ui->checkBoxLessRandom->setChecked(QtPassSettings::isLessRandom());
  ui->checkBoxUseSymbols->setChecked(QtPassSettings::isUseSymbols());
  ui->plainTextEditTemplate->setPlainText(QtPassSettings::getPassTemplate());
  ui->checkBoxTemplateAllFields->setChecked(
      QtPassSettings::isTemplateAllFields());
  ui->checkBoxAutoPull->setChecked(QtPassSettings::isAutoPull());
  ui->checkBoxAutoPush->setChecked(QtPassSettings::isAutoPush());
  ui->checkBoxAlwaysOnTop->setChecked(QtPassSettings::isAlwaysOnTop());

#if defined(Q_OS_WIN) || defined(__APPLE__)
  ui->checkBoxUseOtp->hide();
  ui->checkBoxUseQrencode->hide();
  ui->label_10->hide();
#endif

  if (!isPassOtpAvailable()) {
    ui->checkBoxUseOtp->setEnabled(false);
    ui->checkBoxUseOtp->setToolTip(
        tr("Pass OTP extension needs to be installed"));
  }

  if (!isQrencodeAvailable()) {
    ui->checkBoxUseQrencode->setEnabled(false);
    ui->checkBoxUseQrencode->setToolTip(
        tr("qrencode needs to be installed"));
  }

  setProfiles(QtPassSettings::getProfiles(), QtPassSettings::getProfile());
  setPwgenPath(QtPassSettings::getPwgenExecutable());
  setPasswordConfiguration(QtPassSettings::getPasswordConfiguration());

  usePass(QtPassSettings::isUsePass());
  useAutoclear(QtPassSettings::isUseAutoclear());
  useAutoclearPanel(QtPassSettings::isUseAutoclearPanel());
  useTrayIcon(QtPassSettings::isUseTrayIcon());
  useGit(QtPassSettings::isUseGit());

  useOtp(QtPassSettings::isUseOtp());
  useQrencode(QtPassSettings::isUseQrencode());

  usePwgen(QtPassSettings::isUsePwgen());
  useTemplate(QtPassSettings::isUseTemplate());

  ui->profileTable->verticalHeader()->hide();
  ui->profileTable->horizontalHeader()->setStretchLastSection(true);
  ui->label->setText(ui->label->text() + VERSION);
  ui->comboBoxClipboard->clear();

  ui->comboBoxClipboard->addItem(tr("No Clipboard"));
  ui->comboBoxClipboard->addItem(tr("Always copy to clipboard"));
  ui->comboBoxClipboard->addItem(tr("On-demand copy to clipboard"));

  int currentIndex = QtPassSettings::getClipBoardTypeRaw();
  ui->comboBoxClipboard->setCurrentIndex(currentIndex);
  on_comboBoxClipboard_activated(currentIndex);

  QClipboard *clip = QApplication::clipboard();
  if (!clip->supportsSelection()) {
    useSelection(false);
    ui->checkBoxSelection->setVisible(false);
  } else {
    useSelection(QtPassSettings::isUseSelection());
  }

  connect(ui->profileTable, &QTableWidget::itemChanged, this,
          &ConfigDialog::onProfileTableItemChanged);
  connect(this, &ConfigDialog::accepted, this, &ConfigDialog::on_accepted);
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
 * @brief ConfigDialog::setGitPath set the git executable path.
 * Make sure the checkBoxUseGit is updated.
 * @param path
 */
void ConfigDialog::setGitPath(QString path) {
  ui->gitPath->setText(path);
  ui->checkBoxUseGit->setEnabled(!path.isEmpty());
  if (path.isEmpty()) {
    useGit(false);
  }
}

/**
 * @brief ConfigDialog::usePass set wether or not we want to use pass.
 * Update radio buttons accordingly.
 * @param usePass
 */
void ConfigDialog::usePass(bool usePass) {
  ui->radioButtonNative->setChecked(!usePass);
  ui->radioButtonPass->setChecked(usePass);
  setGroupBoxState();
}

void ConfigDialog::validate(QTableWidgetItem *item) {
  bool status = true;

  if (item == nullptr) {
    for (int i = 0; i < ui->profileTable->rowCount(); i++) {
      for (int j = 0; j < ui->profileTable->columnCount(); j++) {
        QTableWidgetItem *_item = ui->profileTable->item(i, j);

        if (_item->text().isEmpty()) {
          _item->setBackgroundColor(Qt::red);
          status = false;
          break;
        }
      }

      if (!status)
        break;
    }
  } else {
    if (item->text().isEmpty()) {
      item->setBackgroundColor(Qt::red);
      status = false;
    }
  }

  ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(status);
}

void ConfigDialog::on_accepted() {
  QtPassSettings::setPassExecutable(ui->passPath->text());
  QtPassSettings::setGitExecutable(ui->gitPath->text());
  QtPassSettings::setGpgExecutable(ui->gpgPath->text());
  QtPassSettings::setPassStore(
      Util::normalizeFolderPath(ui->storePath->text()));
  QtPassSettings::setUsePass(ui->radioButtonPass->isChecked());
  QtPassSettings::setClipBoardType(ui->comboBoxClipboard->currentIndex());
  QtPassSettings::setUseSelection(ui->checkBoxSelection->isChecked());
  QtPassSettings::setUseAutoclear(ui->checkBoxAutoclear->isChecked());
  QtPassSettings::setAutoclearSeconds(ui->spinBoxAutoclearSeconds->value());
  QtPassSettings::setUseAutoclearPanel(ui->checkBoxAutoclearPanel->isChecked());
  QtPassSettings::setAutoclearPanelSeconds(
      ui->spinBoxAutoclearPanelSeconds->value());
  QtPassSettings::setHidePassword(ui->checkBoxHidePassword->isChecked());
  QtPassSettings::setHideContent(ui->checkBoxHideContent->isChecked());
  QtPassSettings::setAddGPGId(ui->checkBoxAddGPGId->isChecked());
  QtPassSettings::setUseTrayIcon(ui->checkBoxUseTrayIcon->isEnabled() &&
                                 ui->checkBoxUseTrayIcon->isChecked());
  QtPassSettings::setHideOnClose(ui->checkBoxHideOnClose->isEnabled() &&
                                 ui->checkBoxHideOnClose->isChecked());
  QtPassSettings::setStartMinimized(ui->checkBoxStartMinimized->isEnabled() &&
                                    ui->checkBoxStartMinimized->isChecked());
  QtPassSettings::setProfiles(getProfiles());
  QtPassSettings::setUseGit(ui->checkBoxUseGit->isChecked());
  QtPassSettings::setUseOtp(ui->checkBoxUseOtp->isChecked());
  QtPassSettings::setUseQrencode(ui->checkBoxUseQrencode->isChecked());
  QtPassSettings::setPwgenExecutable(ui->pwgenPath->text());
  QtPassSettings::setUsePwgen(ui->checkBoxUsePwgen->isChecked());
  QtPassSettings::setAvoidCapitals(ui->checkBoxAvoidCapitals->isChecked());
  QtPassSettings::setAvoidNumbers(ui->checkBoxAvoidNumbers->isChecked());
  QtPassSettings::setLessRandom(ui->checkBoxLessRandom->isChecked());
  QtPassSettings::setUseSymbols(ui->checkBoxUseSymbols->isChecked());
  QtPassSettings::setPasswordConfiguration(getPasswordConfiguration());
  QtPassSettings::setUseTemplate(ui->checkBoxUseTemplate->isChecked());
  QtPassSettings::setPassTemplate(ui->plainTextEditTemplate->toPlainText());
  QtPassSettings::setTemplateAllFields(
      ui->checkBoxTemplateAllFields->isChecked());
  QtPassSettings::setAutoPush(ui->checkBoxAutoPush->isChecked());
  QtPassSettings::setAutoPull(ui->checkBoxAutoPull->isChecked());
  QtPassSettings::setAlwaysOnTop(ui->checkBoxAlwaysOnTop->isChecked());

  QtPassSettings::setVersion(VERSION);
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
QStringList ConfigDialog::getSecretKeys() {
  QList<UserInfo> keys = QtPassSettings::getPass()->listKeys("", true);
  QStringList names;

  if (keys.size() == 0)
    return names;

  foreach (const UserInfo &sec, keys)
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
    return QString();
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
    return QString();
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
 * @brief ConfigDialog::genKey tunnel function to make MainWindow generate a
 * gpg key pair.
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
    if (nullptr != pathItem) {
      QTableWidgetItem *item = ui->profileTable->item(i, 0);
      if (item == nullptr) {
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
  ui->profileTable->setItem(n, 0, new QTableWidgetItem());
  ui->profileTable->setItem(n, 1, new QTableWidgetItem(ui->storePath->text()));
  ui->profileTable->selectRow(n);
  ui->deleteButton->setEnabled(true);

  validate();
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

  validate();
}

/**
 * @brief ConfigDialog::criticalMessage weapper for showing critical messages
 * in a popup.
 * @param title
 * @param text
 */
void ConfigDialog::criticalMessage(const QString &title, const QString &text) {
  QMessageBox::critical(this, title, text, QMessageBox::Ok, QMessageBox::Ok);
}

bool ConfigDialog::isQrencodeAvailable() {
#ifdef Q_OS_WIN
  return false;
#elif defined(__APPLE__)
  return false;
#else
  QProcess which;
  which.start("which", QStringList() << "qrencode");
  which.waitForFinished();
  return which.exitCode() == 0;
#endif
}

bool ConfigDialog::isPassOtpAvailable() {
#ifdef Q_OS_WIN
  return false;
#elif defined(__APPLE__)
  return false;
#else
  QFileInfo file("/usr/lib/password-store/extensions/otp.bash");

  return file.exists();
#endif
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
           "<strong>gpg</strong> using your favorite package manager<br>or "
           "<a "
           "href=\"https://www.gnupg.org/download/#sec-1-2\">download</a> it "
           "from GnuPG.org"));
    clean = true;
  }

  QStringList names = getSecretKeys();

#ifdef QT_DEBUG
  dbg() << names;
#endif

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
      if (ui->checkBoxUseGit->isChecked())
        emit mainWindow->passGitInitNeeded();
      mainWindow->userDialog(passStore);
    }
  }

  if (!QFile(QDir(passStore).filePath(".gpg-id")).exists()) {
#ifdef QT_DEBUG
    dbg() << ".gpg-id file does not exist";
#endif
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
#ifdef QT_DEBUG
      dbg() << ".gpg-id file still does not exist :/";
#endif
      // appears not to be store
      // init yes / no ?
      mainWindow->userDialog(passStore);
    }
  }
}

/**
 * @brief ConfigDialog::useTrayIcon set preference for using trayicon.
 * Enable or disable related checkboxes accordingly.
 * @param useSystray
 */
void ConfigDialog::useTrayIcon(bool useSystray) {
  if (QSystemTrayIcon::isSystemTrayAvailable() == true) {
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
 * @brief ConfigDialog::useOtp set preference for using otp plugin.
 * @param useOtp
 */
void ConfigDialog::useOtp(bool useOtp) {
  ui->checkBoxUseOtp->setChecked(useOtp);
}

/**
 * @brief ConfigDialog::useOtp set preference for using otp plugin.
 * @param useOtp
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

void ConfigDialog::setPasswordConfiguration(
    const PasswordConfiguration &config) {
  ui->spinBoxPasswordLength->setValue(config.length);
  ui->passwordCharTemplateSelector->setCurrentIndex(config.selected);
  if (config.selected != PasswordConfiguration::CUSTOM)
    ui->lineEditPasswordChars->setEnabled(false);
  ui->lineEditPasswordChars->setText(config.Characters[config.selected]);
}

PasswordConfiguration ConfigDialog::getPasswordConfiguration() {
  PasswordConfiguration config;
  config.length = ui->spinBoxPasswordLength->value();
  config.selected = static_cast<PasswordConfiguration::characterSet>(
      ui->passwordCharTemplateSelector->currentIndex());
  config.Characters[PasswordConfiguration::CUSTOM] =
      ui->lineEditPasswordChars->text();
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
  if (index == 3) {
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

void ConfigDialog::onProfileTableItemChanged(QTableWidgetItem *item) {
  validate(item);
}

/**
 * @brief ConfigDialog::useTemplate set preference for using templates.
 * @param useTemplate
 */
void ConfigDialog::useTemplate(bool useTemplate) {
  ui->checkBoxUseTemplate->setChecked(useTemplate);
  on_checkBoxUseTemplate_clicked();
}
