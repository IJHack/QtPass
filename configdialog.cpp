#include "configdialog.h"
#include "ui_configdialog.h"
#include "mainwindow.h"
#include "keygendialog.h"
#include <QDebug>
#include <QMessageBox>
#include <QDir>

/**
 * @brief Dialog::Dialog
 * @param parent
 */
ConfigDialog::ConfigDialog(MainWindow *parent) :
    QDialog(parent),
    ui(new Ui::ConfigDialog)
{
    mainWindow = parent;
    ui->setupUi(this);
    ui->profileTable->verticalHeader()->hide();
    ui->profileTable->horizontalHeader()->setStretchLastSection(true);
    ui->label->setText(ui->label->text() + VERSION);
}

/**
 * @brief Dialog::~Dialog
 */
ConfigDialog::~ConfigDialog()
{
    mainWindow->setGitExecutable(ui->gitPath->text());
    mainWindow->setGpgExecutable(ui->gpgPath->text());
    mainWindow->setPassExecutable(ui->passPath->text());
}

/**
 * @brief Dialog::setPassPath
 * @param path
 */
void ConfigDialog::setPassPath(QString path) {
    ui->passPath->setText(path);
}

/**
 * @brief Dialog::setGitPath
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
 * @brief Dialog::setGpgPath
 * @param path
 */
void ConfigDialog::setGpgPath(QString path) {
    ui->gpgPath->setText(path);
}

/**
 * @brief Dialog::setStorePath
 * @param path
 */
void ConfigDialog::setStorePath(QString path) {
    ui->storePath->setText(path);
}

/**
 * @brief Dialog::getPassPath
 * @return
 */
QString ConfigDialog::getPassPath() {
    return ui->passPath->text();
}

/**
 * @brief Dialog::getGitPath
 * @return
 */
QString ConfigDialog::getGitPath() {
    return ui->gitPath->text();
}

/**
 * @brief Dialog::getGpgPath
 * @return
 */
QString ConfigDialog::getGpgPath() {
    return ui->gpgPath->text();
}

/**
 * @brief Dialog::getStorePath
 * @return
 */
QString ConfigDialog::getStorePath() {
    return ui->storePath->text();
}

/**
 * @brief Dialog::usePass
 * @return
 */
bool ConfigDialog::usePass() {
    return ui->radioButtonPass->isChecked();
}

/**
 * @brief Dialog::usePass
 * @param pass
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
 * @brief Dialog::on_radioButtonNative_clicked
 */
void ConfigDialog::on_radioButtonNative_clicked()
{
    setGroupBoxState();
}

/**
 * @brief Dialog::on_radioButtonPass_clicked
 */
void ConfigDialog::on_radioButtonPass_clicked()
{
    setGroupBoxState();
}

/**
 * @brief Dialog::setGroupBoxState
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
 * @brief Dialog::selectExecutable
 * @return
 */
QString ConfigDialog::selectExecutable() {
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setOption(QFileDialog::ReadOnly);
    if (dialog.exec()) {
        return dialog.selectedFiles().first();
    }
    else return "";
}

/**
 * @brief Dialog::selectFolder
 * @return
 */
QString ConfigDialog::selectFolder() {
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setFilter(QDir::NoFilter);
    dialog.setOption(QFileDialog::ShowDirsOnly);
    if (dialog.exec()) {
        return dialog.selectedFiles().first();
    }
    else return "";
}

/**
 * @brief Dialog::on_toolButtonGit_clicked
 */
void ConfigDialog::on_toolButtonGit_clicked()
{
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
 * @brief Dialog::on_toolButtonGpg_clicked
 */
void ConfigDialog::on_toolButtonGpg_clicked()
{
    QString gpg = selectExecutable();
    if (!gpg.isEmpty()) {
        ui->gpgPath->setText(gpg);
    }
}

/**
 * @brief Dialog::on_toolButtonPass_clicked
 */
void ConfigDialog::on_toolButtonPass_clicked()
{
    QString pass = selectExecutable();
    if (!pass.isEmpty()) {
        ui->passPath->setText(pass);
    }
}

/**
 * @brief Dialog::on_toolButtonStore_clicked
 */
void ConfigDialog::on_toolButtonStore_clicked()
{
    QString store = selectFolder();
    if (!store.isEmpty()) { // TODO call check
        ui->storePath->setText(store);
    }
}

/**
 * @brief Dialog::on_checkBoxClipboard_clicked
 */
void ConfigDialog::on_checkBoxClipboard_clicked()
{
    if (ui->checkBoxClipboard->isChecked()) {
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
        ui->checkBoxAutoclear->setEnabled(false);
        ui->spinBoxAutoclearSeconds->setEnabled(false);
        ui->labelSeconds->setEnabled(false);
        ui->checkBoxHidePassword->setEnabled(false);
        ui->checkBoxHideContent->setEnabled(false);
    }
}

/**
 * @brief Dialog::on_checkBoxAutoclearPanel_clicked
 */
void ConfigDialog::on_checkBoxAutoclearPanel_clicked()
{
    if (ui->checkBoxAutoclearPanel->isChecked()) {
        ui->spinBoxAutoclearPanelSeconds->setEnabled(true);
        ui->labelPanelSeconds->setEnabled(true);
    } else {
        ui->spinBoxAutoclearPanelSeconds->setEnabled(false);
        ui->labelPanelSeconds->setEnabled(false);
    }
}

/**
 * @brief Dialog::useClipboard
 */
void ConfigDialog::useClipboard(bool useClipboard)
{
    ui->checkBoxClipboard->setChecked(useClipboard);
    on_checkBoxClipboard_clicked();
}

/**
 * @brief Dialog::useAutoclear
 * @param useAutoclear
 */
void ConfigDialog::useAutoclear(bool useAutoclear)
{
    ui->checkBoxAutoclear->setChecked(useAutoclear);
    on_checkBoxAutoclear_clicked();
}

/**
 * @brief Dialog::setAutoclear
 * @param seconds
 */
void ConfigDialog::setAutoclear(int seconds)
{
    ui->spinBoxAutoclearSeconds->setValue(seconds);
}

/**
 * @brief Dialog::useAutoclearPanel
 * @param useAutoclearPanel
 */
void ConfigDialog::useAutoclearPanel(bool useAutoclearPanel)
{
    ui->checkBoxAutoclearPanel->setChecked(useAutoclearPanel);
    on_checkBoxAutoclearPanel_clicked();
}

/**
 * @brief Dialog::setAutoclearPanel
 * @param seconds
 */
void ConfigDialog::setAutoclearPanel(int seconds)
{
    ui->spinBoxAutoclearPanelSeconds->setValue(seconds);
}

/**
 * @brief Dialog::useClipboard
 * @return
 */
bool ConfigDialog::useClipboard()
{
    return ui->checkBoxClipboard->isChecked();
}

/**
 * @brief Dialog::useAutoclear
 * @return
 */
bool ConfigDialog::useAutoclear()
{
    return ui->checkBoxAutoclear->isChecked();
}

/**
 * @brief Dialog::getAutoclear
 * @return
 */
int ConfigDialog::getAutoclear()
{
    return ui->spinBoxAutoclearSeconds->value();
}

/**
 * @brief Dialog::on_checkBoxAutoclear_clicked
 */
void ConfigDialog::on_checkBoxAutoclear_clicked()
{
    on_checkBoxClipboard_clicked();
}

/**
 * @brief Dialog::useAutoclearPanel
 * @return
 */
bool ConfigDialog::useAutoclearPanel()
{
    return ui->checkBoxAutoclearPanel->isChecked();
}

/**
 * @brief Dialog::getAutoclearPanel
 * @return
 */
int ConfigDialog::getAutoclearPanel()
{
    return ui->spinBoxAutoclearPanelSeconds->value();
}

/**
 * @brief Dialog::hidePassword
 * @return
 */
bool ConfigDialog::hidePassword()
{
    return ui->checkBoxHidePassword->isChecked();
}

/**
 * @brief Dialog::hideContent
 * @return
 */
bool ConfigDialog::hideContent()
{
    return ui->checkBoxHideContent->isChecked();
}

/**
 * @brief Dialog::hidePassword
 * @param hidePassword
 */
void ConfigDialog::hidePassword(bool hidePassword)
{
    ui->checkBoxHidePassword->setChecked(hidePassword);
}

/**
 * @brief Dialog::hideContent
 * @param hideContent
 */
void ConfigDialog::hideContent(bool hideContent)
{
    ui->checkBoxHideContent->setChecked(hideContent);
}

/**
 * @brief Dialog::addGPGId
 * @return
 */
bool ConfigDialog::addGPGId()
{
    return ui->checkBoxAddGPGId->isChecked();
}

/**
 * @brief Dialog::addGPGId
 * @param addGPGId
 */
void ConfigDialog::addGPGId(bool addGPGId)
{
    ui->checkBoxAddGPGId->setChecked(addGPGId);
}

/**
 * @brief Dialog::genKey
 * @param QString batch
 */
void ConfigDialog::genKey(QString batch, QDialog *dialog)
{
    mainWindow->generateKeyPair(batch, dialog);
}

/**
 * @brief Dialog::setProfiles
 * @param profiles
 * @param profile
 */
void ConfigDialog::setProfiles(QHash<QString, QString> profiles, QString profile)
{
    //qDebug() << profiles;
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
            //qDebug() << "naam:" + i.key();
            if (i.key() == profile) {
                ui->profileTable->selectRow(n);
            }
        }
        n++;
    }
}

/**
 * @brief Dialog::getProfiles
 * @return
 */
QHash<QString, QString> ConfigDialog::getProfiles()
{
    QHash<QString, QString> profiles;
    // Check?
    for (int i = 0; i < ui->profileTable->rowCount(); i++) {
        QTableWidgetItem* pathItem = ui->profileTable->item(i, 1);
        if (0 != pathItem) {
            QTableWidgetItem* item = ui->profileTable->item(i, 0);
            if (item == 0) {
                qDebug() << "empty name, shoud fix in frontend";
                continue;
            }
            profiles.insert(item->text(),
                            pathItem->text());
        }
    }
    return profiles;
}

/**
 * @brief Dialog::on_addButton_clicked
 */
void ConfigDialog::on_addButton_clicked()
{
    int n = ui->profileTable->rowCount();
    ui->profileTable->insertRow(n);
    ui->profileTable->setItem(n, 1, new QTableWidgetItem(ui->storePath->text()));
    ui->profileTable->selectRow(n);
    ui->deleteButton->setEnabled(true);
}

/**
 * @brief Dialog::on_deleteButton_clicked
 */
void ConfigDialog::on_deleteButton_clicked()
{
    QSet<int> selectedRows; //we use a set to prevent doubles
    QList<QTableWidgetItem*> itemList = ui->profileTable->selectedItems();
    if (itemList.count() == 0) {
        QMessageBox::warning(this, tr("No profile selected"),
            tr("No profile selected to delete"));
        return;
    }
    QTableWidgetItem * item;
    foreach(item, itemList)
    selectedRows.insert(item->row());
    //get a list, and sort it big to small
    QList<int> rows = selectedRows.toList();
    qSort(rows.begin(), rows.end());
    //now actually do the removing:
    foreach(int row, rows) {
     ui->profileTable->removeRow(row);
    }
    if (ui->profileTable->rowCount() < 1) {
        ui->deleteButton->setEnabled(false);
    }
}

void ConfigDialog::criticalMessage(const QString &title, const QString &text)
{
    QMessageBox::critical(this, title, text, QMessageBox::Ok, QMessageBox::Ok);
}

/**
 * @brief Dialog::wizard
 */
void ConfigDialog::wizard()
{
    //mainWindow->checkConfig();
    bool clean = false;

    QString gpg = ui->gpgPath->text();
    //QString gpg = mainWindow->getGpgExecutable();
    if(!QFile(gpg).exists()){
        criticalMessage(tr("GnuPG not found"),
            tr("Please install GnuPG on your system.<br>Install <strong>gpg</strong> using your favorite package manager<br>or <a href=\"https://www.gnupg.org/download/#sec-1-2\">download</a> it from GnuPG.org"));
        clean = true;
    }

    QStringList names = mainWindow->getSecretKeys();
    qDebug() << names;
    if (QFile(gpg).exists() && names.empty()) {
        KeygenDialog d(this);
        if (!d.exec()) {
            return;
        }
    }

    QString passStore = ui->storePath->text();

    if (!QFile(passStore).exists()) {
        // TODO pass version?
        if (QMessageBox::question(this, tr("Create password-store?"),
            tr("Would you like to create a password-store at %1?").arg(passStore),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
                QDir().mkdir(passStore);
#ifdef Q_OS_WIN
                SetFileAttributes(passStore, FILE_ATTRIBUTE_HIDDEN);
#endif
                if(useGit()) {
                    mainWindow->executePassGitInit();
                }
                mainWindow->userDialog(passStore);
        }
    }

    if(!QFile(passStore + ".gpg-id").exists()){
        qDebug() << ".gpg-id file does not exist";

        if (!clean) {
            criticalMessage(tr("Password store not initialised"),
                tr("The folder %1 doesn't seem to be a password store or is not yet initialised.").arg(passStore));
        }
        while(!QFile(passStore).exists()) {
            on_toolButtonStore_clicked();
            // allow user to cancel
            if (passStore == ui->storePath->text())
                return;
            passStore = ui->storePath->text();
        }
        if (!QFile(passStore + ".gpg-id").exists()) {
            qDebug() << ".gpg-id file still does not exist :/";
            // appears not to be store
            // init yes / no ?
            mainWindow->userDialog(passStore);
        }
    }
}

/**
 * @brief Dialog::useTrayIcon
 * @return
 */
bool ConfigDialog::useTrayIcon() {
    return ui->checkBoxUseTrayIcon->isChecked();
}

/**
 * @brief Dialog::hideOnClose
 * @return
 */
bool ConfigDialog::hideOnClose() {
    return ui->checkBoxHideOnClose->isEnabled() && ui->checkBoxHideOnClose->isChecked();
}

/**
 * @brief Dialog::useTrayIcon
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
 * @brief Dialog::hideOnClose
 * @param hideOnClose
 */
void ConfigDialog::hideOnClose(bool hideOnClose) {
    ui->checkBoxHideOnClose->setChecked(hideOnClose);
}

/**
 * @brief Dialog::on_checkBoxUseTrayIcon_clicked
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
 * @brief Dialog::closeEvent
 * @param event
 */
void ConfigDialog::closeEvent(QCloseEvent *event) {
    // TODO save window size or something?
    event->accept();
}

/**
 * @brief Dialog::useGit
 * @param useGit
 */
void ConfigDialog::useGit(bool useGit)
{
    ui->checkBoxUseGit->setChecked(useGit);
    on_checkBoxUseGit_clicked();
}

/**
 * @brief Dialog::useGit
 * @return
 */
bool ConfigDialog::useGit()
{
    return ui->checkBoxUseGit->isChecked();
}

/**
 * @brief Dialog::on_checkBoxUseGit_clicked
 */
void ConfigDialog::on_checkBoxUseGit_clicked()
{
    ui->checkBoxAddGPGId->setEnabled(ui->checkBoxUseGit->isChecked());
    ui->checkBoxAutoPull->setEnabled(ui->checkBoxUseGit->isChecked());
    ui->checkBoxAutoPush->setEnabled(ui->checkBoxUseGit->isChecked());
}

/**
 * @brief Dialog::on_toolButtonPwgen_clicked
 */
void ConfigDialog::on_toolButtonPwgen_clicked()
{
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
 * @brief Dialog::getPwgenPath
 * @return
 */
QString ConfigDialog::getPwgenPath() {
    return ui->pwgenPath->text();
}

/**
 * @brief Dialog::setPwgenPath
 * @param pwgen
 */
void ConfigDialog::setPwgenPath(QString pwgen)
{
    ui->pwgenPath->setText(pwgen);
    if (pwgen.isEmpty()) {
        ui->checkBoxUsePwgen->setChecked(false);
        ui->checkBoxUsePwgen->setEnabled(false);
    }
    on_checkBoxUsePwgen_clicked();
}

/**
 * @brief Dialog::on_checkBoxUsPwgen_clicked
 */
void ConfigDialog::on_checkBoxUsePwgen_clicked()
{
    ui->checkBoxUseSymbols->setEnabled(ui->checkBoxUsePwgen->isChecked());
    ui->lineEditPasswordChars->setEnabled(!ui->checkBoxUsePwgen->isChecked());
    ui->labelPasswordChars->setEnabled(!ui->checkBoxUsePwgen->isChecked());
}

/**
 * @brief Dialog::usePwgen
 * @param usePwgen
 */
void ConfigDialog::usePwgen(bool usePwgen) {
    if (ui->pwgenPath->text().isEmpty()) {
        usePwgen = false;
    }
    ui->checkBoxUsePwgen->setChecked(usePwgen);
    on_checkBoxUsePwgen_clicked();
}

/**
 * @brief Dialog::useSymbols
 * @param useSymbols
 */
void ConfigDialog::useSymbols(bool useSymbols) {
    ui->checkBoxUseSymbols->setChecked(useSymbols);
}

/**
 * @brief Dialog::setPasswordLength
 * @param pwLen
 */
void ConfigDialog::setPasswordLength(int pwLen) {
    ui->spinBoxPasswordLength->setValue(pwLen);
}

void ConfigDialog::setPasswordChars(QString pwChars) {
    ui->lineEditPasswordChars->setText(pwChars);
}

/**
 * @brief Dialog::usePwgen
 * @return
 */
bool ConfigDialog::usePwgen() {
    return ui->checkBoxUsePwgen->isChecked();
}

/**
 * @brief Dialog::useSymbols
 * @return
 */
bool ConfigDialog::useSymbols() {
    return ui->checkBoxUseSymbols->isChecked();
}

/**
 * @brief Dialog::getPasswordLength
 * @return
 */
int ConfigDialog::getPasswordLength() {
    return ui->spinBoxPasswordLength->value();
}

/**
 * @brief Dialog::getPasswordChars
 * @return
 */
QString ConfigDialog::getPasswordChars() {
    return ui->lineEditPasswordChars->text();
}

/**
 * @brief Dialog::startMinimized
 * @return
 */
bool ConfigDialog::startMinimized() {
    return ui->checkBoxStartMinimized->isChecked();
}

/**
 * @brief Dialog::startMinimized
 * @param startMinimized
 */
void ConfigDialog::startMinimized(bool startMinimized) {
    ui->checkBoxStartMinimized->setChecked(startMinimized);
}

/**
 * @brief Dialog::on_checkBoxUseTemplate_clicked
 */
void ConfigDialog::on_checkBoxUseTemplate_clicked() {
    ui->plainTextEditTemplate->setEnabled(ui->checkBoxUseTemplate->isChecked());
    ui->checkBoxTemplateAllFields->setEnabled(ui->checkBoxUseTemplate->isChecked());
}

void ConfigDialog::useTemplate(bool useTemplate) {
   ui->checkBoxUseTemplate->setChecked(useTemplate);
   on_checkBoxUseTemplate_clicked();
}

bool ConfigDialog::useTemplate() {
    return ui->checkBoxUseTemplate->isChecked();
}

void ConfigDialog::setTemplate(QString passTemplate) {
    ui->plainTextEditTemplate->setPlainText(passTemplate);
}

QString ConfigDialog::getTemplate() {
    return ui->plainTextEditTemplate->toPlainText();
}

void ConfigDialog::autoPull(bool autoPull) {
    ui->checkBoxAutoPull->setChecked(autoPull);
}

void ConfigDialog::autoPush(bool autoPush) {
    ui->checkBoxAutoPush->setChecked(autoPush);
}

bool ConfigDialog::autoPull() {
    return ui->checkBoxAutoPull->isChecked();
}

bool ConfigDialog::autoPush() {
    return ui->checkBoxAutoPush->isChecked();
}

bool ConfigDialog::templateAllFields() {
    return ui->checkBoxTemplateAllFields->isChecked();
}

void ConfigDialog::templateAllFields(bool templateAll) {
    ui->checkBoxTemplateAllFields->setChecked(templateAll);
}
