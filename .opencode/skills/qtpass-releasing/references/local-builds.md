# Local release builds

CI builds every release asset (see `.github/workflows/release-installers.yml`).
Use these to reproduce a packaging problem, or to produce a signed and
notarized dmg, which CI does not.

## Linux AppImage

```bash
./scripts/build-appimage.sh [output-directory]   # -> dist/QtPass-X.Y.Z-x86_64.AppImage
```

Needs Qt 6.8+ (`qmake6`) and network access for `linuxdeploy` and its Qt
plugin. `VERSION` overrides the version read from `qtpass.pri`.

**Verify:** the AppImage starts and stays running (CI smoke-tests it on X11
and Wayland).

`scripts/release-linux.sh` is not a packaging script: it builds and runs
`sudo make install`. Source archives come from `git archive`, as in the
`publish` job:

```bash
git archive --prefix=qtpass-X.Y.Z/ -o QtPass-X.Y.Z.tar.gz vX.Y.Z
```

## macOS dmg

```bash
brew install create-dmg   # once; the same tool the workflow uses
./scripts/release-mac.sh  # -> QtPass-X.Y.Z.dmg (unsigned)
```

To sign and notarize you need a paid Apple Developer Program membership, a
**Developer ID Application** certificate in the login keychain (the script
refuses any other type, such as Apple Development) and notarization
credentials saved once:

```bash
xcrun notarytool store-credentials qtpass-notary --apple-id you@example.com --team-id TEAMID
MAC_SIGN_IDENTITY="Developer ID Application: Name (TEAMID)" \
  MAC_NOTARY_PROFILE=qtpass-notary ./scripts/release-mac.sh
```

The script checks both before building, signs the bundle with the hardened
runtime (`macdeployqt -sign-for-notarization`), signs the dmg, submits it with
`notarytool --wait` and staples the ticket. With only `MAC_SIGN_IDENTITY` set
it signs without notarizing.

**Verify:** when notarizing, the script ends by running `spctl` on the dmg
and the app inside it; both must be accepted. On a rejected notarization it
prints the `notarytool log` command that shows why.

## Windows installer

Release installers come from CI, where SignPath signs `qtpass.exe` and the
installer. A local build is unsigned. From a plain `cmd.exe`
(`scripts\build-windows.cmd` loads MSVC x64 and picks a Qt; see `Windows.md`):

```bat
scripts\build-windows.cmd
mkdir dist\qtpass
copy main\release\qtpass.exe dist\qtpass\   & rem or src\release\qtpass.exe
copy README.md dist\qtpass\README.txt
copy LICENSE dist\qtpass\LICENSE.txt
windeployqt --release --dir dist\qtpass dist\qtpass\qtpass.exe
iscc /DMyAppVersion=X.Y.Z /DMyAppNumericVersion=X.Y.Z qtpass.iss
```

**Verify:** `Output\qtpass-X.Y.Z.exe` exists and installs. For an rc,
`MyAppVersion` is `X.Y.Z-rcN` and `MyAppNumericVersion` stays `X.Y.Z`.
