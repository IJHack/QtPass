---
name: qtpass-localization
description: Refreshes QtPass translation sources with make lupdate, edits and verifies localization/*.ts files, bootstraps new languages in src/src.pro, and handles reviewer fixes alongside Weblate. Use when working on translations, i18n, a locale or .ts file, Weblate, adding a language, lupdate, lrelease, or unfinished strings.
license: GPL-3.0-or-later
metadata:
  audience: developers
  workflow: localization
  version: "1.1"
---

# QtPass Localization

Translations are Qt Linguist `.ts` files in `localization/localization_<lang>.ts`
(64 locales at the time of writing; re-derive with
`ls localization/localization_*.ts | wc -l`). They are managed on
[Weblate](https://hosted.weblate.org/projects/qtpass/qtpass/). The build turns
them into `.qm` files (`CONFIG += lrelease embed_translations` in `src/src.pro`)
and embeds them under `:/localization`; `.qm` files are never committed.

Reference files (one level deep, read only when needed):

- [references/adding-a-language.md](references/adding-a-language.md): read when
  adding a new locale (code choice, skeleton, `src/src.pro` registration).
- [references/mnemonics.md](references/mnemonics.md): read when a string has an
  `&` accelerator or a reviewer flags a missing or clashing shortcut.
- [references/status-and-runtime.md](references/status-and-runtime.md): read
  when you need per-locale completion stats or want to see a translation in the
  running app.
- Structural checks (placeholders, HTML balance, mnemonics, mixed-script MT
  artifacts) live in the sibling skill
  [qtpass-localization-audit](../qtpass-localization-audit/SKILL.md). Use it
  rather than writing new audit scripts.

## Workflow 1: refresh sources after `tr()` changes

`lupdate` is an explicit target in `qtpass.pro`. A plain `qmake6` run no longer
touches `localization/`.

```bash
make distclean   # only if you built in-source: stale ui_*.h in src/ get scanned
qmake6           # at the repository root, Qt 6.8 or newer
make lupdate     # lupdate over src/ and main/, rewrites every localization/*.ts
```

Never run `lupdate` on `src/src.pro` alone: that misses the strings in `main/`.

What `make lupdate` changes: source text, `<location>` line numbers and file
names, and the status of changed messages (set to `type="unfinished"`; removed
ones become `type="vanished"`). Existing `<translation>` text is kept.

Then run the verify loop below on a few locales and commit `localization/` as
its own change (for example `chore: update localization source references`).

## Workflow 2: edit a `.ts` file directly

Weblate is the normal path. Direct edits are appropriate for:

- Reviewer-flagged bugs: placeholder mismatches (`% 1` vs `%1`), broken HTML,
  made-up words from machine translation, mixed-script artifacts.
- Batch corrections from native-speaker reviewers on a PR.
- Filling empty entries in an **existing** locale with best-effort text marked
  `type="unfinished"`, so Weblate reviewers can refine it.
- Normalising terminology across strings (for example one word for
  "clipboard" instead of three variants).

Rules while editing:

- Keep `%1`, `%2`, `%n` and `\n` exactly as in the source.
- Keep HTML tags balanced and identical in structure to the source.
- Keep tokens such as `.gpg-id`, `pass`, `GPG`, `Git` untranslated.
- Mark new text `type="unfinished"` unless a native speaker confirmed it.
- Use Qt Linguist (`linguist localization/localization_<lang>.ts`) or a text
  editor; with a script, edit only the `<translation>` element, never the
  `<source>` line (tools strip its trailing whitespace and the message then no
  longer matches).

### Handling reviewer findings

Findings tend to arrive in batches (CodeRabbit, GitHub code quality, native
speakers):

1. Confirm each finding against the current `.ts`; reviewer diffs may target an
   older state.
2. Fix only what is wrong. Reject suggestions that break placeholders,
   mnemonics or established terminology, and say why.
3. Search the file for sibling occurrences of the same mistake and fix them too;
   list flagged versus proactive fixes in the commit message.
4. Run the verify loop.

## Workflow 3: add a language

Short form (details and a worked example in
[references/adding-a-language.md](references/adding-a-language.md)):

1. Prefer a language-only code (`fa`, `de`) unless the text is region-specific.
2. Create a 4-line XML skeleton (do not copy `localization_en_US.ts`).
3. Append `../localization/localization_<lang>.ts` to `TRANSLATIONS +=` in
   `src/src.pro`.
4. `qmake6 && make lupdate`, confirm the file was populated.
5. Commit with all entries `type="unfinished"`; Weblate translators fill them.

## Verify loop

Run after any `.ts` change, before pushing:

```bash
f=localization/localization_<lang>.ts
xmllint --noout "$f"                          # well-formed XML
lrelease6 "$f" -qm "$(mktemp --suffix=.qm)"   # compiles, prints counts
```

1. `xmllint` must print nothing. `lrelease6` (or `lrelease-qt6` / `lrelease`)
   must print `Generated N translation(s) (F finished and U unfinished)` with no
   errors. Writing the `.qm` to a temporary file keeps the tree clean.
2. Run the Quick audit from
   [qtpass-localization-audit](../qtpass-localization-audit/SKILL.md) on the
   same file. `lrelease` does not catch placeholder or HTML problems.
3. If anything is reported, fix the `.ts` and go back to step 1. Stop only when
   all checks are clean.
4. For source-reference refreshes or a new locale, also build and run
   `tests/auto/locale/tst_locale`: its project globs every
   `localization_*.ts`, so a broken file fails the test build.

## Pull requests

- Branch, then commit with `git commit -s -S` (DCO sign-off plus GPG
  signature). See the `qtpass-github` skill for the full PR flow.
- To fix a translation on someone else's PR: `gh pr checkout <PR>`, `git pull`
  first (bots may have pushed), edit, verify, push to the same branch.
- Translation PRs are usually squash-merged:
  `gh pr merge <PR> --squash --auto --delete-branch`.
- `.ts` merge conflicts are mostly `<location>` line numbers. Resolve by taking
  one side's file for the conflicting locale, rerun `make lupdate` if your
  branch changed `tr()` strings, then run the verify loop. During a rebase,
  `--theirs` is your own commit, not `main`.

## Source-code side

- Wrap every user-facing string in `tr()`.
- Use `tr("Delete %1?").arg(name)`, never string concatenation inside `tr()`.
- Use the plural form for counts: `tr("Found %n password(s)", nullptr, count)`.
- Keep technical terms consistent (`GPG`, `Git`, `clipboard`) so translators
  can reuse them.

## Common pitfalls

- **New strings missing from Linguist:** you did not run `make lupdate`, or
  Linguist still has the old file open.
- **Junk `ui_*.h` locations in `.ts` files:** an in-source build left generated
  headers in `src/`. Run `make distclean`, then `qmake6 && make lupdate`, and
  stage only the `localization/` changes you intended.
- **String not translated at runtime:** not wrapped in `tr()`, locale not listed
  in `src/src.pro` (so not embedded), or the entry is still empty. See
  [references/status-and-runtime.md](references/status-and-runtime.md).
- **Translation looks right but is `unfinished`:** the English source changed.
  Check the new source text and placeholders before clearing the flag.
- **Do not run prettier on `.ts` files:** prettier would treat them as
  TypeScript.
