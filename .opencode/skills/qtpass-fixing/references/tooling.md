# Formatting, Linting and clangd for Fixes

Commands run from the repository root. For the full CI picture see the
`qtpass-linting` skill.

## Formatting

```bash
# C++: check, then apply
clang-format --style=file --dry-run src/mainwindow.cpp
clang-format --style=file -i src/mainwindow.cpp

# Markdown and YAML
npx prettier --write "**/*.md" "**/*.yml"
npx prettier --check "**/*.md" "**/*.yml"
```

## Lint before pushing

```bash
act push -W .github/workflows/lint.yml -j build
```

`act` does not reproduce super-linter exactly; when it disagrees with CI,
trust CI and see the `qtpass-linting` skill.

## Doxygen

CI fails on any Doxygen warning. A fix that adds or changes a public symbol in
a header needs a `/** @brief ... */` block with `@param`/`@return`:

```bash
doxygen Doxyfile   # any output means CI will fail
```

## clangd and compile_commands.json

Without `compile_commands.json`, clangd reports false errors about missing Qt
headers. Generate it (needs `bear` and `qmake6`; it runs `make distclean`
first):

```bash
./scripts/generate-compile-commands.sh
```

Then check the file you changed:

```bash
clangd --check=src/mainwindow.cpp
```

clangd picks up `.clang-tidy`, which enables the `modernize-*`,
`performance-*` and `bugprone-*` groups (plus a few `google-*` checks) with
`WarningsAsErrors: '*'`. A common one on fixes is
`[performance-unnecessary-copy-initialization]`: use `const T &` instead of
`const T`.

Editor quick fixes (Visual Studio Code `Ctrl+.`, JetBrains `Alt+Enter`, the
`textDocument/codeAction` request in vim/neovim) are suggestions: check each
one makes sense before applying it.
