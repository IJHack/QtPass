QtPass
======

[![Build Status](https://travis-ci.org/IJHack/qtpass.svg?branch=master)](https://travis-ci.org/IJHack/qtpass)
[![Build status](https://ci.appveyor.com/api/projects/status/9rjnj72rdir7u9eg/branch/master?svg=true)](https://ci.appveyor.com/project/annejan/qtpass/branch/master)
[![Coverity scan](https://scan.coverity.com/projects/5266/badge.svg)](https://scan.coverity.com/projects/ijhack-qtpass)

QtPass is a GUI for [pass](https://www.passwordstore.org/),
the standard unix password manager.

Features
--------

* Using pass or git and gpg2 directly
* Configurable shoulder surfing protection options
* Cross platform: Linux, BSD, OS X and Windows
* Per-folder user selection for multi recipient encryption
* Multiple profiles

Logo based on [Heart-padlock by AnonMoos](https://commons.wikimedia.org/wiki/File:Heart-padlock.svg).

Installation
------------

### Dependencies

* QtPass requires Qt 4.8 or later, preferably Qt5.
* The Linguist package is required to compile the translations.
* For use of the fallback icons the SVG library is required.

At runtime the only real dependency is `gpg2` but to make the most of it, you'll need `git` and `pass` too.

Your GPG has to be set-up with a graphical pinentry when applicable, same goes for git authentication.
On Mac OS X this currently seems to only work with MacGPG2 from gpgtools.

On most *nix systems all you need is:
```
qmake && make && make install
```

Security considerations
-----------------------

Using this program will not magically keep your passwords secure against
compromised computers even if you use it in combination with a smartcard.

It does protect future and changed passwords though against anyone with access to
your password store only but not your keys.
Used with a smartcard it also protects against anyone just monitoring/copying
all files/keystrokes on that machine and such an attacker would only gain access
to the passwords you actually use.
Once you plug in your smartcard and enter your PIN (or due to CVE-2015-3298
even without your PIN) all your passwords available to the machine can be
decrypted by it, if there is malicious software targeted specifically against
it installed (or at least one that knows how to use a smartcard).

To get better protection out of use with a smartcard even against a targeted
attack I can think of at least two options:

* The smartcard must require explicit confirmation for each decryption operation.
  Or if it just provides a counter for decrypted data you could at least notice
  an attack afterwards, though at quite some effort on your part.
* Use a different smartcard for each (group of) key.
* If using a YubiKey or U2F module or similar that requires a "button" press for
  other authentication methods you can use one OTP/U2F enabled WebDAV account per
  password (or groups of passwords) as a quite inconvenient workaround.
  Unfortunately I do not know of any WebDAV service with OTP support except ownCloud
  (so you would have to run your own server).

Known issues
------------

* Filtering (searching) breaks the tree/model sometimes
* Starting without a correctly set password-store folder
  gives weird results in the tree view
* On Mac OS X only the gpgtools MacGPG2 version works with passphrase or PIN

Planned features
----------------

* Re-encryption after users-change (optional of course)
* Plugins based on field name, plugins follow same format as password files
* Colour coding folders (possibly disabling folders you can't decrypt)
* WebDAV (configuration) support
* Optional table view of decrypted folder contents
* Opening of (basic auth) urls in default browser?
  Possibly with helper plugin for filling out forms?
* Some other form of remote storage that allows for
  accountability / auditing (web API to retrieve the .gpg files?)

Further reading
---------------

[FAQ](docs/FAQ.md) and [CONTRIBUTING](CONTRIBUTING.md) documentation.

[Documentation](https://qtpass.org/)

[Source code](https://github.com/IJHack/qtpass)
