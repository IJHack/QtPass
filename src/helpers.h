// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_HELPERS_H_
#define SRC_HELPERS_H_

/**
 * @file helpers.h
 * @brief Utility macros for QtPass.
 */

/**
 * @brief Cross-platform const_cast for range-based for loops.
 *
 * Provides std::as_const (C++17) or qAsConst (older) for safe
 * iteration over containers that might be modified.
 */
#if __cplusplus >= 201703L
#define AS_CONST(x) std::as_const(x)
#else
#define AS_CONST(x) qAsConst(x)
#endif

#endif // SRC_HELPERS_H_
