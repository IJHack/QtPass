// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DEBUG_H
#define DEBUG_H

#include <QDebug>

//  this is soooooo ugly...
#define dbg() qDebug() << __FILE__ ":" << __LINE__

#endif // DEBUG_H
