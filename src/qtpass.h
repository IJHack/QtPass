// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_QTPASS_H_
#define SRC_QTPASS_H_

#include <QDialog>
#include <QMimeData>
#include <QObject>
#include <QPixmap>
#include <QProcess>
#include <QTimer>

class MainWindow;
class Pass;

/**
 * @brief Build clipboard MIME data with platform-specific security hints.
 * @param text - Plain text to copy
 * @return QMimeData* - Ownership transferred to caller. Caller must delete
 *         or transfer to QClipboard::setMimeData which takes ownership.
 */
auto buildClipboardMimeData(const QString &text) -> QMimeData *;

/**
 * @brief Convert quint32 to byte array for Windows clipboard formats.
 * @param value - DWORD value
 * @return QByteArray with raw bytes
 */
static inline auto dwordBytes(quint32 value) -> QByteArray {
  return QByteArray(reinterpret_cast<const char *>(&value), sizeof(value));
}

/**
 * @class QtPass
 * @brief Orchestrates clipboard management, pass signal handling, and
 * application-level operations for the QtPass application.
 */
class QtPass : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Construct a QtPass instance associated with the given main window.
   * @param mainWindow Pointer to the application's MainWindow.
   */
  QtPass(MainWindow *mainWindow);
  ~QtPass();

  /**
   * @brief Initialize internal state and signal connections.
   * @return true if initialization succeeded.
   */
  auto init() -> bool;

  /**
   * @brief Update the tracked clipped text value.
   * @param text Primary text to store.
   * @param p_output Optional additional output used when computing the value.
   */
  void setClippedText(const QString &text, const QString &p_output = QString());

  /**
   * @brief Remove any stored clipped text value.
   */
  void clearClippedText();

  /**
   * @brief Configure and start the clipboard-clear timer.
   */
  void setClipboardTimer();

  /**
   * @brief Return whether this instance is in a fresh-start state.
   * @return true if in fresh-start state.
   */
  auto isFreshStart() -> bool { return this->freshStart; }

  /**
   * @brief Set the fresh-start state.
   * @param fs New fresh-start state value.
   */
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
  /**
   * @brief Clear the system clipboard contents immediately.
   */
  void clearClipboard();

  /**
   * @brief Copy text into the system clipboard.
   * @param text Text to copy.
   */
  void copyTextToClipboard(const QString &text);

  /**
   * @brief Request display of text as a QR code in the UI.
   * @param text Text to convert into a QR code.
   */
  void showTextAsQRCode(const QString &text);

public:
  /**
   * @brief Create a modal dialog configured to display the given QR code.
   * @param image Pixmap containing the rendered QR code.
   * @return Pointer to a QDialog showing the image; caller takes ownership.
   */
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
