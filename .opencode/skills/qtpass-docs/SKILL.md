---
name: qtpass-docs
description: Documentation guide for QtPass - README, FAQ, localization
license: GPL-3.0-or-later
compatibility: opencode
metadata:
  audience: developers
  workflow: documentation
---

## Documentation Files

| File               | Purpose                                     |
| ------------------ | ------------------------------------------- |
| README.md          | Main documentation, installation, usage     |
| FAQ.md             | Frequently asked questions, troubleshooting |
| CHANGELOG.md       | Release history, changes                    |
| CONTRIBUTING.md    | Developer contribution guidelines           |
| CODE_OF_CONDUCT.md | Community code of conduct                   |

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
- GPG/Key issues
- Git integration
- Password store issues
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

Location: `localization/localization_<lang>.ts`

### Update Translations

```bash
# Run lupdate to scan for new strings
lrelease localization/*.ts
```

### Key Conventions

- Use Qt Linguist (`linguist`) for editing
- Don't translate placeholders like `%1`, `%2`
- Preserve `\n` line breaks

### Adding New Language

1. Copy `localization/localization_en_US.ts`
2. Rename to `localization_<code>.ts`
3. Update in `qtpass.pro`: `TRANSLATIONS += ...`
4. Run lupdate

## Docs Build

### Doxygen

```bash
# Generate docs
doxygen Doxyfile

# View
firefox html/index.html
```

## Linting

### Markdown (prettier)

```bash
npx prettier --write README.md
npx prettier --write FAQ.md
```

### YAML (prettier)

```bash
npx prettier --write .github/workflows/*.yml
```
