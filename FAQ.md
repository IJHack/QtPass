# FAQ

## Issues

### Can't save a password

* Is folder initialised? Easiest way is to use the [Users] button
  and make sure you can encrypt for someone (eg. yourself)
* Are you using git? If not, make sure it is switched off.

### I have an issue with GNOME keyring

* Disable GNOME keyring
* Create a `~/.gnupg/gpg-agent.conf` containing:

```
enable-ssh-support
write-env-file
use-standard-socket
default-cache-ttl 600
max-cache-ttl 7200
```

Also, the following is useful to add to
your .bashrc if you are using Yubikey NEO on Ubuntu:

```
# OpenPGP applet support for YubiKey NEO
if [ ! -f /tmp/gpg-agent.env ]; then
    killall gpg-agent;
        eval $(gpg-agent --daemon --enable-ssh-support > /tmp/gpg-agent.env);
fi
. /tmp/gpg-agent.env
```

* More info: [issue 60](https://github.com/IJHack/qtpass/issues/60) and [issue 73](https://github.com/IJHack/qtpass/issues/73)

### I don't get a passphrase / PIN dialog

* You'll need to install pinentry-qt (or -qt4 or -qt5 or even -gtk) and
  possibly set the full path to that executable in your `~/.gnupg/gpg-agent.conf`
  for example: `pinentry-program /usr/bin/pinentry-qt4`
* On some esotheric systems it might be necessary to create a symbolic
  link `/usr/bin/pinentry` to your pinentry application of choice
  eg: `ln -s /usr/bin/pinentry-qt5 /usr/bin/pinentry`

### I have an other issue with gpg

* Possibly you have you key only in gpg and not in gpg2

```
gpg --export [ID] > public.key
gpg --export-secret-key [ID] > private.key
gpg2 --import public.key
gpg2 --import private.key
rm public.key private.key
```
Where [ID] is your gpg key-id.
* It might be the case where it is the other way around, exchange gpg and gpg2 accordingly . .

### Git doesn't work on Windows

git for Windows comes with an `ssh-askpass` compatible command, git `gui--askpass` (located in `/mingw64/libexec/git-core/git-gui--askpass` on PortableGit version, presumably some place similar for the installed version).

### Git has issues with GPG SSH Authentication

This tutorial might resolve your issues.
https://github.com/git-for-windows/git/wiki/OpenSSH-Integration-with-Pageant

### Where is the configuration stored?

QtPass tries to use the native config choice for the OS it's running.

* Linux and BSD: `$HOME/.config/IJHack/QtPass.conf`
* macOS: `$HOME/Library/Preferences/com.IJHack.QtPass.plist`
* Windows registry: `HKEY_CURRENT_USER\Software\IJhack\QtPass`

These settings can be over-ruled by a `qtpass.ini` file in the folder where the application resides.
So called "portable config".

There are some things to take care of when trying to sync on some systems (especially OSX, with regards to text and binary .plist files).

More information: http://doc.qt.io/qt-5/qsettings.html#platform-specific-notes

### Where can I ask for help?

* Create an [issue](https://github.com/IJHack/qtpass/) issues on github.
* Send an email to [help@qtpass.org](help@qtpass.org)

### Can I import from KeePass, LastPass or X?

* Yes, check [passwordstore.org/#migration](https://www.passwordstore.org/#migration)
  for more info.

### I don't see icons on the buttons

You do not have the Qt SVG library installed.
Please install using your favorite package manager.

### I get icons that do not fit my (X11) default

* On some WindowManagers, Qt doesn't know what icon set to use. A trick:
```
export DESKTOP_SESSION=gnome
```

* Another possible reason is, that the currently installed Qt Version gives problems (e.g. on Linux Mint 17.3)
Then you'll have to install the current version via your package manager or if this is not up-to-date,
download it from https://www.qt.io/download/ install it and run:
```
/PATHTOYOURQTINSTALLATION/5.5/gcc_64/bin/qmake
make
(sudo) make install
```
where `PATHTOYOURINSTALLATION` is the path you selected in the qt installer (default `/home/YOURUSER/Qt/` )
and 5.5 has to be adapted for the Qt version you downloaded.

### I don't like the design, what gives?

* It's all on github, clone, change and send a pull request.
* Open an issue and point out defects or better yet propose changes.

### QtPass is not in my native language

* Unfortunately, QtPass might not support your native language, or the translations might be incomplete. Check if newer versions of QtPass support it.
* If translations are available but aren't working, try to set the language manually (see below) or open an issue.

### How do I set the language manually?

QtPass uses the system language. Changing it depends on your system:
* on Linux: ```LANGUAGE=fr qtpass``` will run QtPass in French.

## How can I help improve QtPass?

### I would like to donate!

* Time:
  * Read [contributing](CONTRIBUTING.md) documentation.
  * Fork, clone hack and send a pull request.
  * Find an [issue](https://github.com/IJHack/qtpass/issues) to work on..
  * Participate in our bug bounty, you submit an issue and help us
    fix it, I send you a bounty.
* Money:
  * IJhack takes donations in [Bitcoin](https://blockchain.info/address/146dqz8zXn9iNZMv5s7JVqwZKjrmumHBfb)
