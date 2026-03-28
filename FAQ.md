# FAQ

## Issues

### Can't save a password

- Is folder initialized? Easiest way is to use the [Users] button
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

- You'll need to install a pinentry package (often called `pinentry-qt`, `pinentry-qt5`, or a similar name like `pinentry-gtk` depending on your distribution; search in your package manager) and
  possibly set the full path to that executable in your `~/.gnupg/gpg-agent.conf`
  for example: `pinentry-program /usr/bin/pinentry-qt`
- On some systems it might be necessary to create a symbolic
  link `/usr/bin/pinentry` to your pinentry application of choice
  eg: `ln -s /usr/bin/pinentry-qt /usr/bin/pinentry`
- On macOS `pinentry-program /usr/local/bin/pinentry-mac` works after installing `pinentry-mac` from Homebrew.

### I have another issue with gpg

- Possibly you have your key only in gpg and not in gpg2

```bash
gpg --export [ID] > public.key
gpg --export-secret-key [ID] > private.key
gpg2 --import public.key
gpg2 --import private.key
rm public.key private.key
```

Where [ID] is your gpg key-id.

- It might be the case where it is the other way around, exchange gpg and gpg2 accordingly.

### Git doesn't work on Windows

Git for Windows comes with an `ssh-askpass` compatible command, `git-gui--askpass` (located in `/mingw64/libexec/git-core/git-gui--askpass` on PortableGit version, presumably some place similar for the installed version).

### Git has issues with GPG SSH Authentication

This tutorial might resolve your issues.
<https://github.com/git-for-windows/git/wiki/OpenSSH-Integration-with-Pageant>

### GPG says "Public key unusable" or "No secret key"

Your GPG key may not be trusted or has expired. Run `gpg --edit-key <KEYID>` and:

- Set the trust level to "ultimate" (`trust` command)
- Check if the key has expired (`expire` command to extend if needed)
- For smartcards/YubiKeys, ensure the card is connected and unlocked (you can verify with `gpg --card-status`)

### What is the "Signing Key" in profile settings?

The Signing Key field in profile configuration is **optional** and usually should be **left empty**.

It is only needed if you want to create **detached signatures** for your `.gpg-id` files to verify the recipients list hasn't been tampered with. Most users can ignore this field.

Common mistake: Putting your GPG key ID here will cause "Signature does not exist" errors when saving passwords. Leave it empty unless you specifically need signature verification.

### OTP QR codes don't work on macOS

Applications launched from Finder don't inherit the shell PATH. Install QtPass via Homebrew (which sets up PATH correctly) or create a wrapper script that sets PATH before launching QtPass.

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

More information: <https://doc.qt.io/qt-5/qsettings.html#platform-specific-notes>

### Where can I ask for help?

- Create an issue on [GitHub](https://github.com/IJHack/QtPass/issues).
- Send an email to [help@qtpass.org](mailto:help@qtpass.org)

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

Then you'll have to install the current version via your package manager or download it from <https://www.qt.io/download/> and build from source.

### I don't like the design, what gives?

- It's all on GitHub, clone, change and send a pull request.
- Open an issue and point out defects or better yet propose changes.

### QtPass is not in my native language

- Unfortunately, QtPass might not support your native language, or the translations might be incomplete. Check if newer versions of QtPass support it.
- If translations are available but aren't working, try to set the language manually (see below) or open an issue.

### How do I set the language manually?

QtPass uses the system language. Changing it depends on your system:

- on Linux: `LANGUAGE=fr qtpass` will run QtPass in French.

## How can I help improve QtPass?

### I would like to donate

- Time:
  - Read [contributing](CONTRIBUTING.md) documentation.
  - Fork, clone, hack and send a pull request.
  - Find an [issue](https://github.com/IJHack/QtPass/issues) to work on.
  - Participate in our bug bounty, you submit an issue and help us
    fix it, I send you a bounty.
