# QtPass Fix Patterns Catalogue

The less frequent fix patterns from past QtPass reviews. The five most common
(logging, Qt version floor, null checks, index instead of pointer,
`std::as_const`) live inline in [SKILL.md](../SKILL.md).

## Contents

- [Code patterns](#code-patterns)
- [UI and accessibility](#ui-and-accessibility)
- [Test hygiene](#test-hygiene)
- [Style nits reviewers flag](#style-nits-reviewers-flag)

## Code patterns

### Return after dialog rejection

After an error dialog and `reject()` in a constructor, `return` so the
constructor does not continue with invalid state:

```cpp
if (users.isEmpty()) {
  QMessageBox::critical(parent, tr("Error"), tr("No users found"));
  reject();
  return;
}
```

### CLI argument parsing

Use `QCoreApplication::arguments()` instead of raw `argv[]`, so Qt normalizes
paths and handles quoting:

```cpp
const QStringList args = QCoreApplication::arguments();
```

### Cross-platform paths

Use `QDir::cleanPath()` to normalize `/` versus `\` and `.`/`..` components:

```cpp
const QString normalized = QDir::cleanPath(gpgIdPath);
```

### Optional external tools

Check that an optional binary is available before using it, and report a
translated error instead of failing silently. OTP no longer needs the
`pass-otp` extension: TOTP is built-in (`src/totp.h`), so do not add a
`pass-otp` check.

### Dropping env vars by prefix

`QStringList::filter()` keeps entries that contain the key anywhere, so it
selects instead of removing and over-matches (`FOO` also hits `FOOBAR=x`).
Remove by prefix instead:

```cpp
// Bad: keeps the matches, drops everything else
env = env.filter(key);

// Good (Qt 6.1+, so fine on the 6.8 floor)
env.removeIf([&key](const QString &e) { return e.startsWith(key); });
```

### Boolean logic

Comparing booleans with `==` yields true when both are false:

```cpp
// Bad: false == false is true for a public key
handlePubSecRecord(props, secret == (type == GpgRecordType::Sec));

// Good: both conditions must hold
handlePubSecRecord(props, secret && (type == GpgRecordType::Sec));
```

### Regular expressions on URLs

Use `\\S+` (non-whitespace) so a pattern does not run across spaces:

```cpp
QRegularExpression urlRegex("(\\S+)://(\\S+)");
```

## UI and accessibility

### Theme-aware colors

Prefer `QPalette` roles over fixed colors, which can have poor contrast on
some themes:

```cpp
// Fixed color: may be unreadable on dark themes
item->setForeground(Qt::blue);

// Theme-aware
item->setForeground(QApplication::palette().color(QPalette::Link));
```

### Color plus text

When status is shown by color (invalid, expired, partial), also add a text
prefix or tooltip for color blind users:

```cpp
item->setBackground(Qt::darkRed);
item->setText(tr("[INVALID] ") + originalText);
item->setToolTip(tr("Invalid key"));
```

### Monospace fonts

For monospace text in tables and lists, call
`font.setStyleHint(QFont::Monospace)` instead of relying on a platform default
(see `src/mainwindow.cpp`).

## Test hygiene

### Tautological assertions

```cpp
// Bad: always true
QVERIFY(profiles.isEmpty() || !profiles.isEmpty());

// Good: a real check
QVERIFY(profiles.isEmpty());
QVERIFY2(store.isEmpty() || store.startsWith("/"),
         "Pass store should be empty or a plausible path");
```

### Check setup return values

```cpp
// Bad: ignores failure
(void)QDir(srcDir.path()).mkdir("source");
(void)f1.open(QFile::WriteOnly);

// Good
QVERIFY(QDir(srcDir.path()).mkdir("source"));
QVERIFY(f1.open(QFile::WriteOnly));
```

## Style nits reviewers flag

### Copyright year in SPDX headers

Use the single real year the file was created: no `YYYY` placeholder (flagged
in PR #1162) and no year range.

```cpp
// SPDX-FileCopyrightText: 2026 Your Name
// SPDX-License-Identifier: GPL-3.0-or-later
```

### Contractions in comments

Write "cannot", not "can't":

```cpp
// On Windows, we cannot safely back up
```
