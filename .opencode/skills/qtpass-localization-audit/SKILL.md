---
name: qtpass-localization-audit
description: Audits QtPass localization/*.ts files for broken %1/%n placeholders, unbalanced HTML tags, lost & mnemonics, mixed-script and cross-language artifacts, then fixes and re-audits until clean. Use when reviewing translations or a Weblate PR, checking a machine-translated batch, or chasing a placeholder mismatch or broken HTML in a .ts file.
license: GPL-3.0-or-later
metadata:
  audience: developers
  workflow: localization
  version: "1.1"
---

# QtPass Localization Audit

Structural checks for Qt Linguist `.ts` files. They catch translations that read
plausibly but break the UI: a `% 1` that Qt never substitutes, a dropped `</b>`
that turns a whole dialog bold, a menu entry that lost its `&` shortcut.

For the general translation workflow (`make lupdate`, editing `.ts` files,
adding a language, mnemonic conventions, PR mechanics) use the sibling skill
[qtpass-localization](../qtpass-localization/SKILL.md). This skill only audits
and cleans up.

Bundled files (one level deep):

- [scripts/audit_ts.py](scripts/audit_ts.py): the checker. Run it, do not
  rewrite it inline.
- [references/mt-artifacts.md](references/mt-artifacts.md): read when a string
  passes the script but looks wrong, or when deciding whether a machine
  translation is salvageable.

## What the script checks

Run from the repository root. With no file arguments it audits every
`localization/localization_*.ts`. Vanished and empty entries are skipped;
`type="unfinished"` entries with text are audited.

| Code    | Fires when                                                                                                                    | Exit status |
| ------- | ----------------------------------------------------------------------------------------------------------------------------- | ----------- |
| `PH`    | `%1`, `%2` ... differ from the source in any plural form, `%n` is missing from every form, or a `% 1` / `% n` space appears   | 1           |
| `MN`    | source has a mnemonic (`&Show`) and the translation has none                                                                  | 1           |
| `HTML`  | per-tag open or close counts differ from the source (the missing or extra tags are printed)                                   | 1           |
| `MIX`   | Latin letters glued to a word in the locale's own script, unless that Latin run also appears in the source (`gpg`, `QtPass`)  | 1           |
| `ALPHA` | the password generator alphabet lost a source character or gained a separator (appending local letters is fine)               | 1           |
| `XML`   | the file declares XML entities: refused unparsed (a real `.ts` has only `<!DOCTYPE TS>`); parsed with defusedxml if installed | 1           |
| `SAME`  | with `--verbatim`: translation identical to the source (MT fallback or a never-translated string)                             | 0           |
| `LEAK`  | with `--leak`: translation contains one of the given words from another language                                              | 0           |

`MIX` runs only for locales with a non-Latin script in the script's `SCRIPT`
table. Add a range there when a new non-Latin locale lands.

## Quick audit

```bash
A=.opencode/skills/qtpass-localization-audit/scripts/audit_ts.py

python3 "$A" localization/localization_<lang>.ts   # one file, every finding
python3 "$A" --summary                             # all files, counts only
python3 "$A" --verbatim localization/localization_<lang>.ts
python3 "$A" --leak mogoče,prepričani,odložišče,počiščeno --skip sl,hr,sr_RS,sr_Cyrl
```

For `--leak`, pick words that only the unintended language uses, and `--skip`
that language plus its close relatives (Slovenian shares many words with
Croatian and Serbian). Output is `locale: [CODE] 'source'` followed by
`-> 'translation'`; the last line is `total: clean` or the counts.

To audit only the files a branch or PR touches:

```bash
git diff --name-only origin/main...HEAD -- 'localization/*.ts' | xargs -r python3 "$A"
```

## Audit, fix, re-audit loop

Run this on every machine-translated batch, every Weblate PR, and after
reviewer refinements to long or HTML-bearing strings.

1. **Audit**: `python3 "$A" <files>`. Also run once with `--verbatim` and read
   [references/mt-artifacts.md](references/mt-artifacts.md) for a large MT
   batch; the script cannot judge meaning.
2. **Fix** each finding:

   | Finding                          | Action                                                                                       |
   | -------------------------------- | -------------------------------------------------------------------------------------------- |
   | `PH` with an obvious fix         | Correct the placeholder (`% 1` to `%1`, restore a dropped `%2`)                              |
   | `PH` where the meaning is unsure | Revert to `<translation type="unfinished"></translation>` for a native reviewer              |
   | `HTML` on a long paragraph       | Revert to empty unfinished; re-tagging prose you cannot read invents errors                  |
   | `MN`                             | Add the `&` per the mnemonic rules in [qtpass-localization](../qtpass-localization/SKILL.md) |
   | `MIX`                            | Revert to empty unless one obvious fix applies (an English word left in, a glued fragment)   |
   | `ALPHA`                          | Restore the source string verbatim                                                           |
   | `XML`                            | Do not merge it: entity declarations in a contributed `.ts` are an attack or a broken tool   |
   | `LEAK`, `SAME`                   | Read the string: keep product names and accepted loanwords, revert real leaks to empty       |

   When editing `.ts` files directly, keep the `<source>` line out of the edit:
   trailing whitespace in it is significant and editors strip it.

3. **Re-audit**: run the same command again. Repeat steps 2 and 3 until it
   prints `total: clean` (exit status 0). Do not stop on "mostly clean".
4. **Compile check**:

   ```bash
   f=localization/localization_<lang>.ts
   xmllint --noout "$f"                          # must print nothing
   lrelease6 "$f" -qm "$(mktemp --suffix=.qm)"   # Generated N translation(s) ...
   ```

   `lrelease6` catches malformed XML but not placeholder
   or HTML problems, so it complements the audit rather than replacing it.
   Writing the `.qm` to a temporary file keeps `localization/` clean.

## Cleaning up a machine-translated contribution

Keep the contribution and the cleanup as separate commits so reviewers can see
what changed:

```bash
git switch -c l10n/<lang>-from-<contributor> origin/main
cp /path/to/contributed/localization_<lang>.ts localization/
git add localization/localization_<lang>.ts
git commit -s -S --author="Contributor Name <email>" \
  -m "l10n: <lang> translations from <Contributor>"
# audit, fix, re-audit loop above, then:
git add localization/localization_<lang>.ts
git commit -s -S -m "l10n: <lang>: fix placeholders, HTML and MT artifacts"
```

Push and open the PR as described in
[qtpass-localization](../qtpass-localization/SKILL.md), crediting the
contributor in the PR description.

## When to skip the audit

- A pure terminology rename on one locale (for example `krātuve` to
  `glabātuve`): structure is unchanged by definition.
- A single-string typo fix from a known reviewer.
- Dropping `type="unfinished"` on translations that already passed an audit.

Everything else, especially machine-translated input, gets the loop: a full
sweep takes seconds and saves a follow-up PR.
