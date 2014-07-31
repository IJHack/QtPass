#include "dialog.h"
#include "ui_dialog.h"

/**
 * @brief Dialog::Dialog
 * @param parent
 */
Dialog::Dialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog)
{
    ui->setupUi(this);
}

/**
 * @brief Dialog::~Dialog
 */
Dialog::~Dialog()
{
    delete ui;
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
