#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <stdio.h>
#include <stdlib.h>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->treeView->setModel(&model);
    ui->treeView->setRootIndex(model.index(QDir::homePath()+"/.password-store"));
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
    system("pass git pull");
}

void MainWindow::on_treeView_clicked(const QModelIndex &index)
{
    QMessageBox msgBox;
    msgBox.setText(model.fileInfo(index).filePath());
    msgBox.exec();

}

