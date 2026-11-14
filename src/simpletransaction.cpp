// SPDX-FileCopyrightText: 2017 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief Implements SimpleTransaction as a queue of (last process, result)
 * pairs.
 */
#include "simpletransaction.h"
#include <utility>

#include "qtpasslogging.h"

using Enums::INVALID;
using Enums::PROCESS;

void SimpleTransaction::transactionStart() {
  qCDebug(lcQtPass) << "START" << transactionDepth;
  transactionDepth++;
}

void SimpleTransaction::transactionAdd(PROCESS id) {
  qCDebug(lcQtPass) << "ADD" << transactionDepth << id;
  if (transactionDepth > 0) {
    lastInTransaction = id;
  } else {
    transactionQueue.emplace(id, id);
  }
}

void SimpleTransaction::transactionEnd(PROCESS pid) {
  qCDebug(lcQtPass) << "END" << transactionDepth;
  if (transactionDepth > 0) {
    transactionDepth--;
    if (transactionDepth == 0 && lastInTransaction != INVALID) {
      transactionQueue.emplace(lastInTransaction, pid);
      lastInTransaction = INVALID;
    }
  }
}

auto SimpleTransaction::transactionIsOver(PROCESS id) -> PROCESS {
  qCDebug(lcQtPass) << "OVER" << transactionDepth << id;
  if (!transactionQueue.empty() && id == transactionQueue.front().first) {
    PROCESS ret = transactionQueue.front().second;
    transactionQueue.pop();
    return ret;
  }
  return INVALID;
}
