# FAQ

## Issues

### Can't save a password

- Is the folder initialized? Easiest way is to use the [Users] button
  and make sure you can encrypt for someone (e.g. yourself)
- Are you using Git? If not, make sure it is switched off.

### The edit dialog says "too many to edit" or "too long to edit"

The edit dialog does not open an entry with more than 10,000 lines, or with
a line longer than 1,048,576 characters (1 MiB, far beyond any real key file
kept as one line of base64). A password or field that long would have to be
cut to fit, and the cut value would be saved over the entry. An entry that
size also costs QtPass a lot of memory and time, and in a shared store
someone else can make one. QtPass still shows such an entry, but you cannot
edit it there: use `pass edit` in a terminal instead.

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

### Git or pass keeps running in WSL after QtPass gave up on it

When QtPass runs pass, gpg or Git through WSL (an executable set to `wsl pass`, `wsl gpg` or `wsl git`), it can stop only `wsl.exe` itself.

QtPass gives up on such a command when it runs out of time: the Git pull before an edit, a key import, or the Git setup of a new profile. The program inside WSL can then keep running, and a pull can still change the store after QtPass said it timed out.

To see whether `git`, `pass` or `gpg` is still running, run `wsl -d <distribution> ps -ef` for the distribution QtPass uses (the one after `-d` in its executable setting, or your default one). To stop such a process, run `wsl -d <distribution> kill <pid>` with each PID from that list.

As a last resort, `wsl --terminate <distribution>` stops the whole distribution, and `wsl --shutdown` stops every distribution. Both also stop everything else running there, so save your work in other WSL sessions first.

### Git has issues with GPG SSH Authentication

This tutorial might resolve your issues.
<https://gitforwindows.org/OpenSSH-Integration-with-Pageant>

### Users, move or copy says "Changes Git has not recorded"

With the `pass` backend and Git, `pass` commits the whole folder it changes, and everything staged with it. A plaintext export left in the store would be committed and pushed. The message lists the files. QtPass refuses:

- **Users**, move and copy while the folder holds files Git does not track or changes Git has not committed.
- These and new entries while anything is staged in the repository `pass` commits in.

Commit only what belongs in the store. Move a plaintext export or anything else private out of the store, or delete it; if you staged it, unstage it first with `git restore --staged <file>`, and if Git already tracks it, remove it from Git with `git rm --cached <file>` and `git commit` (it stays in the history). Have Git ignore what should stay but not be shared. Then try again.

Finder (macOS), Explorer (Windows) and Dolphin (KDE) write `.DS_Store`, `Thumbs.db`, `desktop.ini` or `.directory` into folders they open. Removing them does not last, so the message names the lines to add to the `.gitignore` at the top of the store (or of a folder with a Git repository of its own), for example:

```gitignore
.DS_Store
Thumbs.db
```

Commit the `.gitignore` and push it, so your teammates' devices skip these files too.

A rule does not untrack a file Git already tracks: remove such a file from Git with `git rm --cached` and commit that along with the rule. If Git still lists the file, a rule deeper down un-ignores it; `git check-ignore -v <file>` names that rule.

QtPass does not skip these files on its own: `pass` would commit them, and a `.DS_Store` names the files in its folder.

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

The dialog takes one public key per import. A block that holds several keys, or a private key, is refused and nothing is imported: QtPass names the one key it imported so you can check its fingerprint, and a second key in the same block would enter your keyring unchecked. Import such keys one by one, or run `gpg --import` yourself.

The dialog ignores `gpg.conf`, so options there such as `import-filter` or `import-options` do not apply to it. Run `gpg --import` yourself when you need them.

That includes `keyring`, `primary-keyring` and `no-default-keyring`: the dialog imports into your default keyring. When `gpg.conf` points gpg at another keyring, the Users list reads that one and may not show the key you just imported. Run `gpg --import` yourself in that case, so the key lands where your configuration says (see [#2312](https://github.com/IJHack/QtPass/pull/2312)).

### What does the Share submenu do?

Right-click on a **folder** in the password tree to access the **Share** submenu. It bundles the recipient-management actions in one place:

- **Re-encrypt all passwords**: runs `pass init` on the folder so its contents match the current `.gpg-id` recipients.
- **Export my public key…**: saves your public key as an ASCII-armored file you can hand to a teammate.
- **Add recipient…**: opens the user dialog scoped to this folder so you can toggle who can decrypt its passwords.
- **What is this?**: a short explainer dialog linking the three flows.

The submenu only appears on folders, not individual password files.

### I removed someone, but they can still read some passwords

Saving recipients in the **Users** dialog changes the recipient list (`.gpg-id`) of that folder and re-encrypts the entries that use it. Some entries are not reached:

- **Folders below with a list of their own.** For those, their own `.gpg-id` decides, not the one you saved. The Users dialog names them (up to five), and also names folders below that it could not read. Earlier QtPass versions gave every folder made with **Add folder** its own copy of the parent's list. To find every such list, run this with your own store path:

  ```sh
  find ~/.password-store -mindepth 2 -name .gpg-id
  ```

  - Without a signing key: delete a copy you do not need, then use **Share › Re-encrypt all passwords** on that folder.
  - With a signing key: open **Users** for that folder, untick the person and save. A deleted list that this device has accepted is not replaced by the parent's list, also one without a generation line (written by `pass` or by QtPass before 2.0), and saving into that folder then stops with "Recipient list missing".
  - If someone removed such a list on purpose, the folder cannot go back to the parent's list on this device. If Git still lists the deletion as a change, commit it first. Then open **Users** for it and save the recipients it should have, which gives it a fresh signed list of its own.

- **Hidden folders** (names that start with a dot). QtPass does not show, search or re-encrypt them, as `pass ls` does not list them (`pass init` does re-encrypt them). Move the entries into a normal folder, then re-encrypt.
- **Copies elsewhere.** Re-encrypting does not take back what the person could already read: Git history, backups and synced copies keep the old files. Change the passwords that matter.

### Saving says "Recipient list is a link"

With the `pass` backend, QtPass does not run `pass` where a folder's `.gpg-id` is a symbolic link (or, on Windows, a junction). `pass` follows such a link and encrypts to the recipients in the file it points to, while QtPass and its **Users** dialog skip it and show the list of a folder above. That would encrypt to people you cannot see in QtPass.

A shared recipient list kept in one place and linked into several folders therefore no longer works with QtPass. Replace each link with a copy of the list, or remove it so that the folder uses its parent's list:

```sh
find ~/.password-store -name .gpg-id -type l
```

This includes the list at the top of the store: a store whose own `.gpg-id` is a link, as a dotfile manager such as GNU Stow or chezmoi may set it up, cannot be saved to with the `pass` backend until the link is replaced by a regular file.

### Saving says "Recipient list reads differently"

With the `pass` backend, `pass` reads each `.gpg-id` itself, and it reads some lists differently from the **Users** dialog. In such a list a line can name a recipient that the dialog does not show. QtPass then does not run `pass` for that folder. This happens when the list:

- starts with a byte-order mark, holds a NUL byte, or is not valid UTF-8;
- has Windows line ends (a carriage return);
- has whitespace other than blanks and tabs before a `#` comment, such as a no-break space;
- has a line that starts with a dash;
- ends without a line feed after its last recipient. `pass` skips that last line, possibly your own key.

Open **Users** for that folder, check the recipients and press **OK**. QtPass then writes the list again, one recipient per line. A list you edit by hand must use plain Unix line ends and end with a line feed.

With a signing key, a linked list was already refused, also with the native backend.

### Users lists "Key not found in keyring" or "Not looked up in keyring"

The **Users** dialog ticks every key that a line in `.gpg-id` names, by fingerprint, key ID (of the key or of one of its subkeys), email address or any of its user IDs. A line that names no key in your keyring is listed as a ticked "Key not found in keyring" row, so saving keeps that recipient. Import their key to see who it is, or untick the row to remove them.

A line in a form QtPass does not read itself, such as a keygrip (`&...`) or a fingerprint written with spaces, needs a gpg lookup of its own while the dialog opens. After ten such lookups, further lines of that kind are listed unticked as "Not looked up in keyring", and **OK** leaves them out. Tick a row to keep it, or write those recipients as fingerprints.

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

### I can't select or copy the OTP code

The code is not selectable text. Copy it with the copy button next to it, or
with the **OTP** action in the toolbar: those copies are marked as a secret,
which clipboard managers that honour it keep out of their history, and with
autoclear on they are removed again. Selecting the code skipped both.

With **No clipboard** chosen on the **Clipboard** page of the settings, the
panel shows no copy buttons. The **OTP** action in the toolbar still copies the
code, as the **Copy** action copies the password; or choose **On-demand copy to
clipboard** to get the button back.

### How do I hide a PIN or other fields, like the password?

Turn on **Hide password** on the **Display** page of the settings. The
password is then behind an eye button, and so is:

- **Also hide other secrets**: every field whose name holds one of the words
  next to it (PIN, CVV, key, token, recovery and the like), as whole words in
  any case, also in the plural and as APIs write them (`apiKey`, `api_key`,
  `PIN2`). On by default; "Default" puts the words back.
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

### Settings won't save: "Another profile already has this name"

Each profile needs a name of its own. Since 2.0, QtPass ignores upper and
lower case when it compares names: "Work" and "work" are the same name. Settings
marks both profiles in red and keeps OK off until one of them has another name.

Why: QtPass stores each profile under its name in the configuration (see
[Where is the configuration stored?](#where-is-the-configuration-stored)). On
Windows the registry and `qtpass.ini` ignore case, so two such profiles were
one, and one silently replaced the other
([#2222](https://github.com/IJHack/QtPass/pull/2222)). A portable or synced
configuration can move between systems, so the rule is the same on all of
them.

To fix it:

1. Open Settings and select one of the two profiles marked in red.
2. Type another name, such as "Work (laptop)".
3. Click OK.

A new name changes only the label. The store folder, signing key and Git
settings of the profile stay as they are, and the active profile stays active
under its new name.

A profile name also cannot contain `/` or `\`: the configuration reads `/` as
a group separator, and `\` too on Windows.

### Settings won't open: "Settings not opened"

Since 2.0, Settings and the profile box wait until no `gpg`, `git` or `pass`
command runs or waits. A slow push or a `gpg` that waits for your PIN keeps
them closed until it is done. If a PIN dialog is open, answer or cancel it,
then open Settings again. If the command never ends (`git` waiting for a
password, a `gpg` that hangs), restart QtPass: Settings open at once, so you
can fix the path or option that caused it. A restart forgets a re-encryption
that was still to run, and the hold on automatic pushes that a failed,
cancelled or refused one left. So first turn off **Automatically push** in
Settings: each run that finishes would push the whole store, also a folder
still to do. Then re-encrypt each folder whose re-encryption did not finish or
that you moved entries into with **Share › Re-encrypt all passwords** on that
folder. For the top of the store, which has no Share menu, right-click an
empty part of the tree, open **Users** and save the recipients as they are. Turn
automatic push back on once all are done. On a sub-folder, the Users dialog
would give it a recipient list of its own.

Why: what is still queued runs with the store and programs that are set when
it starts. A store changed in Settings meanwhile was the one a pending
`pass init` re-encrypted, for the recipients you chose for the other store.

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

### Starting QtPass opens a second window instead of the open one

A second start of QtPass normally hands its search text to the window that is
already open and quits. It does that only when that window runs as you: the
names it uses to find it live in a folder only you can open (Linux, BSD and
macOS), and on Windows, where those names are visible to every user, QtPass
checks who owns the window it reaches. When it cannot be sure, it opens a
window of its own instead.

On Windows with several user accounts signed in on one machine, this check is
**untested**: if you see a second window where you expect the first, or a
start that does nothing, please
[open an issue](https://github.com/IJHack/QtPass/issues) and say how many
accounts were signed in. A second window also opens when the folder for those
names is missing or its path is too long; that loses only the hand-over, not
anything else.

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
once you have installed and configured qt6ct.

The variable applies to every Qt application started from that session (and,
from `/etc/env.d`, to every user). To change it for QtPass only, start it with
`env QT_QPA_PLATFORMTHEME=gtk3 qtpass` instead: copy `qtpass.desktop` to
`~/.local/share/applications` and change its `Exec=` line to that.

The AppImage bundles Qt's `xdgdesktopportal` theme but not `gtk3`, and its
start-up hook overwrites `QT_QPA_PLATFORMTHEME` on GNOME and Xfce, so pass the
theme as an option instead:
`./QtPass-*.AppImage -platformtheme xdgdesktopportal`. That theme reads your
light or dark preference from the desktop's settings portal, so it needs
`xdg-desktop-portal` with a backend for your desktop (such as
`xdg-desktop-portal-gtk`, or `xdg-desktop-portal-xapp` on Cinnamon, MATE and
Xfce); without one Qt keeps its default colours.

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
