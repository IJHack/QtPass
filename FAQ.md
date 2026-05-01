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

Once imported, QtPass refreshes the user list and selects the new key so you can tick it as a recipient. Binary keyrings aren't accepted in this dialog — convert them first with `gpg --armor --export <KEYID>`.

### What does the Share submenu do?

Right-click on a **folder** in the password tree to access the **Share** submenu. It bundles the recipient-management actions in one place:

- **Re-encrypt all passwords** — runs `pass init` on the folder so its contents match the current `.gpg-id` recipients.
- **Export my public key…** — saves your public key as an ASCII-armored file you can hand to a teammate.
- **Add recipient…** — opens the user dialog scoped to this folder so you can toggle who can decrypt its passwords.
- **What is this?** — short explainer dialog linking the three flows.

The submenu only appears on folders, not individual password files.

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

### Where can I ask for help?

- Create an issue on [GitHub](https://github.com/IJHack/QtPass/issues).
- Send an email to [help@qtpass.org](mailto:help@qtpass.org)
- Visit the [Official QtPass site](https://qtpass.org/)

### Can I import from KeePass, LastPass or X?

- Yes, check [passwordstore.org/#migration](https://www.passwordstore.org/#migration)
  for more info.

### I don't see icons on the buttons

You do not have the Qt SVG library installed.
Please install using your favorite package manager.

### I get icons that do not fit my (X11) default

- On some WindowManagers, Qt doesn't know what icon set to use. A trick:

```sh
export DESKTOP_SESSION=gnome
```

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
- **macOS:** environment variables like `LANG` are **not** honored by Qt on macOS — Qt reads CoreFoundation locale instead. Override per-app with:

  ```sh
  defaults write com.IJHack.QtPass AppleLanguages '(fr)'
  ```

  Or pass the override as a launch argument:

  ```sh
  open -a QtPass --args -AppleLanguages '(fr)'
  ```

  Use `defaults delete com.IJHack.QtPass AppleLanguages` to revert.

- **Windows:** there is no environment-variable override — Qt reads the Windows display language via `GetUserDefaultLocaleName()`. Change the **system display language** in _Settings → Time & Language → Language_, or add the desired language and move it to the top of your preferred-language list.

## How can I help improve QtPass?

### I would like to donate

- Time:
  - Read [contributing](CONTRIBUTING.md) documentation.
  - Fork, clone, hack and send a pull request.
  - Find an [issue](https://github.com/IJHack/QtPass/issues) to work on.
  - Participate in our bug bounty, you submit an issue and help us
    fix it, I send you a bounty.
