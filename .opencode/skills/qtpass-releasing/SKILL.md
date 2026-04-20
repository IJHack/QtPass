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
