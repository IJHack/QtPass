#ifndef SIMPLETRANSACTION_H
#define SIMPLETRANSACTION_H

#include "enums.h"
#include <queue>

class simpleTransaction {

  int transactionDepth;
  Enums::PROCESS lastInTransaction;
  std::queue<std::pair<Enums::PROCESS, Enums::PROCESS> > transactionQueue;

public:
  simpleTransaction()
      : transactionDepth(0), lastInTransaction(Enums::INVALID) {}
  /**
   * @brief transactionStart this function is used to mark start of the sequence
   *                         of processes that shall be treated as one
   * operation.
   */
  void transactionStart();
  /**
   * @brief transactionAdd If called after call to transactionStart() and before
   *                       transactionEnd(), this method marks given process as
   *                       next step in transaction. Otherwise it marks given
   *                       process as the only step in transaction(it's value is
   *                       treated as transaction result).
   *
   * @param id process that shall be treated as part of transaction
   */
  void transactionAdd(Enums::PROCESS);
  /**
   * @brief transactionEnd marks end of transaction
   *
   * @param pid value that will be used as a result of transaction
   */
  void transactionEnd(Enums::PROCESS);
  /**
   * @brief transactionIsOver checks wheather currently finished process is last
   *                          in current transaction
   *
   * @return result of transaction as set by transactionAdd or transactionEnd if
   *         the transaction is over or PROCESS::INVALID if it's not yet over
   */
  Enums::PROCESS transactionIsOver(Enums::PROCESS);
};

#endif // SIMPLETRANSACTION_H
