// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "clipboardmanager.h"
#include "qtpasssettings.h"
#include "util.h"
#include <QApplication>
#include <QClipboard>

ClipboardManager::ClipboardManager(QObject *parent) : QObject(parent) {
  setAutoclearTimer();
  m_clearTimer.setSingleShot(true);
  connect(&m_clearTimer, &QTimer::timeout, this, &ClipboardManager::clear);
  connect(QApplication::instance(), &QApplication::aboutToQuit, this,
          &ClipboardManager::clear);
}

void ClipboardManager::setAutoclearTimer() {
  m_clearTimer.setInterval(MS_PER_SECOND *
                           QtPassSettings::getAutoclearSeconds());
}

void ClipboardManager::copyIfAlways(const QString &text,
                                    const QString &p_output) {
  const AppSettings s = QtPassSettings::load();
  // Only "always copy" mode actually places text on the clipboard here; in
  // on-demand mode the copy happens later from the field's copy button.
  // Crucially, do not touch m_clippedText otherwise — it tracks what is
  // currently on the clipboard so the autoclear timer knows what to clear.
  // Overwriting it when merely showing another entry left a previously copied
  // password on the clipboard past its autoclear window (#1607).
  if (s.clipBoardType == Enums::CLIPBOARD_ALWAYS && !p_output.isEmpty()) {
    copyText(text);
  }
}

void ClipboardManager::copyText(const QString &text) {
  const AppSettings s = QtPassSettings::load();
  QClipboard *clip = QApplication::clipboard();

  QClipboard::Mode mode = QClipboard::Clipboard;
  if (s.useSelection && clip->supportsSelection()) {
    mode = QClipboard::Selection;
  }

  clip->setMimeData(buildClipboardMimeData(text), mode);

  m_clippedText = text;
  emit statusMessage(tr("Copied to clipboard"));
  if (s.useAutoclear) {
    m_clearTimer.start();
  }
}

void ClipboardManager::clear() {
  if (m_clippedText.isEmpty()) {
    // Nothing of ours is out there. The old code compared the empty tracker
    // with an empty selection, "cleared" that and said so on every deselect.
    return;
  }
  QClipboard *clipboard = QApplication::clipboard();
  bool cleared = false;
  if (m_clippedText == clipboard->text(QClipboard::Selection)) {
    clipboard->clear(QClipboard::Selection);
    clipboard->setText(QString(""), QClipboard::Selection);
    cleared = true;
  }
  if (m_clippedText == clipboard->text(QClipboard::Clipboard)) {
    clipboard->clear(QClipboard::Clipboard);
    cleared = true;
  }
  emit statusMessage(cleared ? tr("Clipboard cleared")
                             : tr("Clipboard not cleared"));
  m_clippedText.clear();
}

/**
 * @brief Build clipboard MIME data with platform-specific security hints.
 * @param text - Plain text to copy
 * @return QMimeData with text and security hints
 */
auto buildClipboardMimeData(const QString &text) -> QMimeData * {
  auto *mimeData = new QMimeData();
  mimeData->setText(text);
#ifdef Q_OS_LINUX
  mimeData->setData("x-kde-passwordManagerHint", QByteArray("secret"));
#endif
#ifdef Q_OS_MAC
  mimeData->setData("application/x-nspasteboard-concealed-type", QByteArray());
#endif
#ifdef Q_OS_WIN
  mimeData->setData("ExcludeClipboardContentFromMonitorProcessing",
                    dwordBytes(1));
  mimeData->setData("CanIncludeInClipboardHistory", dwordBytes(0));
  mimeData->setData("CanUploadToCloudClipboard", dwordBytes(0));
#endif
  return mimeData;
}
