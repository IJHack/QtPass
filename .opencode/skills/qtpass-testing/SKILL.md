---
name: qtpass-testing
description: Comprehensive guide for QtPass unit testing with Qt Test
license: GPL-3.0-or-later
metadata:
  audience: developers
  workflow: testing
---

# QtPass Testing Guide

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

## Existing Test Suites (8 total)

### tests/auto/gpgkeystate/tst_gpgkeystate.cpp

Tests for `src/gpgkeystate.cpp`:

- `parseMultiKeyPublic()` - Multiple public keys parsing
- `parseSecretKeys()` - Secret key detection (have_secret flag)
- `parseSingleKey()` - Single key with and without fingerprint
- `parseKeyRollover()` - Multiple keys in sequence
- `classifyRecordTypes()` - GPG record type classification (pub, sec, uid, fpr, etc.)

### Test Fixtures

Store sample test data in `tests/fixtures/`:

```bash
ls tests/fixtures/
# gpg-colons-multi-key.txt
# gpg-colons-public.txt
# gpg-colons-secret.txt
```

These contain real GPG `--with-colons` output for deterministic testing.

### tests/auto/util/tst_util.cpp

Tests for `src/util.cpp`:

- `normalizeFolderPath()` - Path normalization
- `fileContent()` / `fileContentEdgeCases()` - FileContent parsing
- `regexPatterns()` / `regexPatternEdgeCases()` - URL detection regular expression
- `totpHiddenFromDisplay()` - OTP field hiding
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
// SPDX-FileCopyrightText: YYYY Your Name
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

## Test plist File Template (qtpass.plist)

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

### Never use `||` in assertions

Bad (tautology or ambiguous):

```cpp
QVERIFY(result == expected || result == INVALID);  // unclear intent
QVERIFY(result != INVALID || result == INVALID);  // ALWAYS TRUE - tautology!
```

Good - just call the method to verify it doesn't crash:

```cpp
simpleTransaction trans;
trans.transactionAdd(Enums::PASS_INSERT);
trans.transactionIsOver(Enums::PASS_INSERT);  // verify it runs without crash
```

Or use deterministic setup with `QCOMPARE`:

```cpp
simpleTransaction st;
st.transactionAdd(Enums::PASS_INSERT);
Enums::PROCESS result = st.transactionIsOver(Enums::PASS_INSERT);
QCOMPARE(result, Enums::PASS_INSERT);  // deterministic
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

### Path Comparison on Windows

Windows uses backslashes (`\`) while Unix uses forward slashes (`/`). When comparing paths, use `QDir::cleanPath()` to normalize:

```cpp
void tst_util::testPathComparison() {
    QString path = Pass::getGpgIdPath(passStore);
    QString expected = passStore + "/.gpg-id";
    // Use cleanPath to normalize for cross-platform compatibility
    QVERIFY2(QDir::cleanPath(path) == QDir::cleanPath(expected),
             qPrintable(QString("Expected %1, got %2")
                        .arg(QDir::cleanPath(expected), QDir::cleanPath(path))));
}
```

### Test Settings Pollution

Tests that modify QtPass settings can pollute the user's live config. This is especially problematic on Windows where settings use the registry.

**Solution: Backup and restore settings in tests**

```cpp
// In tst_settings::initTestCase()
void tst_settings::initTestCase() {
    // Check for portable mode (qtpass.ini in app directory)
    QString portable_ini = QCoreApplication::applicationDirPath() +
                         QDir::separator() + "qtpass.ini";
    bool isPortableMode = QFile::exists(portable_ini);

    if (isPortableMode) {
        // Backup settings file
        QtPassSettings::getInstance()->sync();
        QString settingsFile = QtPassSettings::getInstance()->fileName();
        m_settingsBackupPath = settingsFile + ".bak";
        QFile::remove(m_settingsBackupPath);
        QFile::copy(settingsFile, m_settingsBackupPath);
    } else {
        // Warn on non-portable mode (registry on Windows)
        qWarning() << "Non-portable mode detected. Tests may modify registry settings.";
    }
}

// In tst_settings::cleanupTestCase()
void tst_settings::cleanupTestCase() {
    // Restore original settings after all tests
    if (isPortableMode && !m_settingsBackupPath.isEmpty()) {
        QString settingsFile = QtPassSettings::getInstance()->fileName();
        QFile::remove(settingsFile);
        QFile::copy(m_settingsBackupPath, settingsFile);
        QFile::remove(m_settingsBackupPath);
    }
}
```

**Key points:**

- Only backup in portable mode (file-based settings)
- On registry mode (Windows non-portable), warn but cannot back up
- Always restore after tests to prevent pollution

### Gitleaks-Safe Test Values

- DON'T: "ABC123DEF456", "sk-xxx", real API keys
- DO: "testkey123", "/usr/bin/pass", "example.com"

### Qt5/Qt6 Compatibility

When checking variant types, prefer `canConvert<T>()` over `metaType().id()` for broader compatibility:

```cpp
// Qt6-only (fails on Qt5)
QVERIFY(displayData.metaType().id() == QMetaType::QString);

// Qt5/Qt6 compatible
QVERIFY(displayData.canConvert<QString>());
```

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

### Testing Getters with Default Parameters

When testing settings that have getters with default parameters, pass a _different_ default value to verify persistence:

```cpp
// Bad - returns default if persistence fails
setter(testValue);
QCOMPARE(getter(testValue), testValue);

// Good - uses different default, must return stored value
setter(testValue);
QCOMPARE(getter(!testValue), testValue);        // bool: use negation
QCOMPARE(getter(-1), testValue);                 // int: use sentinel
QCOMPARE(getter(QString()), testValue);          // string: use empty
```

### Compound Types Don't Always Fit Data-Driven

Data-driven tests work well for simple bool/int/string settings. Compound types like `PasswordConfiguration` often don't fit:

- Different tests test different fields (length vs selected vs characters)
- Nested data (QMap, structs) complicates the table
- Keep tests explicit and readable over forcing a pattern

### Qt Test Macro Reminder

Always use the proper Qt Test macros:

```cpp
// Good
QCOMPARE(actual, expected);
QVERIFY(condition);
QVERIFY2(condition, "failure message");

// Bad - won't compile or is confusing
COMPARE(actual, expected);      // missing Q
QQCOMPARE(actual, expected);     // extra Q
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

See `qtpass-linting` skill for full CI workflow. Pattern:

```bash
# Run linter locally BEFORE pushing
act push -W .github/workflows/linter.yml -j build
```

### Web/Config Files (prettier)

Formats: .md, .yml, .html, .css, .js, .json, etc.

```bash
npx prettier --write <file>
npx prettier --write .github/workflows/*.yml
npx prettier --write ".opencode/skills/*/SKILL.md"
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
