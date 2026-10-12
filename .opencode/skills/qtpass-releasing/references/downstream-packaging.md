# Downstream packaging

Per-venue steps for a new QtPass release. Submit on release day, or open a
tracking issue, before announcing.

## FreeBSD `sysutils/qtpass` (ours)

- Where: [Bugzilla](https://bugs.freebsd.org/bugzilla/enter_bug.cgi?product=Ports%20%26%20Packages),
  component "Individual Port(s)", summary `sysutils/qtpass: Update to X.Y.Z`.
- How: bump `DISTVERSION`, run `make makesum`, attach the diff with the
  `maintainer-approval+` flag (we are the maintainer, so no approval wait).
  The port lists the desktop file, metainfo and hicolor icons that
  `make install` produces; 2.0 adds `share/man/man1/qtpass.1.gz`. The port
  uses `USE_GITHUB`, so its checksum is of GitHub's codeload archive, not of
  our release asset.
- Test first in [annejan/qtpass-freebsd-port-test](https://github.com/annejan/qtpass-freebsd-port-test):
  commit the diff as `patches/qtpass-X.Y.Z.patch` and push, or dispatch
  _sysutils/qtpass port test_ with the patch path and a ports branch (`main`,
  `2026Q3`, ...). Leave `run_suite` at `true` so QtPass's `make check` runs.
  - Job 1 (FreeBSD 14.5 and 15.1 amd64, 14.5 aarch64): `portlint -AC`,
    `make checksum` (`makesum` must reproduce `distinfo`), `stage`,
    `check-plist`, `stage-qa`, `package` + `pkg add`, `ldd`, `make check`
    offscreen.
  - Job 2: `poudriere testport` in clean jails (14.5, 15.1, native i386).
    This is the one that catches a missing `*_DEPENDS`, since job 1
    pre-installs dependencies.
- **Verify:** all jobs green, then attach the patch to Bugzilla. The test
  repository's readme records which patch became which commit.
- Watch: [portscout](https://portscout.freebsd.org/brouwer@annejan.com.html),
  [FreshPorts](https://www.freshports.org/sysutils/qtpass/).
- [Maintainer rules](https://docs.freebsd.org/en/books/porters-handbook/makefiles/#makefile-maintainer):
  a ticket on our port unanswered for two weeks is a maintainer timeout and
  goes in without approval; three months of silence, or three timeouts in a
  row, and our ports go back to `ports@FreeBSD.org`.

## OpenBSD `security/qtpass` (not ours)

Maintained by Stefan Hagen. The FreeBSD test bed has a third job: a diff
against ports `-current` in `patches/openbsd/` runs through `portcheck`,
`makesum`, `build`, `fake`, `update-plist`, `port-lib-depends-check`,
`package` and `install` in an OpenBSD VM. Send the tested diff to
`ports@openbsd.org` with the maintainer in Cc; do not expect to commit it.

## MacPorts `aqua/QtPass` (ours)

- Where: PR to [macports/macports-ports](https://github.com/macports/macports-ports),
  title `QtPass: update to X.Y.Z`.
- How: `github.setup ... X.Y.Z v`, `revision 0`, new `checksums` (`rmd160`,
  `sha256`, `size`) of our `QtPass-X.Y.Z.tar.gz` release asset (the port
  fetches from releases).
- AI-assisted commits carry `Assisted-by:`, not `Co-Authored-By:`.

## winget `IJHack.QtPass` (ours)

- Where: PR to [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs),
  branched from upstream `master`.
- How: new `manifests/i/IJHack/QtPass/X.Y.Z/` (version, installer, locale),
  copied from the previous version. Installer: `x64` only,
  `InstallerType: inno`, `InstallerUrl` of `qtpass-X.Y.Z.exe`, its `InstallerSha256`,
  `ProductCode: '{C64A2871-0C42-4A90-9071-D84DC30563BF}_is1'` (the `AppId` in
  `qtpass.iss` plus `_is1`), `ReleaseDate`.
- **Verify:** `winget validate --manifest manifests/i/IJHack/QtPass/X.Y.Z`
  (Windows only; say so in the PR when it could not be run).

## Flathub `org.qtpass.QtPass` (ours)

- Where: PR to the Flathub app repository.
- How: bump the `qtpass` module `tag` and `commit` in the manifest (this
  repository's `flatpak/org.qtpass.QtPass.yml` mirrors it), make sure the
  `<release>` is in `org.qtpass.QtPass.metainfo.xml`, run
  `flatpak-builder-lint`. Flathub ships the stable line (currently 1.8.x).
- Flathub's [generative-AI policy](https://docs.flathub.org/docs/for-app-authors/requirements#generative-ai-policy):
  AI-generated code, packaging or metadata must be disclosed and is accepted
  at reviewer discretion. AI tools must not open or automate the PR, or write
  its commit messages, description, review comments or replies. Prepare the
  change; the maintainer commits and writes the PR by hand.

## Debian `qtpass` (not ours, and everything downstream of it)

- Maintained by Philip Rinn, [salsa.debian.org/debian/qtpass](https://salsa.debian.org/debian/qtpass).
- Coordinate here, because the rest follows Debian:
  - Ubuntu syncs from Debian unstable only until DebianImportFreeze; after
    that it needs a sync request or manual upload (and after FeatureFreeze an
    exception too).
  - Linux Mint main editions inherit Ubuntu; LMDE follows Debian stable.
  - One upload to Debian unstable reaches all of them eventually; an
    Ubuntu-only fix reaches neither Debian nor LMDE.
- `debian/watch` scrapes our releases for `QtPass-<version>.tar.gz` plus its
  `.asc`, so a signed release is enough for `uscan`.
- If a release sits unpackaged, file a wishlist bug (`reportbug qtpass`, or
  mail `submit@bugs.debian.org`):

  ```text
  Package: qtpass
  Version: <version currently in unstable>
  Severity: wishlist

  <upstream release X.Y.Z, its date, and the releases skipped since>
  <security-relevant fixes, one bullet each, with what they mean for a
   Debian user (the argument for stable-updates or backports)>
  <changelog and release URLs>
  <packaging notes: new build dependencies, files the build now installs
   itself, which of their patches became obsolete>
  ```

  Keep it factual, leave the stable-update call to the maintainer, offer
  upstream help. #1682 has the packaging notes behind the 1.8.1 report.

- Keep their delta small: `02-make-reproducible.patch` went upstream in
  #1757; `01-disable-tests.patch` and `03-fix-gpg-detection.patch` are
  obsolete as of 1.8.x.

## Chocolatey `qtpass` (not ours)

Community maintained; nothing to do unless the maintainer asks.

## Homebrew cask (disabled)

Homebrew disabled the `qtpass` cask on 2026-09-01 because the dmg is not
notarized (#1542). Nothing to bump until a notarized dmg ships.
