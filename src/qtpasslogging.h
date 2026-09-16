// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_QTPASSLOGGING_H_
#define SRC_QTPASSLOGGING_H_

#include <QLoggingCategory>

/**
 * @file qtpasslogging.h
 * @brief The "qtpass" logging category.
 *
 * Debug output is off by default in every build; switch it on at run time
 * with `QT_LOGGING_RULES="qtpass.debug=true"` (optionally with
 * `QT_MESSAGE_PATTERN="%{file}:%{line} %{message}"`). Use
 * `qCDebug(lcQtPass) << ...` for tracing and `qCWarning(lcQtPass)` for
 * things that should not happen but are survivable.
 */
Q_DECLARE_LOGGING_CATEGORY(lcQtPass)

#endif // SRC_QTPASSLOGGING_H_
