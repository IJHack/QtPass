# Adding a new language

Bootstrap an empty skeleton and let `make lupdate` fill it. Do not copy
`localization_en_US.ts`: that imports English-as-translation strings that then
have to be cleared one by one.

## Choose the locale code

Prefer the **language-only** code (`fa`, `ar`, `de`) when the translation is the
standard form understood across regions. `main/main.cpp` loads translations
with the `QLocale`-aware `QTranslator::load()` overload, so Qt falls back from
`fa_IR`, `ar_MA` or `de_AT` to the language-only file. One `fa` file covers
Iran, Afghanistan (Dari) and every other Persian-speaking user.

Use `xx_YY` (or a script suffix such as `zh_Hant`, `sr_Cyrl`) only when the
text is genuinely regional: Brazilian versus European Portuguese, Simplified
versus Traditional Chinese, regional Spanish variants.

## Bootstrap

1. Create the skeleton. `lupdate` refuses to populate a 0-byte file:

   ```bash
   cat > localization/localization_<lang>.ts <<'EOF'
   <?xml version="1.0" encoding="utf-8"?>
   <!DOCTYPE TS>
   <TS version="2.1" language="<lang>">
   </TS>
   EOF
   ```

2. Append `../localization/localization_<lang>.ts` to the `TRANSLATIONS +=` list
   in `src/src.pro` (the list is not sorted; recent locales go at the end).
   `qtpass.pro` is only the parent project. The `make lupdate` target globs
   `localization/*.ts`, so it would find the file anyway, but without the
   `src/src.pro` entry the `.qm` is not embedded and the app never loads it.

3. Populate it:

   ```bash
   qmake6 && make lupdate 2>&1 | grep "localization_<lang>.ts"
   # Updating 'localization/localization_<lang>.ts'...
   ```

   Run `make distclean` first if you built in-source.

4. Run the verify loop from [SKILL.md](../SKILL.md#verify-loop). `lrelease`
   should report every entry as unfinished.

5. Optional: if the locale is language-only, add a `<lang>_<REGION> -> <lang>`
   row to `loadFallback_data()` in `tests/auto/locale/tst_locale.cpp`, like the
   existing `fa_IR -> fa` and `bn_BD -> bn` rows.

## What the PR contains

- The populated file with every entry `type="unfinished"` and an empty
  `<translation>`. That is the deliverable; Weblate translators fill it.
- The one-line `src/src.pro` change.

The "fill empty entries with best-effort text" guidance applies only when
patching an **existing** locale that reviewers flagged, not to a fresh skeleton.
Optionally translate a few high-value strings (window title, About box) in Qt
Linguist before pushing.

## Worked example: Persian (`fa`)

Commit `75bbdfa6e` ("i18n: add Persian (fa) translation skeleton"): the 4-line
skeleton, one `TRANSLATIONS +=` line in `src/src.pro`, one source refresh. At
the time this produced 304 entries marked `type="unfinished"` (302 empty plus 2
`numerusform` plural skeletons). Bengali (`bn`, commit `5a2340d40`) followed the
same pattern.
