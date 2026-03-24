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

## Common Fix Patterns

### Null Pointer Checks

Qt classes like `QGuiApplication::screenAt()` can return null on some platforms.
Always verify pointers before dereferencing:

```cpp
QScreen *screen = QGuiApplication::screenAt(pos);
if (!screen)
    return;
```

### CLI Argument Parsing

Use `QCoreApplication::arguments()` instead of raw `argv[]` to let Qt normalize paths and handle quoting:

```cpp
QStringList args = QCoreApplication::arguments();
```

### External Tool Availability

Before using optional tools like `pass-otp`, check availability:

```cpp
if (!passOTPexists) {
    showError("pass-otp is required for OTP features");
    return;
}
```

### Cross-Platform Path Handling

Use `QDir` for path normalization to handle `/` vs `\` and `.`/`..` components:

```cpp
QDir dir;
QString normalized = dir.cleanPath(gpgIdPath);
```

### String Parsing Edge Cases

For regular expression patterns matching URLs or special characters, use `\\S+` for non-whitespace to avoid matching spaces:

```cpp
QRegularExpression urlRegex("(\\S+)://(\\S+)");
```

### UI Font Consistency

For monospace fonts in tables/lists, use `setStyleHint(QFont::Monospace)` to avoid platform-specific defaults.

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
