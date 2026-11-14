// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Defines the `lcQtPass` logging category with info as its default
 *        level, so a release build can be traced without recompiling.
 */
#include "qtpasslogging.h"

// Info and above are on; debug stays quiet until QT_LOGGING_RULES asks for
// it, so a release build can produce a trace without recompiling.
Q_LOGGING_CATEGORY(lcQtPass, "qtpass", QtInfoMsg)
