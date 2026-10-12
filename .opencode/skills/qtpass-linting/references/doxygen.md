# Doxygen linting reference

Back to [SKILL.md](../SKILL.md).

The `docs` job in `.github/workflows/documentation.yml` runs `doxygen` with the repository `Doxyfile` and fails on any warning. CI installs a pinned **Doxygen 1.18.0** (checksum-verified, GitHub release mirror first, doxygen.nl as fallback). An older local Doxygen may report fewer or different warnings.

## Local check

```bash
doxygen Doxyfile 2>&1 >/dev/null | grep -iE "warn|error"
```

Any output means CI fails. With `WARN_AS_ERROR = FAIL_ON_WARNINGS` the problems print as `error:`, so grep for both words. Progress output on stdout is normal (`QUIET = NO`).

## Enforced settings

| Setting            | Value               | Effect                                  |
| ------------------ | ------------------- | --------------------------------------- |
| `INPUT`            | `. main/main.cpp`   | Whole tree, recursive                   |
| `FILE_PATTERNS`    | `*.h`               | Only headers are scanned                |
| `EXTRACT_ALL`      | `NO`                | Needed for `WARN_NO_PARAMDOC` to report |
| `WARN_NO_PARAMDOC` | `YES`               | Missing `@param`/`@return` is a warning |
| `WARN_AS_ERROR`    | `FAIL_ON_WARNINGS`  | Any warning fails the job               |
| `GENERATE_XML`     | `YES` (into `xml/`) | Input for coverxygen                    |

## Comment style

```cpp
/**
 * @brief One-line description.
 * @param count What the parameter means.
 * @return What the function returns.
 */
int frobnicate(int count);
```

## Common causes

- **Unnamed parameter in a declaration**: `void foo(int);` cannot be documented. Name it: `void foo(int count);`.
- **Orphaned doc block**: a `/** */` that is not directly above its declaration gets attached to the wrong symbol. Move it.
- **Missing `@return`** on a non-void function.
- **Qt signals**: also need named parameters and `@param` lines.
- **Undocumented struct fields and operators**: every public member needs a doc comment.
- **Unknown `@word`**: Doxygen treats any `@word` as a command. Write `@brief Like ...`, not `@like`.

## Coverage report (optional)

`GENERATE_XML` is already on, so after `doxygen Doxyfile`:

```bash
pip install coverxygen
python -m coverxygen --xml-dir xml/ --src-dir . --output coverage.info
```
