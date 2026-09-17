// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_QTPASS_H_
#define SRC_QTPASS_H_

#include "clipboardmanager.h"
#include <QDialog>
#include <QObject>
#include <QPixmap>

class Pass;

/**
 * @class QtPass
 * @brief Application-level glue between the pass backends and whatever shows
 * their results: turns the backends' completion signals into UI-neutral
 * signals (formatted output, status messages, "the operation is over"), owns
 * the ClipboardManager, migrates the settings of older versions on start-up
 * and renders QR codes. It knows nothing about the main window; the window
 * connects to the signals below.
 */
class QtPass : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Construct the glue and connect it to both pass backends.
   * @param parent Owner.
   */
  explicit QtPass(QObject *parent = nullptr);
  /**
   * @brief Destroy the QtPass instance and clean up signal connections.
   */
  ~QtPass() override;

  /**
   * @brief Resolve the password store and the executables and migrate the
   * settings of an older version (fresh-install defaults, the native-OTP
   * switch). Whether the resulting configuration is usable is the caller's
   * question: see Util::configIsValid().
   */
  void init();

  /**
   * @brief The clipboard handling for this application instance.
   * @return The manager; lives as long as this object.
   */
  auto clipboard() -> ClipboardManager & { return m_clipboard; }

  /**
   * @brief Turn a process' plain-text output into the HTML the text browser
   * shows: http(s) URLs become links, everything else is escaped, line breaks
   * become `<br />`.
   * @param output Plain text from a process.
   * @param prefix HTML placed before the converted output.
   * @param postfix HTML placed after it.
   * @return The HTML.
   */
  static auto formatOutput(QString output, const QString &prefix = QString(),
                           const QString &postfix = QString()) -> QString;

  /**
   * @brief Create a modal dialog configured to display the given QR code.
   * @param image Pixmap containing the rendered QR code.
   * @return Pointer to a QDialog showing the image; caller takes ownership.
   */
  static QDialog *createQRCodePopup(const QPixmap &image);

public slots:
  /**
   * @brief Render text as a QR code with qrencode and show it in a popup at
   * the cursor; failures are reported through statusMessage().
   * @param text Text to convert into a QR code.
   */
  void showTextAsQRCode(const QString &text);

signals:
  /**
   * @brief Formatted output (see formatOutput()) or an error rendered in
   * colour, ready for the text browser.
   * @param html The HTML to show.
   */
  void outputReady(const QString &html);
  /**
   * @brief A backend operation is over, successfully or not; the interface
   * can be enabled again.
   */
  void operationFinished();
  /**
   * @brief A short message for the status bar.
   * @param message The text.
   * @param timeout Milliseconds to keep it, as QStatusBar::showMessage().
   */
  void statusMessage(const QString &message, int timeout);
  /**
   * @brief The store changed and the settings ask for an automatic push.
   */
  void pushRequested();
  /**
   * @brief An entry was written; the view of the current entry is stale.
   */
  void entryInserted();

private:
  ClipboardManager m_clipboard;

  void connectPassSignalHandlers(Pass *pass);
  void reportError(int exitCode, const QString &error);

private slots:
  void processErrorExit(int exitCode, const QString &error);
  void processFinished(const QString &output, const QString &errout);

  void passStoreChanged(const QString &output, const QString &errout);

  void doGitPush();
  void finishedInsert(const QString &output, const QString &errout);
  void onKeyGenerationComplete(const QString &output, const QString &errout);
};

#endif // SRC_QTPASS_H_
