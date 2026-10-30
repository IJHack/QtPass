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

  // What was there before the first secret, to put back when autoclear
  // runs (as pass -c does). Not a text of ours: a second copy in the same
  // window keeps the user's own text, not the first password.
  if (s.useAutoclear && s.restoreClipboard) {
    std::optional<QString> &previous = mode == QClipboard::Selection
                                           ? m_previousSelection
                                           : m_previousClipboard;
    const QString now = clip->text(mode);
    if (!previous && now != m_clippedText && !m_otherCopies.contains(now)) {
      previous = now;
    }
  }
  clip->setMimeData(buildClipboardMimeData(text), mode);

  m_clippedText = text;
  emit statusMessage(tr("Copied to clipboard"));
  if (s.useAutoclear) {
    m_clearTimer.start();
  }
}

void ClipboardManager::trackCopied(const QString &text) {
  if (text.isEmpty()) {
    return;
  }
  // Kept beside m_clippedText, not instead of it: replacing it would leave a
  // password copied earlier on the clipboard past its autoclear (#1607).
  constexpr qsizetype kMaxOtherCopies = 16;
  if (text != m_clippedText && !m_otherCopies.contains(text)) {
    // A mouse selection reports itself on every change while it grows or
    // shrinks (the X11 primary selection); only where it ends counts, or a
    // passing "a" would stay tracked and autoclear could take someone
    // else's "a" later.
    // An earlier copy still on the clipboard proper (Ctrl+C) stays tracked.
    const QString &last =
        m_otherCopies.isEmpty() ? QString() : m_otherCopies.last();
    if (!last.isEmpty() && (text.contains(last) || last.contains(text)) &&
        QApplication::clipboard()->text(QClipboard::Clipboard) != last) {
      m_otherCopies.last() = text;
    } else {
      if (m_otherCopies.size() >= kMaxOtherCopies) {
        // Make room, but never by forgetting a text that is still on the
        // clipboard or in the selection: it would then outlive autoclear.
        // At most two are, so the oldest of the rest can always go.
        const QString onClipboard =
            QApplication::clipboard()->text(QClipboard::Clipboard);
        const QString inSelection =
            QApplication::clipboard()->text(QClipboard::Selection);
        for (qsizetype i = 0; i < m_otherCopies.size(); ++i) {
          if (m_otherCopies.at(i) != onClipboard &&
              m_otherCopies.at(i) != inSelection) {
            m_otherCopies.removeAt(i);
            break;
          }
        }
      }
      m_otherCopies.append(text);
    }
  }
  if (QtPassSettings::load().useAutoclear) {
    m_clearTimer.start();
  }
}

void ClipboardManager::clear() {
  if (m_clippedText.isEmpty() && m_otherCopies.isEmpty()) {
    // Nothing of ours is out there. The old code compared the empty tracker
    // with an empty selection, "cleared" that and said so on every deselect.
    return;
  }
  const auto ours = [this](const QString &text) {
    return !text.isEmpty() &&
           (text == m_clippedText || m_otherCopies.contains(text));
  };
  QClipboard *clipboard = QApplication::clipboard();
  const bool restore = QtPassSettings::load().restoreClipboard;
  bool cleared = false;
  bool restored = false;
  // Ours still there: the text from before comes back if there was one,
  // otherwise it is cleared. Something else copied meanwhile is left alone.
  const auto putBack = [&](QClipboard::Mode mode,
                           const std::optional<QString> &previous) {
    if (!ours(clipboard->text(mode))) {
      return;
    }
    if (restore && previous && !previous->isEmpty()) {
      clipboard->setText(*previous, mode);
      restored = true;
      return;
    }
    clipboard->clear(mode);
    if (mode == QClipboard::Selection) {
      clipboard->setText(QString(""), QClipboard::Selection);
    }
    cleared = true;
  };
  putBack(QClipboard::Selection, m_previousSelection);
  putBack(QClipboard::Clipboard, m_previousClipboard);
  emit statusMessage(restored  ? tr("Clipboard restored")
                     : cleared ? tr("Clipboard cleared")
                               : tr("Clipboard not cleared"));
  m_clippedText.clear();
  m_otherCopies.clear();
  m_previousClipboard.reset();
  m_previousSelection.reset();
}

/**
 * @brief Build clipboard MIME data with platform-specific security hints.
 * @param text - Plain text to copy
 * @return QMimeData with text and security hints
 */
auto buildClipboardMimeData(const QString &text) -> QMimeData * {
  auto *mimeData = new QMimeData();
  mimeData->setText(text);
// The KDE hint (what wl-copy --sensitive sets) keeps the secret out of
// Klipper, CopyQ and cliphist history, on the BSDs as well as on Linux.
#if defined(Q_OS_UNIX) && !defined(Q_OS_DARWIN)
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
