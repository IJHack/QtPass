---
name: qtpass-fixing
description: Bug fixing workflow for QtPass - find, fix, test, PR
license: GPL-3.0-or-later
compatibility: opencode
metadata:
  audience: developers
  workflow: bugfix
---

# Fixing QtPass the Anus way

## Bugfix Workflow

### 1. Investigate

- Search existing issues: `gh issue list --search "<keywords>"`
- Check CHANGELOG.md for related fixes
- Search code: `grep -r "pattern" src/`

### 2. Reproduce

- Understand the bug: read issue details, logs, stack traces
- Find relevant source files
- Identify root cause

### 3. Fix

- Make changes in `src/` files
- Fix should be minimal and targeted

### 4. Add Tests

- Always add unit tests when fixing bugs
- See `qtpass-testing` skill for test conventions
- Tests go in `tests/auto/<module>/tst_*.cpp`

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

# Commit with reference
git commit -m "Fix <description> (#issue)"

# Push and create PR
git push origin fix/<branch>
gh pr create --title "Fix <description>" --body "## Summary\n- Fix description"
```

## Common Fix Locations

### Wayland Crash

- File: `src/qtpass.cpp` or `src/trayicon.cpp`
- Issue: `QGuiApplication::screenAt()` returns null
- Fix: Add null check before using screen

### CLI Args Parsing

- File: `main/main.cpp`
- Issue: Raw `argv[]` gets interpreted as search
- Fix: Use `QCoreApplication::arguments()`

### OTP Errors

- Files: `src/imitatepass.cpp`, `src/configdialog.cpp`
- Issues: Missing `pass-otp` check, poor error messages
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

### Web/Config (prettier)

```bash
npx prettier --write <file>
```

### C++ (clang-format)

```bash
# Check
clang-format --style=file --dry-run src/main.cpp
# Apply
clang-format --style=file -i src/main.cpp
```
