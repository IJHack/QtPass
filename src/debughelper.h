// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_DEBUGHELPER_H_
#define SRC_DEBUGHELPER_H_

#include <QDebug>

/**
 * @file debughelper.h
 * @brief Debug utilities for QtPass.
 */

/**
 * @brief Simple debug macro that includes file and line number.
 *
 * Usage: dbg() << "message";
 * Output: "filename.cpp:123 message"
 *
 * Only available in debug builds.
 */
#define dbg() qDebug() << __FILE__ ":" << __LINE__

#endif // SRC_DEBUGHELPER_H_
