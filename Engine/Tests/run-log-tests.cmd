@echo off
setlocal
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath`) do set "ATOM_VS=%%i"
call "%ATOM_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0\..\.."
cl /nologo /std:c++20 /EHsc /utf-8 /DUNICODE /D_UNICODE /I Engine\Source Engine\Tests\LogSystemTests.cpp Engine\Source\Runtime\Core\LogSystem\LogSystem.cpp /Fo:generated\ /Fe:generated\LogSystemTests.exe
if errorlevel 1 exit /b 1
generated\LogSystemTests.exe
