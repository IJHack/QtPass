// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "simpletransaction.h"
#include <utility>

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

using Enums::INVALID;
using Enums::PROCESS;

/**
 * @brief simpleTransaction::transactionStart
 */
void simpleTransaction::transactionStart() {
#ifdef QT_DEBUG
  dbg() << "START" << transactionDepth;
#endif
  transactionDepth++;
}

/**
 * @brief simpleTransaction::transactionAdd
 * @param id
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
 * @brief simpleTransaction::transactionEnd
 * @param pid
 */
void simpleTransaction::transactionEnd(PROCESS pid) {
#ifdef QT_DEBUG
  dbg() << "END" << transactionDepth;
#endif
  if (transactionDepth > 0) {
    transactionDepth--;
    if (transactionDepth == 0 && lastInTransaction != INVALID) {
      transactionQueue.emplace(lastInTransaction, pid);
      lastInTransaction = INVALID;
    }
  }
}

/**
 * @brief simpleTransaction::transactionIsOver
 * @param id
 * @return
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
