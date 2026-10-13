@echo off
setlocal
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath`) do set "ATOM_VS=%%i"
call "%ATOM_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0\..\.."
cl /nologo /std:c++20 /EHsc /Z7 /utf-8 /DUNICODE /D_UNICODE /I Engine\Source /I Engine\External\entt Engine\Tests\FrameworkTests.cpp Engine\Source\Runtime\Function\Scene\Scene.cpp Engine\Source\Runtime\Function\Scene\SceneManager.cpp Engine\Source\Runtime\Function\Framework\ECS\World.cpp Engine\Source\Runtime\Function\Framework\ECS\GameObject.cpp /Fo:generated\ /Fe:generated\FrameworkTests.exe /link /DEBUG
if errorlevel 1 exit /b 1
generated\FrameworkTests.exe
