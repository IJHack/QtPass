// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_QTPASS_H_
#define SRC_QTPASS_H_

#include "clipboardmanager.h"
#include <QDialog>
#include <QObject>
#include <QPixmap>

class MainWindow;
class Pass;

/**
 * @class QtPass
 * @brief Orchestrates pass signal handling and application-level operations
 * for the QtPass application; owns the ClipboardManager.
 */
class QtPass : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Construct a QtPass instance associated with the given main window.
   * @param mainWindow Pointer to the application's MainWindow.
   */
  explicit QtPass(MainWindow *mainWindow);
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
   * @brief The clipboard handling for this application instance.
   * @return The manager; lives as long as this object.
   */
  auto clipboard() -> ClipboardManager & { return m_clipboard; }

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

  ClipboardManager m_clipboard;
  bool freshStart{true};

  void setMainWindow();
  void connectPassSignalHandlers(Pass *pass);

signals:

public slots:
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
