// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gpgkeystate.h"
#include "gpgkeystate_p.h"

#include "util.h"

#include <QRegularExpression>

constexpr int GPG_MIN_FIELDS = 10;
constexpr int GPG_FIELD_VALIDITY = 1;
constexpr int GPG_FIELD_KEY_ID = 4;
constexpr int GPG_FIELD_CREATED = 5;
constexpr int GPG_FIELD_EXPIRY = 6;
constexpr int GPG_FIELD_USERID = 9;

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

  bool okCreated = false;
  const qint64 createdSecs = props[GPG_FIELD_CREATED].toLongLong(&okCreated);
  if (okCreated) {
    current_user.created.setSecsSinceEpoch(createdSecs);
  }

  bool okExpiry = false;
  const qint64 expirySecs = props[GPG_FIELD_EXPIRY].toLongLong(&okExpiry);
  if (okExpiry) {
    current_user.expiry.setSecsSinceEpoch(expirySecs);
  }

  current_user.have_secret = secret;
}

/**
 * @brief How far a uid record's own validity vouches for its name.
 *
 * The key's validity on the pub record is that of its best uid, so the
 * name shown beside it has to be one that earned it: a certified key whose
 * owner adds an uncertified uid and makes it primary must not show that
 * name as valid.
 * @param validity The record's validity field.
 * @return 3 for ultimate or full, 2 for marginal, 1 for unknown or
 *         undefined, 0 for anything else (invalid, revoked, expired, never).
 */
auto uidNameRank(const QString &validity) -> int {
  switch (validity.isEmpty() ? '\0' : validity.at(0).toLatin1()) {
  case 'u':
  case 'f':
    return 3;
  case 'm':
    return 2;
  case '-':
  case 'q':
    return 1;
  default:
    return 0;
  }
}

/**
 * @brief Handle a uid record in GPG colon output: its name is the key's
 *        name unless an earlier uid vouches at least as well for its own.
 * @param props The colon-split fields of the record
 * @param current_user UserInfo to populate with user name
 * @param nameRank uidNameRank() of the name taken so far; updated
 */
void handleUidRecord(const QStringList &props, UserInfo &current_user,
                     int &nameRank) {
  const int rank = uidNameRank(props[GPG_FIELD_VALIDITY]);
  if (current_user.name.isEmpty() || rank > nameRank) {
    current_user.name = props[GPG_FIELD_USERID];
    nameRank = rank;
  }
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
  if (!current_user.key_id.isEmpty() &&
      props[GPG_FIELD_USERID].endsWith(current_user.key_id)) {
    current_user.key_id = props[GPG_FIELD_USERID];
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

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  const QStringList lines =
      output.split(Util::newLinesRegex(), Qt::SkipEmptyParts);
#else
  const QStringList lines =
      output.split(Util::newLinesRegex(), QString::SkipEmptyParts);
#endif

  UserInfo current_user;
  // uidNameRank() of current_user.name; a name GnuPG 1 put on the pub
  // record itself counts as unknown.
  int nameRank = 0;

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
      nameRank = current_user.name.isEmpty() ? 0 : 1;
      break;
    case GpgRecordType::Uid:
      handleUidRecord(props, current_user, nameRank);
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
