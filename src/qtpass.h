// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_QTPASS_H_
#define SRC_QTPASS_H_

#include <QDialog>
#include <QObject>
#include <QPixmap>
#include <QProcess>
#include <QTimer>

class MainWindow;
class Pass;
/**
 * Construct a QtPass instance associated with the given main window.
 * @param mainWindow Pointer to the application's MainWindow used for UI
 * interactions.
 */
/**
 * Destroy the QtPass instance and perform necessary cleanup.
 */
/**
 * Initialize internal state and connections required for QtPass operation.
 * @returns `true` if initialization succeeded, `false` otherwise.
 */
/**
 * Update the tracked clipped text value.
 * @param text Primary text to store as clipped content.
 * @param p_output Optional additional output text used when computing the
 * clipped value.
 */
/**
 * Remove any stored clipped text value.
 */
/**
 * Configure and start the timer that will clear the system clipboard after a
 * delay.
 */
/**
 * Return whether this instance was created in a fresh-start state.
 * @returns `true` if in fresh-start state, `false` otherwise.
 */
/**
 * Set the fresh-start state for this instance.
 * @param fs New fresh-start state value.
 */
/**
 * Set up internal references and Qt connections related to the associated
 * MainWindow.
 */
/**
 * Connect QtPass handlers to the provided Pass instance's signals.
 * @param pass Pointer to the Pass instance whose signals will be connected.
 */
/**
 * Perform WebDAV mounting operations required by the application.
 */
/**
 * Clear the system clipboard contents immediately.
 */
/**
 * Copy the provided text into the system clipboard and trigger any clipboard
 * lifecycle handling.
 * @param text Text to copy into the clipboard.
 */
/**
 * Request display of the provided text as a QR code in the UI.
 * @param text Text to convert into a QR code for display.
 */
/**
 * Create a modal dialog configured to display the given QR code image.
 * @param image Pixmap containing the rendered QR code.
 * @returns Pointer to a QDialog configured to show the provided image.
 */
/**
 * Handle errors reported by the internal QProcess instance.
 * @param error The process error code emitted by QProcess.
 */
/**
 * Handle an external process exiting with an error code and message.
 * @param exitCode Numeric exit code returned by the process.
 * @param message Associated error or status message from the process.
 */
/**
 * Handle completion of an external process and provide its stdout and stderr.
 * @param stdout Output captured from the process's standard output.
 * @param stderr Output captured from the process's standard error.
 */
/**
 * React to changes in the pass store and update relevant UI/state.
 * @param arg1 First string parameter describing the change (context-specific).
 * @param arg2 Second string parameter describing the change (context-specific).
 */
/**
 * Handle completion of a "pass show" handler and process its output.
 * @param output Output produced by the pass show handler.
 */
/**
 * Initiate a Git push operation for the repository managed by the application.
 */
/**
 * Handle completion of an insert operation and react to its outputs.
 * @param arg1 First result string related to the insert operation.
 * @param arg2 Second result string related to the insert operation.
 */
/**
 * Handle completion of a key generation operation and its outputs.
 * @param p_output Standard output produced during key generation.
 * @param p_errout Standard error produced during key generation.
 */
/**
 * Display the provided output string inside the application's text browser
 * widget, optionally wrapping with prefix and postfix strings.
 * @param output Text to display in the text browser.
 * @param prefix Optional prefix to prepend to the output before display.
 * @param postfix Optional postfix to append to the output before display.
 */
class QtPass : public QObject {
  Q_OBJECT

public:
  QtPass(MainWindow *mainWindow);
  ~QtPass();

  auto init() -> bool;
  void setClippedText(const QString &, const QString &p_output = QString());
  void clearClippedText();
  void setClipboardTimer();
  auto isFreshStart() -> bool { return this->freshStart; }
  void setFreshStart(const bool &fs) { this->freshStart = fs; }

private:
  MainWindow *m_mainWindow;

  QProcess fusedav;

  QTimer clearClipboardTimer;
  QString clippedText;
  bool freshStart;

  void setMainWindow();
  void connectPassSignalHandlers(Pass *pass);
  void mountWebDav();

signals:

public slots:
  void clearClipboard();
  void copyTextToClipboard(const QString &text);
  void showTextAsQRCode(const QString &text);

public:
  static QDialog *createQRCodePopup(const QPixmap &image);

private slots:
  void processError(QProcess::ProcessError);
  void processErrorExit(int exitCode, const QString &);
  void processFinished(const QString &, const QString &);

  void passStoreChanged(const QString &, const QString &);
  void passShowHandlerFinished(QString output);

  void doGitPush();
  void finishedInsert(const QString &, const QString &);
  void onKeyGenerationComplete(const QString &p_output,
                               const QString &p_errout);

  void showInTextBrowser(QString output, const QString &prefix = QString(),
                         const QString &postfix = QString());
};

#endif // SRC_QTPASS_H_
