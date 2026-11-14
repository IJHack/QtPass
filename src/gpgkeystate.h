// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Parsing of `gpg --with-colons` key listings into UserInfo records.
 */
#ifndef SRC_GPGKEYSTATE_H_
#define SRC_GPGKEYSTATE_H_

#include "userinfo.h"

#include <QList>
#include <QString>
#include <QStringList>

/**
 * @brief The record types of a `gpg --with-colons` listing that the parser
 * tells apart; the first field of each line names one.
 */
enum class GpgRecordType {
  Unknown, /**< Any record type not listed here; skipped. */
  Pub,     /**< `pub`: a public primary key, starting a new key block. */
  Sec,     /**< `sec`: a secret primary key, starting a new key block. */
  Uid,     /**< `uid`: a user ID of the current key. */
  Fpr,     /**< `fpr`: the fingerprint of the key or subkey before it. */
  Sub,     /**< `sub`: a public subkey. */
  Ssb,     /**< `ssb`: a secret subkey. */
  Grp      /**< `grp`: a keygrip. */
};

/**
 * @brief Classify a record of a gpg colon listing by its first field.
 * @param record_type The first field of the record (`pub`, `sec`, `uid`,
 *        and so on).
 * @return The matching GpgRecordType, or GpgRecordType::Unknown.
 */
GpgRecordType classifyRecord(const QString &record_type);

/**
 * @brief Parse a gpg colon listing into one UserInfo per key.
 *
 * A `pub` or `sec` record starts a key; the first `uid` after it supplies
 * the name when the key record carried none, and an `fpr` record whose
 * value ends in the key ID replaces the key ID with the full fingerprint.
 * Lines with fewer than ten fields are skipped.
 * @param output Raw output of `gpg --with-colons --with-fingerprint`.
 * @param secret True when @p output lists secret keys
 *        (`--list-secret-keys`); the keys are then marked as having a
 *        secret part.
 * @return The keys, in the order gpg listed them.
 */
QList<UserInfo> parseGpgColonOutput(const QString &output, bool secret);

/**
 * @brief The primary key fingerprint of the key that @p key names.
 *
 * gpg accepts a signature by a subkey for its primary key, and verification
 * here takes either fingerprint from VALIDSIG: a key written as a subkey's
 * fingerprint, or as a key ID, signs as its primary key. Only the block that
 * holds @p key is taken, whatever else gpg printed; when several do (a short
 * ID, which collides easily), none is.
 * @param output Raw output from GPG --with-colons --with-fingerprint
 *        --with-subkey-fingerprint --list-keys.
 * @param key A fingerprint (40 or 64 hex) or a key ID (8 or 16 hex),
 *        optionally with 0x; any case. A key ID is matched against the
 *        record's own key ID, so it works for v4 keys (the fingerprint's
 *        tail) and v5/v6 keys (its head) alike.
 * @return The primary fingerprint in upper case, or empty when no block, or
 *         more than one, in @p output holds @p key.
 */
QString primaryFingerprintOf(const QString &output, const QString &key);

#endif // SRC_GPGKEYSTATE_H_
