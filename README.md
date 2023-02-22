# QtPass

[![latest packaged version(s)](https://repology.org/badge/latest-versions/qtpass.svg)](https://repology.org/metapackage/qtpass)
[![Build status](https://ci.appveyor.com/api/projects/status/9rjnj72rdir7u9eg/branch/master?svg=true)](https://ci.appveyor.com/project/annejan/qtpass/branch/master)
[![Coverity scan](https://scan.coverity.com/projects/5266/badge.svg)](https://scan.coverity.com/projects/ijhack-qtpass)
[![Coverage Status](https://coveralls.io/repos/github/IJHack/QtPass/badge.svg)](https://coveralls.io/github/IJHack/QtPass)
[![codecov](https://codecov.io/gh/IJhack/QtPass/branch/master/graph/badge.svg)](https://codecov.io/gh/IJhack/QtPass)
[![CodeFactor](https://www.codefactor.io/repository/github/ijhack/qtpass/badge)](https://www.codefactor.io/repository/github/ijhack/qtpass)
[![Packaging status](https://repology.org/badge/tiny-repos/qtpass.svg)](https://repology.org/metapackage/qtpass)
[![FOSSA Status](https://app.fossa.io/api/projects/git%2Bgithub.com%2FIJHack%2FQtPass.svg?type=shield)](https://app.fossa.io/projects/git%2Bgithub.com%2FIJHack%2FQtPass?ref=badge_shield)
[![Translation status](https://hosted.weblate.org/widgets/qtpass/-/qtpass/svg-badge.svg)](https://hosted.weblate.org/engage/qtpass/?utm_source=widget)
[![QMake Github Action](https://github.com/IJHack/QtPass/workflows/QMake/badge.svg)](https://github.com/IJHack/QtPass/actions)

QtPass is a GUI for [pass](https://www.passwordstore.org/),
the standard unix password manager.

## Features

* Using `pass` or `git` and `gpg2` directly
* Configurable shoulder surfing protection options
* Cross platform: Linux, BSD, macOS and Windows
* Per-folder user selection for multi recipient encryption
* Multiple profiles
* Easy onboarding

Logo based on [Heart-padlock by AnonMoos](https://commons.wikimedia.org/wiki/File:Heart-padlock.svg).

## Installation

### From package

OpenSUSE & Fedora
`yum install qtpass`
`dnf install qtpass`

Debian, Ubuntu and derivates like Mint, Kali & Raspbian
`apt-get install qtpass`

Arch Linux
`pacman -S qtpass`

Gentoo
`emerge -atv qtpass`

Sabayon
`equo install qtpass`

FreeBSD
`pkg install qtpass`

macOS
`brew install --cask qtpass`

Windows
`choco install qtpass`

[![Packaging status](https://repology.org/badge/vertical-allrepos/qtpass.svg)](https://repology.org/metapackage/qtpass)
[![Translation status](https://hosted.weblate.org/widgets/qtpass/-/multi-auto.svg)](https://hosted.weblate.org/engage/qtpass/?utm_source=widget)

### From Source

#### Dependencies

* QtPass requires Qt 5.10 or later (Qt 6 works too)
* The Linguist package is required to compile the translations.
* For use of the fallback icons the SVG library is required.

At runtime the only real dependency is `gpg2` but to make the most of it, you'll need `git` and `pass` too.

Your GPG has to be set-up with a graphical pinentry when applicable, same goes for git authentication.
On Mac macOS this currently seems to only work best with `pinentry-mac` from homebrew, although gpgtools works too.

On most unix systems all you need is:

```sh
qmake && make && make install
```

## Using profiles

Profiles allow to group passwords. Each profile might use a different git repository and/or different gpg key.
A typical use case is to separate personal and work passwords.

> **Hint**<br>
> Instead of using different git repositories for the various profiles passwords could be synchronized with different
> branches from the same repository. Just clone the repository into the profile folders and checkout the related
> branch.

### Example

The following commands set up two profile folders:

```sh
cd ~/.password-store/
git clone https://github.com/vendor/personal-passwords personal && echo "personal/" >> .gitignore
git clone https://github.com/company/group-passwords work && echo "work/" >> .gitignore
pass init -p personal [personal GnuPG-ID] && git -C personal push
pass init -p work [work GnuPG-ID] && git -C work push
```

**Note:**

* Replace `[personal GnuPG-ID]` and `[work GnuPG-ID]` with the ID from the related GnuPG key.
* The parts `echo ... >> .gitignore` are just needed in case there is a git repository present in the base directory.

Once the repositories and GnuPG-ID's have been defined the profiles can be set up in QtPass.

### Links of interest

* [Git Tools - Credential Storage](https://git-scm.com/book/en/v2/Git-Tools-Credential-Storage)
* [Dealing with secrets](https://gist.github.com/maelvls/79d49740ce9208c26d6a1b10b0d95b5e)
* [Git Credential Manager](https://github.com/GitCredentialManager/git-credential-manager)
* [password-store](https://git.zx2c4.com/password-store/about/)

## Testing

This is done with `make check`

Codecoverage can be done with `make lcov`, `make gcov`, `make coveralls` and/or `make codecov`.

Be sure to first run: `make distclean && qmake CONFIG+=coverage qtpass.pro`

## Security considerations

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

## Known issues

* Filtering (searching) breaks the tree/model sometimes
* Starting without a correctly set password-store folder
  gives weird results in the tree view

## Planned features

* Plugins based on field name, plugins follow same format as password files
* Colour coding folders (possibly disabling folders you can't decrypt)
* Optional table view of decrypted folder contents
* Opening of (basic auth) URLs in default browser?
  Possibly with helper plugin for filling out forms?
* WebDAV (configuration) support
* Some other form of remote storage that allows for
  accountability / auditing (web API to retrieve the .gpg files?)

## Further reading

[FAQ](FAQ.md) and [CONTRIBUTING](CONTRIBUTING.md) documentation.
[CHANGELOG](CHANGELOG.md)

[Site](https://qtpass.org/)
[Source code](https://github.com/IJHack/qtpass)
[Issue queue](https://github.com/IJHack/qtpass/issues)
[Chat](https://gitter.im/IJHack/qtpass)

## License

### GNU GPL v3.0

[![GNU GPL v3.0](http://www.gnu.org/graphics/gplv3-127x51.png)](http://www.gnu.org/licenses/gpl.html)

View official GNU site <http://www.gnu.org/licenses/gpl.html>.

[![OSI](http://opensource.org/trademarks/opensource/OSI-Approved-License-100x137.png)](https://opensource.org/licenses/GPL-3.0)

[View the Open Source Initiative site.](https://opensource.org/licenses/GPL-3.0)
