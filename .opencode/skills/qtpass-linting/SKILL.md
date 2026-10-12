---
name: qtpass-linting
description: Reproduces QtPass CI lint failures locally and fixes them with clang-format, Prettier, textlint, shfmt, zizmor, Doxygen or the pinned super-linter image. Use when lint failed, super-linter or CI is red, clang-format or Prettier complains, markdownlint or textlint flags a file, Doxygen warns, or before pushing a PR to check formatting.
license: GPL-3.0-or-later
metadata:
  audience: developers
  workflow: linting
  version: "1.1"
---

# QtPass Linting and CI

## CI checks at a glance

| Workflow (`.github/workflows/`) | Job / check name              | What fails it                                                                             |
| ------------------------------- | ----------------------------- | ----------------------------------------------------------------------------------------- |
| `lint.yml`                      | `Super-linter` (required)     | clang-format, Prettier, markdownlint, textlint, gitleaks, codespell, zizmor, EditorConfig |
| `build-and-test.yml`            | `Ubuntu (Qt 6.8)` etc. (req.) | compile errors, `make check`                                                              |
| `documentation.yml`             | `Build, with code coverage`   | any Doxygen warning (`WARN_AS_ERROR = FAIL_ON_WARNINGS`)                                  |
| `reuse.yml`                     | `Check compliance` (required) | missing SPDX/REUSE licensing                                                              |

Required checks on `main`: `Super-linter`, `Ubuntu (Qt 6.8)`, `Windows (Qt 6.8)`, `macOS (Qt 6.11)`, `Check compliance`.

Super-linter is skipped when no relevant path changed (the `changes` job), but when it runs it lints the whole tree (`VALIDATE_ALL_CODEBASE=true`), minus `localization/` and `tests/fuzz/corpus/`.

## The fix loop

Repeat until every check is clean, then push:

1. **Run** the narrowest local check for what failed (table below), or read the CI log:
   `gh run view --log-failed` on the failing run.
2. **Fix**: apply the formatter (`-i`, `--write`, `--fix`) or edit by hand.
3. **Re-run** the same check. Not clean yet: back to step 2.
4. **Widen**: run the pre-PR checklist below, so one fix did not break another linter.

| Failure in CI log          | Local check                                                              |
| -------------------------- | ------------------------------------------------------------------------ |
| `CLANG_FORMAT`             | `clang-format --style=file --dry-run --Werror <file>`                    |
| `PRETTIER` (Markdown/YAML) | `npx prettier --check <file>`                                            |
| `MARKDOWN` (markdownlint)  | `npx markdownlint-cli --disable MD013 MD004 MD029 MD033 MD036 -- <file>` |
| `NATURAL_LANGUAGE`         | textlint, see below                                                      |
| `SHELL_SHFMT`              | `shfmt -d scripts/*.sh`                                                  |
| `GITHUB_ACTIONS_ZIZMOR`    | `zizmor --config .github/linters/zizmor.yaml .github/workflows/`         |
| Documentation job          | `doxygen Doxyfile 2>&1 >/dev/null \| grep -iE "warn\|error"`             |
| Anything else, or unsure   | run that linter inside the CI image (next section)                       |

## Reproduce super-linter locally

CI pins `super-linter/super-linter` by SHA to **v9.0.0**, image `ghcr.io/super-linter/super-linter:v9.0.0`. Check the `# vX.Y.Z` comment in `lint.yml` after Dependabot bumps.

Run any bundled linter with the exact CI version through `--entrypoint`:

```bash
IMG=ghcr.io/super-linter/super-linter:v9.0.0

docker run --rm -v "$PWD:/work" -w /work --entrypoint clang-format "$IMG" \
  --style=file --dry-run --Werror src/mainwindow.cpp

docker run --rm -v "$PWD:/work" -w /work --entrypoint zizmor "$IMG" \
  --config .github/linters/zizmor.yaml .github/workflows/
```

Native binaries are faster for iteration (distro clang-format/yamllint/codespell, `cargo install zizmor`), but versions may differ from CI. clang-format in particular: v9.0.0 ships clang-format 22.

**CI does not read most repository-root dotfiles.** Super-linter looks for configs only in `.github/linters/` and otherwise uses its own templates. So in CI: clang-format runs with `BasedOnStyle: LLVM` (not the root `.clang-format`), textlint enforces only `terminology`, markdownlint uses MD013 at 400 columns with MD004/MD029/MD033/MD036 off. codespell still reads the root `.codespellrc` (it loads it from the working directory). Details: [references/super-linter.md](references/super-linter.md).

**Do not trust `act` for `lint.yml`.** Its mock event JSON lacks fields super-linter needs, and it fails on fresh branches (`fatal: ambiguous argument 'HEAD~0'`). Use the Docker run above instead. `act` remains fine for simple workflows such as `reuse.yml`; `build-and-test.yml` usually fails at the Qt install step under `act`, which is expected.

Config rules (env file format, where linter configs live, the `GITHUB_*` trap) and the bundled tool versions: read [references/super-linter.md](references/super-linter.md) when changing `lint.yml`, `.github/super-linter.env` or `.github/linters/`.

## Formatters and checkers

### clang-format (C++)

```bash
clang-format --style=file -i src/*.cpp src/*.h      # apply
clang-format --style=file --dry-run --Werror src/foo.cpp   # check
```

Always format C/C++ before committing. The root `.clang-format` is an expanded LLVM style, so it agrees with CI's `BasedOnStyle: LLVM`; if local and CI ever disagree, check with `--style=LLVM` inside the CI image.

### Prettier (Markdown, YAML, JSON)

```bash
npx prettier --write "**/*.md" "**/*.yml"
npx prettier --check "**/*.md" "**/*.yml"
```

The repository has no Prettier config, so defaults apply. Prettier reflows a code span that wraps across two lines; keep inline code on one line.

### textlint (NATURAL_LANGUAGE)

CI enforces `textlint-rule-terminology` (canonical spelling: `Git`, `GitHub`, `macOS`, `ID`, `filename`). The root `.textlintrc` adds `common-misspellings` for local runs. From the repository root:

```bash
npx -p textlint -p textlint-rule-terminology -p textlint-rule-common-misspellings \
  textlint README.md
# add --fix to rewrite in place
```

Super-linter checks every Markdown file, so lint the whole tree before pushing: `*.md scripts/*.md .github/**/*.md .opencode/skills/**/*.md`.

### markdownlint

Emphasis style must be consistent per file (MD049): if the file uses `_x_`, do not add `*x*`. Headings must not end in punctuation such as `:` (MD026). Plain `markdownlint-cli` defaults to 80 columns; CI allows 400, hence `--disable MD013` locally.

### Doxygen

CI pins Doxygen 1.18.0 and fails on any warning, which appears as `error:`. Every public symbol in a header needs `/** @brief ... */` with `@param`/`@return`. Common causes and the full settings: read [references/doxygen.md](references/doxygen.md) when the Documentation job is red.

Other tools (gitleaks, shfmt, clangd, IDE setup): read [references/other-linters.md](references/other-linters.md) when one of those fails or you set up an editor.

## Pre-PR checklist

```bash
# 1. Format changed C/C++ files
git diff --name-only origin/main -- '*.cpp' '*.h' | xargs -r clang-format --style=file -i

# 2. Format and verify Markdown/YAML
npx prettier --write "**/*.md" "**/*.yml"
npx prettier --check "**/*.md" "**/*.yml"

# 3. Terminology on changed Markdown
git diff --name-only origin/main -- '*.md' | xargs -r npx -p textlint \
  -p textlint-rule-terminology -p textlint-rule-common-misspellings textlint

# 4. Doxygen, if headers changed (any output means CI fails)
doxygen Doxyfile 2>&1 >/dev/null | grep -iE "warn|error"

# 5. Rebase on latest main, then push
git pull --rebase origin main
git push --force-with-lease
```

Any step not clean: go back to the fix loop. Never push directly to `main`.

## Troubleshooting

- **Documentation job red with a download error**: the Doxygen download (GitHub mirror, then doxygen.nl) had a blip. Re-run the job before debugging.
- **Super-linter red on a file you did not touch**: the whole tree is linted, so a super-linter bump (new tool versions) or a red `main` shows up in every PR. Check the latest `main` run first; fix it in its own PR when `main` is red too.
- **Super-linter fails with HTTP 403 on statuses**: `MULTI_STATUS: false` must stay set in `lint.yml` (the job has a read-only token).
- **codespell flags a valid project word**: add it to `ignore-words-list` in `.codespellrc` with a comment.
- **Gitleaks flags test data**: use obviously fake values (`testkey123`, `example.com`), not strings shaped like real keys.

## Related skills

- **qtpass-testing**: `make check` and test patterns
- **qtpass-fixing**: bugfix workflow with tests
- **qtpass-github**: PRs, CI status, merging
- **qtpass-docs**: Doxygen and Markdown content
