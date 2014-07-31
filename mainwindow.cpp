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
    passStore = QDir::homePath()+"/.password-store/";
    process = new QProcess(this);

    ui->setupUi(this);
    ui->treeView->setModel(&model);
    ui->treeView->setRootIndex(model.setRootPath(passStore));
    ui->treeView->setColumnHidden( 1, true );
    ui->treeView->setColumnHidden( 2, true );
    ui->treeView->setColumnHidden( 3, true );
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
    if (passExecutable == "" && (gitExecutable == "" || gpgExecutable == "")) {
        config();
    }
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

    if (d->exec()) {
        if (d->result() == QDialog::Accepted) {
            passExecutable = d->getPassPath();
            gitExecutable = d->getGitPath();
            gpgExecutable = d->getGpgPath();
        }
    }
}

/**
 * @brief MainWindow::on_updateButton_clicked
 */
void MainWindow::on_updateButton_clicked()
{
    if (passExecutable == "") {
        executeWrapper(gitExecutable + " pull");
    } else {
        executePass("git pull");
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
        if (passExecutable == "") {
            executeWrapper(gpgExecutable + " --no-tty -dq " + passFile);
        } else {
            passFile.replace(".gpg", "");
            passFile.replace(passStore, "");
            executePass(passFile);
        }
    }
}

/**
 * @brief MainWindow::executePass
 * @param args
 */
void MainWindow::executePass(QString args) {
    executeWrapper("pass "+args);
}

/**
 * @brief MainWindow::executeWrapper
 * @param args
 */
void MainWindow::executeWrapper(QString args) {
    process->setWorkingDirectory(passStore);
    process->start("bash", QStringList() << "-c" << args);
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
