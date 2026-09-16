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

- `qtpass.pri` - `VERSION = X.Y.Z` (note: .pri not .pro, unquoted number)
- `qtpass.spec` - `Version:`
- `qtpass.iss` - `AppVerName=`
- `Doxyfile` - `PROJECT_NUMBER`
- `downloads.html` (gh-pages) - multiple references
- `index.html` (gh-pages)
- `getting-started.html` (gh-pages)
- `changelog.html` (gh-pages)
- `changelog.1.4.html` (gh-pages)
- `old.html` (gh-pages)

**NOTE:** `qtpass.appdata.xml` and `appdmg.json` don't have version fields to update.

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
```

#### macOS

```bash
./scripts/release-mac.sh
```

#### Windows

```bash
# Via GitHub Actions or locally with Inno Setup
qtpass.iss
```

### 5. Git Tags

```bash
git tag -a vX.Y.Z -m "QtPass vX.Y.Z Release"
git push origin vX.Y.Z
```

### 6. GitHub Release

```bash
gh release create vX.Y.Z \
  --title "QtPass vX.Y.Z" \
  --notes-file CHANGELOG.md \
  qtpass-x.y.z.tar.gz
```

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

#### FreeBSD `sysutils/qtpass`

- Where: [Bugzilla](https://bugs.freebsd.org/bugzilla/enter_bug.cgi?product=Ports%20%26%20Packages),
  component "Individual Port(s)", summary `sysutils/qtpass: Update to X.Y.Z`.
- How: bump `DISTVERSION`, run `make makesum`, attach the diff with the
  `maintainer-approval+` flag (we are the maintainer, so no approval wait).
- Test in a FreeBSD VM: `make stage check-plist stage-qa` and `portlint -AC`.

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
  add a `<release>` to `qtpass.appdata.xml`, run `flatpak-builder-lint`.
- Flathub's [generative-AI policy](https://docs.flathub.org/docs/for-app-authors/requirements#generative-ai-policy):
  AI-generated code, packaging or metadata must be disclosed (which parts,
  how much) and is accepted at reviewer discretion; AI tools must not open
  or automate the submission PR, or write its commit messages, description,
  review comments or replies, and no AI-agent reviews may be requested.
  Open and write the PR by hand.

#### Debian `qtpass` (and everything downstream of it)

- Not ours: maintained by Philip Rinn <rinni@debian.org>,
  [salsa.debian.org/debian/qtpass](https://salsa.debian.org/debian/qtpass).
- Ubuntu syncs the package from Debian and Linux Mint inherits Ubuntu's, so
  Debian unstable is the only lever; an Ubuntu-only upload would be reverted
  at the next sync.
- `debian/watch` scrapes our GitHub releases for `QtPass-<version>.tar.gz`
  plus the detached `.asc`, so a signed release is enough for `uscan` to see
  it. Signing the release assets is therefore not optional.
- If a release sits unpackaged for a while, file a wishlist bug
  (`reportbug qtpass`, severity wishlist, subject
  `qtpass: new upstream release X.Y.Z`) listing the security-relevant fixes,
  and say whether they are worth stable-updates or backports. Draft used for
  1.8.1: `~/debian-qtpass-1.8.1-bug.txt` (see #1682 packaging notes).
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

| Platform | Output                   |
| -------- | ------------------------ |
| Linux    | `qtpass-x.y.z.tar.gz`    |
| macOS    | `QtPass-x.y.z.dmg`       |
| Windows  | `QtPass-Setup-x.y.z.exe` |

## CI/CD

Release workflow via GitHub Actions: `.github/workflows/release-installers.yml`

## Linting

See `qtpass-linting` skill for full CI workflow. Pattern:

```bash
# Run linter locally BEFORE pushing
act push -W .github/workflows/linter.yml -j build
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
