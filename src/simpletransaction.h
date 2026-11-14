// SPDX-FileCopyrightText: 2017 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief SimpleTransaction, the bookkeeping that lets ImitatePass report
 * several queued processes as one finished operation.
 */
#ifndef SRC_SIMPLETRANSACTION_H_
#define SRC_SIMPLETRANSACTION_H_

#include "enums.h"
#include <queue>

/**
 * @class SimpleTransaction
 * @brief Tracks a sequence of processes that should be treated as one atomic
 * operation.
 *
 * ImitatePass queues several processes for one operation (gpg, then
 * `git add`, then `git commit` for an insert) and wants to report the
 * operation once, when the last of them ends. Between transactionStart()
 * and transactionEnd() it calls transactionAdd() for each process it
 * queues; transactionEnd() names the result to report. As processes finish,
 * transactionIsOver() answers with that result for the last one of a
 * transaction and with Enums::INVALID for every earlier one. Transactions
 * may nest; only the outermost one counts.
 */
class SimpleTransaction {
  /** @brief How many transactionStart() calls are still open. */
  int transactionDepth{0};
  /**
   * @brief The process most recently added to the open transaction, or
   * Enums::INVALID when none was added yet.
   */
  Enums::PROCESS lastInTransaction{Enums::INVALID};
  /**
   * @brief Closed transactions awaiting completion, oldest first: the
   * process that ends each one, and the result to report when it does.
   */
  std::queue<std::pair<Enums::PROCESS, Enums::PROCESS>> transactionQueue;

public:
  /**
   * @brief Construct a SimpleTransaction in its initial idle state.
   */
  SimpleTransaction() {}
  /**
   * @brief Open a transaction: the processes added until the matching
   * transactionEnd() are reported as one operation.
   */
  void transactionStart();
  /**
   * @brief Note that @p process has been queued.
   *
   * Inside an open transaction it becomes the transaction's latest step.
   * Outside one it is a transaction of its own, reported as itself.
   * @param process The process just queued.
   */
  void transactionAdd(Enums::PROCESS process);
  /**
   * @brief Close the transaction opened by the matching transactionStart().
   *
   * When the outermost transaction closes and at least one process was
   * added, the last one added becomes the step that completes it.
   * @param process What transactionIsOver() reports once that step ends.
   */
  void transactionEnd(Enums::PROCESS process);
  /**
   * @brief Tell whether the process that just finished ends the oldest
   * pending transaction, and take that transaction off the queue if so.
   * @param current The process that just finished.
   * @return The transaction's result as set by transactionAdd() or
   *         transactionEnd(), or Enums::INVALID while the transaction has
   *         steps left.
   */
  auto transactionIsOver(Enums::PROCESS current) -> Enums::PROCESS;
};

#endif // SRC_SIMPLETRANSACTION_H_
