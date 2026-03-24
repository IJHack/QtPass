---
name: qtpass-fixing
description: Bug fixing workflow for QtPass - find, fix, test, PR
license: GPL-3.0-or-later
compatibility: opencode
metadata:
  audience: developers
  workflow: bugfix
---

# Bugfix Workflow for QtPass

## Bugfix Workflow

### 1. Investigate

- Search existing issues: `gh issue list --search "<keywords>"`
- Check CHANGELOG.md for related fixes
- Search code: `grep -r "pattern" src/`

### 2. Reproduce

- Read issue details, logs, and stack traces
- Locate relevant source files
- Identify root cause

### 3. Fix

- Make minimal, targeted changes in source files
- Keep changes focused on the root cause

### 4. Add Tests

- Add unit tests for the bugfix
- Follow project testing conventions (see `qtpass-testing` skill)
- Place tests in `tests/auto/<module>/`

### 5. Verify

```bash
# Build and run tests
make check

# Build binary
make -j4
```

### 6. Commit & PR

```bash
# Create branch
git checkout -b fix/<issue-number>-short-description

# Commit with issue reference
git commit -m "Fix <description> (#issue)"

# Push and create PR
git push origin fix/<branch>
gh pr create --title "Fix <description>" --body "## Summary\n- Fix description"
```

## Common QtPass Fix Patterns

### Wayland Crash

- File: `src/qtpass.cpp` or `src/trayicon.cpp`
- Issue: `QGuiApplication::screenAt()` returns null
- Fix: Add null check before using screen

### CLI Args Parsing

- File: `src/main/main.cpp`
- Issue: Raw `argv[]` gets interpreted as search
- Fix: Use `QCoreApplication::arguments()`

### OTP Errors

- Files: `src/imitatepass.cpp`, `src/configdialog.cpp`
- Issue: Missing `pass-otp` check, poor error messages
- Fix: Add availability check, improve error handling

### URL Detection

- File: `src/util.cpp`
- Function: `protocolRegex()`
- Issue: URLs include spaces or invalid chars
- Fix: Use `\\S+` instead of character class

### GPG-ID Creation

- File: `src/mainwindow.cpp`
- Function: `addFolder()`
- Issue: `.gpg-id` not created when adding folders
- Fix: Add code to write .gpg-id file

### Path Separators

- File: `src/pass.cpp`
- Function: `getGpgIdPath()`
- Issue: Windows path handling
- Fix: Use QDir for path normalization

### Monospace Font

- File: `src/mainwindow.cpp`
- Issue: Font not monospace in tables
- Fix: Use `setStyleHint(QFont::Monospace)`

## Key Source Files

| File                 | Purpose                                 |
| -------------------- | --------------------------------------- |
| src/mainwindow.cpp   | Main UI, tree view, dialogs             |
| src/pass.cpp         | GPG operations, path handling           |
| src/util.cpp         | Utilities, regular expression, file ops |
| src/filecontent.cpp  | Password file parsing                   |
| src/imitatepass.cpp  | CLI pass imitation                      |
| src/configdialog.cpp | Settings dialog                         |
| src/executor.cpp     | Command execution                       |

## Linting

See `qtpass-linting` skill for full CI workflow. Pattern:

```bash
# Run linter locally BEFORE pushing
act push -W .github/workflows/linter.yml -j build
```

### Config Files (prettier)

```bash
npx prettier --write <config-file>
npx prettier --write .github/workflows/*.yml
npx prettier --write .opencode/skills/*/SKILL.md
```

### C++ (clang-format)

```bash
# Check formatting
clang-format --style=file --dry-run <source-file>

# Apply formatting
clang-format --style=file -i <source-file>
```
