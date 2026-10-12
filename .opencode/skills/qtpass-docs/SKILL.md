---
name: qtpass-docs
description: Updates QtPass documentation (README.md, FAQ.md, CHANGELOG.md, CONTRIBUTING.md, Doxygen comments), formats it with prettier and keeps qtpass.org pages in step. Use when editing docs, adding a FAQ or troubleshooting entry, writing a CHANGELOG line, fixing a Doxygen warning that fails CI, or formatting Markdown.
license: GPL-3.0-or-later
metadata:
  audience: developers
  workflow: documentation
  version: "1.1"
---

# QtPass Documentation

## Files

| File                 | Purpose                                                                 |
| -------------------- | ----------------------------------------------------------------------- |
| `README.md`          | Overview, features, installation, profiles, security, known issues      |
| `FAQ.md`             | Troubleshooting questions and answers                                   |
| `CHANGELOG.md`       | Release history, newest first, `[Unreleased]` on top                    |
| `CONTRIBUTING.md`    | Getting started, PR process, AI assistance, translations, Windows setup |
| `CODE_OF_CONDUCT.md` | Community code of conduct                                               |
| `ROADMAP.md`         | Plans, in "Now" and "Next" sections                                     |
| `SECURITY.md`        | Supported versions, vulnerability reporting, what QtPass protects       |
| `Windows.md`         | Windows install, build from source, troubleshooting                     |
| `Doxyfile`           | API documentation config, enforced in CI (see below)                    |

Translations of the application are a separate workflow: use the
`qtpass-localization` skill.

## Writing rules

- No em dashes in prose: use a colon, semicolon, comma or parentheses,
  whichever the sentence needs.
- Spell `Git`, `macOS`, `ID` and `filename` the way textlint wants them.
- Link issues and PRs with full URLs,
  `[#123](https://github.com/IJHack/QtPass/issues/123)`; a bare `issues/123`
  path resolves against the file's folder and breaks.
- Link between repository files relatively (`FAQ.md#heading-id`); the
  site build rewrites those links (see below).

## Site relation

qtpass.org lives on the `gh-pages` branch. Three of its pages are rendered from
this repository by `tools/build-page.py` on that branch:

| Repository file      | Page                                 |
| -------------------- | ------------------------------------ |
| `FAQ.md`             | <https://qtpass.org/faq>             |
| `CONTRIBUTING.md`    | <https://qtpass.org/contributing>    |
| `CODE_OF_CONDUCT.md` | <https://qtpass.org/code-of-conduct> |

No workflow reruns it: a change to these files reaches the site only when the
page is rebuilt on `gh-pages`. Headings keep GitHub-style IDs, so renaming a
heading breaks `faq#...` deep links on the site as well as on GitHub. The
Doxygen HTML is different: CI publishes it to <https://qtpass.org/docs/> on
every push to `main`.

## Adding a FAQ entry

`FAQ.md` has `## Issues` (troubleshooting) and
`## How can I help improve QtPass?`. Entries are `###` questions phrased the
way a user would ask them:

```markdown
### GPG says "Public key unusable" or "No secret key"

Your GPG key may not be trusted or has expired. Run `gpg --edit-key <KEYID>` and:

- Set the trust level to "ultimate" (`trust` command)
- Check if the key has expired (`expire` command to extend if needed)
```

1. Add the question under the right `##` section, next to related entries.
2. Answer with short bullets or a paragraph; put commands and config in
   fenced code blocks with a language tag.
3. Link the issue that prompted it with a full URL.
4. Run the lint block below.
5. Check rendering: in the PR's "Files changed" tab, switch the file to the
   rich diff and confirm the new heading's anchor is the one you expect.
6. If you renamed a heading, find links to the old anchor:
   `grep -rn "old-heading-id" --include=*.md .`

## Updating README.md

- Download links point at <https://qtpass.org/downloads>, not at release
  assets, so a release does not touch README.md. Version bumps are covered by
  the `qtpass-releasing` skill.
- When changing install or build instructions, run the commands first and
  check the dependency list against `qtpass.pri` and CI.
- Screenshots live on the site (`gh-pages`), not in this repository.

## Writing a CHANGELOG entry

Add a bullet under the matching section of `## [Unreleased]`, ending with the
PR link. Read [references/changelog.md](references/changelog.md) for the
heading format, section names, MD024 markers and the lint traps.

## Doxygen comments

Every public symbol in a header needs a `/** @brief ... */` block with
`@param` for each parameter and `@return` for non-void functions. CI fails on
any Doxygen warning. Read [references/doxygen.md](references/doxygen.md) when
the `docs` job fails, for the Doxyfile settings, the local check and the
mistakes that cause warnings.

Quick check before pushing a header change:

```bash
doxygen Doxyfile >/dev/null   # anything printed, or a non-zero exit, fails CI
```

## Linting

Run before every push that touches Markdown or YAML:

```bash
npx prettier --write <changed .md or .yml files>
npx prettier --check "**/*.md" "**/*.yml"
grep -nP "\x{2014}" <changed .md files>   # em dashes; must print nothing
```

Super-linter in CI also runs markdownlint and textlint. To reproduce it
locally, use the `qtpass-linting` skill.

## Version-specific notes (October 2026)

These facts change over time; check them against the repository before
relying on them.

- CI pins Doxygen 1.18.0 (`DOXYGEN_VERSION` in `documentation.yml`) and
  verifies the archive's SHA-256. A different local version can report
  differently.
- README.md contains no version numbers:
  `grep -nE "[0-9]+\.[0-9]+\.[0-9]+" README.md` prints nothing.
- The CHANGELOG example in `references/changelog.md` is the 1.8.1 release of
  September 2026; copy its shape, not its content.
- The no-em-dash rule has been applied to `FAQ.md`, `README.md` and
  `CONTRIBUTING.md`; the `[Unreleased]` heading in `CHANGELOG.md` still has one.
