// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_SSHAUTHSOCK_H_
#define SRC_SSHAUTHSOCK_H_

#include <QString>
#include <cstdint>

/**
 * @class SshAuthSock
 * @brief SSH_AUTH_SOCK discovery and validation for child processes.
 *
 * Extracted from Util. GUI-launched applications don't inherit shell-set
 * environment variables, so users with gpg-agent's SSH support or another
 * external SSH agent see git push/pull fail. See initialise(). Issue #543.
 */
class SshAuthSock {
public:
  /**
   * @brief Classify a non-empty SSH_AUTH_SOCK override path supplied by the
   *        user.
   *
   * Empty / unset input is handled by the caller (no warning when the field
   * is blank). For a non-empty path, this returns one of:
   *
   * - `Valid` — exists, readable, and on Unix is a Unix domain socket.
   * - `DoesNotExist` — `QFileInfo::exists()` returned false.
   * - `NotReadable` — exists but `QFileInfo::isReadable()` returned false.
   * - `NotUnixDomainSocket` — Unix-only: `stat()` says it's not `S_ISSOCK`.
   *
   * On Windows the socket check is skipped — ssh-agent uses a named pipe.
   */
  enum class OverrideStatus : std::uint8_t {
    Valid,
    DoesNotExist,
    NotReadable,
    NotUnixDomainSocket,
  };
  /**
   * @brief Validate an SSH_AUTH_SOCK override path. Caller is expected to
   *        skip validation when the input is empty / whitespace-only.
   * @param path Non-empty candidate path.
   * @return Classification per OverrideStatus.
   */
  static auto overrideStatus(const QString &path) -> OverrideStatus;

  /**
   * @brief Ensure SSH_AUTH_SOCK is set for child processes.
   *
   * GUI-launched applications don't inherit shell-set environment variables
   * (`.bashrc`/`.zshrc`/etc.), so users with gpg-agent's SSH support or other
   * external SSH agents see `git push`/`git pull` fail when QtPass launches
   * from a desktop launcher rather than a terminal. Issue #543.
   *
   * Resolution order:
   * 1. If `SSH_AUTH_SOCK` is already set (terminal launch, .desktop override,
   *    parent process), do nothing.
   * 2. If a `sshAuthSockOverride` setting is configured in QtPass, use it
   *    verbatim as an explicit user choice (no validation is performed here).
   *    Users should ensure this path points to a valid, accessible SSH agent
   *    socket; otherwise SSH operations may fail for child processes. The
   *    Settings dialog shows a non-blocking warning at save time when the
   *    override path looks bogus (missing, unreadable, or not a socket).
   * 3. Probe `gpgconf --list-dirs agent-ssh-socket` (canonical for gpg-agent),
   *    then validate the candidate with `ssh-add -l` (must exit 0 or 1) before
   *    adopting. Validation prevents silently switching users from a working
   *    external SSH agent to an empty gpg-agent SSH socket.
   * 4. On macOS, fall back to `launchctl getenv SSH_AUTH_SOCK`, with the same
   *    `ssh-add -l` validation.
   *
   * Sets the variable via qputenv so child processes inherit it.
   *
   * @param override Manual override path from settings (empty → auto-probe).
   */
  static void initialise(const QString &override = {});

private:
  SshAuthSock() = default;
};

#endif // SRC_SSHAUTHSOCK_H_
