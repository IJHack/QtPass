// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @class SettingsSerializer
 * @brief AppSettings <-> QSettings mapping implementation.
 *
 * @see settingsserializer.h
 */

#include "settingsserializer.h"
#include "settingsconstants.h"

#include <QSettings>

auto SettingsSerializer::load(QSettings &qs) -> AppSettings {
  AppSettings s;

  // Window / session
  s.version = qs.value(SettingsConstants::version).toString();
  s.geometry = qs.value(SettingsConstants::geometry).toByteArray();
  s.savestate = qs.value(SettingsConstants::savestate).toByteArray();
  s.pos = qs.value(SettingsConstants::pos).toPoint();
  s.size = qs.value(SettingsConstants::size).toSize();
  s.maximized = qs.value(SettingsConstants::maximized, false).toBool();
  s.activeProfile = qs.value(SettingsConstants::profile).toString();

  // Backend selection / store
  s.usePass = qs.value(SettingsConstants::usePass, false).toBool();
  s.passStore = qs.value(SettingsConstants::passStore).toString();
  s.passSigningKey = qs.value(SettingsConstants::passSigningKey).toString();

  // Executables
  s.passExecutable = qs.value(SettingsConstants::passExecutable).toString();
  s.gitExecutable = qs.value(SettingsConstants::gitExecutable).toString();
  s.gpgExecutable = qs.value(SettingsConstants::gpgExecutable).toString();
  s.pwgenExecutable = qs.value(SettingsConstants::pwgenExecutable).toString();
  s.qrencodeExecutable =
      qs.value(SettingsConstants::qrencodeExecutable).toString();
  s.gpgHome = qs.value(SettingsConstants::gpgHome).toString();
  s.sshAuthSockOverride =
      qs.value(SettingsConstants::sshAuthSockOverride).toString();

  // Clipboard / autoclear
  s.clipBoardType = static_cast<Enums::clipBoardType>(
      qs.value(SettingsConstants::clipBoardType,
               static_cast<int>(Enums::CLIPBOARD_NEVER))
          .toInt());
  s.useSelection = qs.value(SettingsConstants::useSelection, false).toBool();
  s.useAutoclear = qs.value(SettingsConstants::useAutoclear, false).toBool();
  s.autoclearSeconds = qs.value(SettingsConstants::autoclearSeconds, 0).toInt();
  s.useAutoclearPanel =
      qs.value(SettingsConstants::useAutoclearPanel, false).toBool();
  s.autoclearPanelSeconds =
      qs.value(SettingsConstants::autoclearPanelSeconds, 0).toInt();

  // Content display
  s.hidePassword = qs.value(SettingsConstants::hidePassword, false).toBool();
  s.hideContent = qs.value(SettingsConstants::hideContent, false).toBool();
  s.useMonospace = qs.value(SettingsConstants::useMonospace, false).toBool();
  s.displayAsIs = qs.value(SettingsConstants::displayAsIs, false).toBool();
  s.noLineWrapping =
      qs.value(SettingsConstants::noLineWrapping, false).toBool();

  // Features
  s.addGPGId = qs.value(SettingsConstants::addGPGId, false).toBool();
  s.useGit = qs.value(SettingsConstants::useGit, false).toBool();
  s.useGrepSearch = qs.value(SettingsConstants::useGrepSearch, false).toBool();
  s.useOtp = qs.value(SettingsConstants::useOtp, false).toBool();
  s.useQrencode = qs.value(SettingsConstants::useQrencode, false).toBool();
  s.usePwgen = qs.value(SettingsConstants::usePwgen, false).toBool();
  s.useWebDav = qs.value(SettingsConstants::useWebDav, false).toBool();
  s.webDavUrl = qs.value(SettingsConstants::webDavUrl).toString();
  s.webDavUser = qs.value(SettingsConstants::webDavUser).toString();
  s.webDavPassword = qs.value(SettingsConstants::webDavPassword).toString();
  s.autoPull = qs.value(SettingsConstants::autoPull, false).toBool();
  s.autoPush = qs.value(SettingsConstants::autoPush, false).toBool();
  s.showProcessOutput =
      qs.value(SettingsConstants::showProcessOutput, false).toBool();

  // Templates
  s.passTemplate = qs.value(SettingsConstants::passTemplate).toString();
  s.useTemplate = qs.value(SettingsConstants::useTemplate, false).toBool();
  s.templateAllFields =
      qs.value(SettingsConstants::templateAllFields, false).toBool();

  // Password generation
  int length = qs.value(SettingsConstants::passwordLength, 16).toInt();
  if (length <= 0) {
    length = 16;
  }
  s.passwordConfiguration.length = length;
  s.passwordConfiguration.selected =
      static_cast<PasswordConfiguration::characterSet>(
          qs.value(SettingsConstants::passwordCharsSelection, 0).toInt());
  s.passwordConfiguration.Characters[PasswordConfiguration::CUSTOM] =
      qs.value(SettingsConstants::passwordChars, QString()).toString();
  s.avoidCapitals = qs.value(SettingsConstants::avoidCapitals, false).toBool();
  s.avoidNumbers = qs.value(SettingsConstants::avoidNumbers, false).toBool();
  s.lessRandom = qs.value(SettingsConstants::lessRandom, false).toBool();
  s.useSymbols = qs.value(SettingsConstants::useSymbols, false).toBool();

  // System / tray
  s.useTrayIcon = qs.value(SettingsConstants::useTrayIcon, false).toBool();
  s.hideOnClose = qs.value(SettingsConstants::hideOnClose, false).toBool();
  s.startMinimized =
      qs.value(SettingsConstants::startMinimized, false).toBool();
  s.alwaysOnTop = qs.value(SettingsConstants::alwaysOnTop, false).toBool();

  return s;
}

void SettingsSerializer::save(QSettings &qs, const AppSettings &s) {
  // Window / session
  qs.setValue(SettingsConstants::version, s.version);
  qs.setValue(SettingsConstants::geometry, s.geometry);
  qs.setValue(SettingsConstants::savestate, s.savestate);
  qs.setValue(SettingsConstants::pos, s.pos);
  qs.setValue(SettingsConstants::size, s.size);
  qs.setValue(SettingsConstants::maximized, s.maximized);
  qs.setValue(SettingsConstants::profile, s.activeProfile);

  // Backend selection / store
  qs.setValue(SettingsConstants::usePass, s.usePass);
  qs.setValue(SettingsConstants::passStore, s.passStore);
  qs.setValue(SettingsConstants::passSigningKey, s.passSigningKey);

  // Executables
  qs.setValue(SettingsConstants::passExecutable, s.passExecutable);
  qs.setValue(SettingsConstants::gitExecutable, s.gitExecutable);
  qs.setValue(SettingsConstants::gpgExecutable, s.gpgExecutable);
  qs.setValue(SettingsConstants::pwgenExecutable, s.pwgenExecutable);
  qs.setValue(SettingsConstants::qrencodeExecutable, s.qrencodeExecutable);
  qs.setValue(SettingsConstants::gpgHome, s.gpgHome);
  qs.setValue(SettingsConstants::sshAuthSockOverride, s.sshAuthSockOverride);

  // Clipboard / autoclear
  qs.setValue(SettingsConstants::clipBoardType,
              static_cast<int>(s.clipBoardType));
  qs.setValue(SettingsConstants::useSelection, s.useSelection);
  qs.setValue(SettingsConstants::useAutoclear, s.useAutoclear);
  qs.setValue(SettingsConstants::autoclearSeconds, s.autoclearSeconds);
  qs.setValue(SettingsConstants::useAutoclearPanel, s.useAutoclearPanel);
  qs.setValue(SettingsConstants::autoclearPanelSeconds,
              s.autoclearPanelSeconds);

  // Content display
  qs.setValue(SettingsConstants::hidePassword, s.hidePassword);
  qs.setValue(SettingsConstants::hideContent, s.hideContent);
  qs.setValue(SettingsConstants::useMonospace, s.useMonospace);
  qs.setValue(SettingsConstants::displayAsIs, s.displayAsIs);
  qs.setValue(SettingsConstants::noLineWrapping, s.noLineWrapping);

  // Features
  qs.setValue(SettingsConstants::addGPGId, s.addGPGId);
  qs.setValue(SettingsConstants::useGit, s.useGit);
  qs.setValue(SettingsConstants::useGrepSearch, s.useGrepSearch);
  qs.setValue(SettingsConstants::useOtp, s.useOtp);
  qs.setValue(SettingsConstants::useQrencode, s.useQrencode);
  qs.setValue(SettingsConstants::usePwgen, s.usePwgen);
  qs.setValue(SettingsConstants::useWebDav, s.useWebDav);
  qs.setValue(SettingsConstants::webDavUrl, s.webDavUrl);
  qs.setValue(SettingsConstants::webDavUser, s.webDavUser);
  qs.setValue(SettingsConstants::webDavPassword, s.webDavPassword);
  qs.setValue(SettingsConstants::autoPull, s.autoPull);
  qs.setValue(SettingsConstants::autoPush, s.autoPush);
  qs.setValue(SettingsConstants::showProcessOutput, s.showProcessOutput);

  // Templates
  qs.setValue(SettingsConstants::passTemplate, s.passTemplate);
  qs.setValue(SettingsConstants::useTemplate, s.useTemplate);
  qs.setValue(SettingsConstants::templateAllFields, s.templateAllFields);

  // Password generation
  qs.setValue(SettingsConstants::passwordLength,
              s.passwordConfiguration.length);
  qs.setValue(SettingsConstants::passwordCharsSelection,
              static_cast<int>(s.passwordConfiguration.selected));
  qs.setValue(
      SettingsConstants::passwordChars,
      s.passwordConfiguration.Characters[PasswordConfiguration::CUSTOM]);
  qs.setValue(SettingsConstants::avoidCapitals, s.avoidCapitals);
  qs.setValue(SettingsConstants::avoidNumbers, s.avoidNumbers);
  qs.setValue(SettingsConstants::lessRandom, s.lessRandom);
  qs.setValue(SettingsConstants::useSymbols, s.useSymbols);

  // System / tray
  qs.setValue(SettingsConstants::useTrayIcon, s.useTrayIcon);
  qs.setValue(SettingsConstants::hideOnClose, s.hideOnClose);
  qs.setValue(SettingsConstants::startMinimized, s.startMinimized);
  qs.setValue(SettingsConstants::alwaysOnTop, s.alwaysOnTop);
}
