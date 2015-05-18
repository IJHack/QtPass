#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeView>
#include <QFileSystemModel>
#include <QProcess>
#include <QQueue>
#include <QSettings>
#include "storemodel.h"
#if SINGLE_APP
#include "singleapplication.h"
#else
#define SingleApplication QApplication
#endif

namespace Ui {
class MainWindow;
}

struct execQueueItem {
    QString app;
    QString args;
    QString input;
};

struct UserInfo;

class MainWindow : public QMainWindow
{
    Q_OBJECT

enum actionType { GPG, GIT, EDIT, DELETE, GPG_INTERNAL };

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void setPassExecutable(QString);
    void setGitExecutable(QString);
    void setGpgExecutable(QString);
    bool checkConfig();
    void setApp(SingleApplication* app);
    void setText(QString);

private slots:
    void on_updateButton_clicked();
    void on_pushButton_clicked();
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
    void on_usersButton_clicked();
    void messageAvailable(QString message);

private:
    QApplication *QtPass;
    QScopedPointer<QSettings> settings;
    QScopedPointer<Ui::MainWindow> ui;
    QFileSystemModel model;
    StoreModel proxyModel;
    QScopedPointer<QItemSelectionModel> selectionModel;
    QScopedPointer<QProcess> process;
    bool usePass;
    bool useClipboard;
    bool useAutoclear;
    bool hidePassword;
    bool hideContent;
    bool addGPGId;
    int autoclearSeconds;
    QString passStore;
    QString passExecutable;
    QString gitExecutable;
    QString gpgExecutable;
    QString gpgHome;
    bool useWebDav;
    QString webDavUrl;
    QString webDavUser;
    QString webDavPassword;
    QProcess fusedav;
    QString clippedPass;
    actionType currentAction;
    QString lastDecrypt;
    bool wrapperRunning;
    QStringList env;
    QQueue<execQueueItem> *execQueue;
    bool firstRun;
    void updateText();
    void executePass(QString, QString = QString());
    void executeWrapper(QString, QString, QString = QString());
    void config();
    void enableUiElements(bool);
    void selectFirstFile();
    QModelIndex firstFile(QModelIndex parentIndex);
    QString getDir(const QModelIndex &, bool);
    QString getFile(const QModelIndex &, bool);
    void setPassword(QString, bool);
    QSettings &getSettings();
    QList<UserInfo> listKeys(QString keystring = "", bool secret = false);
    QStringList getRecipientList(QString for_file);
    QString getRecipientString(QString for_file, QString separator = " ", int *count = NULL);
    void mountWebDav();
};

#endif // MAINWINDOW_H
