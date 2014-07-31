#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDirModel>
#include <QFileSystemModel>
#include <QTreeView>
#include <QFileSystemModel>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_pushButton_clicked();

    void on_treeView_clicked(const QModelIndex &index);

private:
    Ui::MainWindow *ui;
    QFileSystemModel model;
};

#endif // MAINWINDOW_H
