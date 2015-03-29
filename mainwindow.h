#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeView>
#include <QFileSystemModel>
#include <QProcess>
#include <QSettings>
#include "storemodel.h"
#include "dialog.h"
#include "singleapplication.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

enum actionType { GPG, GIT, EDIT, DELETE };

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void setPassExecutable(QString);
    void setGitExecutable(QString);
    void setGpgExecutable(QString);
    void checkConfig();
    void setApp(SingleApplication* app);

private slots:
    void on_updateButton_clicked();
    void on_treeView_clicked(const QModelIndex &index);
    void on_configButton_clicked();
    void readyRead(bool finished);
    void processFinished(int, QProcess::ExitStatus);
    void processError(QProcess::ProcessError);
    void clearClipboard();
    void on_lineEdit_textChanged(const QString &arg1);
    void on_lineEdit_returnPressed();
    void on_clearButton_clicked();
    void on_addButton_clicked();
    void on_deleteButton_clicked();
    void on_editButton_clicked();
    void messageAvailable(QString message);

private:
    QSettings *settings;
    Ui::MainWindow *ui;
    QFileSystemModel model;
    StoreModel proxyModel;
    QItemSelectionModel *selectionModel;
    QProcess *process;
    SingleApplication *a;
    Dialog* d;
    bool usePass;
    bool useClipboard;
    bool useAutoclear;
    bool hidePassword;
    bool hideContent;
    int autoclearSeconds;
    QString passStore;
    QString passExecutable;
    QString gitExecutable;
    QString gpgExecutable;
    QString clippedPass;
    actionType currentAction;
    QString lastDecrypt;
    void updateText();
    void executePass(QString, QString = QString());
    void executeWrapper(QString, QString, QString = QString());
    void config();
    void enableUiElements(bool);
    void selectFirstFile();
    QModelIndex firstFile(QModelIndex parentIndex);
    QString getFile(const QModelIndex &, bool);
    void setPassword(QString, bool);
    void normalizePassStore();
    QSettings &getSettings();
};

#endif // MAINWINDOW_H
