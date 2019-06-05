#include "simpletransaction.h"
#include <utility>

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

using std::pair;
using namespace Enums;

void simpleTransaction::transactionStart() {
#ifdef QT_DEBUG
  dbg() << "START" << transactionDepth;
#endif
  transactionDepth++;
}

void simpleTransaction::transactionAdd(PROCESS id) {
#ifdef QT_DEBUG
  dbg() << "ADD" << transactionDepth << id;
#endif
  if (transactionDepth > 0) {
    lastInTransaction = id;
  } else {
    transactionQueue.push(pair<PROCESS, PROCESS>(id, id));
  }
}

void simpleTransaction::transactionEnd(PROCESS pid) {
#ifdef QT_DEBUG
  dbg() << "END" << transactionDepth;
#endif
  if (transactionDepth > 0) {
    transactionDepth--;
    if (transactionDepth == 0 && lastInTransaction != INVALID) {
      transactionQueue.push(pair<PROCESS, PROCESS>(lastInTransaction, pid));
      lastInTransaction = INVALID;
    }
  }
}

PROCESS simpleTransaction::transactionIsOver(PROCESS id) {
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
