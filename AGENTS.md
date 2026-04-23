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
- Use `QT_VERSION_CHECK(6,0,0)` for Qt5/Qt6 compatibility

## Niche Knowledge

### GPG Key Inheritance

`Pass::getGpgIdPath(dir)` walks up parent directories to find `.gpg-id`:

```cpp
QString gpgIdPath = Pass::getGpgIdPath(folderPath);
bool exists = QFile(gpgIdPath).exists();
```

This allows folders to inherit recipients from parent directories.

### Atomic File Writes

Use `QSaveFile` for atomic writes to prevent corruption:

```cpp
QSaveFile file(path);
file.open(QIODevice::WriteOnly);
QTextStream out(&file);
out << content;
out.flush();
if (out.status() == QTextStream::Ok) {
    file.commit();  // Only commits if write succeeded
}
```

### Context Menu Pattern

Right-click menus in Qt use parented menus for automatic cleanup:

```cpp
QMenu contextMenu(this);
QMenu *subMenu = new QMenu("Share", &contextMenu);  // Parented to contextMenu
contextMenu.addMenu(subMenu);
// Both menus destroyed when contextMenu.exec() returns
```

### HTML in QMessageBox

Always escape user input when embedding in HTML messages:

```cpp
QString path = userInput.toHtmlEscaped();
QMessageBox::information(this, title, tr("<p>Path: %1</p>").arg(path));
```

### Signal Connections with Lambdas

When connecting dialog signals, prefer lambdas or member functions over SLOT():

```cpp
// Good - compile-time checked
connect(action, &QAction::triggered, this, &MainWindow::myMethod);

// Good - explicit captures
connect(action, &QAction::triggered, this, [this, dirPath]() {
    myMethod(dirPath);
});

// Avoid - runtime string matching
connect(action, SIGNAL(triggered()), SLOT(myMethod()));
```

### Git Workflow in QtPass

Git operations are handled in `ImitatePass`:

- `executeGit(GIT_ADD, {path})` stages files
- `gitCommit(file, message)` commits with message
- Use `QtPassSettings::isUseGit()` to check if Git is enabled

### Qt Version Compatibility

```cpp
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif
```

### Testing Patterns

**Test file fixtures** are in `tests/fixtures/`:

```bash
ls tests/fixtures/
# gpg-colons-multi-key.txt
# gpg-colons-public.txt
# gpg-colons-secret.txt
```

These contain real GPG `--with-colons` output for deterministic testing.

**Avoid tautology assertions:**

```cpp
// Bad - always true
QVERIFY(profiles.isEmpty() || !profiles.isEmpty());

// Good - meaningful check
QVERIFY2(store.isEmpty() || store.startsWith("/"), "Pass store should be empty or a plausible path");
```

### Gitleaks-Safe Test Values

- DON'T: "ABC123DEF456", "sk-xxx", real API keys
- DO: "testkey123", "/usr/bin/pass", "example.com"

### Template File Format

Templates use INI-style format:

```ini
[Login]
username
password
url

[SSH]
username
key_path
key_type
```

Per-folder `.default_template` contains just the template name.

### Path Walking for .gpg-id

When checking for `.gpg-id`, walk up parent directories:

```cpp
QString gpgIdPath = dir;
while (QDir::cleanPath(gpgIdPath) != QDir::cleanPath(storeRoot)) {
    if (QFile(gpgIdPath + "/.gpg-id").exists()) {
        return gpgIdPath + "/.gpg-id";
    }
    if (!QDir(gpgIdPath).cdUp()) break;
    gpgIdPath = QDir(gpgIdPath).path();
}
```

This supports nested folder inheritance.

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
