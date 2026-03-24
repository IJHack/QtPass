---
name: qtpass-linting
description: QtPass CI/CD and linting workflow - run linters locally with act
license: GPL-3.0-or-later
compatibility: opencode
metadata:
  audience: developers
  workflow: linting
---

# QtPass Linting and CI Workflow

## Why Use act?

Run GitHub Actions workflows locally to validate changes before pushing. This catches linter failures early.

## Quick Start

```bash
# Run full linter workflow
act push -W .github/workflows/linter.yml

# Run specific job
act push -W .github/workflows/linter.yml -j build
```

## Available Workflows

### Linter Workflow (.github/workflows/linter.yml)

Runs super-linter with many linters:

- **GITLEAKS** - Secret detection
- **CHECKOV** - Infrastructure scanning
- **CLANG_FORMAT** - C++ formatting
- **ACTIONLINT** - GitHub Actions YAML
- **PRETTIER** - Web/config file formatting
- And many more...

```bash
# Run linter locally
act push -W .github/workflows/linter.yml
```

### Build & Test Workflow (.github/workflows/ccpp.yml)

QtPass build with Qt6, runs unit tests, generates coverage:

```bash
# Run build workflow
act push -W .github/workflows/ccpp.yml
```

Note: Qt installation may fail in act due to environment limitations. Real CI handles this.

### Documentation Workflow (.github/workflows/docs.yml)

```bash
# Run docs workflow
act push -W .github/workflows/docs.yml
```

### Reuse Compliance (.github/workflows/reuse.yml)

Check license headers and REUSE compliance:

```bash
act push -W .github/workflows/reuse.yml
```

## Common Linters

### Gitleaks (Secret Detection)

Detects API keys, tokens, passwords in code.

```bash
# Scan for secrets
gitleaks detect

# Or via super-linter in linter.yml
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
```

## CI Environment Variables

Some linters need secrets or tokens. In local act, these may not be available:

```bash
# Pass fake token for codecov
act push -W .github/workflows/ccpp.yml --secret-map "CODECOV_TOKEN=fake"
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

## GitHub Actions Files

| File                           | Purpose                    |
| ------------------------------ | -------------------------- |
| `.github/workflows/linter.yml` | Super-linter (many checks) |
| `.github/workflows/ccpp.yml`   | Build & test with Qt       |
| `.github/workflows/docs.yml`   | Doxygen docs generation    |
| `.github/workflows/reuse.yml`  | REUSE compliance           |
| `.github/super-linter.env`     | Linter configuration       |

## Run Before PR

Always run linter locally before creating PR:

```bash
# Full linter check
act push -W .github/workflows/linter.yml

# Should exit with code 0
echo $?  # Should be 0
```
