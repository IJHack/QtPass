# Other linters and editor setup

Back to [SKILL.md](../SKILL.md).

## gitleaks (secret detection)

```bash
gitleaks detect
```

Test data that looks like a real credential (`ABC123DEF456`, `sk-...`) gets flagged. Use obviously fake values such as `testkey123`, `/usr/bin/pass`, `example.com`. The repository has no gitleaks config today; if one is needed, it belongs in `.github/linters/` (see [super-linter.md](super-linter.md)).

## shfmt (shell scripts)

Scripts in `scripts/` are tab-indented, as pinned in `.editorconfig`.

```bash
shfmt -d scripts/*.sh   # check
shfmt -w scripts/*.sh   # apply
```

Install with your package manager, `brew install shfmt`, or `go install mvdan.cc/sh/v3/cmd/shfmt@latest`.

## codespell

Super-linter passes files to codespell one by one, so the `skip` setting in `.codespellrc` does not exclude paths; `FILTER_REGEX_EXCLUDE` in `.github/super-linter.env` does. Add valid project words to `ignore-words-list` in `.codespellrc` with a comment.

## editorconfig-checker

Checks files against the `.editorconfig` rules (CI ignores the root `.ecrc`, see [super-linter.md](super-linter.md)). Line endings must match `.gitattributes` (for example, `.cmd` files are CRLF).

## clangd and compile_commands.json

clangd needs `compile_commands.json` to resolve Qt headers. Generate it with Bear (needs `bear` and `qmake6`):

```bash
./scripts/generate-compile-commands.sh
clangd --check=src/gpgkeystate.cpp
```

`compile_commands.json` is in `.gitignore`; regenerate it after a clean or reconfigure. Typical diagnostics: `performance-unnecessary-copy-initialization` (take `const T&`), `readability-static-definition`. Editors expose clangd fixes as code actions (Visual Studio Code `Ctrl+.`, JetBrains `Alt+Enter`, Neovim `vim.lsp.buf.code_action()`).

`.clang-tidy` holds the clang-tidy checks; super-linter does not run clang-tidy (and `VALIDATE_CPP=false` turns off its cpplint).
