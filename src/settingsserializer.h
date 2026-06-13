// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_SETTINGSSERIALIZER_H_
#define SRC_SETTINGSSERIALIZER_H_

#include "appsettings.h"

class QSettings;

/**
 * @class SettingsSerializer
 * @brief Maps AppSettings to and from a QSettings store.
 *
 * Pure, side-effect-free translation between the flat AppSettings value object
 * and the on-disk QSettings keys (the same keys QtPassSettings uses). Unlike
 * the legacy getters it performs no path normalisation, directory creation,
 * screen-centring or `.git` auto-detection — it only reads and writes raw
 * values. That makes it unit-testable against a throwaway QSettings without any
 * singleton or widget setup.
 */
class SettingsSerializer {
public:
  /**
   * @brief Read all flat settings from a QSettings store.
   * @param qs Source settings store.
   * @return Populated AppSettings (defaults applied for absent keys).
   */
  static auto load(QSettings &qs) -> AppSettings;
  /**
   * @brief Write all flat settings to a QSettings store.
   * @param qs Destination settings store.
   * @param settings Values to persist.
   */
  static void save(QSettings &qs, const AppSettings &settings);

private:
  SettingsSerializer() = default;
};

#endif // SRC_SETTINGSSERIALIZER_H_
