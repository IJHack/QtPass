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

### Tautology Assertions in Tests

Avoid assertions that always evaluate to true:

```cpp
// Bad - always true
QVERIFY(profiles.isEmpty() || !profiles.isEmpty());
QVERIFY(!store.isEmpty() || store.isEmpty());

// Good - meaningful check
QVERIFY(profiles.isEmpty());
QVERIFY2(store.isEmpty() || store.startsWith("/"), "Pass store should be empty or a plausible path");
```

### Verify Test Setup Return Values

Always verify that test setup operations succeed:

```cpp
// Bad - ignores return value
(void)QDir(srcDir.path()).mkdir("source");
(void)f1.open(QFile::WriteOnly);

// Good - verify success
QVERIFY(QDir(srcDir.path()).mkdir("source"));
QVERIFY(f1.open(QFile::WriteOnly));
```

### Contractions in Comments

Use "cannot" instead of "can't" for formal consistency:

```cpp
// Bad
// On Windows, we can't safely backup

// Good
// On Windows, we cannot safely backup
```

### Copyright Year in Templates

Use `YYYY` as placeholder instead of current year:

```cpp
// Bad
// SPDX-FileCopyrightText: 2026 Your Name

// Good
// SPDX-FileCopyrightText: YYYY Your Name
```

### Return After Dialog Rejection

When showing an error dialog followed by `reject()` in a constructor, add `return` to prevent the constructor from continuing with invalid state:

```cpp
// After error dialog in constructor
if (users.isEmpty()) {
    QMessageBox::critical(parent, tr("Error"), tr("No users found"));
    reject();
    return;  // Prevent constructor from continuing
}
```

### Index Instead of Pointer

When storing references to list items in UI (e.g., Qt::UserRole), prefer indices over pointers to avoid dangling references:

```cpp
// Instead of storing pointer to local reference
for (const auto &user : m_userList) {
    item->setData(Qt::UserRole, QVariant::fromValue(&user));  // DANGLING!
}

// Store index instead
for (int i = 0; i < m_userList.size(); ++i) {
    item->setData(Qt::UserRole, QVariant::fromValue(i));
}
```

}

// Good - store index
for (int i = 0; i < m_userList.size(); ++i) {
item->setData(Qt::UserRole, QVariant::fromValue(i));
}

// Later, lookup by index
const int index = item->data(Qt::UserRole).toInt(&ok);
if (ok && index >= 0 && index < m_userList.size()) {
m_userList[index].enabled = item->checkState() == Qt::Checked;
}

````

### Theme-Aware Colors

For list items or labels indicating status (e.g., secret keys), prefer `QPalette` colors over hardcoded values for better accessibility:

```cpp
// Hardcoded may have poor contrast on some themes
item->setForeground(Qt::blue);

// Theme-aware alternative
const QPalette palette = QApplication::palette();
item->setForeground(palette.color(QPalette::Link));
````

### Accessibility: Color + Text

When indicating status through colors (invalid, expired, partial), add text prefixes or tooltips for colorblind users:

```cpp
// Color only - not accessible
item->setBackground(Qt::darkRed);

// Color + text + tooltip - accessible
item->setBackground(Qt::darkRed);
item->setText(tr("[INVALID] ") + originalText);
item->setToolTip(tr("Invalid key"));
```

### Debug Logging

Add debug logging for validation failures using `#ifdef QT_DEBUG`:

```cpp
bool ok = false;
const int index = item->data(Qt::UserRole).toInt(&ok);
if (!ok) {
#ifdef QT_DEBUG
    qWarning() << "UsersDialog::itemChange: invalid user index data for item";
#endif
    return;
}
if (index < 0 || index >= m_userList.size()) {
#ifdef QT_DEBUG
    qWarning() << "UsersDialog::itemChange: user index out of range:" << index;
#endif
    return;
}
```

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
