// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @class SshAuthSock
 * @brief SSH_AUTH_SOCK discovery and validation implementation.
 *
 * @see sshauthsock.h
 */

#include "sshauthsock.h"
#include "executor.h"
#include <QFileInfo>
#include <QProcessEnvironment>
#ifndef Q_OS_WIN
#include <sys/stat.h>
#endif

#ifdef QT_DEBUG
#include "debughelper.h"
#endif

namespace {
/**
 * @brief Verify that a candidate SSH_AUTH_SOCK socket has a live agent
 *        listening, before letting QtPass switch the environment to it.
 *
 * Why: a user may have a working external SSH agent (OpenSSH ssh-agent,
 * KeePassXC / 1Password / yubikey-agent / gnome-keyring) configured in
 * their shell, but its env didn't propagate to the GUI launcher. If we
 * blindly adopt whatever `gpgconf --list-dirs agent-ssh-socket` reports —
 * even when gpg-agent isn't running with SSH support, or has no keys —
 * we'd be silently switching them away from their real agent.
 *
 * Approach: spawn `ssh-add -l` with SSH_AUTH_SOCK pointed at the candidate
 * (via Executor's env-aware overload, so the parent env isn't disturbed).
 * Treat exit codes 0 (keys present) and 1 (agent alive but key list empty —
 * legitimate for YubiKey-backed setups that enumerate on tap) as
 * "reachable". Anything else, including ssh-add not being on PATH, means
 * we don't adopt the candidate.
 *
 * @param candidate Path to validate.
 * @return true if the socket has a reachable agent; false otherwise.
 */
auto isSshAgentReachable(const QString &candidate) -> bool {
  if (candidate.isEmpty()) {
    return false;
  }
  // Build a minimal env that overrides SSH_AUTH_SOCK without polluting the
  // parent process. systemEnvironment() captures whatever was set when
  // QtPass started, then we override the one var of interest.
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert(QStringLiteral("SSH_AUTH_SOCK"), candidate);
  QString out;
  QString err;
  const int exitCode = Executor::executeBlocking(
      env, QStringLiteral("ssh-add"), {QStringLiteral("-l")}, &out, &err);
  // OpenSSH ssh-add(1) exit codes: 0 = success / has keys, 1 = no
  // identities present, 2 = couldn't open a connection. Anything else is
  // also treated as unreachable (e.g. ssh-add not on PATH).
  return exitCode == 0 || exitCode == 1;
}
} // namespace

auto SshAuthSock::overrideStatus(const QString &path) -> OverrideStatus {
  const QFileInfo fi(path);
  if (!fi.exists()) {
    return OverrideStatus::DoesNotExist;
  }
  if (!fi.isReadable()) {
    return OverrideStatus::NotReadable;
  }
#ifdef Q_OS_UNIX
  // S_ISSOCK isn't exposed by QFileInfo; go through stat(2). On Windows
  // ssh-agent uses a named pipe (\\.\pipe\openssh-ssh-agent.<sid>) and the
  // socket check would always fail, so skip it there — exists + readable is
  // the best we can do.
  struct stat st{};
  if (::stat(path.toLocal8Bit().constData(), &st) != 0 ||
      !S_ISSOCK(st.st_mode)) {
    return OverrideStatus::NotUnixDomainSocket;
  }
#endif
  return OverrideStatus::Valid;
}

/**
 * @brief Probe and set SSH_AUTH_SOCK if missing — see header for full rules.
 *
 * Implementation notes:
 * - Reads QtPassSettings::getSshAuthSockOverride() for the manual override
 *   path; if non-empty, uses it verbatim (no validation — explicit override).
 * - Otherwise runs `gpgconf --list-dirs agent-ssh-socket` (gpg-agent's
 *   canonical socket reporter). On macOS, additionally tries `launchctl
 *   getenv SSH_AUTH_SOCK`.
 * - Auto-probed candidates are validated via `ssh-add -l` before adoption,
 *   so users with a different external SSH agent aren't silently switched
 *   to an empty gpg-agent SSH socket.
 * - All probes go through Executor::executeBlocking with short, bounded
 *   subprocess executions; any failure is silently absorbed (the user just
 *   doesn't get the auto-fix).
 */
void SshAuthSock::initialise(const QString &override) {
  // Honour any value already in the environment — terminal launches,
  // explicit `.desktop` Exec= overrides, and parent-process exports must win.
  if (!qgetenv("SSH_AUTH_SOCK").isEmpty()) {
    return;
  }

  // Manual override from settings takes precedence over auto-probe.
  if (!override.isEmpty()) {
    qputenv("SSH_AUTH_SOCK", override.toUtf8());
#ifdef QT_DEBUG
    dbg() << "SshAuthSock::initialise(): set from settings override:"
          << override;
#endif
    return;
  }

  // Auto-probe via gpgconf (canonical for gpg-agent's SSH support).
  QString out;
  QString err;
  if (Executor::executeBlocking(
          QStringLiteral("gpgconf"),
          {QStringLiteral("--list-dirs"), QStringLiteral("agent-ssh-socket")},
          &out, &err) == 0) {
    const QString socket = out.trimmed();
    if (!socket.isEmpty() && isSshAgentReachable(socket)) {
      qputenv("SSH_AUTH_SOCK", socket.toUtf8());
#ifdef QT_DEBUG
      dbg() << "SshAuthSock::initialise(): set from gpgconf:" << socket;
#endif
      return;
    }
#ifdef QT_DEBUG
    if (!socket.isEmpty()) {
      dbg() << "SshAuthSock::initialise(): gpgconf reported" << socket
            << "but ssh-add -l rejected it; not adopting";
    }
#endif
  }

#ifdef Q_OS_MACOS
  // On macOS, GUI-launched apps may have SSH_AUTH_SOCK in launchd's
  // per-session environment but not in the inherited process env.
  out.clear();
  err.clear();
  if (Executor::executeBlocking(
          QStringLiteral("launchctl"),
          {QStringLiteral("getenv"), QStringLiteral("SSH_AUTH_SOCK")}, &out,
          &err) == 0) {
    const QString socket = out.trimmed();
    if (!socket.isEmpty() && isSshAgentReachable(socket)) {
      qputenv("SSH_AUTH_SOCK", socket.toUtf8());
#ifdef QT_DEBUG
      dbg() << "SshAuthSock::initialise(): set from launchctl:" << socket;
#endif
    }
#ifdef QT_DEBUG
    else if (!socket.isEmpty()) {
      dbg() << "SshAuthSock::initialise(): launchctl reported" << socket
            << "but ssh-add -l rejected it; not adopting";
    }
#endif
  }
#endif
}
