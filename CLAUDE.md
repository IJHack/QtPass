# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## About QtPass

QtPass is a multi-platform GUI for [pass](https://www.passwordstore.org/), the standard Unix password manager. It supports Linux, BSD, macOS, and Windows, using either the `pass` command-line tool or direct `gpg2`/`Git` integration.

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

Qt 5.15 and Qt 6 are both supported (CI tests Qt 5.15 + Qt 6.8). The project uses qmake with a subdirs layout: `src/` (library), `main/` (executable), `tests/` (unit tests), shared config in `qtpass.pri`.

## Testing

Qt Test framework. Tests live in `tests/auto/` subdirectories: util, ui, model, settings, passwordconfig, filecontent, simpletransaction, gpgkeystate (all platforms); executor and integration are excluded on Windows per `tests/auto/auto.pro`.

```bash
# Run all tests
make check

# Run a specific test binary
./tests/auto/util/tst_util

# Run a single test function
./tests/auto/util/tst_util testFunctionName

# Verbose output
./tests/auto/util/tst_util -v2
```

## Linting

```bash
# C++ formatting
clang-format --style=file -i <source-file>

# Markdown/YAML formatting
npx prettier --write "**/*.md" "**/*.yml"
npx prettier --check "**/*.md" "**/*.yml"

# Local CI check
act push -W .github/workflows/linter.yml -j build
```

## Architecture

**Two-mode design:** QtPass operates in two modes controlled by `QtPassSettings`:

- **RealPass** (`src/realpass.h`): delegates all operations to the `pass` command-line tool
- **ImitatePass** (`src/imitatepass.h`): directly invokes `gpg2`/`git` when `pass` is unavailable

Both inherit from `Pass` (`src/pass.h`), an abstract base exposing the password store API via Qt signals/slots.

**Core classes:**

- `MainWindow` — central UI orchestrator; owns the `Pass` instance and `StoreModel`
- `Pass` / `RealPass` / `ImitatePass` — password store operations (add, edit, delete, copy, show, Git)
- `Executor` (`src/executor.h`) — FIFO queue for external process execution; all `gpg`/`git`/`pass` calls go through here
- `StoreModel` (`src/storemodel.h`) — `QSortFilterProxyModel` wrapping `QFileSystemModel` for the password tree
- `QtPassSettings` (`src/qtpasssettings.h`) — singleton managing all app configuration via `QSettings`
- `FileContent` (`src/filecontent.h`) — parses password files; supports template fields beyond the first line
- `Util` (`src/util.h`) — static helpers for path normalization and binary discovery

**Signal/slot flow:** `MainWindow` calls `Pass` methods → `Pass` queues commands via `Executor` → `Executor` emits signals with stdout/stderr → `Pass` processes output and emits higher-level signals → `MainWindow` updates the UI.

## Git Workflow

```bash
git checkout -b fix/description
git commit -S -m "description"          # always sign commits
git push -u origin fix/description
gh pr create --title "..." --body "## Summary\n- ..."
# Before merge: rebase onto main
git pull --rebase origin main
```

When CodeRabbit/AI flags a PR issue: verify it, fix if real, push, comment with resolution. If false positive, explain and mark resolved.

## Key Conventions

- Use `QCoreApplication::arguments()` instead of raw `argv[]` for CLI argument parsing
- Wrap all user-facing strings with `tr()`
- Use `QDir::cleanPath()` for cross-platform path normalization
- Use `std::as_const()` (project is C++17); `qAsConst()` is a legacy pre-C++17 fallback and should not be introduced in this codebase, including Qt 5.15 builds
- Store indices (not pointers) in `Qt::UserRole` data on model items
- Use `QPalette` colors instead of hardcoded values for theme-aware UI
- Check for null from `screenAt()` before dereferencing
- C++17 standard; SPDX license headers (`GPL-3.0-or-later`)

## Doxygen

CI enforces zero Doxygen warnings (`WARN_AS_ERROR = FAIL_ON_WARNINGS` in `Doxyfile`). Every public symbol in a header must have a `/** @brief ... */` doc block with `@param`/`@return` where applicable.

```bash
# Check locally — any output means CI will fail
doxygen Doxyfile
```

Common pitfalls: unnamed parameters in declarations, orphaned doc blocks not immediately above their declaration, missing `@return` on non-void functions.

## Localization

Translation files are `.ts` files in `localization/`. Translations are normally managed via [Weblate](https://hosted.weblate.org/projects/qtpass/qtpass/), but direct `.ts` edits are appropriate for:

- Fixing reviewer-flagged bugs (placeholder mismatches like `% 1` vs `%1`, broken HTML tags, made-up words from MT, mixed-script artifacts).
- Batch corrections suggested by native-speaker reviewers on a PR.
- Filling empty entries with best-effort translations marked `type="unfinished"` for Weblate to refine.

After changing **source** strings (English in code), run `qmake6` to refresh translation source references; that step is purely mechanical and affects every locale file.

## Skills

Specialized skills are available for common workflows:

- `qtpass-fixing` — bugfix patterns
- `qtpass-testing` — Qt Test structure and patterns
- `qtpass-linting` — CI/CD and formatters
- `qtpass-localization` — translation workflow
- `qtpass-localization-audit` — structural audit of `.ts` files (placeholders, HTML balance, mnemonics, mixed-script)
- `qtpass-github` — PRs, issues, merging
- `qtpass-releasing` — release process
- `qtpass-docs` — documentation guide
