// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @class PassBackendFactory
 * @brief Pass backend lifecycle implementation.
 *
 * @see passbackendfactory.h
 */

#include "passbackendfactory.h"
#include "pass.h"
#include "qtpasssettings.h"

Pass *PassBackendFactory::pass = nullptr;
QScopedPointer<RealPass> PassBackendFactory::realPass;
QScopedPointer<ImitatePass> PassBackendFactory::imitatePass;

auto PassBackendFactory::getPass() -> Pass * {
  if (!pass) {
    if (QtPassSettings::isUsePass()) {
      pass = getRealPass();
    } else {
      pass = getImitatePass();
    }
    if (pass) {
      pass->init();
    }
  }
  return pass;
}

auto PassBackendFactory::getRealPass() -> RealPass * {
  if (realPass.isNull()) {
    realPass.reset(new RealPass());
  }
  return realPass.data();
}

auto PassBackendFactory::getImitatePass() -> ImitatePass * {
  if (imitatePass.isNull()) {
    imitatePass.reset(new ImitatePass());
  }
  return imitatePass.data();
}

void PassBackendFactory::invalidate() { pass = nullptr; }
