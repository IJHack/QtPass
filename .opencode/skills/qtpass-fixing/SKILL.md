---
name: qtpass-fixing
description: Bug fixing workflow for QtPass - find, fix, test, PR
license: GPL-3.0-or-later
compatibility: opencode
metadata:
  audience: developers
  workflow: bugfix
---

# Bug Fixing Workflow

## Bugfix Workflow

### 1. Investigate

- Search existing issues and PRs
- Check changelog for related fixes
- Search codebase for relevant patterns

### 2. Reproduce

- Read issue details, logs, and stack traces
- Locate relevant source files
- Identify root cause

### 3. Fix

- Make minimal, targeted changes in source files
- Keep changes focused on the root cause

### 4. Add Tests

- Add unit tests for the bugfix
- Follow project testing conventions
- Place tests in appropriate test directory

### 5. Verify

```bash
# Build and run tests
# Use project test command (e.g., make check, ctest, etc.)

# Build binary
# Use project build command (e.g., make, ninja, etc.)
```

### 6. Commit & PR

```bash
# Create branch
git checkout -b fix/<short-description>

# Commit with issue reference
git commit -m "Fix <description>"

# Push and create PR
git push origin <branch>
# Create PR via web or CLI
```

## Common Fix Patterns

### Null Pointer Crashes

- Check for null returns before using pointers
- Add defensive null checks

### Argument Parsing

- Use framework argument parsing instead of raw argv
- Validate input arguments

### Missing Availability Checks

- Check for optional dependencies before use
- Provide clear error messages

### Regular Expression Issues

- Test regular expression patterns with edge cases
- Use `\\S+` for non-whitespace matching

### File Creation Issues

- Ensure files are created when expected
- Write required metadata files

### Path Handling

- Use framework path utilities for cross-platform support
- Normalize paths consistently

### Font/Display Issues

- Use appropriate font hints for monospace display
- Test UI rendering across platforms

## Linting

### Config Files (prettier)

```bash
npx prettier --write <config-file>
```

### C++ (clang-format)

```bash
# Check formatting
clang-format --style=file --dry-run <source-file>

# Apply formatting
clang-format --style=file -i <source-file>
```
