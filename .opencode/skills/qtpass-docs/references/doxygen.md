# Doxygen in QtPass

Read this when a class, function, member or file is added, when the landing
page or the topics change, or when the CI `docs` job fails on a Doxygen
warning.

The site is documentation for people working on QtPass. There is no public
API, so nothing is left out for being private.

## What CI runs

`.github/workflows/documentation.yml`, job "Build, with code coverage", runs
`doxygen` when `src/`, `tests/`, `Doxyfile` or `doxygen/` change (and on every
merge-queue run). Before it runs, the job writes `VERSION` from `qtpass.pri` into
`PROJECT_NUMBER`. On a push to `main` the deploy job publishes the HTML to the
`docs/` folder of the `gh-pages` branch, which is served as
<https://qtpass.org/docs/>.

## Doxyfile settings that matter

| Setting                  | Value                                                         | Effect                                                                            |
| ------------------------ | ------------------------------------------------------------- | --------------------------------------------------------------------------------- |
| `INPUT`                  | `. doxygen/mainpage.md`                                       | The whole tree, plus the landing page                                             |
| `FILE_PATTERNS`          | `*.h *.cpp *.dox`                                             | Headers, sources and `doxygen/groups.dox` are scanned                             |
| `USE_MDFILE_AS_MAINPAGE` | `doxygen/mainpage.md`                                         | The architecture overview is the landing page                                     |
| `EXCLUDE`                | `CHANGELOG.md`, `settingsconstants.*`, `qprogressindicator.*` | Files left out on purpose                                                         |
| `EXCLUDE_PATTERNS`       | `*/tests/* */ui_*.h`                                          | Tests and generated UI headers are not documented                                 |
| `EXTRACT_ALL`            | `NO`                                                          | `YES` would switch off the warnings below                                         |
| `EXTRACT_PRIVATE`        | `YES`                                                         | Private members are documented, and warned about, like public ones                |
| `EXTRACT_STATIC`         | `YES`                                                         | So are file-scope `static` functions and constants                                |
| `WARN_IF_UNDOCUMENTED`   | `YES`                                                         | Every symbol needs a doc block                                                    |
| `WARN_IF_UNDOC_ENUM_VAL` | `YES`                                                         | Every enum value needs one too                                                    |
| `WARN_NO_PARAMDOC`       | `YES`                                                         | Every parameter needs `@param`, non-void functions `@return`                      |
| `WARN_AS_ERROR`          | `FAIL_ON_WARNINGS`                                            | Any warning makes `doxygen` exit non-zero and fails CI                            |
| `HTML_OUTPUT`            | `docs`                                                        | Output goes to `docs/` in the repository root (ignored by Git)                    |
| `HTML_EXTRA_STYLESHEET`  | `doxygen/doxygen-awesome-css/*.css`                           | Vendored [Doxygen Awesome](https://github.com/jothepro/doxygen-awesome-css) theme |
| `GENERATE_LATEX/RTF/XML` | `NO`                                                          | Only HTML is built; nothing used the other formats                                |

## Doc block shape

```cpp
/**
 * @brief Brief description.
 * @param name Description of the parameter.
 * @return Description of the return value.
 */
```

What to write:

- **Header block is the contract.** Say what the thing does and when, not
  what its name already says. For a signal: when it is emitted and what the
  receiver should do. For a slot: what triggers it and what it changes. For a
  member variable: what it holds, who owns it, any invariant.
- **For a function declared in a header, a block on the definition in the
  `.cpp` is about the implementation.** Write it as plain paragraphs with no
  `@brief`, `@param` or `@return`; Doxygen appends it to the description from
  the header. Repeating a `@param` there is a warning ("has multiple @param
  documentation sections").
- **A function that exists only in a `.cpp` file keeps its full block.** A
  `static` helper has no header declaration, so `@brief`, `@param` and
  `@return` go on the definition.
- **Every file starts with a `@file` block** right after the SPDX lines.
  Without it Doxygen silently drops the file's free functions, enums and
  constants, documented or not.
- **Every class has `@ingroup <topic>`** in its doc block. The topics are
  defined in `doxygen/groups.dox`; add a class to the one it fits, or define
  a new topic there and list it on the landing page.
- **"QtPass" in prose links to the `QtPass` class.** Write `%QtPass` when the
  application is meant and a link would mislead (the landing page does).

```cpp
// SPDX-FileCopyrightText: ...
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief What this file holds, in one line.
 */

/**
 * @class Example
 * @ingroup store
 * @brief What the class is for.
 */
class Example {
  /** @brief What it holds and who owns it. */
  QString m_name;
};
```

## Landing page and topics

`doxygen/mainpage.md` is the landing page: where to start reading, the two
backends, how an operation flows from the window through `Pass` and `Executor`
and back, and the rules about secrets. Keep it in step with
[CLAUDE.md's Architecture section](../../../../CLAUDE.md#architecture) when the
architecture changes. `doxygen/groups.dox` holds the `@defgroup` blocks.

The style sheet in `doxygen/doxygen-awesome-css/` is vendored unmodified
(version and license are recorded in `REUSE.toml`) and excluded from the
linters. Update it by replacing the files, not by editing them.

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
- A misspelled command (a typo in `@return` or `@param`): Doxygen reports it as unknown.
- Struct fields, member variables, enum values and operators without a doc
  comment, private ones included.
- A `.cpp` block that repeats the header's `@param` or `@return`.
- A `.cpp` block whose `@param` names no longer match the signature.
- A new file without a `@file` block: no warning, but its free functions
  vanish from the site.
