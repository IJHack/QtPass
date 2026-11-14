// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Implements WindowStateStore on top of QWidget::saveGeometry() /
 *        restoreGeometry(); the Wayland check lives in the local onWayland().
 */
#include "windowstatestore.h"
#include "qtpasssettings.h"

#include <QCursor>
#include <QDialog>
#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

namespace {

/**
 * @brief Whether the application runs on a Wayland platform plugin, where
 *        client-side window placement is ignored by the compositor.
 * @return true for any platform name starting with "wayland".
 */
auto onWayland() -> bool {
  return QGuiApplication::platformName().startsWith(QLatin1String("wayland"));
}

} // namespace

namespace WindowStateStore {

auto restore(QWidget &window, const QString &key) -> bool {
  const QByteArray geometry = QtPassSettings::getDialogGeometry(key);
  if (!geometry.isEmpty() && window.restoreGeometry(geometry)) {
    return true;
  }
  centreOnCursorScreen(window);
  return false;
}

void save(const QWidget &window, const QString &key) {
  QtPassSettings::setDialogGeometry(key, window.saveGeometry());
}

void attach(QDialog &dialog, const QString &key) {
  restore(dialog, key);
  QObject::connect(&dialog, &QDialog::finished, &dialog,
                   [&dialog, key] { save(dialog, key); });
}

void centreOnCursorScreen(QWidget &window) {
  if (onWayland()) {
    return;
  }
  QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
  if (screen == nullptr) {
    screen = QGuiApplication::primaryScreen();
  }
  if (screen == nullptr) {
    return;
  }
  QRect frame = window.frameGeometry();
  frame.moveCenter(screen->availableGeometry().center());
  window.move(frame.topLeft());
}

} // namespace WindowStateStore
