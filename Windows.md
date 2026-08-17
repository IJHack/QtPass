# Windows Guide

QtPass builds on Windows using MSVC, qmake and nmake.

MinGW is not supported with nmake.

## Install QtPass

### Using Chocolatey (recommended)

Run in **Administrator PowerShell**:

```powershell
Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))

choco install vcredist140 -y
choco install qtpass -y
```

### Or download from releases

Download the latest `.exe` installer from [GitHub Releases](https://github.com/IJHack/QtPass/releases)

## Install GPG

QtPass requires GPG for encryption. Install [Gpg4win](https://www.gpg4win.org/).

Key generation can take a long time (especially RSA 4096-bit keys) - this is normal.

## Building from Source

> **Use Qt 6 on Windows.** Qt 5.15 cannot be compiled by a current MSVC: it
> defines `QT_MAKE_CHECKED_ARRAY_ITERATOR` as
> `stdext::make_checked_array_iterator`, which Microsoft has removed from its
> STL, so `qlist.h` fails with
> `error C2653: 'stdext': is not a class or namespace name`. CI only tests
> Qt 5.15 on Linux.

### Quick start

From any `cmd.exe` — this loads the MSVC x64 environment, picks an MSVC Qt and
builds:

```cmd
git clone https://github.com/IJHack/QtPass.git
cd QtPass
scripts\build-windows.cmd
```

Pass a target to do something else, for example `scripts\build-windows.cmd check`
or `scripts\build-windows.cmd distclean`. Set `QT_DIR` to choose a specific Qt:

```cmd
set QT_DIR=C:\Qt\6.8.0\msvc2022_64
```

The script checks each prerequisite and explains what is wrong rather than
letting the build fail deep inside Qt headers. The manual steps below do the
same thing by hand.

### Step 1: Install dependencies

Run in **Administrator PowerShell**:

```powershell
choco install -y git python visualstudio2022buildtools visualstudio2022-workload-vctools
```

### Step 2: Install Qt

Install an **MSVC build of Qt** (not MinGW).

Do not use `mingw_64`. It will not work with `nmake`.

Run in **normal PowerShell**:

```powershell
py -m pip install --user -U aqtinstall
py -m aqt install-qt -O C:\Qt windows desktop 6.8.0 win64_msvc2022_64
```

Any install prefix works — `-O C:\Qt` is only an example. `scripts\build-windows.cmd`
searches `C:\Qt`, `R:\Qt`, `D:\Qt` and `%USERPROFILE%\Qt`, or use `QT_DIR` to
point it anywhere.

### Step 3: Open a shell with MSVC environment

Open a **Developer Command Prompt for VS 2022** or run:

```cmd
"%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
```

Then set (adjust the Qt path to your install):

```cmd
set PATH=C:\Qt\6.8.0\msvc2022_64\bin;C:\Program Files\Git\bin;%PATH%
set QMAKESPEC=
set QTDIR=
```

Verify the shell before building — these two checks catch the mistakes that
otherwise surface as confusing compiler errors:

```cmd
where qmake
qmake -query QT_VERSION
cl
```

- `qmake` must resolve inside your Qt `msvc*_64\bin`, and report a 6.x version.
  If a Python distribution is installed, its Qt often comes first on `PATH` —
  Anaconda ships Qt 5.15 as `E:\...\anaconda3\Library\bin\qmake.exe` and will
  hijack the build. Prepending the Qt bin directory, as above, fixes it.
- `cl` must print `for x64`. A 32-bit shell fails later at link time with
  `LNK1112`.

### Step 4: Clone and build

```cmd
git clone https://github.com/IJHack/QtPass.git
cd QtPass
```

If you previously built with another Qt version or toolchain:

```cmd
del .qmake.stash
```

Then build:

```cmd
qmake -spec win32-msvc
nmake
nmake check TESTARGS="--platform offscreen"
```

### Running tests

Two Windows-specific gotchas:

- `nmake check` stops at the **first** failing test binary, so later results are
  never reached. `nmake /K` does not help, because qmake's recursive rules
  invoke `nmake` without it.
- QtTest output is lost when redirected or piped on Windows. Use `-o` to capture
  it; the exit code is reliable either way.

To run one test and read its output:

```cmd
cd tests\auto\totp
release\tst_totp.exe -o results.txt,txt --platform offscreen
type results.txt
```

Add a test function name to narrow it further, e.g.
`release\tst_totp.exe generateRfc6238Vectors -o results.txt,txt`.

## Notes

- qmake must come from `msvc2022_64`, not `mingw_64`
- Qt 6 is required; Qt 5.15 does not compile with a current MSVC (see above)
- nmake requires the MSVC environment (`VsDevCmd.bat`)
- tests expect `bash` (provided by Git for Windows)
- any `qmake` run rewrites `localization/*.ts`; discard with
  `git checkout -- localization/`

## First Run

1. Launch QtPass
2. Click "Autodetect" to find GPG and Git
3. Set your password store location (default: `%APPDATA%\password-store`)
4. Or select an existing `pass` store folder

## Configure GPG Key

1. Go to **Config → User** to select your GPG key
2. If using for the first time, generate a key with **Config → Generate GPG key**

## Initialize Password Store (if new)

1. Click **File → Initialize**
2. Select your GPG key(s) for encryption
3. Your `.gpg-id` file will be created

## Troubleshooting

| Issue                                                                                                | Solution                                                                                                                                |
| ---------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------- |
| "GnuPG not found"                                                                                    | Install [Gpg4win](https://www.gpg4win.org/), restart QtPass, or set path manually in Config                                             |
| "Signature does not exist"                                                                           | Ensure your GPG key is in the `.gpg-id` file via **Config → Users**                                                                     |
| Git not working                                                                                      | Use [Git Credential Manager](https://github.com/GitCredentialManager/git-credential-manager) for HTTPS authentication                   |
| App doesn't start                                                                                    | Install `vcredist140` (Visual C++ Redistributable)                                                                                      |
| `QMAKE_MSC_VER` isn't set                                                                            | delete `.qmake.stash` and rerun qmake                                                                                                   |
| `'stdext': is not a class or namespace name` / `make_checked_array_iterator' : identifier not found` | `qmake` is a Qt 5.15 build, often Anaconda's on `PATH`. Qt 5.15 cannot compile with a current MSVC - use Qt 6. Check with `where qmake` |
| `LNK1112: module machine type 'x64' conflicts with target machine type 'x86'`                        | 32-bit MSVC shell. Use `VsDevCmd.bat -arch=amd64 -host_arch=amd64`, or `scripts\build-windows.cmd`                                      |
| `uic`/`rcc`/`lrelease` run from an unexpected directory                                              | Another Qt is earlier on `PATH`. Prepend your Qt `bin` (Step 3) or use `scripts\build-windows.cmd`                                      |
| `nmake check` reports no test output                                                                 | Expected on Windows - QtTest output is lost when redirected. Run the test binary with `-o results.txt,txt`                              |
| nmake fails with Unix commands (`test`, `mkdir -p`)                                                  | Wrong Qt variant installed (MinGW instead of MSVC) - reinstall using MSVC Qt build                                                      |
| Should find Bash in PATH                                                                             | ensure `C:\Program Files\Git\bin` is on PATH                                                                                            |
| cl not found                                                                                         | MSVC environment not loaded - run VsDevCmd.bat                                                                                          |

### Build fails with weird errors

Clean and reconfigure:

```cmd
del .qmake.stash
nmake distclean
qmake
```
