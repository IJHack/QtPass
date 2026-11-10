# FAQ

## Issues

### Can't save a password

- Is the folder initialized? Easiest way is to use the [Users] button
  and make sure you can encrypt for someone (e.g. yourself)
- Are you using Git? If not, make sure it is switched off.

### I have an issue with GNOME keyring

- Disable GNOME keyring
- Create a `~/.gnupg/gpg-agent.conf` containing:

```bash
enable-ssh-support
write-env-file
use-standard-socket
default-cache-ttl 600
max-cache-ttl 7200
```

- See the [pass FAQ](https://www.passwordstore.org/#faq) for more troubleshooting tips.

### I don't get a passphrase / PIN dialog

- You'll need to install a pinentry package. On modern distributions `pinentry-qt` (Qt 6) is the usual choice; `pinentry-qt5` and `pinentry-gtk` are common fallbacks if your distro hasn't moved to Qt 6 yet. Search your package manager. You may also need to set the full path to that executable in your `~/.gnupg/gpg-agent.conf`,
  for example: `pinentry-program /usr/bin/pinentry-qt`
- On some systems it might be necessary to create a symbolic
  link `/usr/bin/pinentry` to your pinentry application of choice
  eg: `ln -s /usr/bin/pinentry-qt /usr/bin/pinentry`
- On macOS `pinentry-program /usr/local/bin/pinentry-mac` works after installing `pinentry-mac` from Homebrew.

### I have another issue with gpg

- On modern systems, `gpg` is usually GnuPG 2.x already. If your system has separate `gpg` and `gpg2` keyrings (typically older or custom setups), your key may exist in one but not the other.

```bash
gpg --export [ID] > public.key
gpg --export-secret-key [ID] > private.key
gpg2 --import public.key
gpg2 --import private.key
rm public.key private.key
```

Where [ID] is your gpg key-id. If your setup is reversed, exchange `gpg` and `gpg2` accordingly.

### Git doesn't work on Windows

Git for Windows comes with an `ssh-askpass` compatible command, `git-gui--askpass` (located in `/mingw64/libexec/git-core/git-gui--askpass` on PortableGit version, presumably some place similar for the installed version).

To use it:

1. Set `SSH_ASKPASS` environment variable to the full path of `git-gui--askpass`, for example:

   ```text
   C:\Program Files\Git\mingw64\libexec\git-core\git-gui--askpass.exe
   ```

2. Optionally set `GIT_ASKPASS` to the same value so Git uses the same helper.

3. Restart QtPass after setting the environment variables.

Example using PowerShell:

```powershell
$env:SSH_ASKPASS = "C:\Program Files\Git\mingw64\libexec\git-core\git-gui--askpass.exe"
$env:GIT_ASKPASS = "C:\Program Files\Git\mingw64\libexec\git-core\git-gui--askpass.exe"
```

Or to make it permanent:

```powershell
setx SSH_ASKPASS "C:\Program Files\Git\mingw64\libexec\git-core\git-gui--askpass.exe"
setx GIT_ASKPASS "C:\Program Files\Git\mingw64\libexec\git-core\git-gui--askpass.exe"
```

**Note:** After using `setx`, you'll need to start QtPass in a new session (close and reopen your terminal, or log out and back in) for the environment variables to take effect.

### Git has issues with GPG SSH Authentication

This tutorial might resolve your issues.
<https://gitforwindows.org/OpenSSH-Integration-with-Pageant>

### gpg reports a missing `/tmp/tst_integration-…/pubring.kbx`

QtPass is pointing gpg at a `GNUPGHOME` that no longer exists. The 1.7.0
test suite wrote its temporary keyring path into the live configuration when
`make check` was run as your own user (for example by a distribution package
build), and never removed it. Delete the `gpgHome=` line from
`~/.config/IJHack/QtPass.conf` (or set it to empty) and restart QtPass. Since
1.8.1 QtPass ignores a configured GPG home that does not exist and shows a
status message instead.

### GPG says "Public key unusable" or "No secret key"

Your GPG key may not be trusted or has expired. Run `gpg --edit-key <KEYID>` and:

- Set the trust level to "ultimate" (`trust` command)
- Check if the key has expired (`expire` command to extend if needed)
- For smartcards/YubiKeys, ensure the card is connected and unlocked (you can verify with `gpg --card-status`)

### What is the "Signing Key" in profile settings?

The Signing Key field in profile configuration is **optional** and usually should be **left empty**.

It is only needed if you want to create **detached signatures** for your `.gpg-id` files to verify the recipients list hasn't been tampered with. Most users can ignore this field.

Common mistake: Putting your GPG key ID here will cause "Signature does not exist" errors when saving passwords. Leave it empty unless you specifically need signature verification.

### How do I import someone else's GPG key?

Open the **Users** dialog (the _Users_ button in the toolbar) and click **Import key**. You can:

- Pick an ASCII-armored `.asc` file from disk, or
- Paste the armored key block from your clipboard.

Once imported, QtPass refreshes the user list and selects the new key so you can tick it as a recipient. Binary keyrings aren't accepted in this dialog; convert them first with `gpg --armor --export <KEYID>`.

### What does the Share submenu do?

Right-click on a **folder** in the password tree to access the **Share** submenu. It bundles the recipient-management actions in one place:

- **Re-encrypt all passwords**: runs `pass init` on the folder so its contents match the current `.gpg-id` recipients.
- **Export my public key…**: saves your public key as an ASCII-armored file you can hand to a teammate.
- **Add recipient…**: opens the user dialog scoped to this folder so you can toggle who can decrypt its passwords.
- **What is this?**: a short explainer dialog linking the three flows.

The submenu only appears on folders, not individual password files.

### How do I set up a one-time password (OTP/TOTP)?

QtPass generates TOTP codes itself (RFC 6238), so you no longer need the
`pass-otp` extension, and it works on every platform.

1. Make sure **Enable one-time password (OTP) support** is checked on the
   **Settings** tab of the configuration dialog (it is on by default).
2. Store the account's `otpauth://` URI in the entry. Put it in the `OTP`
   template field, or on its own line in the body (the form `pass otp insert`
   writes). A bare base32 secret works too.
3. Select the entry: QtPass shows a live code with a countdown and a copy
   button, refreshing automatically. SHA-1/SHA-256/SHA-512 and Steam Guard
   codes are all supported.

The shared secret itself is never shown on screen or copied; only the current
code is.

### QtPass shows an OTP code and I don't want it to

OTP support is enabled by default. To hide the live codes, uncheck **Enable
one-time password (OTP) support** on the **Settings** tab of the configuration
dialog. The choice is remembered. Your entries are unchanged: the `otpauth://`
secret stays in the file and QtPass simply stops rendering a code for it.

### How do I hide a PIN or other fields, like the password?

Turn on **Hide password** on the **Display** page of the settings. The
password is then behind an eye button, and so is:

- **Also hide other secrets**: every field whose name holds one of the words
  next to it (PIN, CVV, key, token, recovery and the like), as whole words in
  any case. On by default; "Default" puts the words back.
- **Hide every field**: all fields, for shared screens and presenting. The
  notes below the fields and the OTP code stay visible; **Hide content** hides
  everything.
- A field a template marks as secret, with a comment line in the template:

  ```ini
  [bank]
  login
  iban
  pin
  # secret: pin, tan
  ```

  The line goes in a section of the store's `.templates` file, or in the
  template on the **Passwords** page of the settings. Names count whole and in
  any case. `pass` and QtPass 1.8 read the line as a comment.

  **A mark counts for the whole store.** QtPass can't tell which template an
  entry was made with, so a name marked in one section is hidden in every
  entry of the store: marking `login` in `[bank]` hides the login of your
  email entries too. Mark only names that are secret wherever they appear.

### OTP QR codes don't work on macOS

Applications launched from Finder don't inherit the shell PATH. Install QtPass via Homebrew (which sets up PATH correctly) or create a wrapper script that sets PATH before launching QtPass, for example:

```bash
#!/usr/bin/env bash
# Adjust PATH as needed for your Homebrew / QtPass installation
export PATH="/usr/local/bin:/opt/homebrew/bin:$PATH"
exec /Applications/QtPass.app/Contents/MacOS/qtpass "$@"
```

Save this script (e.g. as `~/bin/qtpass-wrapper.sh`), make it executable with `chmod +x ~/bin/qtpass-wrapper.sh`, and configure your launcher (Automator app, custom .app bundle, or shortcut) to run this script instead of starting QtPass directly.

### QtPass doesn't follow my system language

QtPass uses the system language at startup. On some desktop environments, you may need to launch QtPass from the terminal or configure your desktop environment to pass the correct locale environment variables.

### Where is the configuration stored?

QtPass tries to use the native configuration location for the operating system it is running on.

- Linux and BSD: `$HOME/.config/IJHack/QtPass.conf`
- macOS: `$HOME/Library/Preferences/com.IJHack.QtPass.plist`
- Windows registry: `HKEY_CURRENT_USER\Software\IJHack\QtPass`

These settings can be overridden by a `qtpass.ini` file in the folder where the application resides.
So-called "portable config".

There are some things to take care of when trying to sync on some systems (especially macOS, with regards to text and binary .plist files).

More information: <https://doc.qt.io/qt-6/qsettings.html#platform-specific-notes>

### What happened to WebDAV support?

It was removed in QtPass 2.0. The feature mounted your password store from a
WebDAV server: on Windows through `WNetUseConnectionA`, elsewhere by starting
`fusedav`. It had no user interface (the four settings keys could only be set
by editing the configuration file by hand), the WebDAV password was stored
there in plain text, and the Linux side had passed literal quote characters to
`fusedav` since 2020, so it could not have worked. Saving your settings once in
2.0 deletes the `useWebDav`, `webDavUrl`, `webDavUser` and `webDavPassword`
keys, including that plaintext password.

Nothing is lost: the store is a directory of files, so keep syncing it the way
you prefer.

- **Git** is what `pass` itself uses and what QtPass supports directly: enable
  _Use Git_ in the settings and point it at a remote (a private repository,
  your own server, anything you can `git push` to). That gives you history and
  conflict handling for free.
- **A synced folder** works too: Nextcloud, Syncthing, Dropbox, an SMB or
  WebDAV share your desktop already mounts. Put the store inside the synced
  folder and select it as the password store. Mount the share with your
  operating system's own tooling rather than asking QtPass to do it; those
  clients handle credentials, reconnects and conflicts far better than the
  removed code did.

### Ctrl+Q used to hide QtPass to the tray, now it quits

Since 2.0 Ctrl+Q (Cmd+Q on macOS) quits, as it does in KeePassXC, Telegram and
per the KDE, GNOME and macOS guidelines. Closing the _window_ is what "Hide on
close" applies to: the close button, Alt+F4, and File ▸ Close window (Ctrl+W,
Cmd+W) hide QtPass to the tray when that setting is on.

### How do I get a debug log?

Every build, including the installers, can trace what it runs. Start QtPass
with the `qtpass` logging category switched on:

```sh
QT_LOGGING_RULES="qtpass.debug=true" qtpass
```

Add `QT_MESSAGE_PATTERN="%{time} %{file}:%{line} %{message}"` for
timestamps and source locations. On Windows set the same two variables in the
shell before starting `qtpass.exe`, or run it from a terminal to see the
output; on macOS use `open -a QtPass --env QT_LOGGING_RULES=qtpass.debug=true`
(macOS 14 and newer) or start the binary inside the bundle from a terminal.
Passwords are never logged, but paths and key IDs are; check a log before
attaching it to an issue.

### Where can I ask for help?

- Create an issue on [GitHub](https://github.com/IJHack/QtPass/issues).
- Send an email to [help@qtpass.org](mailto:help@qtpass.org)
- Visit the [Official QtPass site](https://qtpass.org/)

### Can I import from KeePass, LastPass or X?

- Yes, check [passwordstore.org/#migration](https://www.passwordstore.org/#migration)
  for more info.

### Can I use my passwords on my phone or in my browser?

Yes: a pass store is a folder of GPG-encrypted files, so other apps can read
the same one. They are separate projects; we list them with what to watch out
for and do not vouch for them (checked October 2026).

- **Browser:** [Browserpass](https://github.com/browserpass/browserpass-extension)
  (Firefox, Chrome and Chromium on Linux, macOS and Windows) runs `gpg` itself,
  so it works without `pass`. It matches sites by entry name, so name entries
  after the domain, such as `github.com`; one-time codes are off by default.
  [PassFF](https://codeberg.org/PassFF/passff) (Firefox) needs `pass` or gopass
  installed and reads one store. Both need their native helper and a graphical
  pinentry.
- **Android:** [Password Store](https://f-droid.org/packages/app.passwordstore.agrahn/)
  (agrahn's fork, on F-Droid) opens entries written by GnuPG 2.4 and newer,
  which the archived original app cannot. It needs a copy of your secret key
  (no YubiKey), syncs only through a Git remote, does not check signed
  `.gpg-id` files, and cannot use post-quantum keys: one such recipient in a
  `.gpg-id` makes that folder unreadable on the phone.
- **iPhone:** [Pass for iOS](https://mssun.github.io/passforios/) saves an
  edited entry encrypted to one key only, so keep shared stores read-only
  there. A YubiKey works with RSA keys only, and the app tries only the first recipient of an entry, so an entry encrypted to several keys may not open with it.
- **Command line:** [gopass](https://github.com/gopasspw/gopass) is
  pass-compatible and runs `gpg` directly; it does not check signed `.gpg-id`
  files.

QtPass itself on Android is an
[experiment](https://github.com/IJHack/QtPass/issues/2005) for now.

### I don't see icons on the buttons

You do not have the Qt SVG library installed.
Please install using your favorite package manager.

### I get icons that do not fit my (X11) default

- On some WindowManagers, Qt doesn't know what icon set to use. A trick:

```sh
export DESKTOP_SESSION=gnome
```

### QtPass flashes dark at start, or ignores my light or dark theme

QtPass uses whatever theme your desktop gives Qt applications; it sets no
colours of its own. Qt picks its platform theme plugin from the desktop
session. KDE Plasma and LXQt bring their own, but on GTK desktops such as
Cinnamon, MATE or Xfce Qt needs to be told, or it may start with its default
colours and repaint once it reads your theme: a dark flash at start, or the
wrong light or dark palette.

Set the platform theme in your session environment, for example in
`~/.profile`, then log out and in again:

```sh
export QT_QPA_PLATFORMTHEME=gtk3
```

On Gentoo you can also set it system-wide: put the line
`QT_QPA_PLATFORMTHEME=gtk3` (without `export`) in a file such as
`/etc/env.d/99qt-platformtheme`, run `env-update`, then log in again.

This needs Qt's GTK 3 platform theme plugin (`libqgtk3.so`), which some
distributions package separately; others, such as Linux Mint, set a platform
theme for you. Instead of `gtk3` you can use `xdgdesktopportal`, or `qt6ct`
once you have installed and configured qt6ct (`qt5ct` for Qt 5 builds of
QtPass 1.8).

The variable applies to every Qt application started from that session (and,
from `/etc/env.d`, to every user). To change it for QtPass only, start it with
`env QT_QPA_PLATFORMTHEME=gtk3 qtpass` instead: copy `qtpass.desktop` to
`~/.local/share/applications` and change its `Exec=` line to that.

The AppImage bundles Qt's `xdgdesktopportal` theme but not `gtk3`, so start it
with `QT_QPA_PLATFORMTHEME=xdgdesktopportal ./QtPass-*.AppImage`.

Whichever way you set it, quit QtPass completely first (it may still be
running in the tray): starting it again only hands over to the running
instance, which keeps the theme it started with.

QtPass does not set the variable itself, because that would override the
platform integration of desktops that have their own. See
[#2126](https://github.com/IJHack/QtPass/issues/2126).

### Can not find either qmake or qmake6

QtPass uses Qt6 by default. Use:

- `qmake6` - for Qt6 (recommended, default)
- `qmake` - for Qt5

If you get an error like "QApplication" not found or Qt installation issues, make sure you're using the correct qmake version for your Qt installation.

On some systems, you may need to specify the full path, e.g., `/usr/lib/qt6/bin/qmake6`.

### Qt installation issues

If you encounter Qt installation issues, you'll have to install the current version via your package manager or download it from <https://www.qt.io/download/> and build from source.

### I don't like the design, what gives?

- It's all on GitHub, clone, change and send a pull request.
- Open an issue and point out defects or better yet propose changes.

### QtPass is not in my native language

- Unfortunately, QtPass might not support your native language, or the translations might be incomplete. Check if newer versions of QtPass support it.
- If translations are available but aren't working, try to set the language manually (see below) or open an issue.

### How do I set the language manually?

QtPass uses the system language (via `QLocale::system()`). Changing it depends on your platform:

- **Linux / BSD:** `LANGUAGE=fr qtpass` will run QtPass in French. `LC_ALL` and `LANG` work too; see the GNU gettext docs for the precedence order.
- **macOS:** environment variables like `LANG` are **not** honored by Qt on macOS; Qt reads the CoreFoundation locale instead. Override per-app with:

  ```sh
  defaults write com.IJHack.QtPass AppleLanguages '(fr)'
  ```

  Or pass the override as a launch argument:

  ```sh
  open -a QtPass --args -AppleLanguages '(fr)'
  ```

  Use `defaults delete com.IJHack.QtPass AppleLanguages` to revert.

- **Windows:** there is no environment-variable override; Qt reads the Windows display language via `GetUserDefaultLocaleName()`. Change the **system display language** in _Settings → Time & Language → Language_, or add the desired language and move it to the top of your preferred-language list.

## How can I help improve QtPass?

### I would like to donate

- Time:
  - Read [contributing](CONTRIBUTING.md) documentation.
  - Fork, clone, hack and send a pull request.
  - Find an [issue](https://github.com/IJHack/QtPass/issues) to work on.
  - Participate in our bug bounty, you submit an issue and help us
    fix it, I send you a bounty.
