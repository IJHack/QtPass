# Roadmap

Where QtPass is heading, as of October 2026. Dates are targets, not
promises; the linked issues hold the current state. Ideas and requests are
welcome in the [issue tracker](https://github.com/IJHack/QtPass/issues).

## Now: QtPass 2.0

The 2.0 release candidates are out for testing
([2.0.0-rc2](https://github.com/IJHack/QtPass/releases/tag/v2.0.0-rc2)).
2.0 needs Qt 6.8 or newer and brings a first-run wizard, a redesigned
settings dialog, a single dialog to add and edit entries, editable field
names, profiles with a list and form editor, built-in TOTP, a Linux AppImage
and much hardening of how QtPass reads a shared store. The
[CHANGELOG](CHANGELOG.md) has the full list.

Left before the `v2.0.0` tag, tracked in
[#1812](https://github.com/IJHack/QtPass/issues/1812):

- Manual smoke tests of the release candidate on Linux (X11 and Wayland),
  Windows and macOS
- The string freeze on [Weblate](https://hosted.weblate.org/projects/qtpass/qtpass/)
  running out, and the translations that come in before it
- Release notes, AppStream metadata and packaging files for 2.0.0
- Downstream packages after the tag (Flathub, winget, MacPorts, FreeBSD,
  Debian, nixpkgs)

## Next: 2.x

- **Signed Windows releases:** code signing through the SignPath Foundation
  is set up; the production certificate follows their review
  ([#1643](https://github.com/IJHack/QtPass/issues/1643),
  [code signing policy](https://qtpass.org/code-signing)).
- **Flathub:** QtPass as a Flatpak on Flathub
  ([#637](https://github.com/IJHack/QtPass/issues/637)).
- **CMake:** a `CMakeLists.txt` next to the qmake files, with CI building
  both, then the build scripts and downstream packages moved over during 2.x.
- **OpenSSF Best Practices silver level**
  ([#1989](https://github.com/IJHack/QtPass/issues/1989)) and human review on
  pull requests ([#1990](https://github.com/IJHack/QtPass/issues/1990)).

## Later

- **Drop qmake** once CMake has carried a release and the downstream packages
  have switched (2.1 or 3.0).
- **Signed and notarized macOS builds**, when the project has an Apple
  Developer ID; until then see the [macOS page](https://qtpass.org/macos)
  ([#1542](https://github.com/IJHack/QtPass/issues/1542)).

## Ideas

Older plans that nobody is working on. Open an issue if you want to pick one
up.

- Plugins based on field name, in the same format as password files
- Colour coding folders, possibly greying out folders you cannot decrypt
- An optional table view of a decrypted folder's contents
- Filling out web forms from an entry
- A form of remote storage that allows accountability or auditing, such as a
  web API to retrieve the `.gpg` files

## QtPass 1.8

The `1.8` branch is the line for Qt 5 and Qt older than 6.8. It gets bugfix
and security releases (1.8.3 is the latest), no new features.
