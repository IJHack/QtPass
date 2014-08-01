#include "mainwindow.h"
#include "ui_mainwindow.h"

/**
 * @brief MainWindow::MainWindow
 * @param parent
 */
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    process = new QProcess(this);
    ui->setupUi(this);
}

/**
 * @brief MainWindow::~MainWindow
 */
MainWindow::~MainWindow()
{
    delete ui;
}

/**
 * @brief MainWindow::checkConfig
 */
void MainWindow::checkConfig() {

    QSettings settings("IJHack", "QtPass");

    usePass = (settings.value("usePass") == "true");

    passStore = settings.value("passStore").toString();
    if (passStore == "") {
        passStore = QDir::homePath()+"/.password-store/";
        /** @TODO exists? */
        settings.setValue("passStore", passStore);
    }

    passExecutable = settings.value("passExecutable").toString();
    if (passExecutable == "") {
        process->start("which pass");
        process->waitForFinished();
        if (process->exitCode() == 0) {
            passExecutable = process->readAllStandardOutput().trimmed();
            settings.setValue("passExecutable", passExecutable);
            usePass = true;
            settings.setValue("usePass", "true");
        }
    }

    gitExecutable = settings.value("gitExecutable").toString();
    if (gitExecutable == "") {
        process->start("which git");
        process->waitForFinished();
        if (process->exitCode() == 0) {
            gitExecutable = process->readAllStandardOutput().trimmed();
            settings.setValue("gitExecutable", gitExecutable);
        }
    }

    gpgExecutable = settings.value("gpgExecutable").toString();
    if (gpgExecutable == "") {
        process->start("which gpg2");
        process->waitForFinished();
        if (process->exitCode() != 0) {
            process->start("which gpg");
            process->waitForFinished();
        }
        if (process->exitCode() == 0) {
            gpgExecutable = process->readAllStandardOutput().trimmed();
            settings.setValue("gpgExecutable", gpgExecutable);
        }
    }

    if (passExecutable == "" && (gitExecutable == "" || gpgExecutable == "")) {
        config();
    }

    ui->treeView->setModel(&model);
    ui->treeView->setRootIndex(model.setRootPath(passStore));
    ui->treeView->setColumnHidden(1, true);
    ui->treeView->setColumnHidden(2, true);
    ui->treeView->setColumnHidden(3, true);
}

/**
 * @brief MainWindow::config
 */
void MainWindow::config() {
    d = new Dialog();
    d->setModal(true);

    d->setPassPath(passExecutable);
    d->setGitPath(gitExecutable);
    d->setGpgPath(gpgExecutable);
    d->setStorePath(passStore);
    d->usePass(usePass);

    if (d->exec()) {
        if (d->result() == QDialog::Accepted) {
            passExecutable = d->getPassPath();
            gitExecutable = d->getGitPath();
            gpgExecutable = d->getGpgPath();
            passStore = d->getStorePath();
            usePass = d->usePass();

            QSettings settings("IJHack", "QtPass");

            settings.setValue("passExecutable", passExecutable);
            settings.setValue("gitExecutable", gitExecutable);
            settings.setValue("gpgExecutable", gpgExecutable);
            settings.setValue("passStore", passStore);
            settings.setValue("usePass", usePass ? "true" : "false");

            ui->treeView->setRootIndex(model.setRootPath(passStore));
        }
    }
}

/**
 * @brief MainWindow::on_updateButton_clicked
 */
void MainWindow::on_updateButton_clicked()
{
    if (usePass) {
        executePass("git pull");
    } else {
        executeWrapper(gitExecutable + " pull");
    }
}

/**
 * @brief MainWindow::on_treeView_clicked
 * @param index
 */
void MainWindow::on_treeView_clicked(const QModelIndex &index)
{
    if (model.fileInfo(index).isFile()){
        QString passFile = model.filePath(index);
        if (usePass) {
            passFile.replace(".gpg", "");
            passFile.replace(passStore, "");
            executePass(passFile);
        } else {
            executeWrapper(gpgExecutable + " --no-tty -dq " + passFile);
        }
    }
}

/**
 * @brief MainWindow::executePass
 * @param args
 */
void MainWindow::executePass(QString args) {
    executeWrapper("pass " + args);
}

/**
 * @brief MainWindow::executeWrapper
 * @param args
 */
void MainWindow::executeWrapper(QString args) {
    process->setWorkingDirectory(passStore);
    process->start("sh", QStringList() << "-c" << args);
    process->waitForFinished();
    QString output = process->readAllStandardError();
    if (output.size() > 0) {
        ui->textBrowser->setTextColor(Qt::red);
    } else {
        ui->textBrowser->setTextColor(Qt::black);
        output = process->readAllStandardOutput();
    }
    ui->textBrowser->setText(output);
}

/**
 * @brief MainWindow::setPassExecutable
 * @param path
 */
void MainWindow::setPassExecutable(QString path) {
    passExecutable = path;
}

/**
 * @brief MainWindow::setGitExecutable
 * @param path
 */
void MainWindow::setGitExecutable(QString path) {
    gitExecutable = path;
}

/**
 * @brief MainWindow::setGpgExecutable
 * @param path
 */
void MainWindow::setGpgExecutable(QString path) {
    gpgExecutable = path;
}

/**
 * @brief MainWindow::on_configButton_clicked
 */
void MainWindow::on_configButton_clicked()
{
    config();
}
