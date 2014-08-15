#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeView>
#include <QFileSystemModel>
#include <QProcess>
#include <QSettings>
#include "storemodel.h"
#include "dialog.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

enum actionType { GPG, GIT };

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
    void readyRead();
    void processFinished(int, QProcess::ExitStatus);
    void processError(QProcess::ProcessError);
    void clearClipboard();

    void on_lineEdit_textChanged(const QString &arg1);

    void on_lineEdit_returnPressed();

private:
    Ui::MainWindow *ui;
    QFileSystemModel model;
    StoreModel proxyModel;
    QItemSelectionModel *selectionModel;
    bool usePass;
    bool useClipboard;
    bool useAutoclear;
    int autoclearSeconds;
    QString passStore;
    QString passExecutable;
    QString gitExecutable;
    QString gpgExecutable;
    QProcess *process;
    Dialog* d;
    void updateText();
    void executePass(QString);
    void executeWrapper(QString, QString);
    void config();
    void enableUiElements(bool);
    void selectFirstFile();
    actionType currentAction;
};

#endif // MAINWINDOW_H
