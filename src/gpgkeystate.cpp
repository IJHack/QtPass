// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gpgkeystate.h"
#include "gpgkeystate_p.h"

#include "util.h"

#include <QDateTime>
#include <QRegularExpression>

constexpr int GPG_MIN_FIELDS = 10;
constexpr int GPG_FIELD_VALIDITY = 1;
constexpr int GPG_FIELD_KEY_ID = 4;
constexpr int GPG_FIELD_CREATED = 5;
constexpr int GPG_FIELD_EXPIRY = 6;
constexpr int GPG_FIELD_USERID = 9;
/// Field 9 of an `fpr` record: the fingerprint, in the slot the user ID
/// occupies in a `uid` record.
constexpr int GPG_FIELD_FINGERPRINT = 9;

/**
 * @brief Classify a GPG colon output record type
 * @param record_type The first field of a GPG colon record (e.g., "pub", "sec",
 * "uid")
 * @return The corresponding GpgRecordType enum value
 */
auto classifyRecord(const QString &record_type) -> GpgRecordType {
  if (record_type == "pub") {
    return GpgRecordType::Pub;
  }
  if (record_type == "sec") {
    return GpgRecordType::Sec;
  }
  if (record_type == "uid") {
    return GpgRecordType::Uid;
  }
  if (record_type == "fpr") {
    return GpgRecordType::Fpr;
  }
  if (record_type == "sub") {
    return GpgRecordType::Sub;
  }
  if (record_type == "ssb") {
    return GpgRecordType::Ssb;
  }
  if (record_type == "grp") {
    return GpgRecordType::Grp;
  }
  return GpgRecordType::Unknown;
}

/**
 * @brief Handle a pub or sec record in GPG colon output
 * @param props The colon-split fields of the record
 * @param secret True if this is a secret key record (sec)
 * @param current_user UserInfo to populate with key data
 */
void handlePubSecRecord(const QStringList &props, bool secret,
                        UserInfo &current_user) {
  if (props.size() < GPG_MIN_FIELDS) {
    return;
  }
  current_user.key_id = props[GPG_FIELD_KEY_ID];
  current_user.name = props[GPG_FIELD_USERID];
  if (!props[GPG_FIELD_VALIDITY].isEmpty()) {
    current_user.validity = props[GPG_FIELD_VALIDITY][0].toLatin1();
  }

  // An empty field (GnuPG's "no expiry") leaves the QDateTime null, so
  // callers distinguish "unset" from "set" with isValid() alone.
  bool okCreated = false;
  const qint64 createdSecs = props[GPG_FIELD_CREATED].toLongLong(&okCreated);
  if (okCreated) {
    current_user.created = QDateTime::fromSecsSinceEpoch(createdSecs);
  }

  bool okExpiry = false;
  const qint64 expirySecs = props[GPG_FIELD_EXPIRY].toLongLong(&okExpiry);
  if (okExpiry) {
    current_user.expiry = QDateTime::fromSecsSinceEpoch(expirySecs);
  }

  current_user.have_secret = secret;
}

/**
 * @brief Handle a uid record in GPG colon output
 * @param props The colon-split fields of the record
 * @param current_user UserInfo to populate with user name
 */
void handleUidRecord(const QStringList &props, UserInfo &current_user) {
  current_user.name = props[GPG_FIELD_USERID];
}

/**
 * @brief Handle an fpr (fingerprint) record in GPG colon output
 * @param props The colon-split fields of the record
 * @param current_user UserInfo to update with fingerprint if it matches key
 */
void handleFprRecord(const QStringList &props, UserInfo &current_user) {
  if (props.size() < GPG_MIN_FIELDS) {
    return;
  }
  // The key ID from the pub/sec record is the fingerprint's tail; take the
  // whole fingerprint, but only something shaped like one (40 hex for v4,
  // 64 for v5/v6): it becomes a recipient and a signing identity later.
  static const QRegularExpression shape(
      QStringLiteral("^(?:[0-9A-Fa-f]{40}|[0-9A-Fa-f]{64})$"));
  const QString &fingerprint = props[GPG_FIELD_FINGERPRINT];
  if (!current_user.key_id.isEmpty() && shape.match(fingerprint).hasMatch() &&
      fingerprint.endsWith(current_user.key_id, Qt::CaseInsensitive)) {
    current_user.key_id = fingerprint;
  }
}

/**
 * @brief Parse GPG --with-colons output into a list of UserInfo
 * @param output Raw output from GPG --with-colons --with-fingerprint
 * @param secret True if parsing secret keys (--list-secret-keys)
 * @return QList of parsed UserInfo objects
 */
auto parseGpgColonOutput(const QString &output, bool secret)
    -> QList<UserInfo> {
  QList<UserInfo> users;

  const QStringList lines =
      output.split(Util::newLinesRegex(), Qt::SkipEmptyParts);

  UserInfo current_user;

  for (const QString &key : lines) {
    QStringList props = key.split(':');
    if (props.size() < GPG_MIN_FIELDS) {
      continue;
    }

    const QString &record_type = props[0];
    const GpgRecordType type = classifyRecord(record_type);

    switch (type) {
    case GpgRecordType::Pub:
    case GpgRecordType::Sec:
      if (!current_user.key_id.isEmpty()) {
        users.append(current_user);
      }
      current_user = UserInfo();
      handlePubSecRecord(props, secret && (type == GpgRecordType::Sec),
                         current_user);
      break;
    case GpgRecordType::Uid:
      if (current_user.name.isEmpty()) {
        handleUidRecord(props, current_user);
      }
      break;
    case GpgRecordType::Fpr:
      handleFprRecord(props, current_user);
      break;
    default:
      break;
    }
  }

  if (!current_user.key_id.isEmpty()) {
    users.append(current_user);
  }

  return users;
}

/**
 * @brief The primary key fingerprint of the key that @p key names.
 *
 * gpg accepts a signature by a subkey for its primary key, and verification
 * here takes either fingerprint from VALIDSIG: a key written as a subkey's
 * fingerprint, or as a key ID, signs as its primary key. Only a block that
 * holds @p key is taken, whatever else gpg printed.
 * @param output Raw output from GPG --with-colons --with-fingerprint
 *        --with-subkey-fingerprint --list-keys.
 * @param key A fingerprint (40 or 64 hex) or a key ID (8 or 16 hex, the
 *        fingerprint's tail), optionally with 0x; any case.
 * @return The primary fingerprint in upper case, or empty when no block in
 *         @p output holds @p key.
 */
auto primaryFingerprintOf(const QString &output, const QString &key)
    -> QString {
  QString wanted = key.trimmed().toUpper();
  if (wanted.startsWith(QLatin1String("0X"))) {
    wanted.remove(0, 2);
  }
  const bool isId = wanted.size() == 8 || wanted.size() == 16;
  const auto names = [&](const QString &fingerprint) {
    const QString upper = fingerprint.toUpper();
    return isId ? upper.endsWith(wanted) : upper == wanted;
  };
  QString primary;
  bool expectPrimary = false;
  bool found = false;
  const QStringList lines =
      output.split(Util::newLinesRegex(), Qt::SkipEmptyParts);
  for (const QString &line : lines) {
    const QStringList props = line.split(QLatin1Char(':'));
    if (props.size() < GPG_MIN_FIELDS) {
      continue;
    }
    switch (classifyRecord(props[0])) {
    case GpgRecordType::Pub:
      if (found) {
        return primary;
      }
      primary.clear();
      expectPrimary = true;
      break;
    case GpgRecordType::Sub:
      expectPrimary = false;
      break;
    case GpgRecordType::Fpr:
      if (expectPrimary) {
        primary = props[GPG_FIELD_FINGERPRINT].toUpper();
        expectPrimary = false;
      }
      if (!wanted.isEmpty() && !primary.isEmpty() &&
          names(props[GPG_FIELD_FINGERPRINT])) {
        found = true;
      }
      break;
    default:
      break;
    }
  }
  return found ? primary : QString();
}
