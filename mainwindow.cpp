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
 * @brief MainWindow::on_pushButton_clicked
 */
void MainWindow::on_pushButton_clicked()
{
    executePass("git pull");
}

/**
 * @brief MainWindow::on_treeView_clicked
 * @param index
 */
void MainWindow::on_treeView_clicked(const QModelIndex &index)
{
    if (model.fileInfo(index).isFile()){
        QString passFile = model.filePath(index);
        passFile.replace(".gpg", "");
        passFile.replace(passStore, "");
        executePass(passFile);
    }
}

/**
 * @brief MainWindow::setPassExecutable
 * @param path
 */
void MainWindow::setPassExecutable(QString path) {
    passExecutable = path;
}

/**
 * @brief MainWindow::executePass
 * @param args
 */
void MainWindow::executePass(QString args) {
    process->start("bash", QStringList() << "-c" << "pass " + args);
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
