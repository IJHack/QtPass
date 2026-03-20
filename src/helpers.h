// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_HELPERS_H_
#define SRC_HELPERS_H_

#if __cplusplus >= 201703L
#define AS_CONST(x) std::as_const(x)
#else
#define AS_CONST(x) qAsConst(x)
#endif

#endif // SRC_HELPERS_H_
