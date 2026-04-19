// SPDX-FileCopyrightText: 2017 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_SIMPLETRANSACTION_H_
#define SRC_SIMPLETRANSACTION_H_

#include "enums.h"
#include <queue>

/**
 * @class simpleTransaction
 * @brief Tracks a sequence of processes that should be treated as one atomic
 * operation.
 */
class simpleTransaction {
  int transactionDepth;
  Enums::PROCESS lastInTransaction;
  std::queue<std::pair<Enums::PROCESS, Enums::PROCESS>> transactionQueue;

public:
  /**
   * @brief Construct a simpleTransaction in its initial idle state.
   */
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
   * @param process process that shall be treated as part of transaction
   */
  void transactionAdd(Enums::PROCESS process);
  /**
   * @brief transactionEnd marks end of transaction
   *
   * @param process value that will be used as a result of transaction
   */
  void transactionEnd(Enums::PROCESS process);
  /**
   * @brief transactionIsOver checks wheather currently finished process is last
   *                          in current transaction
   *
   * @param current the process that just finished.
   * @return result of transaction as set by transactionAdd or transactionEnd if
   *         the transaction is over or PROCESS::INVALID if it's not yet over
   */
  auto transactionIsOver(Enums::PROCESS current) -> Enums::PROCESS;
};

#endif // SRC_SIMPLETRANSACTION_H_
