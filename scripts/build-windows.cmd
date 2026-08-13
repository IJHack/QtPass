@echo off
REM SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
REM SPDX-License-Identifier: GPL-3.0-or-later
REM
REM Configure an MSVC x64 + Qt environment and build QtPass on Windows.
REM
REM Runs from a bare cmd.exe: it loads the Visual Studio x64 environment itself
REM and puts the right Qt first on PATH, then does what Windows.md describes:
REM   qmake -spec win32-msvc && nmake
REM
REM Every assumption is checked up front, because the failures otherwise surface
REM as template instantiation errors deep inside Qt headers. In particular a
REM Python/Anaconda Qt on PATH will win over your real Qt, and Qt 5.15 cannot
REM compile with a current MSVC at all.
REM
REM Usage: scripts\build-windows.cmd [target]
REM   target    nmake target: omitted (build), check, clean, distclean
REM
REM Environment:
REM   QT_DIR    Qt prefix to use, e.g. R:\Qt\6.8.0\msvc2022_64
REM             When unset, the newest Qt 6 msvc*_64 install is auto-detected.
REM   ALLOW_QT5 Set to 1 to attempt a Qt 5 build anyway (expected to fail on
REM             MSVC 2022 17.10 and newer).

setlocal EnableDelayedExpansion

set "TARGET=%~1"
cd /d "%~dp0.." || exit /b 1
set "REPO_ROOT=%CD%"

if not exist "qtpass.pro" (
    >&2 echo Error: qtpass.pro not found in "%REPO_ROOT%".
    >&2 echo        Run this script from a checkout, as scripts\build-windows.cmd.
    exit /b 1
)

REM ---------------------------------------------------------------------------
REM 1. Locate Visual Studio and load the x64 environment
REM ---------------------------------------------------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    >&2 echo Error: vswhere.exe not found, so Visual Studio cannot be located.
    >&2 echo        Install the C++ build tools:
    >&2 echo          choco install -y visualstudio2022buildtools visualstudio2022-workload-vctools
    exit /b 1
)

set "VSINSTALL="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
    >&2 echo Error: no Visual Studio install with the x64 C++ toolset was found.
    >&2 echo        Install the workload:
    >&2 echo          choco install -y visualstudio2022buildtools visualstudio2022-workload-vctools
    exit /b 1
)

set "VCVARS=%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    >&2 echo Error: vcvars64.bat not found under "%VSINSTALL%".
    >&2 echo        The x64 C++ toolset ^(VC.Tools.x86.x64^) is not installed.
    exit /b 1
)

call "%VCVARS%" >nul
if errorlevel 1 (
    >&2 echo Error: failed to initialise the MSVC x64 environment.
    >&2 echo        Tried: "%VCVARS%"
    exit /b 1
)

REM ---------------------------------------------------------------------------
REM 2. Confirm the compiler really targets x64
REM
REM A 32-bit cl on PATH links against an x64 Qt with
REM   LNK1112: module machine type 'x64' conflicts with target machine type 'x86'
REM ---------------------------------------------------------------------------
where cl >nul 2>&1
if errorlevel 1 (
    >&2 echo Error: cl.exe is not on PATH after loading the MSVC environment.
    exit /b 1
)

set "CL_IS_X64="
for /f "usebackq delims=" %%l in (`cl 2^>^&1 ^| findstr /i /c:"for x64"`) do set "CL_IS_X64=1"
if not defined CL_IS_X64 (
    >&2 echo Error: cl.exe does not target x64.
    >&2 echo        Open a shell with: VsDevCmd.bat -arch=amd64 -host_arch=amd64
    exit /b 1
)

REM ---------------------------------------------------------------------------
REM 3. Locate Qt
REM ---------------------------------------------------------------------------
set "QT_BEST="
set "QT_BEST_VER="
set "QT_BEST_MAJ=0"
set "QT_BEST_MIN=0"
set "QT_BEST_PAT=0"

if defined QT_DIR goto :qt_explicit

for %%R in ("C:\Qt" "R:\Qt" "D:\Qt" "%USERPROFILE%\Qt") do (
    if exist "%%~R\" (
        for /f "usebackq delims=" %%Q in (`dir /b /s /a-d "%%~R\qmake.exe" 2^>nul`) do call :consider "%%~fQ"
    )
)

if not defined QT_BEST (
    >&2 echo Error: no MSVC x64 Qt install found.
    >&2 echo        Looked for *\msvc*_64\bin\qmake.exe under C:\Qt, R:\Qt, D:\Qt and %%USERPROFILE%%\Qt.
    >&2 echo        Install one, for example:
    >&2 echo          py -m pip install --user -U aqtinstall
    >&2 echo          py -m aqt install-qt -O C:\Qt windows desktop 6.8.0 win64_msvc2022_64
    >&2 echo        Or point at an existing one:  set QT_DIR=X:\Qt\6.8.0\msvc2022_64
    exit /b 1
)
set "QT_DIR=%QT_BEST%"
goto :qt_ready

:qt_explicit
if not exist "%QT_DIR%\bin\qmake.exe" (
    >&2 echo Error: QT_DIR is set to "%QT_DIR%" but "%QT_DIR%\bin\qmake.exe" does not exist.
    exit /b 1
)

:qt_ready

REM ---------------------------------------------------------------------------
REM 4. Put that Qt first on PATH, and clear the variables that override it
REM ---------------------------------------------------------------------------
set "PATH=%QT_DIR%\bin;%PATH%"
set "QMAKESPEC="
set "QTDIR="

REM ---------------------------------------------------------------------------
REM 5. Reject a hijacked or unusable Qt
REM ---------------------------------------------------------------------------
set "QMAKE_RESOLVED="
for /f "usebackq delims=" %%i in (`where qmake 2^>nul`) do if not defined QMAKE_RESOLVED set "QMAKE_RESOLVED=%%i"
if not defined QMAKE_RESOLVED (
    >&2 echo Error: qmake.exe not on PATH even after prepending "%QT_DIR%\bin".
    exit /b 1
)

echo "%QMAKE_RESOLVED%" | findstr /i /c:"conda" >nul
if not errorlevel 1 goto :err_conda
echo "%QMAKE_RESOLVED%" | findstr /i /c:"mingw" >nul
if not errorlevel 1 goto :err_mingw

set "QT_VERSION="
for /f "usebackq delims=" %%v in (`qmake -query QT_VERSION 2^>nul`) do set "QT_VERSION=%%v"
if not defined QT_VERSION (
    >&2 echo Error: "%QMAKE_RESOLVED%" did not answer -query QT_VERSION.
    >&2 echo        It is probably not a working qmake.
    exit /b 1
)

for /f "tokens=1 delims=." %%m in ("%QT_VERSION%") do set "QT_MAJOR=%%m"
if "%QT_MAJOR%"=="5" if not "%ALLOW_QT5%"=="1" goto :err_qt5

REM ---------------------------------------------------------------------------
REM 6. Build
REM ---------------------------------------------------------------------------
echo Visual Studio : %VSINSTALL%
echo Qt            : %QT_DIR% ^(%QT_VERSION%^)
echo qmake         : %QMAKE_RESOLVED%
echo Repository    : %REPO_ROOT%
if defined TARGET echo Target        : %TARGET%
echo.

REM A stash written by another Qt or toolchain makes qmake reuse stale values.
if exist ".qmake.stash" del /q ".qmake.stash"

qmake -spec win32-msvc
if errorlevel 1 (
    >&2 echo.
    >&2 echo Error: qmake failed.
    exit /b 1
)

if /i "%TARGET%"=="check" (
    REM nmake stops at the first failing test binary, and /K does not help
    REM because qmake's recursive rules invoke nmake without it. See Windows.md
    REM for running a single test, and note that test output is only captured
    REM reliably with -o results.txt,txt on Windows.
    nmake check TESTARGS="--platform offscreen"
) else (
    nmake %TARGET%
)
if errorlevel 1 (
    >&2 echo.
    >&2 echo Error: nmake %TARGET% failed.
    exit /b 1
)

echo.
echo Build finished.
exit /b 0

REM ---------------------------------------------------------------------------
REM Errors
REM ---------------------------------------------------------------------------
:err_conda
>&2 echo Error: qmake resolves to a conda-packaged Qt:
>&2 echo          %QMAKE_RESOLVED%
>&2 echo        Anaconda/Miniconda ship Qt 5.15 for PyQt, with headers under
>&2 echo        include\qt and libraries named Qt5Core_conda.lib. It cannot build
>&2 echo        this project, and it is usually earlier on PATH than your real Qt.
>&2 echo.
>&2 echo        This is what produces, part way through the build:
>&2 echo          qlist.h: error C2653: 'stdext': is not a class or namespace name
>&2 echo          error C3861: 'make_checked_array_iterator': identifier not found
>&2 echo.
>&2 echo        Point at an MSVC Qt 6 instead:  set QT_DIR=X:\Qt\6.8.0\msvc2022_64
exit /b 1

:err_mingw
>&2 echo Error: qmake resolves to a MinGW Qt:
>&2 echo          %QMAKE_RESOLVED%
>&2 echo        MinGW is not supported with nmake. Install an msvc*_64 Qt build.
exit /b 1

:err_qt5
>&2 echo Error: qmake reports Qt %QT_VERSION%, which cannot be built with a current MSVC.
>&2 echo          %QMAKE_RESOLVED%
>&2 echo        Qt 5.15 defines QT_MAKE_CHECKED_ARRAY_ITERATOR as
>&2 echo        stdext::make_checked_array_iterator, which Microsoft removed from its
>&2 echo        STL, so qlist.h fails with:
>&2 echo          error C2653: 'stdext': is not a class or namespace name
>&2 echo        Use Qt 6 on Windows. CI only tests Qt 5.15 on Linux.
>&2 echo        Override with ALLOW_QT5=1 if your MSVC still provides stdext.
exit /b 1

REM ---------------------------------------------------------------------------
REM :consider <full path to qmake.exe>
REM
REM Keeps the highest-versioned msvc*_64 Qt seen so far. Called rather than
REM inlined so each candidate gets its own variable expansion.
REM ---------------------------------------------------------------------------
:consider
set "CAND_QMAKE=%~1"
echo "%CAND_QMAKE%" | findstr /i /r /c:"\\msvc[^\\]*_64\\bin\\qmake\.exe" >nul
if errorlevel 1 goto :eof

for %%P in ("%~dp1..") do set "CAND_PREFIX=%%~fP"
for %%W in ("%CAND_PREFIX%\..") do set "CAND_VER=%%~nxW"

set "CAND_MAJ=0"
set "CAND_MIN=0"
set "CAND_PAT=0"
for /f "tokens=1,2,3 delims=." %%a in ("%CAND_VER%") do (
    set "CAND_MAJ=%%a"
    set "CAND_MIN=%%b"
    set "CAND_PAT=%%c"
)
if not defined CAND_MIN set "CAND_MIN=0"
if not defined CAND_PAT set "CAND_PAT=0"

REM Anything non-numeric sorts as 0 rather than aborting the scan.
set /a "CAND_MAJ=CAND_MAJ+0, CAND_MIN=CAND_MIN+0, CAND_PAT=CAND_PAT+0" 2>nul

REM Prefer Qt 6 over Qt 5 regardless of the rest of the version.
if %CAND_MAJ% LSS %QT_BEST_MAJ% goto :eof
if %CAND_MAJ% GTR %QT_BEST_MAJ% goto :consider_take
if %CAND_MIN% LSS %QT_BEST_MIN% goto :eof
if %CAND_MIN% GTR %QT_BEST_MIN% goto :consider_take
if %CAND_PAT% LEQ %QT_BEST_PAT% goto :eof

:consider_take
set "QT_BEST=%CAND_PREFIX%"
set "QT_BEST_VER=%CAND_VER%"
set "QT_BEST_MAJ=%CAND_MAJ%"
set "QT_BEST_MIN=%CAND_MIN%"
set "QT_BEST_PAT=%CAND_PAT%"
goto :eof
