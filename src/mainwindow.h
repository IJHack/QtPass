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

class QDialog;
class QTreeWidgetItem;
class QtPass;
class TrayIcon;

/**
 * @class MainWindow
 * @brief Main application window orchestrating UI, user interactions, and
 * external process handlers.
 *
 * Provides the central interface for managing items, folders, passwords, and
 * OTPs; coordinates UI components (toolbars, panels, dialogs, status/tray),
 * selection and navigation in the underlying file/store models, and lifecycle
 * interactions with external handlers (e.g., pass, Git, GPG key generation,
 * OTP).
 */
class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(const QString &searchText = QString(),
                      QWidget *parent = nullptr);
  ~MainWindow() override;

  /**
   * @brief Restore window geometry and state from saved settings.
   */
  void restoreWindow();

  /**
   * @brief Open the GPG key generation dialog.
   * @param batch GPG batch parameter string.
   * @param dialog Parent dialog to return to after generation.
   */
  void generateKeyPair(const QString &batch, QDialog *dialog);

  /**
   * @brief Open the user/recipient management dialog.
   * @param dir Directory for which to manage recipients.
   */
  void userDialog(const QString &dir = "");

  /**
   * @brief Open the configuration dialog.
   */
  void config();

  /**
   * @brief Enable or disable the main UI elements.
   * @param state true to enable, false to disable.
   */
  void setUiElementsEnabled(bool state);

  /**
   * @brief Display a transient message in the text panel.
   * @param text Message text to display.
   * @param isError true to style the message as an error.
   * @param isHtml true if text contains HTML markup.
   */
  void flashText(const QString &text, const bool isError,
                 const bool isHtml = false);

  /**
   * @brief Return the currently selected index in the tree view.
   * @return Current QModelIndex.
   */
  auto getCurrentTreeViewIndex() -> QModelIndex;

  /**
   * @brief Return the active key generation dialog, if any.
   * @return Pointer to the keygen QDialog, or nullptr.
   */
  auto getKeygenDialog() -> QDialog * { return this->keygen; }

  /**
   * @brief Destroy and clear the key generation dialog.
   */
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
  /**
   * @brief Clear the current tree view selection.
   */
  void deselect();

  /**
   * @brief Handle an incoming inter-process message (single-instance mode).
   * @param message Message string received from another instance.
   */
  void messageAvailable(const QString &message);

  /**
   * @brief Display a critical error dialog.
   * @param title Dialog title.
   * @param msg Error message body.
   */
  void critical(const QString &title, const QString &msg);

  /**
   * @brief Slot called when an external process wrapper has started.
   */
  void executeWrapperStarted();

  /**
   * @brief Show a message in the status bar for the given duration.
   * @param msg Message to display.
   * @param timeout Duration in milliseconds (default 2000).
   */
  void showStatusMessage(const QString &msg, int timeout = 2000);

  /**
   * @brief Handle output from the pass show command.
   * @param output Decrypted password file content.
   */
  void passShowHandler(const QString &output);

  /**
   * @brief Handle output from the pass OTP command.
   * @param output OTP output string.
   */
  void passOtpHandler(const QString &output);

  /**
   * @brief Handle results from a completed grep search.
   * @param results List of file/match pairs from the grep operation.
   */
  void onGrepFinished(const QList<QPair<QString, QStringList>> &results);

  /**
   * @brief Trigger a git push operation.
   */
  void onPush();

  /**
   * @brief Handle a click on an item in the tree view.
   * @param index The model index that was clicked.
   */
  void on_treeView_clicked(const QModelIndex &index);

  /**
   * @brief Begin a re-encryption pass on the current path.
   */
  void startReencryptPath();

  /**
   * @brief Finish a re-encryption pass on the current path.
   */
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
  bool m_grepCancelled = false;
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
