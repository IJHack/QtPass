---
name: qtpass-docs
description: Documentation guide for QtPass - README, FAQ, localization
license: GPL-3.0-or-later
metadata:
  audience: developers
  workflow: documentation
---

# Project Documentation

## QtPass Documentation Files

| File               | Purpose                                     |
| ------------------ | ------------------------------------------- |
| README.md          | Main documentation, installation, usage     |
| FAQ.md             | Frequently asked questions, troubleshooting |
| CHANGELOG.md       | Release history, changes                    |
| CONTRIBUTING.md    | Developer contribution guidelines           |
| CODE_OF_CONDUCT.md | Community code of conduct                   |
| Doxyfile           | API documentation configuration             |
| Windows.md         | Windows-specific installation and build     |

## README.md Sections

- Badges (build, license, version)
- Description
- Features
- Installation (Linux, macOS, Windows)
- Usage / Getting Started
- Configuration
- Acknowledgments

## FAQ.md Sections

- Installation issues
- Configuration issues
- GPG/Key issues
- Git integration
- Platform-specific (macOS, Windows, Linux)

### FAQ Template

```markdown
## Question Title

**Problem:** Description of the issue

**Solution:** Step-by-step solution

**Related:** Links to relevant issues
```

## Localization

See [qtpass-localization](../qtpass-localization/SKILL.md) skill for comprehensive guide.

## Docs Build

### API Documentation

```bash
# Generate API docs
doxygen Doxyfile
# Or use project-specific docs command

# View
open html/index.html
```

## Linting

**THIS IS THE PATTERN - always run before pushing:**

```bash
# Check formatting (this is the pattern)
npx prettier --check "**/*.md"

# Format all markdown files
npx prettier --write "**/*.md"

# Format specific file
npx prettier --write README.md
```

### Markdown (prettier)

```bash
npx prettier --write <markdown-file>
npx prettier --write .claude/skills/*/SKILL.md
```

### YAML (prettier)

```bash
npx prettier --write <yaml-file>
npx prettier --write .github/workflows/*.yml
```

## Updating Documentation

### Adding New FAQ Entry

1. Edit `FAQ.md`
2. Use the FAQ template section above
3. Run prettier: `npx prettier --write FAQ.md`
4. Test the changes render correctly

### Updating Version in readme

When releasing a new version, update download links:

```bash
# Find version strings in README
grep -n "1\.5\|download" README.md
```

Update:

- Download links for each platform
- Badge version numbers
- Any version-specific instructions

### Building API Docs

```bash
# Generate with doxygen
doxygen Doxyfile

# Output goes to docs/html/
ls docs/html/index.html
```

## Common Pitfalls

### Forgetting to Run Prettier

Always format Markdown with prettier before committing:

```bash
# Wrong - may fail CI
git commit -m "Update FAQ"

# Correct
npx prettier --write FAQ.md
git commit -m "Update FAQ"
```

### Broken Links

When adding links to issues or PRs:

```bash
# Use full GitHub URLs (they redirect correctly)
[Issue #123](https://github.com/IJHack/QtPass/issues/123)

# Not relative paths
[Issue #123](issues/123)  # Broken
```

### Outdated Platform Instructions

QtPass changes frequently. When updating installation instructions:

- Verify the commands still work
- Check for new dependencies
- Update screenshots if UI changed

### CHANGELOG Format

Keep CHANGELOG entries consistent:

```markdown
## [1.5.1] - 2026-03-30

### Fixed

- Issue #123: Description of fix

### Added

- New feature description
```

### Doxygen Comments in Code

When adding new public APIs, every public symbol in a header needs a Doxygen doc block:

```cpp
/**
 * @brief Brief description.
 * @param param1 Description of first parameter.
 * @return Description of return value.
 */
```

The CI enforces **zero Doxygen warnings** via `docs.yml` + `WARN_AS_ERROR = FAIL_ON_WARNINGS`.

#### Enforced Doxyfile settings

| Setting            | Value              | Purpose                                           |
| ------------------ | ------------------ | ------------------------------------------------- |
| `FILE_PATTERNS`    | `*.h`              | Avoids duplicate warnings from `.cpp` docs        |
| `EXTRACT_ALL`      | `NO`               | Enables `WARN_NO_PARAMDOC` to fire                |
| `WARN_NO_PARAMDOC` | `YES`              | Requires `@param`/`@return` on all public symbols |
| `WARN_AS_ERROR`    | `FAIL_ON_WARNINGS` | Fails CI on any undocumented symbol               |

#### Run locally before pushing

```bash
doxygen Doxyfile
# Any output = warning = CI will fail
```

#### Common doc mistakes that cause warnings

- Unnamed parameters in header declarations — name every parameter
- Orphaned `/** */` blocks not immediately above their declaration
- Missing `@return` on non-void functions
- Signals with unnamed parameters (Qt signals need docs too)
- `@unknowncommand` typos in doc blocks
