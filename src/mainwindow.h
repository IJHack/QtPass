// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_MAINWINDOW_H_
#define SRC_MAINWINDOW_H_

#include "storemodel.h"

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
// http://doc.qt.io/qt-5/qkeysequence.html#qt_set_sequence_auto_mnemonic
void qt_set_sequence_auto_mnemonic(bool b);
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
class QDialog;
class QTreeWidgetItem;
class QtPass;
class TrayIcon;
/**
 * Main application window that orchestrates UI, user interactions, and external
 * process handlers.
 *
 * Provides the central interface for managing items, folders, passwords, and
 * OTPs; coordinates UI components (toolbars, panels, dialogs, status/tray),
 * selection and navigation in the underlying file/store models, and lifecycle
 * interactions with external handlers (e.g., pass, Git, GPG key generation,
 * OTP). Exposes methods to restore and configure window state, control grouped
 * UI element enablement, display transient messages, and access or reset the
 * key-generation dialog.
 */
class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(const QString &searchText = QString(),
                      QWidget *parent = nullptr);
  ~MainWindow() override;

  void restoreWindow();
  void generateKeyPair(const QString &, QDialog *);
  void userDialog(const QString & = "");
  void config();

  void setUiElementsEnabled(bool state);
  void flashText(const QString &text, const bool isError,
                 const bool isHtml = false);

  auto getCurrentTreeViewIndex() -> QModelIndex;

  auto getKeygenDialog() -> QDialog * { return this->keygen; }
  void cleanKeygenDialog();

protected:
  void closeEvent(QCloseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void changeEvent(QEvent *event) override;
  auto eventFilter(QObject *obj, QEvent *event) -> bool override;

signals:
  void passShowHandlerFinished(const QString &output);
  void passGitInitNeeded();
  void generateGPGKeyPair(const QString &batch);

public slots:
  void deselect();

  void messageAvailable(const QString &message);
  void critical(const QString &, const QString &);

  void executeWrapperStarted();
  void showStatusMessage(const QString &msg, int timeout = 2000);
  void passShowHandler(const QString &);
  void passOtpHandler(const QString &);
  void onGrepFinished(const QList<QPair<QString, QStringList>> &results);

  void onPush();
  void on_treeView_clicked(const QModelIndex &index);

  void startReencryptPath();
  void endReencryptPath();

private slots:
  void on_grepButton_toggled(bool checked);
  void on_grepResultsList_itemClicked(QTreeWidgetItem *item, int column);
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  void on_profileBox_currentIndexChanged(QString);
#else
  void on_profileBox_currentTextChanged(const QString &);
#endif
  void showContextMenu(const QPoint &pos);
  void showBrowserContextMenu(const QPoint &pos);
  void openFolder();
  void renameFolder();
  void editPassword(const QString &);
  void renamePassword();
  void focusInput();
  void copyPasswordFromTreeview();
  void passwordFromFileToClipboard(const QString &text);
  void onTimeoutSearch();

private:
  QtPass *m_qtPass;
  QScopedPointer<Ui::MainWindow> ui;
  bool m_grepMode = false;
  bool m_grepBusy = false;
  QFileSystemModel model;
  StoreModel proxyModel;
  QScopedPointer<QItemSelectionModel> selectionModel;
  QTimer clearPanelTimer, searchTimer;
  QDialog *keygen;
  QString currentDir;
  TrayIcon *tray;

  void initToolBarButtons();
  void initStatusBar();

  void updateText();
  void selectFirstFile();
  auto firstFile(QModelIndex parentIndex) -> QModelIndex;
  auto getFile(const QModelIndex &, bool) -> QString;
  void setPassword(const QString &, bool isNew = true);

  void updateProfileBox();
  void initTrayIcon();
  void destroyTrayIcon();
  void clearTemplateWidgets();
  void reencryptPath(const QString &dir);
  void addToGridLayout(int position, const QString &field,
                       const QString &value);

  void applyTextBrowserSettings();
  void applyWindowFlagsSettings();

  void updateGitButtonVisibility();
  void updateOtpButtonVisibility();
  void updateGrepButtonVisibility();
  void enableGitButtons(const bool &);
};

#endif // SRC_MAINWINDOW_H_
