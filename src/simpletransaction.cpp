#include "simpletransaction.h"
#include "debughelper.h"
#include <utility>

using std::pair;
using namespace Enums;

void simpleTransaction::transactionStart() {
  dbg() << "START" << transactionDepth;
  transactionDepth++;
}

void simpleTransaction::transactionAdd(PROCESS id) {
  dbg() << "ADD" << transactionDepth << id;
  if (transactionDepth > 0) {
    lastInTransaction = id;
  } else {
    transactionQueue.push(pair<PROCESS, PROCESS>(id, id));
  }
}

void simpleTransaction::transactionEnd(PROCESS pid) {
  dbg() << "END" << transactionDepth;
  if (transactionDepth > 0) {
    transactionDepth--;
    if (transactionDepth == 0 && lastInTransaction != INVALID) {
      transactionQueue.push(pair<PROCESS, PROCESS>(lastInTransaction, pid));
      lastInTransaction = INVALID;
    }
  }
}

PROCESS simpleTransaction::transactionIsOver(PROCESS id) {
  dbg() << "OVER" << transactionDepth << id;
  if (!transactionQueue.empty() && id == transactionQueue.front().first) {
    PROCESS ret = transactionQueue.front().second;
    transactionQueue.pop();
    return ret;
  }
  return INVALID;
}
