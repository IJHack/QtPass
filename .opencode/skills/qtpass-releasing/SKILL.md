---
name: qtpass-releasing
description: Cuts QtPass releases from main or the 1.8 branch, bumps version files, writes the CHANGELOG and metainfo entries, tags signed vX.Y.Z or rc tags, drives the draft release with installers, GPG-signs and publishes it. Use when preparing a release, rc, version bump, changelog, tag, installers, publishing or downstream packages.
license: GPL-3.0-or-later
metadata:
  audience: developers
  workflow: release
  version: "1.1"
---

# Release Workflow for QtPass

Two release lines exist side by side:

- `main`: 2.x, including pre-releases tagged `vX.Y.Z-rcN`.
- `1.8`: the maintenance branch for 1.8.x (Qt 5.15 and Qt 6). Its workflow
  builds installers but has **no publish job**, see step 6b.

Both branches are protected: every change, the version bump included, goes in
through a pull request. Commits are signed with a DCO sign-off
(`git commit -s -S`).

## 1. Release branch and version bump

```bash
git checkout -b release/X.Y.Z origin/main   # or origin/1.8
```

| File                             | What to change                                                                                       |
| -------------------------------- | ---------------------------------------------------------------------------------------------------- |
| `qtpass.pri`                     | `VERSION = X.Y.Z` (unquoted; the single source the release workflow reads)                           |
| `qtpass.spec`                    | `Version:`, reset `Release:` to `1%{?dist}`, add a `%changelog` entry                                |
| `qtpass.iss`                     | `MyAppVersion`, and on main `MyAppNumericVersion` (no `-rcN`: it feeds the Windows version resource) |
| `Doxyfile`                       | `PROJECT_NUMBER`                                                                                     |
| `org.qtpass.QtPass.metainfo.xml` | new `<release version="X.Y.Z" date="YYYY-MM-DD">` at the top of `<releases>`                         |
| `publiccode.yml` (main)          | `softwareVersion` and `releaseDate` of the latest stable release                                     |
| `SECURITY.md` (main)             | supported-versions table, on a new minor or major only                                               |

**Verify:** no old version is left behind.

```bash
git grep -n "OLD\.VERSION" -- qtpass.pri qtpass.spec qtpass.iss Doxyfile publiccode.yml
```

## 2. Changelog

Turn the `## [Unreleased]` section of `CHANGELOG.md` into
`## [X.Y.Z](https://github.com/IJHack/QtPass/tree/vX.Y.Z) (YYYY-MM-DD)`,
grouped the way the file already is (Upgrade Notes, Features, Bugfixes, ...),
with the merged PRs and fixed issues. The metainfo `<release>` gets a one
paragraph summary of the same.

**Verify:** `npx prettier --check CHANGELOG.md` passes, and the metainfo date
matches the CHANGELOG date. The `qtpass-docs` skill has the CHANGELOG lint
rules.

## 3. Tests

```bash
qmake6 && make -j4 && make check
```

**Verify:** every test binary passes. Do not tag on a red run.

## 4. Pull request and merge

```bash
git commit -s -S -m "Release X.Y.Z"
git push -u origin release/X.Y.Z
gh pr create --base main --title "Release X.Y.Z"   # --base 1.8 for 1.8.x
```

**Verify:** `gh pr checks` is all green (including the `Release installers`
builds, which run on pull requests too), then squash-merge. If the release
date slips, land a follow-up "Release X.Y.Z: dated YYYY-MM-DD" PR before
tagging.

## 5. Tag

Tag the merge commit on the release branch, not the PR branch:

```bash
git fetch origin && git checkout origin/main   # or origin/1.8
git tag -s vX.Y.Z -m "QtPass vX.Y.Z"
git tag -v vX.Y.Z                              # verify: good signature
git push origin vX.Y.Z
```

## 6a. GitHub release from main

The tag push runs `.github/workflows/release-installers.yml`:

- `build-windows`: qtpass.exe and then the Inno Setup installer are signed
  through the SignPath Foundation (tag pushes and manual runs on `main` only).
  The `release-signing` policy waits for an approver in SignPath (two hour
  timeout); approve it there or the job fails. SignPath refuses a run that was
  re-run more than three times.
- `build-macos`: the dmg (unsigned, not notarized).
- `build-appimage`: the AppImage, smoke-tested on X11 and Wayland.
- `publish`: adds `git archive` source archives, creates the release **as a
  draft** if none exists, attaches everything plus a SLSA provenance bundle.

```bash
gh run watch                                   # the Release installers run
gh release view vX.Y.Z --json isDraft,assets --jq '.isDraft, .assets[].name'
```

**Verify:** a draft with the six assets from the table below. Re-running the
workflow replaces a draft's assets (and drops their stale `.asc`); on a
published release it only adds missing assets, so signed files never change
under their signature.

Then write the notes, sign, publish:

```bash
gh release edit vX.Y.Z --notes-file <(sed -n '/^## \[X.Y.Z\]/,/^## \[/p' CHANGELOG.md | sed '$d')
./scripts/sign-release-assets.sh --dryrun vX.Y.Z   # signs locally, uploads nothing
./scripts/sign-release-assets.sh vX.Y.Z            # .asc per asset, maintainer key
gh release edit vX.Y.Z --draft=false               # rc: add --prerelease --latest=false
```

**Verify:** every asset has a matching `.asc` and the release shows as
Latest (or Pre-release for an rc).

## 6b. GitHub release from the 1.8 branch

The 1.8 workflow (`Release Build`) builds the Windows installer (unsigned) and
the macOS dmg, with no AppImage and no publish job. Assemble the draft by hand:

```bash
gh run download <run-id> -D dist               # the tag's Release Build run
git archive --prefix=qtpass-X.Y.Z/ -o dist/QtPass-X.Y.Z.tar.gz vX.Y.Z
git archive --prefix=qtpass-X.Y.Z/ -o dist/QtPass-X.Y.Z.zip vX.Y.Z
gh release create vX.Y.Z --draft --verify-tag --title "QtPass X.Y.Z" \
  --notes-file <(sed -n '/^## \[X.Y.Z\]/,/^## \[/p' CHANGELOG.md | sed '$d') \
  dist/*/qtpass-X.Y.Z.exe dist/*/QtPass-X.Y.Z.dmg dist/QtPass-X.Y.Z.*
```

**Verify:** four assets (exe, dmg, tar.gz, zip). Then sign and publish as in
6a, with `--latest` when 1.8.x is the newest stable release.

## Release assets

| Asset                   | Name                           | Built by            |
| ----------------------- | ------------------------------ | ------------------- |
| Windows installer       | `qtpass-X.Y.Z.exe`             | `build-windows`     |
| macOS disk image        | `QtPass-X.Y.Z.dmg`             | `build-macos`       |
| Linux AppImage (main)   | `QtPass-X.Y.Z-x86_64.AppImage` | `build-appimage`    |
| Source archives         | `QtPass-X.Y.Z.tar.gz`, `.zip`  | `publish` / by hand |
| Build provenance (main) | `QtPass-X.Y.Z.intoto.jsonl`    | `publish`           |

Only uploaded assets are signed. GitHub's automatic "Source code" archives are
generated on the fly and differ from the `git archive` assets; never claim
they are identical. Debian's `uscan` and MacPorts use the signed
`QtPass-X.Y.Z.tar.gz`, so signing is not optional.

## 7. Website (gh-pages)

qtpass.org is the `gh-pages` branch. Pages carry the version (headings,
download links, structured data) and a cache stamp such as
`styles.css?v=1.8.3-14`:

```bash
git checkout gh-pages
git grep -n "OLD\.VERSION" -- "*.html"
# update downloads, index, changelog, getting-started; bump the ?v= stamp
npx htmlhint "*.html"                          # verify: the site lint passes
```

Commit and push to `gh-pages`. GitHub Pages publishes on push; the secondary
host is synced with `scripts/precompress.sh` and then `./sync.sh` (dry run by
default, `--live` to deploy).

## 8. Downstream packages (release day)

QtPass maintains several downstream packages itself, and a release not
submitted on release day tends to stay unsubmitted. Submit each update, or
open a tracking issue, before announcing.

- Read [references/downstream-packaging.md](references/downstream-packaging.md)
  when bumping FreeBSD, OpenBSD, MacPorts, winget, Flathub, Debian, Chocolatey
  or Homebrew (the Homebrew cask is disabled until the app is notarized).

## Local builds

CI produces every release asset. Build locally only to debug a packaging
problem or to sign and notarize a dmg by hand.

- Read [references/local-builds.md](references/local-builds.md) when building
  the AppImage, the macOS dmg (with Developer ID signing and notarization) or
  the Windows installer on your own machine.

## Script conventions

Release scripts that change remote state take `--dryrun` and say what they
would do (as `scripts/sign-release-assets.sh` does). Iterate over globs with
`files=(*)` plus `[ -f "$file" ] || continue`, so an empty match is skipped.
Lint and CI details are in the `qtpass-linting` skill.
