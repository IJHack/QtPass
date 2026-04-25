---
name: qtpass-fixing
description: Bug fixing workflow for QtPass - find, fix, test, PR
license: GPL-3.0-or-later
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

# Commit with issue reference (always use -S for signing)
git commit -S -m "Fix <description> (#issue)"

# Push and create PR
git push origin fix/<branch>
gh pr create --title "Fix <description>" --body "## Summary\n- Fix description"
```

### Handling Static Analysis Findings

When CodeRabbit/CodeAnt-AI flag issues in PRs:

1. **Verify the finding** - Check if it's a real issue or false positive
2. **If correct, fix it** - Make minimal changes to address
3. **Push update** - Force push to update the PR
4. **Respond to comments** - Comment that fix is applied

```bash
# After fixing, amend commit and force push
git add -A
git commit -S --amend --no-edit
git push --force
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

### Copyright Year in SPDX Headers

Use the actual year the file was created — never a placeholder, never a year range. Repo convention is a single literal year (the existing files are 2014/2015/2016/2018/2020/2026, all real years; PR #1162 review explicitly flagged `YYYY` placeholders).

```cpp
// Bad — placeholder; reviewers will ask for a real year
// SPDX-FileCopyrightText: YYYY Your Name

// Bad — year range
// SPDX-FileCopyrightText: 2014-2026 Your Name

// Good — single real year
// SPDX-FileCopyrightText: 2026 Your Name
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


// Later, lookup by index
bool success = false;
const int index = item->data(Qt::UserRole).toInt(&success);
if (success && index >= 0 && index < m_userList.size()) {
    m_userList[index].enabled = item->checkState() == Qt::Checked;
}
```

### Theme-Aware Colors

For list items or labels indicating status (e.g., secret keys), prefer `QPalette` colors over hardcoded values for better accessibility:

```cpp
// Hardcoded may have poor contrast on some themes
item->setForeground(Qt::blue);

// Theme-aware alternative
const QPalette palette = QApplication::palette();
item->setForeground(palette.color(QPalette::Link));
```

### Accessibility: Color + Text

When indicating status through colors (invalid, expired, partial), add text prefixes or tooltips for color blind users:

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
bool success = false;
const int index = item->data(Qt::UserRole).toInt(&success);
if (!success) {
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

### Qt Version Compatibility (Qt 5.15 vs Qt 6.x)

CI tests both Qt 5.15 and Qt 6.8. Some Qt 6 APIs do not exist in Qt 5.15:

| Qt 6 API                  | Qt 5.15-compatible alternative            |
| ------------------------- | ----------------------------------------- |
| `QStringList::removeIf()` | `erase/remove_if` with a predicate lambda |
| `QList::removeIf()`       | same pattern                              |

```cpp
// Bad — Qt 6.1+ only, fails on Qt 5.15
env.removeIf([&key](const QString &e) { return e.startsWith(key); });

// Good — works on Qt 5.15 and Qt 6.x
env.erase(std::remove_if(env.begin(), env.end(),
                          [&key](const QString &entry) {
                            return entry.startsWith(key);
                          }),
          env.end());
```

When filtering with a substring match (e.g., environment variable removal), use `startsWith` rather than `contains` or `filter()` to avoid removing unrelated entries with similar prefixes:

```cpp
// Bad — filter("FOO") also removes "FOOBAR=value"
env = env.filter(key);

// Good — explicit prefix check
env.erase(std::remove_if(env.begin(), env.end(),
                          [&key](const QString &e) { return e.startsWith(key); }),
          env.end());
```

### std::as_const on Range-For to Prevent Implicit Detachment

When iterating over a Qt container that is a non-const member variable, add `std::as_const()` to prevent Qt from making an implicit copy (detach):

```cpp
// Bad — may detach the container implicitly
for (const auto &field : m_fields) { ... }

// Good — no detach, no copy
for (const auto &field : std::as_const(m_fields)) { ... }
```

### Boolean Logic Bugs

When comparing booleans, use `&&` instead of `==` to avoid unexpected true values:

```cpp
// Bad - yields true when both are false (e.g., public key case)
bool secret = false;
bool isSec = (type == GpgRecordType::Sec);  // false
handlePubSecRecord(props, secret == isSec);  // false == false = true!

// Good - requires both conditions to be true
handlePubSecRecord(props, secret && (type == GpgRecordType::Sec));
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
npx prettier --write "*/*/SKILL.md"
```

### C++ (clang-format)

```bash
# Check formatting
clang-format --style=file --dry-run <source-file>

# Apply formatting
clang-format --style=file -i <source-file>
```

## Bug Type Playbooks

### UI Bugs (mainwindow.cpp, dialogs)

UI bugs often involve signal/slot connections, widget state, or clipboard operations.

**Common patterns:**

- Check signal connections are properly connected
- Verify widget parent/ownership
- Clipboard operations may fail on some platforms (GNOME, KDE)
- Theme-aware colors vs hardcoded (see above)

**Debugging:**

```cpp
// Add debug output
qDebug() << "Button clicked, state:" << ui->someWidget->isVisible();
```

### GPG/Pass Bugs (pass.cpp, executor.cpp)

GPG-related bugs often involve command execution, path handling, or key operations.

**Common patterns:**

- Check executor return codes
- Verify GPG binary is in PATH
- Watch for path normalization issues (QDir::cleanPath)
- GPG may lock the keyring - handle retries

**Debugging:**

```cpp
// Enable verbose GPG output
// Check /tmp for temporary GPG files
```

### Path/Model Bugs (storemodel.cpp, configdialog.cpp)

Path handling bugs often involve separators, encoding, or model indexing.

**Common patterns:**

- Use QDir for path operations (handles / vs \)
- Store indices, not pointers (see above)
- Check model filter/sort state

**Debugging:**

```cpp
qDebug() << "Path:" << path << "Cleaned:" << QDir::cleanPath(path);
```

## IDE/LSP Setup

For proper code analysis (resolving Qt types like `QString`), generate `compile_commands.json`:

```bash
./scripts/generate-compile-commands.sh
```

This enables clangd/LSP to provide accurate completions and catch real issues. Without it, the LSP shows false positives about missing Qt headers.

### Using Editor Fix Suggestions

When the LSP shows "(fix available)" on errors:

1. **Visual Studio Code**: Click the 💡 lightbulb or press `Ctrl+.` (Quick Fix)
2. **JetBrains**: Press `Alt+Enter` on the error
3. **vim/neovim**: Use clangd's `textDocument/codeAction` via your LSP plugin

**Note:** LSP suggestions are just that - suggestions. Always verify the fix makes sense before applying, especially for complex refactoring.

### Running Clangd from CLI

To check a file directly for issues:

```bash
# Generate compile_commands.json first (required for Qt headers)
./scripts/generate-compile-commands.sh

# Check a specific file
clangd --check=/path/to/file.cpp
```

Common clangd diagnostics:

- `[performance-unnecessary-copy-initialization]` - Use `const T&` instead of `const T`
- `[readability-static-definition]` - Consider making static definitions inline
