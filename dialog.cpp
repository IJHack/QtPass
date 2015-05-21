#include "dialog.h"
#include "ui_dialog.h"
#include "mainwindow.h"
#include <QDebug>
#include <QMessageBox>

/**
 * @brief Dialog::Dialog
 * @param parent
 */
Dialog::Dialog(MainWindow *parent) :
    QDialog(parent),
    ui(new Ui::Dialog)
{
    mainWindow = parent;
    ui->setupUi(this);
}

/**
 * @brief Dialog::~Dialog
 */
Dialog::~Dialog()
{
}

/**
 * @brief Dialog::setPassPath
 * @param path
 */
void Dialog::setPassPath(QString path) {
    ui->passPath->setText(path);
}

/**
 * @brief Dialog::setGitPath
 * @param path
 */
void Dialog::setGitPath(QString path) {
    ui->gitPath->setText(path);
}

/**
 * @brief Dialog::setGpgPath
 * @param path
 */
void Dialog::setGpgPath(QString path) {
    ui->gpgPath->setText(path);
}

/**
 * @brief Dialog::setStorePath
 * @param path
 */
void Dialog::setStorePath(QString path) {
    ui->storePath->setText(path);
}

/**
 * @brief Dialog::getPassPath
 * @return
 */
QString Dialog::getPassPath() {
    return ui->passPath->text();
}

/**
 * @brief Dialog::getGitPath
 * @return
 */
QString Dialog::getGitPath() {
    return ui->gitPath->text();
}

/**
 * @brief Dialog::getGpgPath
 * @return
 */
QString Dialog::getGpgPath() {
    return ui->gpgPath->text();
}

/**
 * @brief Dialog::getStorePath
 * @return
 */
QString Dialog::getStorePath() {
    return ui->storePath->text();
}

/**
 * @brief Dialog::usePass
 * @return
 */
bool Dialog::usePass() {
    return ui->radioButtonPass->isChecked();
}

/**
 * @brief Dialog::usePass
 * @param pass
 */
void Dialog::usePass(bool usePass) {
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
void Dialog::on_radioButtonNative_clicked()
{
    setGroupBoxState();
}

/**
 * @brief Dialog::on_radioButtonPass_clicked
 */
void Dialog::on_radioButtonPass_clicked()
{
    setGroupBoxState();
}

/**
 * @brief Dialog::setGroupBoxState
 */
void Dialog::setGroupBoxState() {
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
QString Dialog::selectExecutable() {
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
QString Dialog::selectFolder() {
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly);
    if (dialog.exec()) {
        return dialog.selectedFiles().first();
    }
    else return "";
}

/**
 * @brief Dialog::on_toolButtonGit_clicked
 */
void Dialog::on_toolButtonGit_clicked()
{
    QString git = selectExecutable();
    if (git != "") {
        ui->gitPath->setText(git);
    }
}

/**
 * @brief Dialog::on_toolButtonGpg_clicked
 */
void Dialog::on_toolButtonGpg_clicked()
{
    QString gpg = selectExecutable();
    if (gpg != "") {
        ui->gpgPath->setText(gpg);
    }
}

/**
 * @brief Dialog::on_toolButtonPass_clicked
 */
void Dialog::on_toolButtonPass_clicked()
{
    QString pass = selectExecutable();
    if (pass != "") {
        ui->passPath->setText(pass);
    }
}

/**
 * @brief Dialog::on_toolButtonStore_clicked
 */
void Dialog::on_toolButtonStore_clicked()
{
    QString store = selectFolder();
    if (store != "") {
        ui->storePath->setText(store);
    }
}

/**
 * @brief Dialog::on_checkBoxClipboard_clicked
 */
void Dialog::on_checkBoxClipboard_clicked()
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
 * @brief Dialog::useClipboard
 */
void Dialog::useClipboard(bool useClipboard)
{
    ui->checkBoxClipboard->setChecked(useClipboard);
    on_checkBoxClipboard_clicked();
}

/**
 * @brief Dialog::useAutoclear
 * @param useAutoclear
 */
void Dialog::useAutoclear(bool useAutoclear)
{
    ui->checkBoxAutoclear->setChecked(useAutoclear);
    on_checkBoxAutoclear_clicked();
}

/**
 * @brief Dialog::setAutoclear
 * @param seconds
 */
void Dialog::setAutoclear(int seconds)
{
    ui->spinBoxAutoclearSeconds->setValue(seconds);
}

/**
 * @brief Dialog::useClipboard
 * @return
 */
bool Dialog::useClipboard()
{
    return ui->checkBoxClipboard->isChecked();
}

/**
 * @brief Dialog::useAutoclear
 * @return
 */
bool Dialog::useAutoclear()
{
    return ui->checkBoxAutoclear->isChecked();
}

/**
 * @brief Dialog::getAutoclear
 * @return
 */
int Dialog::getAutoclear()
{
    return ui->spinBoxAutoclearSeconds->value();
}

/**
 * @brief Dialog::on_checkBoxAutoclear_clicked
 */
void Dialog::on_checkBoxAutoclear_clicked()
{
    on_checkBoxClipboard_clicked();
}

/**
 * @brief Dialog::hidePassword
 * @return
 */
bool Dialog::hidePassword()
{
    return ui->checkBoxHidePassword->isChecked();
}

/**
 * @brief Dialog::hideContent
 * @return
 */
bool Dialog::hideContent()
{
    return ui->checkBoxHideContent->isChecked();
}

/**
 * @brief Dialog::hidePassword
 * @param hidePassword
 */
void Dialog::hidePassword(bool hidePassword)
{
    ui->checkBoxHidePassword->setChecked(hidePassword);
}

/**
 * @brief Dialog::hideContent
 * @param hideContent
 */
void Dialog::hideContent(bool hideContent)
{
    ui->checkBoxHideContent->setChecked(hideContent);
}

/**
 * @brief Dialog::addGPGId
 * @return
 */
bool Dialog::addGPGId()
{
    return ui->checkBoxAddGPGId->isChecked();
}

/**
 * @brief Dialog::addGPGId
 * @param addGPGId
 */
void Dialog::addGPGId(bool addGPGId)
{
    ui->checkBoxAddGPGId->setChecked(addGPGId);
}

void Dialog::wizard()
{
    if(!QFile(ui->gpgPath->text()).exists()){
        QMessageBox::critical(this, tr("GnuPG not found"),
            tr("Please install GnuPG on your system.\nhttps://www.gnupg.org/download/"));
        // TODO REST
    }

    QStringList names = mainWindow->getSecretKeys();
    //qDebug() << names;
    if (names.empty()) {
        QMessageBox::critical(this, tr("Secret key not found"),
            tr("You can not encrypt :("));
        // TODO have usable gpg id wizrd :P
    }

    QString passStore = ui->storePath->text();
    if(!QFile(passStore + ".gpg-id").exists()){
        QMessageBox::critical(this, tr("Password store not initialised"),
            tr("The folder %1 doesn't seem to be a password store or is not yet initialised.").arg(passStore));
        // TODO REST
    }

    // Can you use the store?
 }
