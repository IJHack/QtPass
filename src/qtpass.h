// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_QTPASS_H_
#define SRC_QTPASS_H_

#include <QDialog>
#include <QMimeData>
#include <QObject>
#include <QPixmap>
#include <QProcess>
#include <QStringList>
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
  return {reinterpret_cast<const char *>(&value), sizeof(value)};
}

/**
 * @class CopiedTexts
 * @brief Texts copied out of a view of a secret, so autoclear clears them too.
 *
 * Text selected and copied from a shown entry (Ctrl+C, the context menu, a
 * drag, or the X11/Wayland primary selection) gets onto the clipboard
 * without QtPass's own copy, which tracks only the one text it copied.
 * These are kept beside it. A mouse selection reports itself on every change
 * while it grows; only where it ends is kept. Room is made by dropping the
 * oldest text that is no longer on the clipboard or in the selection, so a
 * copy still there always stays tracked.
 */
class CopiedTexts {
public:
  /**
   * @brief Track @p text, copied out of a secret view.
   * @param text The copied text; nothing happens when it is empty.
   * @param onClipboard What the clipboard holds now.
   * @param inSelection What the primary selection holds now.
   */
  void add(const QString &text, const QString &onClipboard,
           const QString &inSelection);
  /**
   * @brief Whether @p text is one of the tracked texts.
   * @param text Text found on the clipboard or in the selection.
   * @return True for a non-empty tracked text.
   */
  [[nodiscard]] auto contains(const QString &text) const -> bool;
  /**
   * @brief Forget every tracked text, after autoclear ran.
   */
  void clear() { m_texts.clear(); }
  /**
   * @brief How many texts are tracked.
   * @return The count, at most 16.
   */
  [[nodiscard]] auto size() const -> qsizetype { return m_texts.size(); }

private:
  QStringList m_texts;
};

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
  /**
   * @brief Destroy the QtPass instance and clean up signal connections.
   */
  ~QtPass() override;

  /**
   * @brief Initialize internal state and signal connections.
   * @return true if initialization succeeded.
   */
  auto init() -> bool;

  /**
   * @brief Handle clipboard copying for a shown entry.
   *
   * In "always copy" mode (and only then) this copies @p text to the clipboard
   * via copyTextToClipboard(), which also arms the autoclear timer and records
   * the clipboard contents. In other modes it does nothing, leaving any pending
   * autoclear tracking intact.
   * @param text Password (or value) of the entry being shown.
   * @param p_output Full decrypted output; copying is skipped when it is empty.
   */
  void setClippedText(const QString &text, const QString &p_output = QString());

  /**
   * @brief Configure and start the clipboard-clear timer.
   */
  void setClipboardTimer();

  /**
   * @brief Whether a text found on the clipboard or in the selection is one
   * QtPass put there, so autoclear may take it.
   *
   * Empty is never ours: the clipboard reports an image or a file list as an
   * empty text, and with nothing copied yet @p clippedText is empty too, so
   * without this a selection in a shown entry would start autoclear and clear
   * someone else's clipboard content.
   * @param text What the clipboard or the selection holds.
   * @param clippedText What copyTextToClipboard() last put there, or empty.
   * @param copied The texts copied out of a view of a secret.
   * @return True for a non-empty @p text that QtPass copied or tracked.
   */
  [[nodiscard]] static auto isOurs(const QString &text,
                                   const QString &clippedText,
                                   const CopiedTexts &copied) -> bool;

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
  CopiedTexts copiedTexts;
  bool freshStart{true};

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
   * @brief Track text that got onto the clipboard by a copy out of a view
   * of a secret, so autoclear clears it with the rest.
   * @param text The copied text.
   */
  void trackCopied(const QString &text);

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
