---
name: qtpass-testing
description: Writes, runs and debugs QtPass Qt Test suites under tests/auto, adds new tst_ suites, isolates settings, and checks coverage and ASan leaks. Use when asked to write tests, add a test, run tests or make check, fix a failing test, find which suite covers a class, measure coverage, hunt leaks with sanitizers, or set up test fixtures.
license: GPL-3.0-or-later
metadata:
  audience: developers
  workflow: testing
  version: "1.1"
---

# QtPass Testing

QtPass tests use Qt Test. Each suite is a separate binary in
`tests/auto/<suite>/`, linked against the static library built from `src/`.
The suite list is `SUBDIRS` in `tests/auto/auto.pro`.

For the full suite inventory (which suite covers which class, platform
exclusions, shared helpers), read
[references/suites.md](references/suites.md). For longer patterns (getters
with defaults, path comparison, settings isolation details, reading ASan
output), read [references/patterns.md](references/patterns.md).

## Commands

```bash
make check                                   # build and run every suite
QT_QPA_PLATFORM=offscreen make check         # headless, as CI runs it
./tests/auto/util/tst_util                   # one suite
./tests/auto/util/tst_util normalizeFolderPath   # one test function
./tests/auto/util/tst_util -v2               # verbose: every QVERIFY/QCOMPARE
./tests/auto/util/tst_util -vs               # also log every emitted signal
./tests/auto/util/tst_util -functions        # list the test functions
```

Coverage:

```bash
qmake6 -r CONFIG+=coverage
make -j4
make check
make lcov
```

## Workflow

1. Find the suite that owns the class (see
   [references/suites.md](references/suites.md)). Add to an existing suite
   before creating a new one.
2. Write the test (template below). Run that one suite and see it fail first
   when you are testing a bug fix.
3. Run `make check`. If any suite fails, read the failing function's output
   with `-v2`, fix the code or the test, and re-run until every suite passes.
   Do not commit with a failing suite.
4. If the change touches object ownership (parents, `QScopedPointer`, `delete`,
   dialogs, `setWindowFlags`), also run the ASan build below and fix every
   report before committing.
5. Run `clang-format --style=file -i` on the files you changed. Lint and CI
   details are in the `qtpass-linting` skill.

## Test file template

```cpp
// SPDX-FileCopyrightText: 2026 Your Name
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "../../../src/mymodule.h"
#include "../testsettings.h"

class tst_mymodule : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void processReturnsExpected();
  void processEmptyInput();
};

void tst_mymodule::initTestCase() {
  // First statement, before anything touches QtPassSettings.
  isolateTestSettings();
}

void tst_mymodule::processReturnsExpected() {
  QCOMPARE(MyModule::process("input"), QStringLiteral("expected"));
}

void tst_mymodule::processEmptyInput() {
  QVERIFY(MyModule::process("").isEmpty());
}

QTEST_MAIN(tst_mymodule)
#include "tst_mymodule.moc"
```

Drop `isolateTestSettings()` only if the code under test never reads
settings. Suites that need a `Pass` object without a backend derive from
`NullPass` in `../testpass.h`.

## .pro file template

```pro
!include(../auto.pri) { error("Couldn't find the auto.pri file!") }

SOURCES += tst_mymodule.cpp
```

`tests/auto/auto.pri` handles linking `libqtpass.a`, the include path, the
Windows resource and linker cap, and the `tst_` target prefix. Do not list
library headers in `HEADERS` (their moc output is already in the library, and
listing them mocs them twice) and do not compile library sources into the
suite. Add only what the suite needs beyond that (the `locale` suite embeds
the translations, for example).

## Adding a new suite

1. Create `tests/auto/<name>/` with `<name>.pro` and `tst_<name>.cpp`.
2. Add `<name>` to `SUBDIRS` in `tests/auto/auto.pro`. If it cannot run on
   Windows, add it under `!win32:` as `integration` is.
3. Rebuild with `qmake6 -r && make -j4`, then run `make check` and fix
   failures until it passes.

No per-suite `qtpass.plist` or `artwork/` link is needed: `qtpass.pri` anchors
the macOS bundle icon and plist to the repository root.

## Conventions

- Naming: file `tst_<name>.cpp`, class `tst_<name>`, functions that say what
  they check (`parseColonInValue`, `getGpgIdPathSubfolder`).
- Assertions: `QCOMPARE(actual, expected)` for values (it prints both),
  `QVERIFY2(cond, msg)` when the condition needs an explanation, `QVERIFY`
  otherwise. `QSKIP` for a case that cannot run on this platform.
- No `||` in an assertion: set up the state so there is one correct answer.
- Settings round trips: pass a getter a default that differs from the stored
  value (`getter(!value)`, `getter(-1)`, `getter(QString())`), or a broken
  setter still passes.
- Platform-specific cases: guard with `#ifndef Q_OS_WIN` (or `#ifdef`) inside
  the test function, or `QSKIP`.
- Paths: compare through `QDir::cleanPath()` on both sides.
- Temporary files: `QTemporaryDir`, checked with `isValid()`; never write into
  the source tree or `$HOME`.
- Test data: no shared fixtures directory; keep data next to the suite.
- Secret-looking values trip gitleaks: use `testkey123`, `example.com`,
  `/usr/bin/pass`, not strings shaped like real API keys.
- Variant types: prefer `canConvert<QString>()` over comparing
  `metaType().id()`.
- Qt floor is 6.8 (what CI builds). Do not add Qt 5 branches; guard APIs newer
  than 6.8.

Worked examples of these are in
[references/patterns.md](references/patterns.md).

## Leaks and use-after-free: AddressSanitizer

Use ASan and LSan, not valgrind (valgrind is far slower, trips over Qt 6 on a
current glibc, and needs suppressions). Build in a separate directory or a
fresh worktree, since sanitizer objects do not mix with a normal build:

```bash
qmake6 "CONFIG+=debug sanitizer sanitize_address"
make -j8
ASAN_OPTIONS=detect_leaks=1 QT_QPA_PLATFORM=offscreen make check
```

The suite ran clean with no suppressions as of 2026-09-16, so any report is a
regression to fix, then re-run until clean. How to read a report is in
[references/patterns.md](references/patterns.md#reading-addresssanitizer-output).

## Fuzz targets

The untrusted-input parsers also have libFuzzer targets in `tests/fuzz/`; see
the repository `CLAUDE.md` for the build and run commands.
