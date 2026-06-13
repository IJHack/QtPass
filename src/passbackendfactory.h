// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_PASSBACKENDFACTORY_H_
#define SRC_PASSBACKENDFACTORY_H_

#include "imitatepass.h"
#include "realpass.h"

#include <QScopedPointer>

class Pass;

/**
 * @class PassBackendFactory
 * @brief Owns the Pass backend lifecycle (RealPass vs ImitatePass).
 *
 * Extracted from QtPassSettings, which conflated settings storage with
 * deciding and caching which password-store backend is active. The factory
 * lazily constructs the two concrete backends, hands out the active one based
 * on the configured mode, and exposes invalidate() so a settings change can
 * force the active backend to be re-selected on next use.
 */
class PassBackendFactory {
public:
  /**
   * @brief Get the active backend, constructing and initialising it on first
   * use after construction or invalidation.
   * @return Pointer to the active Pass backend (RealPass when "use pass" is
   * enabled, otherwise ImitatePass).
   */
  static auto getPass() -> Pass *;
  /**
   * @brief Get the lazily-constructed RealPass backend.
   * @return Pointer to the RealPass instance.
   */
  static auto getRealPass() -> RealPass *;
  /**
   * @brief Get the lazily-constructed ImitatePass backend.
   * @return Pointer to the ImitatePass instance.
   */
  static auto getImitatePass() -> ImitatePass *;
  /**
   * @brief Forget the currently active backend.
   *
   * The next getPass() call re-selects and re-initialises a backend. Call
   * after changing the "use pass" setting so the switch takes effect.
   */
  static void invalidate();

private:
  static Pass *pass;
  // Go via pointer to avoid dynamic initialization, due to "random"
  // initialization order relative to other globals, especially around QObject
  // metadata dynamic initialization can lead to crashes depending on compiler,
  // linker etc.
  static QScopedPointer<RealPass> realPass;
  static QScopedPointer<ImitatePass> imitatePass;

  PassBackendFactory() = default;
};

#endif // SRC_PASSBACKENDFACTORY_H_
