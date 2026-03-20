// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_DEBUGHELPER_H_
#define SRC_DEBUGHELPER_H_

#include <QDebug>

//  this is soooooo ugly...
#define dbg() qDebug() << __FILE__ ":" << __LINE__

#endif // SRC_DEBUGHELPER_H_
