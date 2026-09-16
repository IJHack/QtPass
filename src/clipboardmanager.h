// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_CLIPBOARDMANAGER_H_
#define SRC_CLIPBOARDMANAGER_H_

#include <QMimeData>
#include <QObject>
#include <QString>
#include <QTimer>

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
 * @class ClipboardManager
 * @brief Puts secrets on the system clipboard and takes them off again.
 *
 * Remembers what it copied so the autoclear timer (and the quit handler)
 * only remove that text, never something the user copied in the meantime.
 * Settings (selection vs. clipboard, autoclear on/off, delay) are read from
 * QtPassSettings; call setAutoclearTimer() after they change. Status text
 * for the main window's status bar is emitted, not shown, so this class has
 * no UI dependency and is unit-testable.
 */
class ClipboardManager : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Construct a manager that also clears its text when the
   *        application quits.
   * @param parent Owner.
   */
  explicit ClipboardManager(QObject *parent = nullptr);

  /**
   * @brief Re-read the autoclear delay from the settings.
   */
  void setAutoclearTimer();

  /**
   * @brief Handle clipboard copying for a shown entry.
   *
   * In "always copy" mode (and only then) this copies @p text via copyText(),
   * which also arms the autoclear timer and records the clipboard contents.
   * In other modes it does nothing, leaving any pending autoclear tracking
   * intact.
   * @param text Password (or value) of the entry being shown.
   * @param p_output Full decrypted output; copying is skipped when it is empty.
   */
  void copyIfAlways(const QString &text, const QString &p_output = QString());

  /**
   * @brief The text this manager last put on the clipboard and still tracks.
   * @return Tracked text, empty once cleared.
   */
  [[nodiscard]] auto trackedText() const -> QString { return m_clippedText; }

public slots:
  /**
   * @brief Copy text into the system clipboard (or the X11 primary selection
   *        when configured) and arm the autoclear timer when enabled.
   * @param text Text to copy.
   */
  void copyText(const QString &text);

  /**
   * @brief Remove the tracked text from the clipboard if it is still there.
   *        Silent no-op when nothing is tracked.
   */
  void clear();

signals:
  /**
   * @brief Something worth showing in the status bar happened.
   * @param message Translated text.
   */
  void statusMessage(const QString &message);

private:
  QTimer m_clearTimer;
  QString m_clippedText;
};

#endif // SRC_CLIPBOARDMANAGER_H_
