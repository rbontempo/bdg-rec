@echo off
echo ============================================
echo  BDG REC - Windows Build Script
echo ============================================
echo.

:: Check for CMake
where cmake >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake not found. Install from https://cmake.org/download/
    pause
    exit /b 1
)

:: Check for Visual Studio
where cl >nul 2>&1
if errorlevel 1 (
    echo ERROR: MSVC compiler not found.
    echo Run this script from "Developer Command Prompt for VS 2022"
    echo or install Visual Studio with "Desktop development with C++" workload.
    pause
    exit /b 1
)

echo [1/3] Configuring...
cmake -B build-win -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo Configuration failed!
    pause
    exit /b 1
)

echo.
echo [2/3] Building Release...
cmake --build build-win --target BDG_REC --config Release
if errorlevel 1 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo [3/3] Creating installer package...
if not exist dist mkdir dist

:: Extract version from CMakeLists.txt
for /f "tokens=3" %%v in ('findstr /R "VERSION [0-9]" CMakeLists.txt') do set "VERSION=%%v"
set "OUTDIR=dist\BDG_REC_%VERSION%_Windows"
if exist "%OUTDIR%" rmdir /s /q "%OUTDIR%"
mkdir "%OUTDIR%"

copy "build-win\BDG_REC_artefacts\Release\BDG rec.exe" "%OUTDIR%\"

echo.
echo ============================================
echo  Build complete!
echo  Output: %OUTDIR%\BDG rec.exe
echo ============================================
echo.
pause
