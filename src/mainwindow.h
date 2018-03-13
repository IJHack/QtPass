#ifndef MAINWINDOW_H_
#define MAINWINDOW_H_

#include "datahelpers.h"
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

#ifdef __APPLE__
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
// http://doc.qt.io/qt-5/qkeysequence.html#qt_set_sequence_auto_mnemonic
void qt_set_sequence_auto_mnemonic(bool b);
#endif
#endif

namespace Ui {
class MainWindow;
}

/*!
    \class MainWindow
    \brief The MainWindow class does way too much, not only is it a switchboard,
    configuration handler and more, it's also the process-manager.

    This class could really do with an overhaul.
 */
class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();
  bool checkConfig();
  void setApp(SingleApplication *app);
  void setText(QString);
  QStringList getSecretKeys();
  void generateKeyPair(QString, QDialog *);
  void userDialog(QString = "");
  void config();
  void executePassGitInit();

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
  void on_updateButton_clicked(bool block = false);
  void on_pushButton_clicked();
  void on_treeView_clicked(const QModelIndex &index);
  void on_treeView_doubleClicked(const QModelIndex &index);
  void on_configButton_clicked();
  void processFinished(const QString &, const QString &);
  void processError(QProcess::ProcessError);
  void clearClipboard();
  void clearPanel(bool notify = true);
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
  void editPassword(const QString &);
  void focusInput();
  void copyTextToClipboard(const QString &text);

  void executeWrapperStarted();
  void showStatusMessage(QString msg, int timeout);
  void startReencryptPath();
  void endReencryptPath();
  void critical(QString, QString);
  void passShowHandler(const QString &);
  void passStoreChanged(const QString &, const QString &);
  void doGitPush();

  void processErrorExit(int exitCode, const QString &);

  void finishedInsert(const QString &, const QString &);
  void keyGenerationComplete(const QString &p_output, const QString &p_errout);

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
  QTimer clearPanelTimer;
  QTimer clearClipboardTimer;
  bool freshStart;
  QDialog *keygen;
  QString currentDir;
  bool startupPhase;
  TrayIcon *tray;

  void updateText();
  void enableUiElements(bool state);
  void selectFirstFile();
  QModelIndex firstFile(QModelIndex parentIndex);
  QString getFile(const QModelIndex &, bool);
  void setPassword(QString, bool isNew = true);

  void mountWebDav();
  void updateProfileBox();
  void initTrayIcon();
  void destroyTrayIcon();
  void clearTemplateWidgets();
  void reencryptPath(QString dir);
  void addToGridLayout(int position, const QString &field,
                       const QString &value);
  void DisplayInTextBrowser(QString toShow, QString prefix = QString(),
                            QString postfix = QString());
  void connectPassSignalHandlers(Pass *pass);

  void updateGitButtonVisibility();
  void hideGitButtons();
  void showGitButtons();
};

#endif // MAINWINDOW_H_
