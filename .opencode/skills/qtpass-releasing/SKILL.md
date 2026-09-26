---
name: qtpass-releasing
description: Release workflow for QtPass - versioning, builds, publishing
license: GPL-3.0-or-later
metadata:
  audience: developers
  workflow: release
---

# Release Workflow for QtPass

## Release Checklist

### 1. Version Bump

Update version in all build files:

- `qtpass.pri` - `VERSION = X.Y.Z` (note: .pri not .pro, unquoted number).
  This is the single source: the release workflow reads it for the installer
  names and fails if it cannot, so nothing else needs a fallback.
- `qtpass.spec` - `Version:` (and reset `Release:` to 1, add a `%changelog`
  entry)
- `qtpass.iss` - `#define MyAppVersion`
- `Doxyfile` - `PROJECT_NUMBER`
- `org.qtpass.QtPass.metainfo.xml` - a new `<release version="X.Y.Z" date="...">` entry
- `publiccode.yml` - `softwareVersion` and `releaseDate`
- `SECURITY.md` - the supported-versions table
- `downloads.html` (gh-pages) - multiple references
- `index.html` (gh-pages)
- `getting-started.html` (gh-pages)
- `changelog.html` (gh-pages)
- `changelog.1.4.html` (gh-pages)
- `old.html` (gh-pages)

```bash
# Find version strings (replace X.Y with actual version)
grep -rn "X\.Y" qtpass.pri qtpass.spec qtpass.iss Doxyfile
```

### 2. Changelog

Update `CHANGELOG.md`:

- Add release date
- List all merged PRs and fixed issues
- Group by: Added, Changed, Fixed, Removed

### 3. Tests

```bash
# Run full test suite
make check
```

### 4. Build Artifacts

#### Linux

```bash
# Create source tarball
./scripts/release-linux.sh

# Or manually:
git archive --prefix=qtpass-x.y.z/ -o qtpass-x.y.z.tar.gz HEAD

# AppImage (CI builds it too; needs Qt 6.8+ and network for linuxdeploy)
./scripts/build-appimage.sh   # -> dist/QtPass-x.y.z-x86_64.AppImage
```

#### macOS

```bash
brew install create-dmg   # once; the same tool release-installers.yml uses
./scripts/release-mac.sh  # -> QtPass-x.y.z.dmg (unsigned)
```

To sign and notarize, set two variables. You need a paid Apple Developer
Program membership, a **Developer ID Application** certificate in the login
keychain (a Personal Team's Apple Development certificate is not accepted), and
notarization credentials saved once:

```bash
xcrun notarytool store-credentials qtpass-notary --apple-id you@example.com --team-id TEAMID
MAC_SIGN_IDENTITY="Developer ID Application: Name (TEAMID)" \
  MAC_NOTARY_PROFILE=qtpass-notary ./scripts/release-mac.sh
```

The script checks both before building, signs the bundle with the hardened
runtime (`macdeployqt -sign-for-notarization`), signs the dmg, submits it with
`notarytool --wait`, staples the ticket and confirms with `spctl`. If you set
only `MAC_SIGN_IDENTITY`, it signs without notarizing. If notarization is
rejected, the script prints the `notarytool log` command that shows why.

#### Windows

```bash
# Via GitHub Actions or locally with Inno Setup
qtpass.iss
```

### 5. Git Tags

```bash
git tag -s vX.Y.Z -m "QtPass vX.Y.Z"
git push origin vX.Y.Z
```

### 6. GitHub Release

Pushing the tag runs `release-installers.yml`. Its `publish` job builds the
Windows installer, the macOS dmg and the Linux AppImage, adds
`QtPass-x.y.z.tar.gz` / `.zip` source archives, creates the release **as a
draft** if none exists yet and attaches all five. Re-running the workflow replaces the assets of a _draft_;
on a published release it only adds missing ones, so signed assets are never
swapped under their `.asc`. Then:

```bash
gh run watch                                   # or wait for the Release installers run
gh release edit vX.Y.Z --notes-file <(sed -n '/^## \[X.Y.Z\]/,/^## \[/p' CHANGELOG.md | sed '$d')
./scripts/sign-release-assets.sh vX.Y.Z        # .asc for every asset (maintainer's key)
gh release edit vX.Y.Z --draft=false
```

If a release for the tag already exists (created by hand), the job only
uploads into it. Nothing needs downloading from the Actions artifacts page
any more.

### 7. GitHub Pages (site)

Update version in HTML files on `gh-pages` branch:

```bash
git checkout gh-pages

# Update downloads.html (download links)
# Update index.html (main page)
# Update getting-started.html
# Update changelog.html (add new release notes + version)
# Update changelog.1.4.html, old.html (version only)

git add -A
git commit -m "Release vX.Y.Z"
git push origin gh-pages
```

**Common version search:**

```bash
grep -rn "1\.5" *.html
```

### 8. Downstream packages (same day as the release)

QtPass is upstream _and_ maintainer of several downstream packages. Nobody
else bumps them, so a release that is not submitted downstream on release
day tends to stay unsubmitted (the FreeBSD port sat on 1.4.0 from 2023 to
2026). Submit each update, or open a tracking issue for it, before
announcing the release:

#### FreeBSD `sysutils/qtpass` (and OpenBSD `security/qtpass`)

- Where: [Bugzilla](https://bugs.freebsd.org/bugzilla/enter_bug.cgi?product=Ports%20%26%20Packages),
  component "Individual Port(s)", summary `sysutils/qtpass: Update to X.Y.Z`.
- How: bump `DISTVERSION`, run `make makesum`, attach the diff with the
  `maintainer-approval+` flag (we are the maintainer, so no approval wait).
  Since 1.8.1 the port lists the desktop file, metainfo and hicolor icons
  that `make install` produces; 2.0 adds `share/man/man1/qtpass.1.gz`.
- Test before filing, in the port test bed:
  [annejan/qtpass-freebsd-port-test](https://github.com/annejan/qtpass-freebsd-port-test).
  Commit the diff as `patches/qtpass-X.Y.Z.patch` and push, or dispatch
  _sysutils/qtpass port test_ by hand with the patch path and a ports branch
  (`main`, `2026Q3`, ...); leave `run_suite` at its default of `true`, the
  `make check` step below only runs with it. It applies the patch to a fresh
  ports tree in a FreeBSD VM and runs what a committer runs: `portlint -AC`, `make checksum`
  (and `makesum` must reproduce `distinfo`), `stage`, `check-plist`,
  `stage-qa`, `package` + `pkg add`, `ldd`, and QtPass's own `make check`
  offscreen — on 14.5 amd64, 15.1 amd64 and 14.5 aarch64. A second job runs
  `poudriere testport` in clean jails (14.5, 15.1, native i386), which is the
  one that catches a missing `*_DEPENDS`: the first job pre-installs the
  dependencies with `pkg` and would not notice. Wait for green on all of it,
  then attach the patch to Bugzilla; the repository's readme tracks which
  patch became which commit.
- OpenBSD is not ours (maintainer Stefan Hagen, `security/qtpass`), but the
  same test bed has a third job for it: a diff against ports `-current` in
  `patches/openbsd/` is run through `portcheck`, `makesum`, `build`, `fake`,
  `update-plist`, `port-lib-depends-check`, `package` and `install` on an
  OpenBSD VM. Send a tested diff to `ports@openbsd.org` with the maintainer
  in Cc; do not expect to commit it.

#### MacPorts `aqua/QtPass`

- Where: PR to [macports/macports-ports](https://github.com/macports/macports-ports),
  title `QtPass: update to X.Y.Z`.
- How: `github.setup … X.Y.Z v`, `revision 0`, new `checksums`
  (`rmd160`, `sha256`, `size` of the GitHub release tarball).

#### winget `IJHack.QtPass`

- Where: PR to [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs).
- How: new `manifests/i/IJHack/QtPass/X.Y.Z/` (version, installer, locale);
  the installer is `x64` (the port is 64-bit only), `InstallerSha256` of
  `qtpass-X.Y.Z.exe`, `ProductCode` from `qtpass.iss`.
- Run `winget validate --manifest manifests/i/IJHack/QtPass/X.Y.Z` first.

#### Flathub `org.qtpass.QtPass`

- Where: PR to the Flathub app repository.
- How: bump the `qtpass` module tag/commit in `flatpak/org.qtpass.QtPass.yml`,
  add a `<release>` to `org.qtpass.QtPass.metainfo.xml`, run
  `flatpak-builder-lint`.
- Flathub's [generative-AI policy](https://docs.flathub.org/docs/for-app-authors/requirements#generative-ai-policy):
  AI-generated code, packaging or metadata must be disclosed (which parts,
  how much) and is accepted at reviewer discretion; AI tools must not open
  or automate the submission PR, or write its commit messages, description,
  review comments or replies, and no AI-agent reviews may be requested.
  Open and write the PR by hand.

#### Debian `qtpass` (and everything downstream of it)

- Not ours: maintained by Philip Rinn <rinni@debian.org>,
  [salsa.debian.org/debian/qtpass](https://salsa.debian.org/debian/qtpass).
- **Debian is where to coordinate**, because everything else follows it:
  - Ubuntu auto-syncs from Debian unstable only while its merge window is
    open (until DebianImportFreeze). After that a new version needs an
    explicit sync request (`requestsync`) or a manual upload by an Ubuntu
    developer, and after FeatureFreeze a Feature Freeze Exception as well.
    Ubuntu also carries its own delta when it wants to — `1.4.0-3ubuntu1`
    added a `Provides: pinentry` line — so an upload there is possible, just
    more work and to be repeated every cycle.
  - Linux Mint's main editions are rebuilt from Ubuntu, so they inherit
    whatever Ubuntu shipped. **LMDE** is built from Debian stable instead and
    follows Debian directly.
  - Net effect: one upload to Debian unstable eventually reaches all of them;
    an Ubuntu-only fix reaches neither Debian nor LMDE.
- `debian/watch` scrapes our GitHub releases for `QtPass-<version>.tar.gz`
  plus the detached `.asc`, so a signed release is enough for `uscan` to see
  it. Signing the release assets is therefore not optional.
- If a release sits unpackaged for a while, file a wishlist bug against the
  package (`reportbug qtpass`, or mail `submit@bugs.debian.org`). Skeleton:

  ```text
  Package: qtpass
  Version: <version currently in unstable>
  Severity: wishlist

  <upstream release X.Y.Z, its date, and the releases skipped since>
  <the security-relevant fixes, one bullet each, with what they mean for a
   Debian user — this is also the argument for stable-updates or backports>
  <changelog and release URLs>
  <packaging notes: new build dependencies, files the build now installs
   itself, which of their patches became obsolete>
  ```

  Keep it factual, leave the stable-update judgement to the maintainer, and
  offer upstream help. See #1682 for the packaging notes behind the 1.8.1
  report.

- Keep the Debian delta small: patches they carry are patches to refresh on
  every upload. `02-make-reproducible.patch` went upstream in #1757;
  `01-disable-tests.patch` and `03-fix-gpg-detection.patch` are obsolete as of
  1.8.x (tests run with `--platform offscreen`, and the gpg probe falls back
  from `gpg2` to `gpg`).

#### Chocolatey `qtpass`

- Not ours (community maintained); nothing to do unless the maintainer asks.

Watch lists: [portscout](https://portscout.freebsd.org/brouwer@annejan.com.html)
and [FreshPorts](https://www.freshports.org/sysutils/qtpass/) for FreeBSD.
Maintainer rules from the
[Porter's Handbook](https://docs.freebsd.org/en/books/porters-handbook/makefiles/#makefile-maintainer):
a Bugzilla ticket on our port unanswered for two weeks (excluding major
public holidays) is a maintainer timeout and the change goes in without
approval; no response for three months, or three consecutive timeouts, and
all our ports are reassigned to the pool (`ports@FreeBSD.org`).

## Version Numbering

Follow semantic versioning: MAJOR.MINOR.PATCH

- MAJOR: Breaking changes
- MINOR: New features, backward compatible
- PATCH: Bugfixes, backward compatible

## Build Locations

| Platform | Output                         |
| -------- | ------------------------------ |
| Linux    | `QtPass-x.y.z-x86_64.AppImage` |
| Source   | `QtPass-x.y.z.tar.gz` / `.zip` |
| macOS    | `QtPass-x.y.z.dmg`             |
| Windows  | `QtPass-Setup-x.y.z.exe`       |

## CI/CD

Release workflow via GitHub Actions: `.github/workflows/release-installers.yml`

## Linting

See `qtpass-linting` skill for full CI workflow. Pattern:

```bash
# Run linter locally BEFORE pushing
act push -W .github/workflows/lint.yml -j build
```

## Protected Main Branch

`main` is protected - cannot push directly. Must create PR from a release branch:

```bash
# Create release branch
git checkout -b release/vX.Y.Z

# Make changes, commit
git add -A
git commit -m "Release vX.Y.Z"

# Update with latest main before pushing
git fetch upstream
git rebase upstream/main

# Push branch (force-with-lease since we rebased)
git push origin release/vX.Y.Z --force-with-lease

# Create PR
gh pr create --base main --head release/vX.Y.Z --title "Release vX.Y.Z"
```

## Script Best Practices

### Dry Run Option

For scripts that modify remote state (uploading releases, signing files), add a `--dryrun` flag to enable testing without making changes:

```bash
# Parse arguments at the start
dryrun=false
while [ $# -gt 0 ]; do
    case "$1" in
        --dryrun)
            dryrun=true
            shift
            ;;
        *)
            break
            ;;
    esac
done

# Use in conditional
if [ "$dryrun" = true ]; then
    echo "[dryrun] Would upload files: ${files[*]}"
else
    gh release upload ...
fi
```

### Glob Handling

When iterating over files matched by a glob pattern, use array assignment to handle the case where no files match:

```bash
# Without nullglob, loops over literal "*" when no files exist
for file in *; do
    [ -f "$file" ] || continue

# With array assignment, iterates over empty array when no files
files=(*)
for file in "${files[@]}"; do
    [ -f "$file" ] || continue
```
