// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_WINDOWSTATESTORE_H_
#define SRC_WINDOWSTATESTORE_H_

#include <QString>

class QWidget;

/**
 * @brief Persist and restore a top-level window's geometry, one way for the
 *        main window and every dialog.
 *
 * QWidget::saveGeometry() already encodes position, size, maximized state
 * and the screen, and QWidget::restoreGeometry() clamps to the screens that
 * exist now. Nothing else needs storing: the separate pos/size/maximized
 * keys the dialogs and the main window used to write were either
 * redundant with it or, applied after it, overrode it.
 */
namespace WindowStateStore {

/**
 * @brief Restore @p window's geometry saved under @p key.
 *
 * Without a saved geometry the window is centred on the screen under the
 * mouse pointer, except on Wayland, where placement belongs to the
 * compositor and a client-side move is ignored anyway.
 * @param window Top-level widget to place.
 * @param key Settings key the geometry was saved under (see save()).
 * @return true if a saved geometry was applied.
 */
auto restore(QWidget &window, const QString &key) -> bool;

/**
 * @brief Save @p window's geometry under @p key.
 * @param window Top-level widget whose geometry to store.
 * @param key Settings key, e.g. the dialog's name; restore() reads it back.
 */
void save(const QWidget &window, const QString &key);

/**
 * @brief Move @p window so its frame is centred on the screen under the
 *        mouse pointer (the primary screen when there is none). No-op on
 *        Wayland.
 * @param window Top-level widget to move.
 */
void centreOnCursorScreen(QWidget &window);

} // namespace WindowStateStore

#endif // SRC_WINDOWSTATESTORE_H_
