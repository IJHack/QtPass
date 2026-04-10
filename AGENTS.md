# Agent Instructions for QtPass

This file provides guidance for AI agents working on QtPass development.

## Build

```bash
# Full build
make -j4

# With tests
make check

# With coverage
qmake6 -r CONFIG+=coverage
make -j4
make lcov
```

## Linting

**Before pushing, always run:**

```bash
# Format all markdown/config files
npx prettier --write "**/*.md" "**/*.yml"

# Verify formatting passes
npx prettier --check "**/*.md" "**/*.yml"

# Local CI check (may fail on new branches due to git issues)
act push -W .github/workflows/linter.yml -j build
```

**C++ formatting:**
```bash
clang-format --style=file -i <source-file>
```

## Git Workflow

- **Create branch:** `git checkout -b fix/description`
- **Commit (always sign):** `git commit -S -m "description"`
- **Push:** `git push -u origin branch-name`
- **Create PR:** `gh pr create --title "description" --body "## Summary\n- details"`
- **Update with main before merging:**
  ```bash
  git fetch upstream
  git pull upstream main --rebase
  git push -f
  ```

## Key Conventions

- Use `QCoreApplication::arguments()` instead of raw `argv[]` for CLI parsing
- Use `QDir::cleanPath()` for cross-platform path normalization
- Check for null from `screenAt()` before dereferencing
- Use `tr()` for all user-facing strings
- Store indices, not pointers, in Qt::UserRole data
- Use `QPalette` colors instead of hardcoded values for theme-aware UI

## Common Fixes

See `qtpass-fixing` skill for common bugfix patterns.

## Release

See `qtpass-releasing` skill for release process.