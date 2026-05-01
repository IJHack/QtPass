# QtPass

[![GitHub release](https://img.shields.io/github/v/release/IJHack/QtPass?include_prereleases)](https://github.com/IJHack/QtPass/releases)
[![Packaging status](https://repology.org/badge/tiny-repos/qtpass.svg)](https://repology.org/metapackage/qtpass)
[![latest packaged version(s)](https://repology.org/badge/latest-versions/qtpass.svg)](https://repology.org/metapackage/qtpass)

[![QMake GitHub Action](https://github.com/IJHack/QtPass/actions/workflows/ccpp.yml/badge.svg?branch=main)](https://github.com/IJHack/QtPass/actions/workflows/ccpp.yml?query=branch%3Amain)
[![Build status](https://ci.appveyor.com/api/projects/status/9rjnj72rdir7u9eg/branch/main?svg=true)](https://ci.appveyor.com/project/annejan/qtpass/branch/main)
[![CodeFactor](https://www.codefactor.io/repository/github/ijhack/qtpass/badge)](https://www.codefactor.io/repository/github/ijhack/qtpass)

[![Coverity scan](https://scan.coverity.com/projects/5266/badge.svg)](https://scan.coverity.com/projects/ijhack-qtpass)
[![Coverage Status](https://coveralls.io/repos/github/IJHack/QtPass/badge.svg)](https://coveralls.io/github/IJHack/QtPass)
[![codecov](https://codecov.io/gh/IJHack/QtPass/branch/main/graph/badge.svg)](https://codecov.io/gh/IJHack/QtPass)

[![FOSSA Status](https://app.fossa.io/api/projects/git%2Bgithub.com%2FIJHack%2FQtPass.svg?type=shield)](https://app.fossa.com/projects/git%2Bgithub.com%2FIJHack%2FQtPass)
[![Translation status](https://hosted.weblate.org/widgets/qtpass/-/qtpass/svg-badge.svg)](https://hosted.weblate.org/engage/qtpass/?utm_source=widget)

QtPass is a multi-platform GUI for [pass](https://www.passwordstore.org/),
the standard Unix password manager, providing a secure and intuitive way to
manage your passwords.

_Available in over 60 languages_

## Features

- Using `pass` or `git` and `gpg2` directly
- Cross platform: Linux, BSD, macOS and Windows
- Native widgets and iconography where possible
- Per-folder user selection for multi-recipient encryption
- Multiple profiles support
- OTP (One-Time Password) support
- Password generation with configurable complexity
- Git integration for version control
- Smartcard and USB token support (OpenPGP, YubiKey)
- Configurable shoulder surfing protection
- Share submenu: re-encrypt folder, export your public key, add recipients
- Import GPG keys from file or clipboard without leaving the app
- Process output panel with command labels, color-coded errors, and auto-scroll
- Experimental WebDAV support
- Easy onboarding for new users

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

- QtPass requires Qt 5.15 or later (Qt 6 recommended; build with `qmake6`)
- The Linguist package is required to compile translations
- For fallback icons, the SVG library is required

Runtime dependencies:

- `gpg2` (GnuPG 2.2+) - required
- `git` - optional, for repository sync
- `pass` (1.7+) - optional, can use native GPG/Git

Your GPG must be configured with a graphical pinentry when applicable. Same goes for Git authentication.
On macOS, `pinentry-mac` from Homebrew works best (gpgtools also works).

On most Unix systems all you need is:

```sh
qmake6 && make && make install
```

### Windows

See [Windows.md](Windows.md) for Windows-specific installation and build instructions.

## Using profiles

Profiles allow grouping passwords. Each profile might use a different Git repository and/or different gpg key.
Each profile also can be associated with a GPG key to sign and verify the .gpg-id file for integrity.
A typical use case is to separate personal and work passwords.

> **Hint**<br>
> Instead of using different git repositories for the various profiles passwords could be synchronized with different
> branches from the same repository. Just clone the repository into the profile folders and checkout the related
> branch.

### Example

The following commands set up two profile folders:

```sh
cd ~/.password-store/
# Replace these with your own repositories (HTTPS or SSH).
PERSONAL_REPO_URL="<your-personal-password-repository-url>"
WORK_REPO_URL="<your-work-password-repository-url>"

# Examples:
# git clone https://git.example.com/you/qtpass-personal.git personal
# git clone git@git.example.com:you/qtpass-work.git work

git clone "${PERSONAL_REPO_URL}" personal && echo "personal/" >> .gitignore
git clone "${WORK_REPO_URL}" work && echo "work/" >> .gitignore
pass init -p personal [personal GnuPG-ID] && git -C personal push
pass init -p work [work GnuPG-ID] && git -C work push
```

**Note:**

- Replace `PERSONAL_REPO_URL` and `WORK_REPO_URL` with repositories you own and control.
- Replace `[personal GnuPG-ID]` and `[work GnuPG-ID]` with the ID from the related GnuPG key.
- The parts `echo ... >> .gitignore` are just needed in case there is a Git repository present in the base directory.

Once the repositories and GnuPG-ID's have been defined the profiles can be set up in QtPass.

### Links of interest

- [Git Tools - Credential Storage](https://git-scm.com/book/en/v2/Git-Tools-Credential-Storage)
- [Dealing with secrets](https://gist.github.com/maelvls/79d49740ce9208c26d6a1b10b0d95b5e)
- [Git Credential Manager](https://github.com/GitCredentialManager/git-credential-manager)
- [password-store](https://git.zx2c4.com/password-store/about/)

## Testing

This is done with `make check`

Codecoverage can be done with `make lcov`, `make gcov`, `make coveralls` and/or `make codecov`.

Be sure to first run: `make distclean && qmake6 CONFIG+=coverage qtpass.pro`

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

To get better protection when using a smartcard even against a targeted
attack I can think of at least two options:

- The smartcard must require explicit confirmation for each decryption operation.
  Or if it just provides a counter for decrypted data you could at least notice
  an attack afterwards, though at quite some effort on your part.
- Use a different smartcard for each (group of) key.
- If using a YubiKey or U2F module or similar that requires a "button" press for
  other authentication methods you can use one OTP/U2F enabled WebDAV account per
  password (or groups of passwords) as a quite inconvenient workaround.
  Unfortunately I do not know of any WebDAV service with OTP support except ownCloud
  (so you would have to run your own server).

## Known issues

- Filtering (searching) breaks the tree/model sometimes
- Starting without a correctly set password-store folder
  gives weird results in the tree view

## Planned features

- Plugins based on field name, plugins follow same format as password files
- Colour coding folders (possibly disabling folders you can't decrypt)
- Optional table view of decrypted folder contents
- Opening of (basic auth) URLs in default browser?
  Possibly with helper plugin for filling out forms?
- Some other form of remote storage that allows for
  accountability / auditing (web API to retrieve the .gpg files?)

## Further reading

[FAQ](FAQ.md) and [CONTRIBUTING](CONTRIBUTING.md) documentation.
[CHANGELOG](CHANGELOG.md)

[Official QtPass site](https://qtpass.org/)
[Source code](https://github.com/IJHack/QtPass)
[Issue queue](https://github.com/IJHack/QtPass/issues)

> **AI Assistance**<br>
> Parts of this project were developed with assistance from AI tools (such as [OpenCode](https://opencode.ai/)).
> AI-generated code is reviewed and tested before inclusion.

## License

This repository follows the [REUSE Specification v3.2](https://reuse.software/spec-3.2/).
Please see [LICENSES/](./LICENSES), [REUSE.toml](./REUSE.toml) and
the individual `*.license` files (if any) for copyright and license information.

### GNU General Public License v3.0

[![GNU GPL v3.0](https://www.gnu.org/graphics/gplv3-127x51.png)](https://www.gnu.org/licenses/gpl.html)

[View official GNU site](https://www.gnu.org/licenses/gpl.html)

[<img src="https://opensource.org/wp-content/uploads/2022/10/osi-badge-dark.svg" alt="OSI-approved license" width="127">](https://opensource.org/licenses/GPL-3.0)

[View the Open Source Initiative site](https://opensource.org/licenses/GPL-3.0)
