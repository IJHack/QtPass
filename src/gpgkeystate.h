// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_GPGKEYSTATE_H_
#define SRC_GPGKEYSTATE_H_

#include "userinfo.h"

#include <QList>
#include <QString>
#include <QStringList>

enum class GpgRecordType { Unknown, Pub, Sec, Uid, Fpr, Sub, Ssb, Grp };

GpgRecordType classifyRecord(const QString &record_type);

void handlePubSecRecord(const QStringList &props, bool secret,
                        UserInfo &current_user);

void handleUidRecord(const QStringList &props, UserInfo &current_user);

void handleFprRecord(const QStringList &props, UserInfo &current_user);

QList<UserInfo> parseGpgColonOutput(const QString &output, bool secret);

#endif // SRC_GPGKEYSTATE_H_
