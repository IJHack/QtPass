// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "processinfo.h"
#include <QString>

#include <array>

namespace Enums {

namespace {

/**
 * @brief The one table, indexed by PROCESS.
 *
 * `secret` is an allow list turned around: a process kind is silent until
 * someone decides its output is harmless. The static_assert below makes the
 * compiler ask that question for every value added to the enum.
 */
constexpr std::array<ProcessInfo, PROCESS_COUNT> kTable{{
    {"git init", false},      // GIT_INIT  // no-tr
    {"git add", false},       // GIT_ADD  // no-tr
    {"git commit", false},    // GIT_COMMIT  // no-tr
    {"git rm", false},        // GIT_RM  // no-tr
    {"git pull", false},      // GIT_PULL  // no-tr
    {"git push", false},      // GIT_PUSH  // no-tr
    {"", true},               // PASS_SHOW: the decrypted entry
    {"pass insert", true},    // PASS_INSERT: the entry being written  // no-tr
    {"pass rm", false},       // PASS_REMOVE  // no-tr
    {"pass init", false},     // PASS_INIT  // no-tr
    {"gpg --gen-key", false}, // GPG_GENKEYS  // no-tr
    {"pass mv", false},       // PASS_MOVE  // no-tr
    {"pass cp", false},       // PASS_COPY  // no-tr
    {"git mv", false},        // GIT_MOVE  // no-tr
    // ImitatePass::Copy literally invokes `git cp` (a git-extras
    // subcommand), so the label matches what is run. Stock-git users without
    // git-extras see the underlying "'cp' is not a git command" failure in
    // the process output panel.
    {"git cp", false},   // GIT_COPY  // no-tr
    {"pass grep", true}, // PASS_GREP: hits inside entries  // no-tr
}};

static_assert(kTable.size() == static_cast<std::size_t>(PROCESS_COUNT),
              "every PROCESS needs a label and a secrecy decision");

constexpr ProcessInfo kNotAProcess{"", false};

} // namespace

auto processInfo(PROCESS pid) -> const ProcessInfo & {
  if (pid < 0 || pid >= PROCESS_COUNT) {
    return kNotAProcess;
  }
  return kTable.at(static_cast<std::size_t>(pid));
}

auto processLabel(PROCESS pid) -> QString {
  return QString::fromLatin1(processInfo(pid).label);
}

} // namespace Enums
