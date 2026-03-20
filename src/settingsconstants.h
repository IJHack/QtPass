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
  static const QString splitterLeft;
  static const QString splitterRight;
  static const QString maximized;
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
  const static QString gpgHome;
  const static QString useWebDav;
  const static QString webDavUrl;
  const static QString webDavUser;
  const static QString webDavPassword;
  const static QString profile;
  const static QString groupProfiles;
  const static QString useGit;
  const static QString useOtp;
  const static QString useQrencode;
  const static QString qrencodeExecutable;
  const static QString useClipboard;
  const static QString usePwgen;
  const static QString avoidCapitals;
  const static QString avoidNumbers;
  const static QString lessRandom;
  const static QString useSymbols;
  const static QString passwordLength;
  const static QString passwordCharsselection;
  const static QString passwordChars;
  const static QString useTrayIcon;
  const static QString hideOnClose;
  const static QString startMinimized;
  const static QString alwaysOnTop;
  const static QString autoPull;
  const static QString autoPush;
  const static QString passTemplate;
  const static QString useTemplate;
  const static QString templateAllFields;
  const static QString clipBoardType;

private:
  explicit SettingsConstants();
};

#endif // SRC_SETTINGSCONSTANTS_H_
