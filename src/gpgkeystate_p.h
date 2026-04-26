// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_GPGKEYSTATE_P_H_
#define SRC_GPGKEYSTATE_P_H_

#include "userinfo.h"

#include <QStringList>

void handlePubSecRecord(const QStringList &props, bool secret,
                        UserInfo &current_user);
void handleFprRecord(const QStringList &props, UserInfo &current_user);

#endif // SRC_GPGKEYSTATE_P_H_
