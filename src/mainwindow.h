#ifndef MAINWINDOW_H_
#define MAINWINDOW_H_

#include "storemodel.h"

#include <QDialog>
#include <QFileSystemModel>
#include <QItemSelectionModel>
#include <QMainWindow>
#include <QProcess>
#include <QTimer>

#if SINGLE_APP
class SingleApplication;
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
class QtPass;
class TrayIcon;
class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(const QString &searchText = QString(),
                      QWidget *parent = nullptr);
  ~MainWindow();
  bool checkConfig();
  QStringList getSecretKeys();
  void generateKeyPair(QString, QDialog *);
  void userDialog(QString = "");
  void config();
  void executePassGitInit();

  void setTextTextBrowser(const QString &text);
  void setUiElementsEnabled(bool state);
  void flashText(const QString &text, const bool isError,
                 const bool isHtml = false);

  const QModelIndex getCurrentTreeViewIndex();

  QDialog *getKeygenDialog() { return this->keygen; }
  void cleanKeygenDialog() {
    this->keygen->close();
    this->keygen = 0;
  }

protected:
  void closeEvent(QCloseEvent *event);
  void keyPressEvent(QKeyEvent *event);
  void changeEvent(QEvent *event);
  bool eventFilter(QObject *obj, QEvent *event);

signals:
  void uiEnabled(bool state);
  void passShowHandlerFinished(QString output);

public slots:
  void deselect();
  void messageAvailable(QString message);

  void executeWrapperStarted();
  void critical(QString, QString);
  void showStatusMessage(QString msg, int timeout = 2000);
  void passShowHandler(const QString &);
  void passOtpHandler(const QString &);

  void onPush();
  void on_treeView_clicked(const QModelIndex &index);

  void startReencryptPath();
  void endReencryptPath();

private slots:
  void addPassword();
  void addFolder();
  void onEdit();
  void onDelete();
  void onOtp();
  void onUpdate(bool block = false);
  void onUsers();
  void onConfig();
  void on_treeView_doubleClicked(const QModelIndex &index);
  void clearPanel(bool notify = true);
  void on_lineEdit_textChanged(const QString &arg1);
  void on_lineEdit_returnPressed();
  void on_profileBox_currentIndexChanged(QString);
  void showContextMenu(const QPoint &pos);
  void showBrowserContextMenu(const QPoint &pos);
  void openFolder();
  void editPassword(const QString &);
  void focusInput();
  void copyPasswordFromTreeview();
  void passwordFromFileToClipboard(const QString &text);

private:
  QtPass *m_qtPass;
  QScopedPointer<Ui::MainWindow> ui;
  QFileSystemModel model;
  StoreModel proxyModel;
  QScopedPointer<QItemSelectionModel> selectionModel;
  QProcess fusedav;
  QTimer clearPanelTimer;
  bool freshStart;
  QDialog *keygen;
  QString currentDir;
  bool startupPhase;
  TrayIcon *tray;

  void initToolBarButtons();
  void initStatusBar();

  void updateText();
  void restoreWindow();
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

  void updateGitButtonVisibility();
  void updateOtpButtonVisibility();
  void enableGitButtons(const bool &);
};

#endif // MAINWINDOW_H_
