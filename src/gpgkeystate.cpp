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

  for (const QString &key : lines) {
    QStringList props = key.split(':');
    if (props.size() < GPG_MIN_FIELDS) {
      continue;
    }

    const QString record_type = props[0];
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
