#include "dialog.h"
#include "ui_dialog.h"
#include "mainwindow.h"
#include "keygendialog.h"
#include <QDebug>
#include <QMessageBox>
#include <QDir>

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
    ui->profileTable->verticalHeader()->hide();
    ui->profileTable->horizontalHeader()->setStretchLastSection(true);
}

/**
 * @brief Dialog::~Dialog
 */
Dialog::~Dialog()
{
    mainWindow->setGitExecutable(ui->gitPath->text());
    mainWindow->setGpgExecutable(ui->gpgPath->text());
    mainWindow->setPassExecutable(ui->passPath->text());
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
    if (store != "") { // TODO call check
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
/**
 * @brief Dialog::wizard
 */
void Dialog::wizard()
{
    //mainWindow->checkConfig();

    QString gpg = ui->gpgPath->text();
    //QString gpg = mainWindow->getGpgExecutable();
    if(!QFile(gpg).exists()){
        QMessageBox::critical(this, tr("GnuPG not found"),
            tr("Please install GnuPG on your system.<br>Install <strong>gpg</strong> using your favorite package manager<br>or <a href=\"https://www.gnupg.org/download/#sec-1-2\">download</a> it from GnuPG.org"));

        // TODO REST ?
    }

    QStringList names = mainWindow->getSecretKeys();
    //qDebug() << names;
    if (QFile(gpg).exists() && names.empty()) {
        KeygenDialog d(this);
        d.exec();
    }

    QString passStore = ui->storePath->text();
    if(!QFile(passStore + ".gpg-id").exists()){
        QMessageBox::critical(this, tr("Password store not initialised"),
            tr("The folder %1 doesn't seem to be a password store or is not yet initialised.").arg(passStore));
        while(!QFile(passStore).exists()) {
            on_toolButtonStore_clicked();
            passStore = ui->storePath->text();
        }
        if (!QFile(passStore + ".gpg-id").exists()) {
            // apears not to be store
            // init yes / no ?
            mainWindow->userDialog(passStore);
        }
    }

    // Can you use the store?


    //ui->gpgPath->setText(gpg);
}

/**
 * @brief Dialog::genKey
 * @param QString batch
 */
void Dialog::genKey(QString batch, QDialog *dialog)
{
    mainWindow->genKey(batch, dialog);
}

/**
 * @brief Dialog::setProfiles
 * @param profiles
 * @param profile
 */
void Dialog::setProfiles(QHash<QString, QString> profiles, QString profile)
{
    ui->profileTable->setRowCount(profiles.count());
    QHashIterator<QString, QString> i(profiles);
    int n = 0;
    while (i.hasNext()) {
        i.next();
        if (!i.value().isEmpty()) {
            ui->profileTable->setItem(n, 0, new QTableWidgetItem(i.key()));
            ui->profileTable->setItem(n, 1, new QTableWidgetItem(i.value()));
            //qDebug() << "naam:" + i.key();
            if (i.key() == profile) {
                ui->profileName->setText(i.key());
                ui->storePath->setText(i.value());
            }
        }
        n++;
    }
}

/**
 * @brief Dialog::getProfiles
 * @return
 */
QHash<QString, QString> Dialog::getProfiles()
{
    QHash<QString, QString> profiles;
    for (int i = 0; i < ui->profileTable->rowCount(); i++) {
        QString path = ui->profileTable->item(i, 1)->text();
        if (!path.isEmpty()) {
            profiles.insert(ui->profileTable->item(i, 0)->text(),
                            path);
        }
    }
    return profiles;
}

/**
 * @brief Dialog::on_addButton_clicked
 */
void Dialog::on_addButton_clicked()
{
    QString name = ui->profileName->text();
    int n = 0;
    bool newItem = true;
    QAbstractItemModel *model = ui->profileTable->model();
    QModelIndexList matches = model->match( model->index(0,0), Qt::DisplayRole, name);
    foreach(const QModelIndex &index, matches)
    {
        QTableWidgetItem *item = ui->profileTable->item(index.row(), index.column());
        n = item->row();
        qDebug() << "overwrite:" << item->text();
        newItem = false;
    }
    if (newItem) {
        n = ui->profileTable->rowCount();
        ui->profileTable->insertRow(n);
    }
    ui->profileTable->setItem(n, 0, new QTableWidgetItem(name));
    ui->profileTable->setItem(n, 1, new QTableWidgetItem(ui->storePath->text()));
    //qDebug() << ui->profileName->text();
    ui->profileTable->selectRow(n);
    if (ui->profileTable->rowCount() < 1) {
        ui->deleteButton->setEnabled(true);
    }
}

/**
 * @brief Dialog::on_profileTable_currentItemChanged
 * @param current
 */
void Dialog::on_profileTable_currentItemChanged(QTableWidgetItem *current)
{
    if (current == 0) {
        return;
    }
    int n = current->row();
    ui->profileName->setText(ui->profileTable->item(n, 0)->text());
    ui->storePath->setText(ui->profileTable->item(n, 1)->text());
}

/**
 * @brief Dialog::on_deleteButton_clicked
 */
void Dialog::on_deleteButton_clicked()
{
    QList<QTableWidgetItem*> selected = ui->profileTable->selectedItems();
    if (selected.count() == 0) {
        QMessageBox::warning(this, tr("No profile selected"),
            tr("No profile selected to delete"));
        return;
    }
    for (int i = 0; i < selected.size(); ++i) {
        QTableWidgetItem* item = selected.at(i);
        ui->profileTable->removeRow(item->row());
    }
    if (ui->profileTable->rowCount() < 1) {
        ui->deleteButton->setEnabled(false);
    }
}
