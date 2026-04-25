// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_SETTINGSCONSTANTS_H_
#define SRC_SETTINGSCONSTANTS_H_

#include <QString>

/*!
    \class SettingsConstants
    \brief Table for the naming of configuration items
*/
class SettingsConstants {
public:
  static const QString version;
  static const QString groupMainwindow;
  static const QString geometry;
  static const QString savestate;
  static const QString pos;
  static const QString size;
  static const QString maximized;

  // Dialog-specific settings (with key prefix)
  static const QString dialogGeometry;
  static const QString dialogPos;
  static const QString dialogSize;
  static const QString dialogMaximized;

  static const QString splitterLeft;
  static const QString splitterRight;
  static const QString usePass;
  static const QString useAutoclear;
  static const QString useSelection;
  static const QString autoclearSeconds;
  static const QString useAutoclearPanel;
  static const QString autoclearPanelSeconds;
  static const QString hidePassword;
  static const QString hideContent;
  static const QString useMonospace;
  static const QString displayAsIs;
  static const QString noLineWrapping;
  static const QString addGPGId;
  static const QString passStore;
  static const QString passSigningKey;
  static const QString passExecutable;
  static const QString gitExecutable;
  static const QString gpgExecutable;
  static const QString pwgenExecutable;
  static const QString gpgHome;
  static const QString useWebDav;
  static const QString webDavUrl;
  static const QString webDavUser;
  static const QString webDavPassword;
  static const QString profile;
  static const QString groupProfiles;
  static const QString useGit;
  static const QString useGrepSearch;
  static const QString useOtp;
  static const QString useQrencode;
  static const QString qrencodeExecutable;
  static const QString useClipboard;
  static const QString usePwgen;
  static const QString avoidCapitals;
  static const QString avoidNumbers;
  static const QString lessRandom;
  static const QString useSymbols;
  static const QString passwordLength;
  static const QString passwordCharsSelection;
  static const QString passwordChars;
  static const QString useTrayIcon;
  static const QString hideOnClose;
  static const QString startMinimized;
  static const QString alwaysOnTop;
  static const QString autoPull;
  static const QString autoPush;
  static const QString passTemplate;
  static const QString useTemplate;
  static const QString templateAllFields;
  static const QString showProcessOutput;
  static const QString clipBoardType;

private:
  explicit SettingsConstants();
};

#endif // SRC_SETTINGSCONSTANTS_H_
