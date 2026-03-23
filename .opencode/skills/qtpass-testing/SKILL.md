---
name: qtpass-testing
description: Comprehensive guide for QtPass unit testing with Qt Test
license: GPL-3.0-or-later
compatibility: opencode
metadata:
  audience: developers
  workflow: testing
---

# QtPass needs lots of tests

## Project Overview

QtPass is a Qt6/C++ password manager GUI for `pass`. Tests use Qt Test framework.

## Quick Start

```bash
# Build and run all tests
make check

# Build with coverage
qmake6 -r CONFIG+=coverage
make -j4

# Generate coverage report
make lcov
```

## Existing Test Suites (7 total)

### tests/auto/util/tst_util.cpp

Tests for `src/util.cpp`:

- `normalizeFolderPath()` - Path normalization
- `fileContent()` / `fileContentEdgeCases()` - FileContent parsing
- `regexPatterns()` / `regexPatternEdgeCases()` - URL detection regular expression
- `totpHiddenFromDisplay()` - OTP field hiding
- `copyDir*()` - Directory operations
- `userInfoValidity()` - User key validation
- `passwordConfigurationCharacters()` - Password character sets
- `simpleTransaction*()` - SimpleTransaction tests

### tests/auto/filecontent/tst_filecontent.cpp

Tests for `src/filecontent.h`:

- `parsePlainPassword()` - Single-line password
- `parsePasswordWithNamedFields()` - Password with key:value
- `parseWithTemplateFields()` - Template field parsing
- `parseWithAllFields()` - All fields mode
- `getRemainingData()` - Non-template fields
- `getRemainingDataForDisplay()` - Hides otpauth://
- `namedValuesTakeValue()` / `namedValuesTakeValueNotFound()`

### tests/auto/passwordconfig/tst_passwordconfig.cpp

Tests for `src/passwordconfiguration.h`:

- `passwordConfigurationDefaults()` - Default values
- `passwordConfigurationSetters()` - Setter methods
- `passwordConfigurationCharacterSets()` - Character set config

### tests/auto/executor/tst_executor.cpp

Tests for `src/executor.h`:

- `executeBlockingEcho()` - Basic execution
- `executeBlockingWithArgs()` - Arguments handling
- `executeBlockingExitCode()` - Exit code checking (Unix only)
- `executeBlockingStderr()` - Error output capture (Unix only)

### tests/auto/model/tst_storemodel.cpp

Tests for `src/storemodel.h`:

- `dataRemovesGpgExtension()` - Display name filtering
- `flagsWithValidIndex()` / `flagsWithInvalidIndex()` - Item flags
- `mimeTypes()` - Drag/drop MIME types
- `lessThan()` - Sorting comparison
- `supportedDropActions()` / `supportedDragActions()`
- `filterAcceptsRowHidden()` / `filterAcceptsRowVisible()`

### tests/auto/settings/tst_settings.cpp

Tests for `src/qtpasssettings.h`:

- Uses set+get pattern for each setting
- Tests isUseGit(), setUseGit(), etc.

### tests/auto/ui/tst_ui.cpp

UI tests:

- `contentRemainsSame()` - Password content integrity
- `emptyPassword()` - Empty password handling
- `multilineRemainingData()` - Multiline field handling

## Test File Template

```cpp
// SPDX-FileCopyrightText: 2024 Your Name
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include "../../../src/mymodule.h"

class tst_mymodule : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void testBasicFunction();
  void testEdgeCase();
  void cleanupTestCase();
};

void tst_mymodule::initTestCase() {}

void tst_mymodule::testBasicFunction() {
    // Use set+get pattern or direct input/output
    QString result = MyModule::process("input");
    QVERIFY2(result == "expected", "Should return expected output");
}

void tst_mymodule::testEdgeCase() {
    // Test empty, null, boundary conditions
    QVERIFY(MyModule::process("").isEmpty());
}

void tst_mymodule::cleanupTestCase() {}

QTEST_MAIN(tst_mymodule)
#include "tst_mymodule.moc"
```

## .pro File Template

```pro
!include(../auto.pri) { error("Couldn't find the auto.pri file!") }

SOURCES += tst_mymodule.cpp

LIBS = -L"$$OUT_PWD/../../../src/$(OBJECTS_DIR)" -lqtpass $$LIBS
clang|gcc:PRE_TARGETDEPS += "$$OUT_PWD/../../../src/$(OBJECTS_DIR)/libqtpass.a"

HEADERS   += mymodule.h

OBJ_PATH += ../../../src/$(OBJECTS_DIR)

VPATH += ../../../src
INCLUDEPATH += ../../../src

win32 {
    RC_FILE = ../../../windows.rc
    QMAKE_LINK_OBJECT_MAX=24
}
```

## Test .pro Pattern (qtpass.plist)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>QMTestSpecification</key>
    <dict>
        <key>Type</key>
        <string>Bundle</string>
        <key>UIElement</key>
        <dict>
            <key>Modified</key>
            <false/>
            <key>SystemEntity</key>
            <string>Test</string>
        </dict>
    </dict>
</dict>
</plist>
```

## Adding a New Test Suite

1. Create directory: `tests/auto/<name>/`
2. Add `<name>.pro` file (copy pattern above)
3. Add `qtpass.plist` (copy from model/)
4. Add `tst_<name>.cpp` test file
5. Add to `tests/auto/auto.pro`: `SUBDIRS += <name>`
6. Rebuild: `qmake6 -r && make -j4`
7. Run: `make check`

## Best Practices

### Test Naming

- Test file: `tst_<classname>.cpp`
- Test class: `tst_<classname>`
- Test methods: `testMethodName()` or `methodDoesWhat()`

### Test Organization

- `initTestCase()` - Setup (runs once before all tests)
- `init()` - Setup before each test
- Test methods in logical order
- `cleanupTestCase()` - Teardown (runs once after all tests)

### Assertions

```cpp
// Basic
QVERIFY(condition);
QVERIFY2(condition, "failure message");

// Equality
QCOMPARE(actual, expected);
QCOMPARE2(actual, expected, "message");

// String matching
QVERIFY2(output.contains("needle"), "should contain needle");
QCOMPARE(QString("hello").toUpper(), QString("HELLO"));
```

### QtTest Macros

- `QFAIL("message")` - Fail with message
- `QSKIP("message")` - Skip test
- `QCOMPARE(a, b)` - Assert equality
- `QVERIFY(a)` - Assert true
- `QVERIFY2(a, msg)` - Assert with message

### Windows Compatibility

```cpp
void tst_executor::unixOnlyTest() {
#ifndef Q_OS_WIN
    // Test code here
#endif
}
```

### Gitleaks-Safe Test Values

- DON'T: "ABC123DEF456", "sk-xxx", real API keys
- DO: "testkey123", "/usr/bin/pass", "example.com"

### Temporary Files/Directories

```cpp
void tst_mymodule::testWithTempFile() {
    QTemporaryDir tempDir;
    QString filePath = tempDir.path() + "/test.txt";
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("test data");
    file.close();
    // Test reads/modifies file
}
```

## Testable Source Files

### src/util.h/cpp

- `normalizeFolderPath()` - Path normalization
- `protocolRegex()` - URL detection
- `endsWithGpg()` - Extension matching
- `newLinesRegex()` - Newline detection

### src/filecontent.h/cpp

- `FileContent::parse()` - Parse password file
- `getPassword()` - Get main password
- `getNamedValues()` - Get key:value fields
- `getRemainingData()` - Non-template fields
- `getRemainingDataForDisplay()` - Display-safe (hides OTP)
- `NamedValues::takeValue()` - Extract and remove value

### src/passwordconfiguration.h

- `PasswordConfiguration` class
- Character set definitions
- Length configuration

### src/executor.h/cpp

- `Executor::executeBlocking()` - Run command synchronously
- Returns exit code, captures output/stderr

### src/storemodel.h/cpp

- `StoreModel` extends QFileSystemModel
- `setModelAndStore()` - Initialize with model and path
- `data()` - Returns display name (removes .gpg)
- `flags()` - Item flags
- `lessThan()` - Sorting

### src/qtpasssettings.h

- 26+ settings: isUseGit(), setUseGit(), isUseMonospace(), etc.

## CI Integration

Tests run via `make check` in CI. Coverage reported with `make lcov`.

## Linting

### Web/Config Files (prettier)

Formats: .md, .yml, .html, .css, .js, .json, etc.

```bash
npx prettier --write <file>
npx prettier --write .github/workflows/*.yml
npx prettier --write FAQ.md
```

### C++ (clang-format)

```bash
# Check formatting
clang-format --style=file --dry-run src/main.cpp

# Apply formatting
clang-format --style=file -i src/main.cpp
```

### Project Lint

```bash
make check  # Runs tests and builds
```

## Debugging Failed Tests

```bash
# Run single test
./tests/auto/util/tst_util testName

# Verbose output
./tests/auto/util/tst_util -v2

# Detailed timing
./tests/auto/util/tst_util -vs
```
