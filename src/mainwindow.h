// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_MAINWINDOW_H_
#define SRC_MAINWINDOW_H_

#include "enums.h"
#include "grepsearchcontroller.h"
#include "storemodel.h"

#include <QDialog>
#include <QFileSystemModel>
#include <QMainWindow>
#include <QPointer>
#include <QProcess>
#include <QTimer>

#ifdef __APPLE__
// https://doc.qt.io/qt-6/qkeysequence.html#qt_set_sequence_auto_mnemonic
void qt_set_sequence_auto_mnemonic(bool b);
#endif

namespace Ui {
class MainWindow;
}

class ProcessOutputPanel;
class QProgressDialog;
class QTreeWidgetItem;
class QtPass;
class PasswordDisplayPanel;
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
  /**
   * @brief Construct the main window.
   * @param searchText Initial search text to populate the search field.
   * @param parent Optional parent widget.
   */
  explicit MainWindow(const QString &searchText = QString(),
                      QWidget *parent = nullptr);
  ~MainWindow() override;

  /**
   * @brief Restore window geometry and state from saved settings.
   */
  void restoreWindow();

  /**
   * @brief Open the configuration dialog.
   *
   * On a fresh start the first-run wizard runs before the dialog is shown.
   * Accepting the dialog does not guarantee a usable configuration: the OK
   * button is not gated on Util::configIsValid(), so QtPass::init() uses the
   * return value to decide whether to ask again or to give up.
   * @return true when the dialog was accepted, false when it was cancelled.
   */
  auto config() -> bool;

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
   * @brief Return whether startup configuration succeeded.
   *
   * Reflects the result of the initial configuration check performed in the
   * constructor (QtPass::init()). When it is false the application cannot be
   * used (e.g. the user cancelled the first-run wizard), and main() should
   * exit before ever showing the window.
   * @return true when QtPass::init() reported a working configuration.
   */
  auto initSucceeded() const -> bool { return m_initSucceeded; }

protected:
  /**
   * @brief Save window state and geometry on close.
   * @param event The close event.
   */
  void closeEvent(QCloseEvent *event) override;
  /**
   * @brief Handle keyboard shortcuts.
   * @param event The key press event.
   */
  void keyPressEvent(QKeyEvent *event) override;
  /**
   * @brief React to language or window state changes.
   * @param event The change event.
   */
  void changeEvent(QEvent *event) override;
  /**
   * @brief First-show hook used to focus the search input once the window
   *        is actually mapped (avoids races with timers fired before show).
   * @param event The show event.
   */
  void showEvent(QShowEvent *event) override;
  /**
   * @brief Filter events from watched objects.
   * @param obj The object that received the event.
   * @param event The event to filter.
   * @return true if the event was consumed.
   */
  auto eventFilter(QObject *obj, QEvent *event) -> bool override;

signals:
  /**
   * @brief Emitted when the pass show handler has finished decrypting.
   * @param output Decrypted password file content.
   */
  void passShowHandlerFinished(const QString &output);

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
   * @brief Handle generic process output for display in output panel.
   * @param output Process output text.
   * @param isError true if output should be styled as error (red).
   * @param pid Identifier of the originating subprocess (for the label
   *            prefix); defaults to Enums::INVALID for unlabelled output.
   */
  void onProcessOutput(const QString &output, bool isError,
                       Enums::PROCESS pid = Enums::INVALID);

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
  void passShowHandler(const QString &output, const QString &file = QString());

  /**
   * @brief Generate a one-time password from decrypted content and copy it.
   *        Ignores decrypts of anything but the entry onOtp() asked for.
   * @param output Decrypted entry content.
   * @param file Entry the content belongs to.
   */
  void otpFromFileToClipboard(const QString &output, const QString &file);

  /**
   * @brief Copy the first line of a decrypted entry (Ctrl+C). Ignores
   *        decrypts of anything but the entry copyPasswordFromTreeview()
   *        asked for; an otpauth:// first line is refused.
   * @param text Decrypted entry content.
   * @param file Entry the content belongs to.
   */
  void passwordFromFileToClipboard(const QString &text, const QString &file);

  /**
   * @brief Forget the pending OTP and copy requests.
   *
   * A failed decrypt never emits Pass::finishedShow; without this a later
   * decrypt of the same entry (the user retrying) would be taken as the
   * answer to a request the user has long given up on.
   */
  void cancelOtpRequest();

  /**
   * @brief Drop a style-imposed toolbar palette that belongs to the other
   * theme (Breeze header palette after a light/dark switch).
   */
  void dropStaleToolBarPalette();

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
   *
   * Disables the UI, shows a cancellable progress dialog and keeps both in
   * place until endReencryptPath(): while the worker runs, unrelated
   * completions must not re-enable the interface (see setUiElementsEnabled).
   */
  void startReencryptPath();

  /**
   * @brief Update the re-encryption progress dialog.
   * @param current Files checked so far.
   * @param total Files found under the re-encrypted folder.
   */
  void reencryptProgress(int current, int total);

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
  /**
   * @brief Show the About box: version, project link, licence.
   */
  void showAbout();
  void on_treeView_doubleClicked(const QModelIndex &index);
  void clearPanel(bool notify = true);
  void on_lineEdit_textChanged(const QString &arg1);
  void on_lineEdit_returnPressed();
  void on_profileBox_currentTextChanged(const QString &);
  void showContextMenu(const QPoint &pos);
  void showBrowserContextMenu(const QPoint &pos);
  void openFolder();
  void renameFolder();
  void editPassword(const QString &);
  void renamePassword();
  void focusInput();
  void copyPasswordFromTreeview();
  void onTimeoutSearch();

private:
  QtPass *m_qtPass;
  QScopedPointer<Ui::MainWindow> ui;
  GrepSearchController m_grep;
  PasswordDisplayPanel *m_displayPanel = nullptr;
  bool m_firstShowCompleted = false;
  /// Entry the display panel is currently rendering. The tree's currentIndex
  /// can move without a repaint (arrow keys and right-click do not emit
  /// QTreeView::clicked), so onOtp() must not assume the visible code belongs
  /// to the selected entry.
  QString m_shownFile;
  /// Entry the in-flight OTP request asked for; empty when none. finishedShow
  /// names its file, so otpFromFileToClipboard() simply ignores everything
  /// else and passShowHandler() skips the password copy for this one.
  QString m_otpRequestFile;
  /// Entry the in-flight Ctrl+C request asked for; empty when none. Each
  /// finishedShow is matched to it by file, so a second Ctrl+C on another
  /// entry just replaces the request instead of racing it.
  QString m_copyRequestFile;
  // The process output console is a QDockWidget at the bottom dock area,
  // created programmatically. It isn't part of the .ui because uic places
  // its QMainWindow children in centralWidget / statusBar / menuBar /
  // toolBars / dock-widget slots only — a sibling widget at the
  // QMainWindow level isn't laid out and ends up obscuring the
  // centralWidget. See #1192 for the symptom.
  ProcessOutputPanel *m_processOutput = nullptr;
  QFileSystemModel model;
  StoreModel proxyModel;
  QTimer clearPanelTimer, searchTimer;
  // Re-enables the UI if a backend operation disables it but never reports
  // completion (see setUiElementsEnabled).
  QTimer m_uiWatchdog;
  static constexpr int UiWatchdogMs = 30000;
  /// Progress/cancel dialog shown between startReencryptPath() and
  /// endReencryptPath(); QPointer so a closed dialog reads back as null.
  QPointer<QProgressDialog> m_reencryptProgress;
  /// True between startReencryptPath() and endReencryptPath(). While set,
  /// setUiElementsEnabled(true) is ignored so the git completions queued by
  /// Init/Move/Copy (or the UI watchdog) cannot re-enable the interface
  /// while the re-encryption worker is still rewriting files.
  bool m_reencryptRunning = false;
  TrayIcon *m_tray{};
  /// Result of QtPass::init() from the constructor; main() consults it via
  /// initSucceeded() to decide whether the application should start at all.
  bool m_initSucceeded = false;

  void initToolBarButtons();
  void initStatusBar();

  void selectFirstFile();
  auto firstFile(QModelIndex parentIndex) -> QModelIndex;
  auto getFile(const QModelIndex &, bool) -> QString;
  void setPassword(const QString &, bool isNew = true);
  auto confirmPathInStore(const QString &candidate) -> bool;

  void updateProfileBox();
  void initTrayIcon();
  void destroyTrayIcon();
  void reencryptPath(const QString &dir);
  void exportPublicKey();
  void addRecipient(const QString &dir);
  void showShareHelp();

  void applyTextBrowserSettings();
  void applyWindowFlagsSettings();
  void saveWindowState();

  void updateGitButtonVisibility();
  /**
   * @brief Refresh the OTP toolbar action from settings.
   * @param uiEnabled false while a backend operation is in flight, so the
   *        action is disabled along with the rest of the UI.
   */
  void updateOtpButtonVisibility(bool uiEnabled = true);
  void updateGrepButtonVisibility();
  void enableGitButtons(const bool &);

  void updateProcessOutputVisibility();
};

#endif // SRC_MAINWINDOW_H_
