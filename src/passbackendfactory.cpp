// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @class PassBackendFactory
 * @brief Pass backend lifecycle implementation.
 *
 * @see passbackendfactory.h
 */

#include "passbackendfactory.h"
#include "appsettings.h"
#include "pass.h"
#include "qtpasssettings.h"

#include <QDir>

Pass *PassBackendFactory::pass = nullptr;
QScopedPointer<RealPass> PassBackendFactory::realPass;
QScopedPointer<ImitatePass> PassBackendFactory::imitatePass;

auto PassBackendFactory::getPass() -> Pass * {
  if (!pass) {
    AppSettings s = QtPassSettings::load();
    // Ensure store directory exists (first-run side effect — load() normalises
    // the path but does not create the directory).
    if (!QDir(s.passStore).exists())
      QDir().mkdir(s.passStore);
    if (s.usePass) {
      pass = getRealPass();
    } else {
      pass = getImitatePass();
    }
    if (pass) {
      pass->init(s);
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
