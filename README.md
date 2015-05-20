QtPass [![Build Status](https://travis-ci.org/IJHack/qtpass.svg?branch=master)](https://travis-ci.org/IJHack/qtpass)
======

QtPass is a GUI for [pass](http://www.passwordstore.org/), the standard unix password manager.


Features
--------
* Using pass or git and gpg2 directly
* Configurable shoulder surfing protection options
* Cross platform: Linux, BSD, OS X and Windows
* Per-folder user selection for multi recipient encryption

While QtPass will work with Qt4, currently multi-line editing is restricted to Qt5 only.

QtPass requires GCC 4.7 or later or any other 2011 ISO C++ standard compliant compiler.

Logo based on https://commons.wikimedia.org/wiki/File:Heart-padlock.svg by AnonMoos.

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
* Starting without a correctly set password-store folder give weird results in the tree view
* On Mac OS X only the gpgtools MacGPG2 version works with passphrase or PIN

Planned features
----------------
* Showing path in Add and Edit screen (currently sometimes confusing where I'm adding this password)
* Right click handlers for file/folder and content
* First use wizards to set up password-store (and decryption key, currently always the gpg default key)
* Profiles (to allow use of multiple password stores and decryption keys) with dropdown in main screen
* Password generation with options for what kind you'd like
* Templates (username, url etc) in Add / Edit screen (configurable templates)
* Colour coding or disabling of people you can't encrypt for (trust settings) in User management
* Colour coding folders (possibly disabling folders you can't decrypt)
* WebDAV (configuration) support
* Optional table view of decrypted folder contents
* Opening of (basic auth) urls in default browser? Possibly with helper plugin for filling out forms?
* Some other form of remote storage that allows for accountability / auditing (web API to retreive the .gpg files?)

Installation
------------
On most systems all you need is:
`qmake && make && make install`

On MacOsX:
`qmake && make && macdeployqt QtPass.app`
* Currently seems to only work with MacGPG2

On some systems there are issues with qt4 and qt5 being installed at the same time.
An easy fix is regenerating the Makefile with: `make clean && rm Makefile && qmake -qt5` or if qmake is ambiguous: `qmake-qt5`

Further reading
---------------
[Documentation](http://qtpass.org/)

[Source code](https://github.com/IJHack/qtpass)
