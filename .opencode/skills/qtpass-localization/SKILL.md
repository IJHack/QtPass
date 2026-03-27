---
name: qtpass-localization
description: QtPass localization workflow - translation files, updating, adding languages
license: GPL-3.0-or-later
compatibility: opencode
metadata:
  audience: developers
  workflow: localization
---

# QtPass Localization

## Translation Files

Location: `localization/localization_<lang>.ts`

Qt uses Qt Linguist (`.ts` files) for translations.

### Existing Languages

```bash
ls localization/localization_*.ts
```

Currently includes: af_ZA, ar_MA, bg_BG, ca_ES, cs_CZ, cy_GB, da_DK, de_DE, de_LU, el_GR, en_GB, en_US, es_ES, et_ET, fi_FI, fr_BE, fr_FR, fr_LU, gl_ES, he_IL, hr_HR, hu_HU, it_IT, ja_JA, ko_KO, lb_LU, nb_NO, nl_BE, nl_NL, pl_PL, pt_PT, ro_RO, ru_RU, sk_SK, sl_SI, sq_AL, sr_RS, sv_SE, ta, tr_TR, uk_UA, zh_CN, zh_Hant

## Updating Translations

### After Code Changes

When source files change (functions moved, code refactored), translation line references become outdated:

```bash
# Run qmake to update translation source references
qmake6
```

This updates `<location filename="..." line="..."/>` in all `.ts` files without changing translations.

### What Gets Updated

- Source file line numbers
- File references (if files renamed)

### What Stays the Same

- Actual translation text
- Translation status

### Creating a PR for Updates

```bash
# Create branch
git checkout -b chore/update-localization

# Add and commit
git add localization/
git commit -m "chore: update localization source references"

# Push and create PR
git push -u origin chore/update-localization
gh pr create --title "chore: update localization source references" --body "## Summary

- Updated translation files with correct source file line numbers
- Translations remain unchanged; only metadata references were updated
"
```

## Adding New Strings

When you add new user-facing strings in C++:

```cpp
// Use tr() for translatable strings
ui->label->setText(tr("New text here"));
```

### Qt Linguist Workflow

1. Open `localization/localization_en_US.ts` in Qt Linguist
2. Find untranslated strings
3. Fill in translations
4. Save (automatically updates the .ts file)

### Key Conventions

- Use `tr()` for all user-facing strings
- Don't translate placeholders like `%1`, `%2`, `%3`
- Preserve `\n` for line breaks
- Keep technical terms consistent (e.g., "GPG", "clipboard")
- Use context comments to help translators understand usage

### String Formatting

```cpp
// Good
tr("Delete %1?").arg(filename)
tr("Found %n password(s)", count)

// Avoid
tr("Delete " + filename + "?")  // Can't be translated properly
```

## Adding New Language

1. Copy base translation file:
   ```bash
   cp localization/localization_en_US.ts localization/localization_XX_YY.ts
   ```

2. Update language code (ISO 639-1 + country code)

3. Add to build config in `qtpass.pro`:
   ```pro
   TRANSLATIONS += localization/localization_XX_YY.ts
   ```

4. Run qmake to register:
   ```bash
   qmake6
   ```

5. Translate strings using Qt Linguist

## Building Translations

### Generate .qm Files

```bash
# For development (or part of build process)
lrelease localization/*.ts

# Or via qmake
make translations
```

### .qm Files

- Binary format, loaded at runtime
- Generated from .ts files
- Typically not committed (generated during build)

## Localization Testing

### Switch Language

QtPass uses system locale. To test:

```bash
# Linux
LANG=nl_NL ./qtpass

# Or set in QtPass settings
```

### Debug Translation Issues

```cpp
// Enable verbose translation debugging
QTranslator translator;
if (translator.load(QLocale(), "qtpass", "_", ":/languages")) {
    qDebug() << "Loaded translation for:" << QLocale().name();
}
```

## Common Issues

### Strings Not Showing Translated

1. String not wrapped in `tr()` - fix in source
2. Translation file not loaded - check .pro TRANSLATIONS
3. .qm file not generated - run lrelease
4. Wrong locale - check system language settings

### Translation File Conflicts

When merging PRs with translation updates:

```bash
# Fetch and rebase
git fetch origin
git pull origin main --rebase

# Resolve conflicts in .ts files (usually just XML merge)
# Test with qmake6
```

## Linting

```bash
# Format with prettier if needed
npx prettier --write localization/*.ts
```
