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
 * Qt 6 has Qt::SingleShotConnection; Qt 5.15 does not, so under Qt 5 the
 * connection must be torn down by the slot on first entry (see
 * disconnectSingleShot()). Any stale connection to the same slot is
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  QObject::connect(sender, signal, receiver, slot, Qt::SingleShotConnection);
#else
  QObject::connect(sender, signal, receiver, slot);
#endif
}

/**
 * @brief Tear down a connectSingleShot() connection from inside its slot.
 *
 * A no-op on Qt 6 (Qt::SingleShotConnection already self-disconnects). On
 * Qt 5.15 it disconnects the slot on first entry so it cannot fire again.
 * Call it once at the top of a slot armed with connectSingleShot().
 *
 * @tparam Sender Sender object type.
 * @tparam Signal Pointer-to-member signal type.
 * @tparam Receiver Receiver object type.
 * @tparam Slot Pointer-to-member slot type.
 * @param sender Object emitting the signal.
 * @param signal Signal that was connected.
 * @param receiver Object owning the slot.
 * @param slot Slot to disconnect.
 */
template <typename Sender, typename Signal, typename Receiver, typename Slot>
inline void disconnectSingleShot(Sender *sender, Signal signal,
                                 Receiver *receiver, Slot slot) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  QObject::disconnect(sender, signal, receiver, slot);
#else
  Q_UNUSED(sender)
  Q_UNUSED(signal)
  Q_UNUSED(receiver)
  Q_UNUSED(slot)
#endif
}

#endif // SRC_QTCOMPAT_H_
