#ifndef MAINWINDOW_H_
#define MAINWINDOW_H_

#include "enums.h"
#include "imitatepass.h"
#include "pass.h"
#include "realpass.h"
#include "storemodel.h"
#include "trayicon.h"
#include <QFileSystemModel>
#include <QMainWindow>
#include <QProcess>
#include <QQueue>
#include <QTimer>
#include <QTreeView>

#if SINGLE_APP
#include "singleapplication.h"
#else
#define SingleApplication QApplication
#endif

namespace Ui {
class MainWindow;
}

struct UserInfo;

/*!
    \struct passwordConfiguration
    \brief  holds the Password configuration settings
 */
struct passwordConfiguration {
  /**
   * @brief passwordConfiguration::selected character set.
   */
  int selected;
  /**
   * @brief passwordConfiguration::length of password.
   */
  int length;
  /**
   * @brief passwordConfiguration::Characters the different character sets.
   */
  QString Characters[4];
  passwordConfiguration() {
    length = 16;
    selected = 0;
    enum selected { ALLCHARS, ALPHABETICAL, ALPHANUMERIC, CUSTOM };
    Characters[ALLCHARS] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890~!@#$%^&"
        "*()_-+={}[]|:;<>,.?"; /*AllChars*/
    Characters[ALPHABETICAL] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstu"
                               "vwxyz"; /*Only Alphabetical*/
    Characters[ALPHANUMERIC] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstu"
                               "vwxyz1234567890"; /*Alphabetical and Numerical*/
    Characters[CUSTOM] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1"
                         "234567890~!@#$%^&*()_-+={}[]|:;<>,.?"; /*Custom*/
  }
};

/*!
    \class MainWindow
    \brief The MainWindow class does way too much, not only is it a switchboard,
    configuration handler and more, it's also the process-manager.

    This class could really do with an overhaul.
 */
class MainWindow : public QMainWindow {
  Q_OBJECT

  enum actionType { GPG, GIT, EDIT, REMOVE, GPG_INTERNAL, PWGEN };

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();
  bool checkConfig();
  void setApp(SingleApplication *app);
  void setText(QString);
  QStringList getSecretKeys();
  void generateKeyPair(QString, QDialog *);
  void userDialog(QString = "");
  QString generatePassword(int length, Enums::characterSet selection);
  void config();
  void executePassGitInit();
  void copyTextToClipboard(const QString &text);

  /**
   * @brief MainWindow::pwdConfig instance of passwordConfiguration.
   * @sa MainWindow::passwordConfiguration
   */
  passwordConfiguration pwdConfig;

protected:
  void closeEvent(QCloseEvent *event);
  void keyPressEvent(QKeyEvent *event);
  void changeEvent(QEvent *event);
  bool eventFilter(QObject *obj, QEvent *event);

public slots:
  void deselect();

private slots:
  void on_updateButton_clicked();
  void on_pushButton_clicked();
  void on_treeView_clicked(const QModelIndex &index);
  void on_treeView_doubleClicked(const QModelIndex &index);
  void on_configButton_clicked();
  void readyRead(bool finished);
  void processFinished(int, QProcess::ExitStatus);
  void processError(QProcess::ProcessError);
  void clearClipboard();
  void clearPanel(bool notify);
  void on_lineEdit_textChanged(const QString &arg1);
  void on_lineEdit_returnPressed();
  void on_addButton_clicked();
  void on_deleteButton_clicked();
  void on_editButton_clicked();
  void on_usersButton_clicked();
  void messageAvailable(QString message);
  void on_profileBox_currentIndexChanged(QString);
  void showContextMenu(const QPoint &pos);
  void showBrowserContextMenu(const QPoint &pos);
  void addFolder();
  void editPassword();
  void focusInput();
  void copyTextByButtonClick(bool checked = false);

  void executeWrapperStarted();
  void showStatusMessage(QString msg, int timeout);
  void startReencryptPath();
  void endReencryptPath();
  void critical(QString, QString);
  void setLastDecrypt(QString);

private:
  QAction *actionAddPassword;
  QAction *actionAddFolder;

  QApplication *QtPass;
  QScopedPointer<Ui::MainWindow> ui;
  QFileSystemModel model;
  StoreModel proxyModel;
  QScopedPointer<QItemSelectionModel> selectionModel;
  QTreeView *treeView;
  QProcess fusedav;
  QString clippedText;
  QString autoclearPass;
  QTimer *autoclearTimer;
  actionType currentAction;
  QString lastDecrypt;
  QQueue<execQueueItem> *execQueue;
  bool freshStart;
  QDialog *keygen;
  QString currentDir;
  bool startupPhase;
  TrayIcon *tray;
  Pass *pass;
  RealPass rpass;
  ImitatePass ipass;

  void updateText();
  void enableUiElements(bool state);
  void selectFirstFile();
  QModelIndex firstFile(QModelIndex parentIndex);
  QString getDir(const QModelIndex &, bool);
  QString getFile(const QModelIndex &, bool);
  void setPassword(QString, bool, bool);
  QList<UserInfo> listKeys(QString keystring = "", bool secret = false);

  void mountWebDav();
  void updateProfileBox();
  void initTrayIcon();
  void destroyTrayIcon();
  void clearTemplateWidgets();
  void reencryptPath(QString dir);
  void addToGridLayout(int position, const QString &field,
                       const QString &value);
};

#endif // MAINWINDOW_H_
