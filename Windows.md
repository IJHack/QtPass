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

### Step 3: Open a shell with MSVC environment

Open a **Developer Command Prompt for VS 2022** or run:

```cmd
"%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
```

Then set:

```cmd
set PATH=C:\Qt\6.8.0\msvc2022_64\bin;C:\Program Files\Git\bin;%PATH%
set QMAKESPEC=
set QTDIR=
```

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

## Notes

- qmake must come from `msvc2022_64`, not `mingw_64`
- nmake requires the MSVC environment (`VsDevCmd.bat`)
- tests expect `bash` (provided by Git for Windows)

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

| Issue                                               | Solution                                                                                                              |
| --------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| "GnuPG not found"                                   | Install [Gpg4win](https://www.gpg4win.org/), restart QtPass, or set path manually in Config                           |
| "Signature does not exist"                          | Ensure your GPG key is in the `.gpg-id` file via **Config → Users**                                                   |
| Git not working                                     | Use [Git Credential Manager](https://github.com/GitCredentialManager/git-credential-manager) for HTTPS authentication |
| App doesn't start                                   | Install `vcredist140` (Visual C++ Redistributable)                                                                    |
| `QMAKE_MSC_VER` isn't set                           | delete `.qmake.stash` and rerun qmake                                                                                 |
| nmake fails with Unix commands (`test`, `mkdir -p`) | Wrong Qt variant installed (MinGW instead of MSVC) - reinstall using MSVC Qt build                                    |
| Should find Bash in PATH                            | ensure `C:\Program Files\Git\bin` is on PATH                                                                          |
| cl not found                                        | MSVC environment not loaded - run VsDevCmd.bat                                                                        |

### Build fails with weird errors

Clean and reconfigure:

```cmd
del .qmake.stash
nmake distclean
qmake
```
