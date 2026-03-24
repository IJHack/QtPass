---
name: qtpass-releasing
description: Release workflow for QtPass - versioning, builds, publishing
license: GPL-3.0-or-later
compatibility: opencode
metadata:
  audience: developers
  workflow: release
---

# Release Workflow

## Release Checklist

### 1. Version Bump

Update version in all build files:

- Build config files (`.pro`, `CMakeLists.txt`, etc.) - version field
- Package specs (`.spec`, `.appdata.xml`, `.iss`) - version field
- Installers/config files - version field
- Documentation - version references

```bash
# Find version strings
grep -rn "<current-version>" <build-files>
```

### 2. Changelog

Update changelog:

- Add release date
- List merged PRs and fixed issues
- Group by: Added, Changed, Fixed, Removed

### 3. Tests

```bash
# Run full test suite
# Use project test command
```

### 4. Build Artifacts

#### Linux

```bash
# Create source tarball
# Use release script or:
git archive --prefix=<project>-x.y.z/ -o <project>-x.y.z.tar.gz HEAD
```

#### macOS

```bash
# Use release script or build .dmg
```

#### Windows

```bash
# Via CI or locally with installer tool
```

### 5. Git Tags

```bash
git tag -a v<version> -m "Release <version>"
git push origin v<version>
```

### 6. Release

```bash
# Create release via web or CLI
gh release create v<version> \
  --title "Release <version>" \
  --notes-file CHANGELOG.md \
  <artifacts>
```

### 7. Documentation (if needed)

```bash
# Update stable version references
# Update download links
git commit -m "Release v<version>"
git push origin <docs-branch>
```

## Version Numbering

Follow semantic versioning: MAJOR.MINOR.PATCH

- MAJOR: Breaking changes
- MINOR: New features, backward compatible
- PATCH: Bugfixes, backward compatible

## Build Outputs

| Platform | Typical Output           |
| -------- | ------------------------ |
| Linux    | `.tar.gz` source archive |
| macOS    | `.dmg` disk image        |
| Windows  | `.exe` installer         |

## CI/CD

Use CI/CD workflows for automated releases
