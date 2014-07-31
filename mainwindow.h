#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemModel>
#include <QTreeView>
#include <QProcess>
#include "dialog.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void setPassExecutable(QString);
    void setGitExecutable(QString);
    void setGpgExecutable(QString);
    void checkConfig();

private slots:
    void on_updateButton_clicked();
    void on_treeView_clicked(const QModelIndex &index);

    void on_configButton_clicked();

private:
    Ui::MainWindow *ui;
    QFileSystemModel model;
    QString passStore;
    QString passExecutable;
    QString gitExecutable;
    QString gpgExecutable;
    QProcess *process;
    void updateText();
    void executePass(QString);
    void executeWrapper(QString);
    Dialog* d;
    void config();
};

#endif // MAINWINDOW_H
