// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_APPSETTINGS_H_
#define SRC_APPSETTINGS_H_

#include "enums.h"
#include "passwordconfiguration.h"

#include <QByteArray>
#include <QPoint>
#include <QSize>
#include <QString>

/**
 * @struct AppSettings
 * @brief Plain value-object holding all flat QtPass configuration.
 *
 * One field per persisted setting, with the same defaults the legacy
 * QtPassSettings getters apply. This is a pure data carrier: it has no
 * QSettings dependency and performs none of the side effects that the old
 * getters did (directory creation for the pass store, screen-centring of the
 * window position, `.git` auto-detection). Those remain application logic in
 * the consumers; SettingsSerializer maps this struct straight to and from
 * QSettings.
 *
 * Nested/keyed settings — profiles (a dedicated Profile type is tracked
 * separately), per-dialog geometry, and splitter positions — are intentionally
 * out of scope here and still go through QtPassSettings directly.
 */
struct AppSettings {
  // --- Window / session ---
  QString version;       ///< Last-run application version.
  QByteArray geometry;   ///< Main-window saved geometry.
  QByteArray savestate;  ///< Main-window saved dock/toolbar state.
  QPoint pos;            ///< Main-window position.
  QSize size;            ///< Main-window size.
  bool maximized{false}; ///< Main-window maximized flag.
  QString activeProfile; ///< Name of the active profile.

  // --- Backend selection / store ---
  bool usePass{false};    ///< Use the `pass` CLI (true) or native git/gpg.
  QString passStore;      ///< Password-store root path.
  QString passSigningKey; ///< GPG key used to sign `.gpg-id` files.

  // --- Executables ---
  QString passExecutable;      ///< Path to the `pass` executable.
  QString gitExecutable;       ///< Path to the `git` executable.
  QString gpgExecutable;       ///< Path to the `gpg`/`gpg2` executable.
  QString pwgenExecutable;     ///< Path to the `pwgen` executable.
  QString qrencodeExecutable;  ///< Path to the `qrencode` executable.
  QString gpgHome;             ///< GPG home directory (read-only externally).
  QString sshAuthSockOverride; ///< Manual SSH_AUTH_SOCK override path.

  // --- Clipboard / autoclear ---
  Enums::clipBoardType clipBoardType{
      Enums::CLIPBOARD_NEVER};   ///< Clipboard copy behaviour.
  bool useSelection{false};      ///< Use the X11 primary selection.
  bool useAutoclear{false};      ///< Clear the clipboard after a delay.
  int autoclearSeconds{0};       ///< Clipboard autoclear delay (seconds).
  bool useAutoclearPanel{false}; ///< Clear the content panel after a delay.
  int autoclearPanelSeconds{0};  ///< Panel autoclear delay (seconds).

  // --- Content display ---
  bool hidePassword{false};   ///< Hide the password line in the panel.
  bool hideContent{false};    ///< Hide the entire content panel.
  bool useMonospace{false};   ///< Render content in a monospace font.
  bool displayAsIs{false};    ///< Show file content unmodified.
  bool noLineWrapping{false}; ///< Disable line wrapping in the panel.

  // --- Features ---
  bool addGPGId{false};          ///< Auto-add `.gpg-id` files to git.
  bool useGit{false};            ///< Enable git integration.
  bool useGrepSearch{false};     ///< Enable content (grep) search.
  bool useOtp{false};            ///< Enable pass-otp support.
  bool useQrencode{false};       ///< Enable qrencode support.
  bool usePwgen{false};          ///< Use pwgen for password generation.
  bool useWebDav{false};         ///< Enable WebDAV synchronisation.
  QString webDavUrl;             ///< WebDAV endpoint URL.
  QString webDavUser;            ///< WebDAV username.
  QString webDavPassword;        ///< WebDAV password.
  bool autoPull{false};          ///< Automatically `git pull` on open.
  bool autoPush{false};          ///< Automatically `git push` after changes.
  bool showProcessOutput{false}; ///< Show external process output.

  // --- Templates ---
  QString passTemplate;          ///< Newline-separated template field names.
  bool useTemplate{false};       ///< Use the password template.
  bool templateAllFields{false}; ///< Treat every `key:` line as a field.

  // --- Password generation ---
  PasswordConfiguration passwordConfiguration; ///< Length/charset/custom chars.
  bool avoidCapitals{false};                   ///< Exclude uppercase letters.
  bool avoidNumbers{false};                    ///< Exclude digits.
  bool lessRandom{false}; ///< Generate memorable (less random) passwords.
  bool useSymbols{false}; ///< Include special symbols.

  // --- System / tray ---
  bool useTrayIcon{false};    ///< Show a system tray icon.
  bool hideOnClose{false};    ///< Hide to tray instead of quitting on close.
  bool startMinimized{false}; ///< Start the window minimised.
  bool alwaysOnTop{false};    ///< Keep the main window always on top.
};

#endif // SRC_APPSETTINGS_H_
