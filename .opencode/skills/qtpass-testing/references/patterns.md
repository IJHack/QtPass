# QtPass test patterns in detail

Read this when a test touches settings getters, compound settings types,
paths that must compare equal on Windows, or when an ASan run reports
something and you need to read the output.

## Assertions without `||`

An `||` in an assertion either hides which branch was expected or is always
true:

```cpp
QVERIFY(result == expected || result == INVALID);  // unclear intent
QVERIFY(result != INVALID || result == INVALID);   // always true
```

Set up the state so there is one correct answer, then compare it:

```cpp
SimpleTransaction st;
st.transactionAdd(Enums::PASS_INSERT);
Enums::PROCESS result = st.transactionIsOver(Enums::PASS_INSERT);
QCOMPARE(result, Enums::PASS_INSERT);
```

If the only claim is "this does not crash", call the code with no assertion
and say so in a comment.

## Getters with a default parameter

A getter that takes a default returns that default when nothing was stored.
Pass a default that differs from the stored value, or the test passes even
when persistence is broken:

```cpp
// Passes even if the setter stored nothing
setter(testValue);
QCOMPARE(getter(testValue), testValue);

// Must return the stored value
setter(testValue);
QCOMPARE(getter(!testValue), testValue);   // bool: negation
QCOMPARE(getter(-1), testValue);           // int: sentinel
QCOMPARE(getter(QString()), testValue);    // string: empty
```

## Data-driven tests and compound types

`_data()` tables suit simple bool, int and string settings (see
`boolRoundTrip_data()` and `intRoundTrip_data()` in `tst_settings.cpp`).
Compound types such as `PasswordConfiguration` usually do not fit: different
tests check different fields, and nested data (`QMap`, structs) makes the table
hard to read. Keep those tests explicit.

## Comparing paths

Normalise both sides with `QDir::cleanPath()` before comparing, and put both
values in the failure message:

```cpp
QString path = Pass::getGpgIdPath(passStore, passStore);
QString expected = passStore + "/.gpg-id";
QVERIFY2(QDir::cleanPath(path) == QDir::cleanPath(expected),
         qPrintable(QString("Expected %1, got %2")
                        .arg(QDir::cleanPath(expected), QDir::cleanPath(path))));
```

## Temporary files

```cpp
QTemporaryDir tempDir;
QVERIFY(tempDir.isValid());
QFile file(tempDir.filePath("test.txt"));
QVERIFY(file.open(QIODevice::WriteOnly));
file.write("test data");
file.close();
```

## Settings isolation

`isolateTestSettings()` (in `tests/auto/testsettings.h`) must be the first
statement of `initTestCase()`, before anything calls
`QtPassSettings::getInstance()`: the singleton fixes its file path on first
use. It switches the default format to `IniFormat` and points both ini and
native paths at a temporary directory, so on Windows the registry is not
touched either. Without it a suite writes to the user's live
`~/.config/IJHack/QtPass.conf`, and a suite killed half-way leaves a temporary
store path in the real config.

## Reading AddressSanitizer output

A leak names the allocation site:

```text
Direct leak of 88 byte(s) in 1 object(s) allocated from:
    #0 ... in operator new(unsigned long)
    #1 ... in ImportKeyDialog::ImportKeyDialog(QString const&, QWidget*) src/importkeydialog.cpp:28
SUMMARY: AddressSanitizer: 440 byte(s) leaked in 5 allocation(s).
```

That one was the `ImportKeyDialog` Ui struct (fixed in #1762). On 2026-09-16
every suite ran clean under ASan and LSan with no suppression file, so a
report from this recipe is a real regression, not Qt noise. Frame `#1` (the
first frame in QtPass code) is usually where the missing parent or `delete`
belongs.
