---
name: qtpass-linting
description: QtPass CI/CD workflow - run GitHub Actions locally with act, linters, formatters
license: GPL-3.0-or-later
compatibility: opencode
metadata:
  audience: developers
  workflow: linting
---

# QtPass Linting and CI Workflow

## The Act Pattern

**Always run local CI before pushing PRs.** Use `act` to run GitHub Actions workflows locally.

### Why Use act?

- Catch linter failures **before** pushing
- Validate changes without waiting for CI
- Faster iteration loop

### The Workflow

```bash
# 1. Make your changes
git add .

# 2. Run linter locally (this is the pattern)
act push -W .github/workflows/linter.yml -j build

# 3. Fix any issues
# 4. Push only when act passes
```

### Quick Reference

| Task                      | Command                                             |
| ------------------------- | --------------------------------------------------- |
| Run linter                | `act push -W .github/workflows/linter.yml`          |
| Run linter (specific job) | `act push -W .github/workflows/linter.yml -j build` |
| Run build & tests         | `act push -W .github/workflows/ccpp.yml`            |
| Run docs                  | `act push -W .github/workflows/docs.yml`            |
| Run reuse check           | `act push -W .github/workflows/reuse.yml`           |

## Available Workflows

### Linter Workflow (.github/workflows/linter.yml)

Runs super-linter with many linters:

- **GITLEAKS** - Secret detection
- **CHECKOV** - Infrastructure scanning
- **CLANG_FORMAT** - C++ formatting
- **ACTIONLINT** - GitHub Actions YAML
- **PRETTIER** - Web/config file formatting
- **Markdown** - Markdown linting
- **NATURAL_LANGUAGE** - Natural language checks
- **YAML** - YAML linting

```bash
# Run linter locally
act push -W .github/workflows/linter.yml -j build
```

### Build & Test Workflow (.github/workflows/ccpp.yml)

QtPass build with Qt5/Qt6 matrix, runs unit tests, generates coverage:

```bash
# Run build workflow
act push -W .github/workflows/ccpp.yml
```

Tests against:

- Ubuntu + Qt 6.8
- macOS + Qt 6.8
- Windows + Qt 6.8
- Ubuntu + Qt 5.15

Note: Qt installation may fail in act due to environment limitations. Real CI handles this.

### Documentation Workflow (.github/workflows/docs.yml)

```bash
# Run docs workflow
act push -W .github/workflows/docs.yml
```

### Reuse Compliance (.github/workflows/reuse.yml)

Check license headers and REUSE compliance:

```bash
# Run reuse check
act push -W .github/workflows/reuse.yml
```

## Common Linters

### Gitleaks (Secret Detection)

Detects API keys, tokens, passwords in code.

```bash
# Scan for secrets
gitleaks detect
```

**Common Fixes:**

- Don't use test values that look like API keys (e.g., "ABC123DEF456", "sk-xxx")
- Use generic test strings: "testkey123", "/usr/bin/pass", "example.com"

### Clang Format (C++)

```bash
# Check formatting
clang-format --style=file --dry-run src/main.cpp

# Apply formatting
clang-format --style=file -i src/main.cpp
```

### Prettier (Web/Config)

```bash
# Format markdown, YAML, JSON, etc.
npx prettier --write README.md
npx prettier --write .github/workflows/*.yml
npx prettier --write FAQ.md
npx prettier --write .opencode/skills/*/SKILL.md
```

## Prettier Patterns

Prettier auto-fixes many linting issues. Run before `act`:

```bash
# Format all common file types
npx prettier --write "**/*.md" "**/*.yml" "**/*.json" "**/*.html" "**/*.css"
```

### Markdown Natural Language Issues

If NATURAL_LANGUAGE fails:

- Use single-word forms instead of two-word combinations
- Use full terms instead of abbreviations
- Use proper spacing and punctuation

```bash
# Run prettier first
npx prettier --write README.md

# Then check again
act push -W .github/workflows/linter.yml -j build
```

## Troubleshooting

### Qt Installation Fails in act

The `install-qt-action` may fail in local act due to missing downloads. This is expected - real CI works fine.

### Linter Fails Due to Missing Files

Some checks need files generated during CI. Run full build first:

```bash
qmake6 -r CONFIG+=coverage
make -j4
```

### Gitleaks False Positives

If gitleaks flags test data:

1. Use generic test values (not like "ABC123DEF456")
2. Add to `.gitleaksignore` if truly non-sensitive

### act Unknown Flag Error

Make sure act is installed and up to date:

```bash
# Check version
act --version

# Update if needed
brew upgrade act  # or your package manager
```

## CI Environment Variables

Some linters need secrets or tokens. In local act, these may not be available:

```bash
# Pass fake token for codecov
act push -W .github/workflows/ccpp.yml --secret-map "CODECOV_TOKEN=fake"
```

## GitHub Actions Files

| File                           | Purpose                    |
| ------------------------------ | -------------------------- |
| `.github/workflows/linter.yml` | Super-linter (many checks) |
| `.github/workflows/ccpp.yml`   | Build & test with Qt       |
| `.github/workflows/docs.yml`   | Doxygen docs generation    |
| `.github/workflows/reuse.yml`  | REUSE compliance           |
| `.github/super-linter.env`     | Linter configuration       |

## Run Before PR Checklist

**THE PATTERN - always run before pushing:**

```bash
# 1. Format files with prettier
npx prettier --write "**/*.md" "**/*.yml" "**/*.json"

# 2. Verify formatting passes
npx prettier --check "**/*.md" "**/*.yml"

# 3. Run linter locally
# WARNING: may fail on new branches with "HEAD~0" error - this is a known act bug
act push -W .github/workflows/linter.yml -j build || echo "act failed - will rely on CI"

# 4. Only then push
git push
```

### Important: act Limitations

**`act` may fail on new branches with error:** `fatal: ambiguous argument 'HEAD~0'`

This is a known issue with the tool, not your code. When this happens:
- The prettier check (step 2) is sufficient
- Trust that formatting is correct
- The real GitHub CI will pass

### Manual Prettier Check (Recommended Alternative)

```bash
# Check specific file
npx prettier --check README.md

# Check all markdown
npx prettier --check "**/*.md"
```

## Related Skills

- **qtpass-testing** - For testing patterns and `make check`
- **qtpass-fixing** - For bugfix workflow with tests
- **qtpass-releasing** - For release process
