QtPass
======

QtPass is a GUI for [pass](http://www.passwordstore.org/)

Features
--------
* Using pass or git and gpg2 directly
* Configurable shoulder surfing protection options
* Cross platform: Linux, BSD, OS X and Windows
* Per-folder user selection for multi recipient encryption

While QtPass will work with Qt4, currently multi-line editing is restricted to Qt5 only.

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

Planned features
----------------
* WebDAV support (configuration)
* Templates (username, url etc)
* First use wizards to set up password-store
* Colour coding folders (possibly disabling folders you can't decrypt)
* Optional table view of decrypted folder contents

Installation
------------
On most systems all you need is:
`qmake && make && make install`

On MacOsX:
`qmake && make && macdeployqt QtPass.app -dmg`

Currently seems to only work with MacGPG2

Further reading
---------------
[Documentation](http://qtpass.org/)

[Source code](https://github.com/IJHack/qtpass)
