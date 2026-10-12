# CHANGELOG.md format

Read this before adding or editing a CHANGELOG.md entry.

## Layout

- Newest release first. Unreleased work sits under the `## [Unreleased]`
  heading at the top.
- A release heading links its tag and gives the date:
  `## [X.Y.Z](https://github.com/IJHack/QtPass/tree/vX.Y.Z) (YYYY-MM-DD)`.
- Sections in use: `Upgrade Notes`, `New Features`, `Security`, `Changed`,
  `Bugfixes`, `Code Quality`, `Tests`, `Localization`, `Build / CI`,
  `Removed`. Use an existing one before inventing a new one.
- A heading that repeats one from an earlier release carries
  `<!-- markdownlint-disable-line MD024 -->`, otherwise markdownlint fails.

## Entries

- One bullet per change, written for users: what was wrong or what is new,
  then the effect.
- End the bullet with the PR link, and `closes` plus the issue link when there
  is one: `[#1234](https://github.com/IJHack/QtPass/pull/1234)`.
- Backports name their origin: `(ported from [#1234](...))`.
- Parallel PRs add lines under the same heading and conflict there: on rebase,
  keep both lines.

## Lint traps

Super-linter runs prettier without the repository config, plus markdownlint
and textlint:

- Emphasis uses `_x_`, not `*x*` (MD049 takes the first style in the file).
- Do not wrap an inline code span across two lines; prettier reflows it.
- textlint terminology: `Git`, `macOS`, `ID`/`IDs`, `filename`.

Reproduce the prettier step before pushing:

```bash
npx --yes prettier@3 --no-config CHANGELOG.md | diff CHANGELOG.md -
```

## Example

Shape of a released entry (1.8.1, September 2026):

```markdown
## [1.8.1](https://github.com/IJHack/QtPass/tree/v1.8.1) (2026-09-15)

### Bugfixes <!-- markdownlint-disable-line MD024 -->

- `make check` no longer kills the developer's own gpg-agent
  [#1739](https://github.com/IJHack/QtPass/pull/1739) (ported from [#1738](https://github.com/IJHack/QtPass/pull/1738))
```
