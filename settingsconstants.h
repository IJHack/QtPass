#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QString>

class SettingsConstants {

public:
  const static QString version;
  const static QString groupMainwindow;
  const static QString geometry;
  const static QString savestate;
  const static QString pos;
  const static QString size;
  const static QString splitterLeft;
  const static QString splitterRight;
  const static QString maximized;
  const static QString usePass;
  const static QString useAutoclear;
  const static QString autoclearSeconds;
  const static QString useAutoclearPanel;
  const static QString autoclearPanelSeconds;
  const static QString hidePassword;
  const static QString hideContent;
  const static QString addGPGId;
  const static QString passStore;
  const static QString passExecutable;
  const static QString gitExecutable;
  const static QString gpgExecutable;
  const static QString pwgenExecutable;
  const static QString gpgHome;
  const static QString useWebDav;
  const static QString webDavUrl;
  const static QString webDavUser;
  const static QString webDavPassword;
  const static QString profile;
  const static QString groupProfiles;
  const static QString useGit;
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

#endif // CONSTANTS_H
