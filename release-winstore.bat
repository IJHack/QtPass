call vcvars64.bat
jom.exe distclean
qmake.exe -config release CONFIG+=winstore
jom.exe clean
jom.exe
rmdir /s /q main\release\deploy
del /q /s main\release\deploy
mkdir main\release\deploy
windeployqt.exe --no-compiler-runtime --no-angle --no-opengl-sw main\release\qtpass.exe --dir main\release\deploy
copy /b /y main\release\qtpass.exe main\release\deploy\qtpass.exe
copy /b /y "%VSINSTALLDIR%"\VC\Redist\MSVC\14.15.26706\x64\Microsoft.VC141.CRT\concrt140.dll main\release\deploy\concrt140.dll
copy /b /y "%VSINSTALLDIR%"\VC\Redist\MSVC\14.15.26706\x64\Microsoft.VC141.CRT\msvcp140.dll main\release\deploy\msvcp140.dll
copy /b /y "%VSINSTALLDIR%"\VC\Redist\MSVC\14.15.26706\x64\Microsoft.VC141.CRT\vcruntime140.dll main\release\deploy\vcruntime140.dll

REM Just a workaround for a bug in the Windows Store validation

mkdir main\release\deploy\Assets

for %%I in (artwork\*.png) do copy /b /y %%I main\release\deploy\Assets

REM DesktopAppConverter.exe -Installer main\release\deploy -AppExecutable qtpass.exe -AppId ReimarDffinger.QtPassApp -PackageName 33893ReimarDffinger.QtPassApp -PackageDisplayName "QtPass App" -AppDisplayName "QtPass App" -Destination . -Publisher CN=7FA184CD-E614-4630-8C10-9BC1E1A38DD7 -PackagePublisherDisplayName "Reimar Döffinger" -Version 1.2.3.0 -MakeAppx
