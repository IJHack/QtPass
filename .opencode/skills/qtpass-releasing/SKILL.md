---
name: qtpass-releasing
description: Release workflow for QtPass - versioning, builds, publishing
license: GPL-3.0-or-later
compatibility: opencode
metadata:
  audience: developers
  workflow: release
---

# Release Workflow for QtPass

## Release Checklist

### 1. Version Bump

Update version in all build files:

- `qtpass.pro` - `VERSION = "x.y.z"`
- `qtpass.spec` - `Version:`
- `qtpass.appdata.xml` - `<release version="">`
- `qtpass.iss` - `AppVerName=`
- `appdmg.json` - `version`
- `README.md` - Download links

```bash
# Find version strings
grep -rn "1\.5" qtpass.pro qtpass.spec qtpass.appdata.xml qtpass.iss appdmg.json
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
./release-linux

# Or manually:
git archive --prefix=qtpass-x.y.z/ -o qtpass-x.y.z.tar.gz HEAD
```

#### macOS

```bash
./release-mac
```

#### Windows

```bash
# Via GitHub Actions or locally with Inno Setup
qtpass.iss
```

### 5. Git Tags

```bash
git tag -a v1.5.1 -m "QtPass 1.5.1 Release"
git push origin v1.5.1
```

### 6. GitHub Release

```bash
gh release create v1.5.1 \
  --title "QtPass 1.5.1" \
  --notes-file CHANGELOG.md \
  qtpass-x.y.z.tar.gz
```

### 7. GitHub Pages (if needed)

```bash
# Update stable version
git checkout gh-pages
# Update download links
git commit -m "Release v1.5.1"
git push origin gh-pages
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
