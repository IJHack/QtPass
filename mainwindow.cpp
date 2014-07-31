#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    passStore = QDir::homePath()+"/.password-store/";

    ui->setupUi(this);
    ui->treeView->setModel(&model);
    ui->treeView->setRootIndex(model.setRootPath(passStore));
    ui->treeView->setColumnHidden( 1, true );
    ui->treeView->setColumnHidden( 2, true );
    ui->treeView->setColumnHidden( 3, true );
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_clicked()
{
    process.start("pass", QStringList() << "git" << "pull");
    process.waitForFinished();
    ui->textBrowser->setText(process.readAllStandardOutput());
}

void MainWindow::on_treeView_clicked(const QModelIndex &index)
{
    if (model.fileInfo(index).isFile()){
        QString passFile = model.filePath(index);
        passFile.replace(".gpg", "");
        passFile.replace(passStore, "");
        process.start("pass", QStringList() << passFile);
        process.waitForFinished();
        ui->textBrowser->setText(process.readAllStandardOutput());
    }
}

