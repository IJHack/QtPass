---
name: qtpass-localization
description: QtPass localization workflow - translation files, updating, adding languages
license: GPL-3.0-or-later
metadata:
  audience: developers
  workflow: localization
---

# QtPass Localization

## Translation Files

Location: `localization/localization_<lang>.ts`

Qt uses Qt Linguist (`.ts` files) for translations.

### Existing Languages

Always re-derive the current list at the moment of work, since new locales land regularly:

```bash
ls localization/localization_*.ts | sed 's|.*localization_||;s|\.ts$||'
```

Per-locale completion stats (finished / unfinished-with-text / empty):

```bash
python3 -c "
import re, glob, os
for f in sorted(glob.glob('localization/localization_*.ts')):
    loc = os.path.basename(f).replace('localization_','').replace('.ts','')
    content = open(f).read()
    fin=ut=ue=0
    for m in re.finditer(r'<translation([^>]*)>(.*?)</translation>', content, re.DOTALL):
        a,b = m.group(1), m.group(2)
        if 'vanished' in a: continue
        if 'unfinished' in a:
            if b.strip(): ut += 1
            else: ue += 1
        else: fin += 1
    print(f'{loc:8s}  {fin:3d}/{fin+ut+ue}  ut={ut:3d}  empty={ue}')
"
```

`unfinished-with-text` means a translation exists but is flagged for native-speaker review (Weblate convention).

## Updating Translations

### After Code Changes

When source files change (strings added, moved, or refactored), run qmake to update translations:

```bash
# IMPORTANT: Run distclean first to avoid stale generated files (ui_*.h) in translations
make distclean

# Run qmake to update translations (uses lupdate internally)
qmake6
```

This updates all `.ts` files with:

- Source file line numbers
- File references (if files renamed)
- **Source text** (if changed in source files - important!)
- Translation status (set to "unfinished" when source changed)

### What Stays the Same

- Translated text (in `<translation>` tags)
- Completed translations remain as-is until re-translated

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

- Updated translation files with current source texts and line numbers
- Translations marked unfinished where source text changed
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

Don't copy `localization_en_US.ts` — that imports stale English-as-translation strings that have to be cleared one by one. Bootstrap an empty skeleton and let `lupdate` (driven by `qmake6`) populate it with the current source strings as `type="unfinished"`.

### Locale code: language-only vs language+country

Prefer the **language-only** code (e.g. `fa`, `ar`, `de`) when the translation is the standard form understood across regions (Modern Standard Arabic, Standard German, Standard Persian).

Qt's locale fallback maps `fa_IR`, `ar_MA`, `de_AT` etc. to the language-only file automatically — one `fa.ts` covers Iran, Afghanistan (Dari), and every other Persian-speaking user.

Use the `xx_YY` form only when the translation is genuinely region-specific (Brazilian vs European Portuguese, Simplified vs Traditional Chinese, regional Spanish variants).

### Bootstrap workflow

1. Create a 4-line skeleton (`lupdate` refuses to populate a 0-byte file):

   ```bash
   cat > localization/localization_<lang>.ts <<'EOF'
   <?xml version="1.0" encoding="utf-8"?>
   <!DOCTYPE TS>
   <TS version="2.1" language="<lang>">
   </TS>
   EOF
   ```

2. Register it in `src/src.pro` (alphabetically) under `TRANSLATIONS +=`. Note: `qtpass.pro` is the parent project file; the actual list lives in `src/src.pro`.

3. Run `make distclean && qmake6` — the build's lupdate step populates the file with all 304 source strings as `type="unfinished"`. Confirm via:

   ```bash
   qmake6 2>&1 | grep "localization_<lang>.ts"
   # -> Updating 'localization/localization_<lang>.ts'...
   ```

4. Translate strings via Qt Linguist or hand-edit the `.ts` XML, then push for Weblate to track.

### Worked example

Adding Persian (`fa`) for Iranian/Afghan/Tajik users involved exactly: 4-line skeleton, one `TRANSLATIONS +=` line in `src/src.pro`, one `make distclean && qmake6`. No copy step. Result: 304 entries marked `type="unfinished"` (302 empty + 2 numerusform plural skeletons), ready for Weblate.

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

## Common Pitfalls

### Forgetting distclean Before qmake

Always run `make distclean` before `qmake6` when updating translations:

```bash
# Wrong - may include stale generated files in translations
qmake6

# Correct - starts fresh
make distclean
qmake6
```

### New Strings Not Showing in Qt Linguist

If you added new `tr()` strings but they don't appear:

1. Run `qmake6` to update the `.ts` files
2. Close and reopen Qt Linguist
3. Check the file was actually saved after previous edit

### Translation Files Showing as "Finished"

When source strings change, translations are marked "unfinished" but may still appear correct. Always check:

- The source text in Qt Linguist matches your code
- Any placeholder formatting (`%1`, `%2`) matches

### Merging Translation Conflicts

When merging translation PRs that conflict:

```bash
# Use theirs strategy for .ts files (they're XML, prefer incoming)
git checkout --theirs localization/localization_*.ts
git add localization/
git commit -m "Resolve merge conflict - use theirs for translations"
```

### Weblate vs Local Editing

Translations are normally managed via [Weblate](https://hosted.weblate.org/projects/qtpass/qtpass/). However, **direct `.ts` edits are appropriate** for:

- Fixing reviewer-flagged bugs (placeholder mismatches, broken HTML, made-up words, mixed-script artifacts).
- Filling empty entries with best-effort translations marked `type="unfinished"` so Weblate native reviewers can refine them.
- Batch corrections suggested in PR review by native speakers.
- Standardising terminology across multiple strings (e.g., normalising `klipboard`/`klipbordas`/`Tabela e kopjimit` onto a single term).

When in doubt, fix and mark `type="unfinished"` so Weblate review can iterate. Only run `qmake6` to update **source** strings (English) when code changes — that step is mechanical and affects every locale file.

### Qt Mnemonic Conventions

Qt UI strings use `&` (rendered as `&amp;` in `.ts` XML) before a letter to mark a keyboard accelerator (e.g., `&Quit` underlines Q so Alt+Q triggers it). Conventions per script family:

- **Latin scripts** (de, fr, es, en, etc.): in-word `&Letter` — `&File`, `&Edit`, `&Quit`. Pick a non-conflicting letter; within a single menu, mnemonics must be unique.
- **CJK scripts** (zh, ja, ko): parens-at-start `(&L) text` — `(&F) 文件`, `(&Q) 退出`. Don't rely on native characters as mnemonics.
- **RTL scripts** (ar, he): either parens-at-start `(&L) text` (preferred for portability) or native-letter `&letter`.
- **Cyrillic / Greek / Tamil / Sinhala**: not strictly defined; in-word native letters (`&Покажи`) work if the user's keyboard supports them, otherwise use `(&L) text` parens form.

Preserve the source mnemonic letter in the translation when feasible, so the same shortcut works across locales. When this is not possible, choose a non-conflicting letter from the translated string.

Audit for missing mnemonics:

```bash
python3 -c "
import re, glob
mnemonic_sources = {'&amp;Use pass','Nati&amp;ve Git/GPG','&amp;Show','&amp;Hide','&amp;Restore','&amp;Quit','Mi&amp;nimize','Ma&amp;ximize'}
for f in sorted(glob.glob('localization/localization_*.ts')):
    content = open(f).read()
    for m in re.finditer(r'<message[^>]*>.*?<source>(.*?)</source>.*?<translation([^>]*)>(.*?)</translation>', content, re.DOTALL):
        src,a,t = m.group(1), m.group(2), m.group(3)
        if 'vanished' in a or src not in mnemonic_sources or not t.strip(): continue
        if not re.search(r'&amp;[^\s&;]', t):
            print(f'{f}: {src} -> {t}')
"
```

## Reviewer-Iteration Workflow

Translation review tends to come in batches of ~5 findings at a time (CodeRabbit, GitHub AI quality findings, native-speaker comments on PRs). Standard handling:

1. **Verify each finding against current code** before fixing. Reviewer-supplied diffs may be against an older state — confirm the broken text actually exists in the latest `.ts`.
2. **Fix only what's needed.** If a reviewer-suggested change is itself wrong (e.g., breaks Qt mnemonic conventions, reverses earlier-established terminology), reject with explanation.
3. **Apply same-pattern fixes if found in passing.** When fixing one occurrence of a typo, search the file for sibling occurrences of the same MT-pattern. Document in commit message which were flagged vs which were proactive.
4. **Run the audit script** (see `qtpass-localization-audit` skill) before pushing — placeholder format and HTML balance are non-obvious bugs that the reviewer's prose description may not catch.
5. **Mark new translations `type="unfinished"`** unless a native speaker has confirmed them. Weblate finalises them on review.

## Fixing Translation Issues in PRs

When static analysis flags translation issues (e.g., filename preservation):

```bash
# Fetch the PR branch
git fetch origin refs/pull/<PR>/head:pr/<PR>-fix
git checkout pr/<PR>-fix

# Fix the translation
# Edit the .ts file to preserve exact filename/token

# Commit with signing and push
git add localization/localization_<lang>.ts
git commit -S -m "fix: preserve .gpg-id filename in <lang> translation"
git push -u origin pr/<PR>-fix
```

### Squash Merge Pattern

Translation PRs often use squash merge:

```bash
gh pr merge <PR_NUMBER> --squash --auto --delete-branch
```
