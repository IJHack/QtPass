// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_SECRETTEXTBROWSER_H_
#define SRC_SECRETTEXTBROWSER_H_

#include <QTextBrowser>

/**
 * @class SecretTextBrowser
 * @brief A QTextBrowser for decrypted entry text: whatever is copied out of
 * it is marked as a secret and reported for autoclear.
 *
 * Every way text leaves a QTextEdit (Ctrl+C, the context menu's Copy, a drag,
 * and the X11 primary selection a mouse selection fills) goes through
 * createMimeDataFromSelection(). Here that builds the same MIME data as
 * QtPass's own copies, with the platform hints that keep clipboard history and
 * cloud clipboard out, and emits copied() so the text is cleared with the
 * rest when the autoclear timer runs.
 */
class SecretTextBrowser : public QTextBrowser {
  Q_OBJECT

public:
  /**
   * @brief Constructs the browser.
   * @param parent Owner.
   */
  explicit SecretTextBrowser(QWidget *parent = nullptr);

signals:
  /**
   * @brief Selected text was copied out of the browser.
   * @param text The text, as put on the clipboard or in the drag.
   */
  void copied(const QString &text);

protected:
  /**
   * @brief The selection as secret-marked plain text.
   * @return MIME data from buildClipboardMimeData(); the caller owns it.
   */
  [[nodiscard]] auto createMimeDataFromSelection() const
      -> QMimeData * override;
};

#endif // SRC_SECRETTEXTBROWSER_H_
