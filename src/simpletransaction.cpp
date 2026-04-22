// SPDX-FileCopyrightText: 2017 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "simpletransaction.h"
#include <utility>

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

using Enums::INVALID;
using Enums::PROCESS;

/**
 * @brief Marks the start of a sequence of processes that shall be treated as
 * one operation.
 */
void simpleTransaction::transactionStart() {
#ifdef QT_DEBUG
  dbg() << "START" << transactionDepth;
#endif
  transactionDepth++;
}

/**
 * @brief If called after transactionStart() and before transactionEnd(),
 *        this marks the given process as the next step in the transaction.
 *        Otherwise it marks the process as the only step in the transaction.
 * @param id Process that shall be treated as part of the transaction
 */
void simpleTransaction::transactionAdd(PROCESS id) {
#ifdef QT_DEBUG
  dbg() << "ADD" << transactionDepth << id;
#endif
  if (transactionDepth > 0) {
    lastInTransaction = id;
  } else {
    transactionQueue.emplace(id, id);
  }
}

/**
 * @brief Marks the end of the current transaction.
 * @param pid Value that will be used as the result of the transaction
 */
void simpleTransaction::transactionEnd(PROCESS pid) {
#ifdef QT_DEBUG
  dbg() << "END" << transactionDepth;
#endif
  if (transactionDepth > 0) {
    if (lastInTransaction != INVALID) {
      transactionQueue.emplace(lastInTransaction, pid);
    }
    transactionDepth--;
    lastInTransaction = INVALID;
  }
}

/**
 * @brief Checks whether the currently finished process is last in current
 * transaction.
 * @param id The process id to check
 * @return Result of transaction as set by transactionAdd or transactionEnd,
 *         or PROCESS::INVALID if the transaction is not yet over
 */
auto simpleTransaction::transactionIsOver(PROCESS id) -> PROCESS {
#ifdef QT_DEBUG
  dbg() << "OVER" << transactionDepth << id;
#endif
  if (!transactionQueue.empty() && id == transactionQueue.front().first) {
    PROCESS ret = transactionQueue.front().second;
    transactionQueue.pop();
    return ret;
  }
  return INVALID;
}
