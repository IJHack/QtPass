// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_QTCOMPAT_H_
#define SRC_QTCOMPAT_H_

#include <QObject>
#include <QtGlobal>

/**
 * @file qtcompat.h
 * @brief Small Qt-version shims so call sites express intent in one line.
 */

/**
 * @brief Arm a signal/slot connection that fires exactly once.
 *
 * Wraps Qt::SingleShotConnection; any stale connection to the same slot is
 * disconnected first so repeated arming never accumulates connections.
 *
 * @tparam Sender Sender object type.
 * @tparam Signal Pointer-to-member signal type.
 * @tparam Receiver Receiver object type.
 * @tparam Slot Pointer-to-member slot type.
 * @param sender Object emitting the signal.
 * @param signal Signal to connect to.
 * @param receiver Object owning the slot.
 * @param slot Slot to invoke once.
 */
template <typename Sender, typename Signal, typename Receiver, typename Slot>
inline void connectSingleShot(Sender *sender, Signal signal, Receiver *receiver,
                              Slot slot) {
  QObject::disconnect(sender, signal, receiver, slot);
  QObject::connect(sender, signal, receiver, slot, Qt::SingleShotConnection);
}

#endif // SRC_QTCOMPAT_H_
