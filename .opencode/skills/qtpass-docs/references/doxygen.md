# Doxygen in QtPass

Read this when a header gains a public symbol, or when the CI `docs` job fails
on a Doxygen warning.

## What CI runs

`.github/workflows/documentation.yml`, job "Build, with code coverage", runs
`doxygen` when `src/`, `tests/` or `docs/` change (and on every merge-queue
run). Before it runs, the job writes `VERSION` from `qtpass.pri` into
`PROJECT_NUMBER`. On a push to `main` the deploy job publishes the HTML to the
`docs/` folder of the `gh-pages` branch, which is served as
<https://qtpass.org/docs/>.

## Doxyfile settings that matter

| Setting                | Value                                                           | Effect                                                         |
| ---------------------- | --------------------------------------------------------------- | -------------------------------------------------------------- |
| `INPUT`                | `. main/main.cpp`                                               | The whole tree, plus `main.cpp`                                |
| `FILE_PATTERNS`        | `*.h`                                                           | Only headers are scanned; doc blocks belong in the header      |
| `RECURSIVE`            | `YES`                                                           | Subdirectories are included                                    |
| `EXCLUDE`              | `CHANGELOG.md src/settingsconstants.h src/qprogressindicator.h` | Files left out on purpose                                      |
| `EXCLUDE_PATTERNS`     | `*/tests/* */ui_*.h`                                            | Tests and generated UI headers are not documented              |
| `EXTRACT_ALL`          | `NO`                                                            | `YES` would switch off both warnings below                     |
| `WARN_IF_UNDOCUMENTED` | `YES`                                                           | Every public symbol needs a doc block                          |
| `WARN_NO_PARAMDOC`     | `YES`                                                           | Every parameter needs `@param`, non-void functions `@return`   |
| `WARN_AS_ERROR`        | `FAIL_ON_WARNINGS`                                              | Any warning makes `doxygen` exit non-zero and fails CI         |
| `HTML_OUTPUT`          | `docs`                                                          | Output goes to `docs/` in the repository root (ignored by Git) |

## Doc block shape

```cpp
/**
 * @brief Brief description.
 * @param name Description of the parameter.
 * @return Description of the return value.
 */
```

Qt signals are public symbols too and need the same block.

## Check locally

`QUIET = NO`, so `doxygen` prints progress on stdout; warnings go to stderr.
Hide stdout and look at what is left:

```bash
doxygen Doxyfile >/dev/null
echo "exit $?"   # non-zero, or any text printed above, means CI will fail
```

View the result with `xdg-open docs/index.html` (Linux) or
`open docs/index.html` (macOS).

## Common mistakes that cause warnings

- Unnamed parameters in a header declaration: name every parameter.
- An orphaned `/** */` block that is not directly above its declaration.
- Missing `@return` on a non-void function.
- Signals with unnamed or undocumented parameters.
- A typo in a command (`@retrun`, `@parma`): Doxygen reports it as unknown.
- Public struct fields and operators without a doc comment.
