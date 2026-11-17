@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo Building desktop-tools using Visual Studio 14 2015
echo ===================================================

set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"
if exist %VCVARS% (
    call %VCVARS% amd64 8.1
) else (
    echo [WARNING] vcvarsall.bat not found at %VCVARS%. Proceeding with system environment.
)

if not exist build mkdir build
cd /d "%~dp0build"

echo Configuring with CMake...
cmake -G "Visual Studio 14 2015 Win64" -DCMAKE_SYSTEM_VERSION=8.1 ..
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed.
    exit /b %ERRORLEVEL%
)

echo Building Release binaries...
cmake --build . --config Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed.
    exit /b %ERRORLEVEL%
)

echo.
echo ===================================================
echo Build completed successfully!
echo Executables generated in: %~dp0build\bin
echo   - %~dp0build\bin\switch_desktop_1.exe
echo   - %~dp0build\bin\switch_desktop_2.exe
echo   - %~dp0build\bin\focus_chatgpt.exe
echo ===================================================

exit /b 0
