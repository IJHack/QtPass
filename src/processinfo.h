// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_PROCESSINFO_H_
#define SRC_PROCESSINFO_H_

#include "enums.h"

class QString;

namespace Enums {

/**
 * @struct ProcessInfo
 * @brief What QtPass knows about one kind of subprocess: what to call it and
 * whether its output may carry a secret.
 *
 * One table (processInfo()) answers both questions, so a process kind cannot
 * be labelled in the output panel while being classified as harmless
 * somewhere else. A new PROCESS value has to be added to it or the build
 * fails, which is how the classification gets made rather than forgotten.
 */
struct ProcessInfo {
  /// The command as it is run, for the process output panel; empty for a
  /// process whose output is never shown.
  const char *label;
  /// Whether the output may contain a decrypted entry or a search hit in
  /// one. Such output reaches neither the panel nor the generic signal.
  bool secret;
};

/**
 * @brief What is known about @p pid.
 * @param pid Process identifier; PROCESS_COUNT and INVALID answer too.
 * @return The entry for @p pid: an empty label and no secret for a value
 * that is not a real process.
 */
auto processInfo(PROCESS pid) -> const ProcessInfo &;

/**
 * @brief The command label for @p pid, as the output panel shows it.
 * @param pid Process identifier.
 * @return e.g. "git push"; empty for a process that is never shown.
 */
auto processLabel(PROCESS pid) -> QString;

} // namespace Enums

#endif // SRC_PROCESSINFO_H_
