# Agent Instructions for QtPass

This file provides guidance for AI agents working on QtPass development.

## Build

```bash
# Full build
qmake6 && make -j4

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
- Use `std::as_const()` instead of deprecated `qAsConst()` for Qt iterations

## Handling AI Findings

When CodeRabbit/CodeAnt AI flags issues in PRs:

1. **Verify the finding** - Check if it's a real issue or false positive
2. **If correct, fix it** - Make minimal changes to address
3. **Push update** - Force push to update the PR
4. **Respond to comments** - Comment that fix is applied

If a finding is incorrect (e.g., code is already correct, or finding is outdated), explain why in a comment and mark as resolved.

## Testing

QtPass uses Qt Test framework. Test files are in `tests/auto/`.

```bash
# Run specific test
./tests/auto/util/tst_util testName

# Verbose output
./tests/auto/util/tst_util -v2
```

## Localization

QtPass uses Qt Linguist (`.ts` files) in `localization/`. Don't manually edit translations - Weblate handles that. Run `qmake6` after source changes to update translation source references.

See `qtpass-localization` skill for details.

## Skills

- `qtpass-fixing` - Common bugfix patterns
- `qtpass-testing` - Qt Test framework and test structure
- `qtpass-linting` - CI/CD and linters
- `qtpass-localization` - Translation workflow
- `qtpass-github` - PRs, issues, merging
- `qtpass-releasing` - Release process
- `qtpass-docs` - Documentation guide

## Release

See `qtpass-releasing` skill for release process.
