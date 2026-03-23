---
name: qtpass-tests
description: Add unit tests for QtPass Qt/C++ project
license: GPL-3.0-or-later
compatibility: opencode
metadata:
  audience: developers
  workflow: testing
---

## Project Context
QtPass is a multi-platform GUI for the `pass` password manager, built with Qt6 and qmake.

## Test Framework
- **Framework**: Qt Test (QtTest)
- **Build system**: qmake
- **Test directories**: `tests/auto/<module>/`

## Key Conventions

### Test File Structure
```
tests/auto/
├── auto.pro              # Main test project (subdirs)
├── model/
│   ├── model.pro         # Test project file
│   ├── tst_*.cpp          # Test source
│   └── qtpass.plist      # macOS plist (required for build)
└── settings/
    ├── settings.pro
    └── tst_settings.cpp
```

### Test .pro File Pattern
```pro
!include(../auto.pri) { error("Couldn't find the auto.pri file!") }

SOURCES += tst_mytest.cpp

LIBS = -L"$$OUT_PWD/../../../src/$(OBJECTS_DIR)" -lqtpass $$LIBS
clang|gcc:PRE_TARGETDEPS += "$$OUT_PWD/../../../src/$(OBJECTS_DIR)/libqtpass.a"

HEADERS   += myclass.h

OBJ_PATH += ../../../src/$(OBJECTS_DIR)

VPATH += ../../../src
INCLUDEPATH += ../../../src

win32 {
    RC_FILE = ../../../windows.rc
    QMAKE_LINK_OBJECT_MAX=24
}
```

### Test Class Pattern
```cpp
#include <QtTest>
#include "../../../src/myclass.h"

class tst_myclass : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void testMethod();
  void cleanupTestCase();
};

void tst_myclass::initTestCase() {}

void tst_myclass::testMethod() {
    // Use set+get pattern, not internal state checks
    MyClass::setValue(42);
    QVERIFY(MyClass::getValue() == 42);
}

QTEST_MAIN(tst_myclass)
#include "tst_myclass.moc"
```

## Build & Run Commands
```bash
# Build with coverage
qmake6 -r CONFIG+=coverage
make -j4

# Run all tests
make check

# Generate coverage report
make lcov
```

## Common Patterns

### Windows-Compatible Tests
Use `#ifndef Q_OS_WIN` guards for Unix-only commands:
```cpp
void tst_executor::executeWithBash() {
#ifndef Q_OS_WIN
    // Test bash-specific functionality
#endif
}
```

### Settings Tests (set+get pattern)
```cpp
void tst_settings::setAndGetUseGit() {
    QtPassSettings::setUseGit(true);
    QVERIFY(QtPassSettings::isUseGit() == true);
    QtPassSettings::setUseGit(false);
    QVERIFY(QtPassSettings::isUseGit() == false);
}
```

### Gitleaks Avoidance
- Don't use test values that look like API keys (e.g., "ABC123DEF456")
- Use generic test strings like "testkey123" or "/usr/bin/pass"

### FileContent Test
- Test `FileContent::parse()` with various inputs
- Test `getRemainingDataForDisplay()` for hidden fields (otpauth://)
- Test `NamedValues::takeValue()` edge cases

## Adding a New Test Suite
1. Create `tests/auto/<name>/` directory
2. Add `.pro` file (copy pattern from existing)
3. Add `qtpass.plist` (copy from model/)
4. Add `tst_<name>.cpp` test file
5. Add to `tests/auto/auto.pro`: `SUBDIRS += <name>`
6. Rebuild: `qmake6 -r && make -j4`
7. Run tests: `make check`

## Useful Source Files for Testing
- `src/qtpasssettings.h` - 26+ is*/get/set methods
- `src/filecontent.h/cpp` - FileContent, NamedValues
- `src/storemodel.h/cpp` - StoreModel
- `src/executor.h/cpp` - Executor
- `src/passwordconfiguration.h` - PasswordConfiguration
- `src/util.h/cpp` - Util static methods