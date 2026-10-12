# Super-linter configuration reference

Back to [SKILL.md](../SKILL.md).

## Pin and image

`lint.yml` uses `super-linter/super-linter@<sha> # v9.0.0`, which runs the image `ghcr.io/super-linter/super-linter:v9.0.0`. Dependabot bumps the SHA; update the image tag in the Docker commands of SKILL.md when the version comment changes.

Bundled tool versions in v9.0.0 (from its `Dockerfile` and `dependencies/`):

| Tool                     | Version      |
| ------------------------ | ------------ |
| clang-format             | 22 (LLVM 22) |
| zizmor                   | 1.30.1       |
| gitleaks                 | 8.30.1       |
| shfmt                    | 3.14.1       |
| Prettier                 | 3.9.x        |
| markdownlint-cli         | 0.49.x       |
| textlint (+ terminology) | 15.8.x       |

A local tool on a different major version can disagree with CI (clang-format is the usual suspect). When in doubt, run the tool from the image.

## `.github/super-linter.env`

The workflow does `cat .github/super-linter.env >> "$GITHUB_ENV"`, so the file must be:

- pure `KEY=value` lines: no comments, no empty lines;
- sorted alphabetically (dotenv-linter `UnorderedKey`).

Document the reason for a non-obvious variable in the comment block above the `cat` step in `lint.yml`, not in the env file.

Current notable settings: `VALIDATE_ALL_CODEBASE=true` (whole tree, not only the diff), `FILTER_REGEX_EXCLUDE=^(localization|tests/fuzz/corpus)/` with `STRIP_DEFAULT_WORKSPACE_FOR_REGEX=true`, and `VALIDATE_CPP`, `VALIDATE_JSCPD`, `VALIDATE_GIT_COMMITLINT` and the TypeScript linters switched off.

Settings that must stay in the `lint.yml` step `env:` instead:

- `MULTI_STATUS: false`: the job has a read-only token, and per-linter commit statuses would fail with HTTP 403.
- `GITHUB_TOKEN`, `DEFAULT_BRANCH`.

## `GITHUB_*` variables do not propagate

Variables named `GITHUB_*` written to `$GITHUB_ENV` never reach Docker-based actions such as super-linter (GitHub reserves that namespace). For example, `GITHUB_ACTIONS_ZIZMOR_CONFIG_FILE` set that way has no effect. Put the config file at the path super-linter discovers by default instead.

## Where linter configs live

`LINTER_RULES_PATH` defaults to `.github/linters/`. For each linter, super-linter v9 looks for its config filename there and passes it explicitly (`--config`, `-c`, `--style=file:`). When the file is missing, it passes its own template from the image instead (`lib/functions/linterRules.sh`). The repository root is **not** searched.

Configs in `.github/linters/` today:

| Linter | Config file                   |
| ------ | ----------------------------- |
| zizmor | `.github/linters/zizmor.yaml` |

zizmor's own `.github/zizmor.yml` convention is not read either. The config relaxes `unpinned-uses` to `ref-pin` and disables `secrets-outside-env`; the workflows still pin actions by SHA with a version comment.

What CI therefore uses for the other linters:

| Linter               | Filename it looks for        | Effective config in CI                                                                 |
| -------------------- | ---------------------------- | -------------------------------------------------------------------------------------- |
| clang-format         | `.clang-format`              | template: `BasedOnStyle: LLVM` (root `.clang-format` ignored)                          |
| textlint             | `.textlintrc`                | template: `terminology` rule plus comments filter (root `.textlintrc` ignored)         |
| markdownlint         | `.markdown-lint.yml`         | template: MD013 400 columns; MD004, MD029, MD033, MD036 off                            |
| yamllint             | `.yaml-lint.yml`             | template (root `.yaml-lint.yml` ignored)                                               |
| editorconfig-checker | `.editorconfig-checker.json` | template; the tree's `.editorconfig` files still apply, root `.ecrc` ignored           |
| codespell            | `.codespellrc`               | template, plus the root `.codespellrc` that codespell loads from the working directory |

To make CI use a repository config, copy or move it into `.github/linters/` under the filename above, then check the next CI run uses it ("User-provided configuration file ... exists" in the log). Files under `.github/linters/` are covered by `REUSE.toml`, so they need no SPDX header.

## Running `act`

`act` is reliable only for simple workflows:

```bash
act push -W .github/workflows/reuse.yml
```

Known failures, none of which mean your change is wrong:

- `lint.yml`: the mock event JSON lacks fields super-linter needs; on a fresh branch it also fails with `fatal: ambiguous argument 'HEAD~0'`. Use the Docker run from SKILL.md.
- `build-and-test.yml`, `documentation.yml`: `install-qt-action` often cannot download Qt inside `act`. `documentation.yml` also wants a secret: add `--secret CODECOV_TOKEN=fake`.
