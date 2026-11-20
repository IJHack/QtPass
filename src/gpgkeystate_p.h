// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @ingroup keys
 * @brief Record handlers of the gpg colon-listing parser, declared separately
 * only so the unit tests can call them; not for use outside gpgkeystate.cpp.
 */
#ifndef SRC_GPGKEYSTATE_P_H_
#define SRC_GPGKEYSTATE_P_H_

#include "userinfo.h"

#include <QStringList>

/**
 * @brief Start a key from a `pub` or `sec` record: key ID, the user ID the
 * record may carry, validity, creation and expiry time.
 *
 * An empty expiry field (gpg's "no expiry") leaves the date null. A record
 * with fewer than ten fields changes nothing.
 * @param props The colon-split fields of the record.
 * @param secret Whether to mark the key as having a secret part.
 * @param current_user The key being built; filled in.
 */
void handlePubSecRecord(const QStringList &props, bool secret,
                        UserInfo &current_user);
/**
 * @brief Replace the key ID of the key being built with its fingerprint.
 *
 * Only when the `fpr` record's value has the expected fingerprint shape and
 * contains the exact 16-hex key ID in its version-specific position: the
 * tail for a 40-hex v4 fingerprint, the head for a 64-hex v5/v6 fingerprint.
 * A subkey's fingerprint therefore cannot replace the primary key's ID.
 * @param props The colon-split fields of the record.
 * @param current_user The key being built; its key ID may be replaced.
 */
void handleFprRecord(const QStringList &props, UserInfo &current_user);

#endif // SRC_GPGKEYSTATE_P_H_
