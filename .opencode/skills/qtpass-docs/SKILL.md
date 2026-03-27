---
name: qtpass-docs
description: Documentation guide for QtPass - README, FAQ, localization
license: GPL-3.0-or-later
compatibility: opencode
metadata:
  audience: developers
  workflow: documentation
---

# Project Documentation

## QtPass Documentation Files

| File               | Purpose                                     |
| ------------------ | ------------------------------------------- |
| README.md          | Main documentation, installation, usage     |
| FAQ.md             | Frequently asked questions, troubleshooting |
| CHANGELOG.md       | Release history, changes                    |
| CONTRIBUTING.md    | Developer contribution guidelines           |
| CODE_OF_CONDUCT.md | Community code of conduct                   |
| Doxyfile           | API documentation configuration             |
| Windows.md         | Windows-specific installation and build     |

## README.md Sections

- Badges (build, license, version)
- Description
- Features
- Installation (Linux, macOS, Windows)
- Usage / Getting Started
- Configuration
- Acknowledgments

## FAQ.md Sections

- Installation issues
- Configuration issues
- GPG/Key issues
- Git integration
- Platform-specific (macOS, Windows, Linux)

### FAQ Template

```markdown
## Question Title

**Problem:** Description of the issue

**Solution:** Step-by-step solution

**Related:** Links to relevant issues
```

## Localization

### Translation Files

Location: `localization/localization_<lang>.ts` (e.g., `localization/localization_en_US.ts`)

### Update Translations

```bash
# Run translation tools to scan for new strings
lrelease localization/*.ts
# Or use project-specific i18n command
```

### Key Conventions

- Use appropriate translation editor
- Don't translate placeholders like `%1`, `%2`
- Preserve `\n` line breaks
- Keep technical terms consistent

### Adding New Language

1. Copy base translation file
2. Rename with language code
3. Update in build config: `TRANSLATIONS += ...`
4. Run translation update tool

## Docs Build

### API Documentation

```bash
# Generate API docs
doxygen Doxyfile
# Or use project-specific docs command

# View
open html/index.html
```

## Linting

**THIS IS THE PATTERN - always run before pushing:**

```bash
# Check formatting (this is the pattern)
npx prettier --check "**/*.md"

# Format all markdown files
npx prettier --write "**/*.md"

# Format specific file
npx prettier --write README.md
```

### Markdown (prettier)

```bash
npx prettier --write <markdown-file>
npx prettier --write .opencode/skills/*/SKILL.md
```

### YAML (prettier)

```bash
npx prettier --write <yaml-file>
npx prettier --write .github/workflows/*.yml
```
