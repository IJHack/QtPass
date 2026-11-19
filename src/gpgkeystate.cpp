// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Implements the colon-listing parser: record classification, one
 * handler per record type, and primary-fingerprint lookup.
 */
#include "gpgkeystate.h"
#include "gpgkeystate_p.h"

#include "util.h"

#include <QDateTime>
#include <QRegularExpression>

/** @brief Fewest fields a record must have to be read at all. */
constexpr int GPG_MIN_FIELDS = 10;
/** @brief Field index of the validity letter (`u`, `f`, `r`, ...). */
constexpr int GPG_FIELD_VALIDITY = 1;
/** @brief Field index of the key ID. */
constexpr int GPG_FIELD_KEY_ID = 4;
/** @brief Field index of the creation time, in seconds since the epoch. */
constexpr int GPG_FIELD_CREATED = 5;
/** @brief Field index of the expiry time; empty for a key without one. */
constexpr int GPG_FIELD_EXPIRY = 6;
/** @brief Field index of the user ID. */
constexpr int GPG_FIELD_USERID = 9;
/**
 * @brief Field 9 of an `fpr` record: the fingerprint, in the slot the user
 * ID occupies in a `uid` record.
 */
constexpr int GPG_FIELD_FINGERPRINT = 9;

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

void handleFprRecord(const QStringList &props, UserInfo &current_user) {
  if (props.size() < GPG_MIN_FIELDS) {
    return;
  }
  // GnuPG reports the v4 key ID from the fingerprint's tail, and the v5/v6
  // key ID from its head. Promote only an exact 16-hex key ID with the
  // version-specific fingerprint shape and location; malformed or mismatched
  // input stays a key ID rather than becoming a false full identity.
  static const QRegularExpression keyIdShape(
      QStringLiteral("^[0-9A-Fa-f]{16}$"));
  static const QRegularExpression v4FingerprintShape(
      QStringLiteral("^[0-9A-Fa-f]{40}$"));
  static const QRegularExpression v5v6FingerprintShape(
      QStringLiteral("^[0-9A-Fa-f]{64}$"));
  const QString &keyId = current_user.key_id;
  if (!keyIdShape.match(keyId).hasMatch()) {
    return;
  }

  const QString &fingerprint = props[GPG_FIELD_FINGERPRINT];
  const bool isV4 = v4FingerprintShape.match(fingerprint).hasMatch() &&
                    fingerprint.endsWith(keyId, Qt::CaseInsensitive);
  const bool isV5V6 = v5v6FingerprintShape.match(fingerprint).hasMatch() &&
                      fingerprint.startsWith(keyId, Qt::CaseInsensitive);
  if (isV4 || isV5V6) {
    current_user.key_id = fingerprint;
  }
}

auto parseGpgColonOutput(const QString &output, bool secret)
    -> QList<UserInfo> {
  QList<UserInfo> users;

  const QStringList lines =
      output.split(Util::newLinesRegex(), Qt::SkipEmptyParts);

  UserInfo current_user;
  bool expectPrimaryFingerprint = false;

  for (const QString &key : lines) {
    QStringList props = key.split(':');
    if (props.size() < GPG_MIN_FIELDS) {
      continue;
    }

    const QString &record_type = props[0];
    const GpgRecordType type = classifyRecord(record_type);
    if (type != GpgRecordType::Fpr && type != GpgRecordType::Pub &&
        type != GpgRecordType::Sec) {
      expectPrimaryFingerprint = false;
    }

    switch (type) {
    case GpgRecordType::Pub:
    case GpgRecordType::Sec:
      if (!current_user.key_id.isEmpty()) {
        users.append(current_user);
      }
      current_user = UserInfo();
      handlePubSecRecord(props, secret && (type == GpgRecordType::Sec),
                         current_user);
      expectPrimaryFingerprint = true;
      break;
    case GpgRecordType::Uid:
      if (current_user.name.isEmpty()) {
        handleUidRecord(props, current_user);
      }
      break;
    case GpgRecordType::Fpr:
      if (expectPrimaryFingerprint) {
        handleFprRecord(props, current_user);
        expectPrimaryFingerprint = false;
      }
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

auto primaryFingerprintOf(const QString &output, const QString &key)
    -> QString {
  QString wanted = key.trimmed().toUpper();
  if (wanted.startsWith(QLatin1String("0X"))) {
    wanted.remove(0, 2);
  }
  if (wanted.isEmpty()) {
    return {};
  }
  const bool isId = wanted.size() == 8 || wanted.size() == 16;
  QStringList holders;
  QString primary;
  bool expectPrimary = false;
  bool holds = false;
  // A pub or sub record carries the key's own ID: the fingerprint's tail
  // for v4 keys, its head for v5/v6. A short ID is the long one's tail.
  const auto namesKeyId = [&](const QStringList &props) {
    return isId && props[GPG_FIELD_KEY_ID].toUpper().endsWith(wanted);
  };
  const auto endBlock = [&] {
    if (holds && !primary.isEmpty() && !holders.contains(primary)) {
      holders << primary;
    }
    primary.clear();
    holds = false;
  };
  const QStringList lines =
      output.split(Util::newLinesRegex(), Qt::SkipEmptyParts);
  for (const QString &line : lines) {
    const QStringList props = line.split(QLatin1Char(':'));
    if (props.size() < GPG_MIN_FIELDS) {
      continue;
    }
    switch (classifyRecord(props[0])) {
    case GpgRecordType::Pub:
      endBlock();
      expectPrimary = true;
      holds = holds || namesKeyId(props);
      break;
    case GpgRecordType::Sub:
      expectPrimary = false;
      holds = holds || namesKeyId(props);
      break;
    case GpgRecordType::Fpr:
      if (expectPrimary) {
        primary = props[GPG_FIELD_FINGERPRINT].toUpper();
        expectPrimary = false;
      }
      if (!isId && props[GPG_FIELD_FINGERPRINT].toUpper() == wanted) {
        holds = true;
      }
      break;
    default:
      break;
    }
  }
  endBlock();
  return holders.size() == 1 ? holders.first() : QString();
}
