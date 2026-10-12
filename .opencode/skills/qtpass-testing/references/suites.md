# QtPass test suite inventory

Read this when choosing which suite a new test belongs in, or when looking for
an existing test of a class. `tests/auto/auto.pro` `SUBDIRS` is the source of
truth; this table can lag behind it.

## Platform rules (from `tests/auto/auto.pro`)

- Every suite below builds on every platform, except:
- `integration` is skipped on Windows (`!win32`).
- `singleapplication` is skipped when built with `CONFIG+=nosingleapp`.
- `executor` builds everywhere; its shell-script cases guard themselves with
  `#ifndef Q_OS_WIN`, while the parser, resolver and gpgconf cases also run on
  Windows.

## Suites and the code they cover

Each suite lives in `tests/auto/<suite>/tst_<suite>.cpp`, except `model`, whose
file is `tst_storemodel.cpp`.

| Suite                   | Main code under test (`src/`)                                                                                                                                 |
| ----------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `base32`                | `base32.h`                                                                                                                                                    |
| `clipboardmanager`      | `clipboardmanager.h`                                                                                                                                          |
| `configdialog`          | `configdialog.h`                                                                                                                                              |
| `executor`              | `executor.h` (blocking and async execution, WSL command parsing, executable resolution, gpgconf)                                                              |
| `exportpublickeydialog` | `exportpublickeydialog.h`                                                                                                                                     |
| `filecontent`           | `filecontent.h` (password line, named fields, template fields, OTP hiding)                                                                                    |
| `firstrunwizard`        | `firstrunwizard.h`                                                                                                                                            |
| `gpgidgeneration`       | `gpgidgeneration.h`                                                                                                                                           |
| `gpgidsigner`           | `gpgidsigner.h`                                                                                                                                               |
| `gpgkeystate`           | `gpgkeystate.h`, `gpgkeystate_p.h` (`gpg --with-colons` parsing, record classification)                                                                       |
| `grepsearchcontroller`  | `grepsearchcontroller.h`                                                                                                                                      |
| `imitatepass`           | `imitatepass.h` (uses a recording fake gpg)                                                                                                                   |
| `importkeydialog`       | `importkeydialog.h`                                                                                                                                           |
| `integration`           | end-to-end: real gpg with a throwaway keyring, `MainWindow`, both backends                                                                                    |
| `keygendialog`          | `keygendialog.h`                                                                                                                                              |
| `locale`                | the embedded translations (`localization/*.ts`)                                                                                                               |
| `mainwindow`            | `mainwindow.h` and the panels it owns                                                                                                                         |
| `model`                 | `storemodel.h` (display names, flags, drag and drop, sorting, filtering)                                                                                      |
| `passbackendfactory`    | `passbackendfactory.h`                                                                                                                                        |
| `passwordconfig`        | `passwordconfiguration.h` (defaults, setters, character sets)                                                                                                 |
| `passworddialog`        | `passworddialog.h`                                                                                                                                            |
| `passworddisplaypanel`  | `passworddisplaypanel.h` and its button widgets                                                                                                               |
| `processoutputpanel`    | `processoutputpanel.h`                                                                                                                                        |
| `profileinit`           | `profileinit.h`                                                                                                                                               |
| `qtpass`                | `qtpass.h`                                                                                                                                                    |
| `realpass`              | `realpass.h`                                                                                                                                                  |
| `settings`              | `qtpasssettings.h`, `appsettings.h`, `settingsserializer.h` (set and get round trips, profiles)                                                               |
| `simpletransaction`     | `simpletransaction.h`                                                                                                                                         |
| `singleapplication`     | `singleapplication.h`                                                                                                                                         |
| `storetree`             | `storetree.h`                                                                                                                                                 |
| `totp`                  | `totp.h`, `base32.h` (RFC 6238 vectors, Steam codes)                                                                                                          |
| `trayicon`              | `trayicon.h`                                                                                                                                                  |
| `ui`                    | small widgets: `qpushbuttonwithclipboard.h`, `qpushbuttonasqrcode.h`, `qpushbuttonshowpassword.h`, `qprogressindicator.h`, `deselectabletreeview.h`           |
| `userinfo`              | `userinfo.h`                                                                                                                                                  |
| `usersdialog`           | `usersdialog.h`                                                                                                                                               |
| `util`                  | `util.h`, `pathvalidator.h`, `sshauthsock.h`, `templateio.h`, `nativegrep.h`, plus `Pass`/`ImitatePass` helpers (env, recipients, grep output, error mapping) |

`util` is by far the largest suite and has historically been the catch-all.
Prefer the suite named after the class you are testing; add to `util` only for
the static helpers in `util.h` and the small helper classes it already covers.

## Shared helpers

Header-only, in `tests/auto/`:

- `testsettings.h`: `isolateTestSettings()` redirects `QSettings` (and so
  `QtPassSettings`) to a temporary directory. Listed in `auto.pri`, so every
  suite can include it.
- `testpass.h`: `NullPass` (a `Pass` whose operations do nothing; derive and
  override what the test records), `QTRY_VERIFY_WITH_TIMEOUT_RETURN` (poll a
  condition inside a helper that returns a value), and a two-key
  `gpg --with-colons` listing (`kColonListing`) for a stand-in gpg.

## Test data

There is no shared fixtures directory. Suites that need gpg output generate a
throwaway keyring (`integration`) or write a recording fake gpg
(`imitatepass`). Keep test data next to the suite that uses it.
