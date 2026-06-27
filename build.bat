@echo off
REM ===========================================================================
REM  VideoSync - build and deploy a standalone release
REM
REM  Builds the project with the Qt-bundled CMake/Ninja/MinGW toolchain, then
REM  runs windeployqt so the output folder contains everything needed to run
REM  VideoSync.exe on a machine without Qt installed.
REM
REM  Output: build\release\
REM ===========================================================================

setlocal

REM --- Toolchain locations (edit these if your Qt install differs) ----------
set "QT_DIR=C:\Qt\6.11.0\mingw_64"
set "MINGW_DIR=C:\Qt\Tools\mingw1310_64"
set "CMAKE=C:\Qt\Tools\CMake_64\bin\cmake.exe"
set "NINJA_DIR=C:\Qt\Tools\Ninja"

set "BUILD_DIR=%~dp0build\release"

REM --- Sanity checks --------------------------------------------------------
if not exist "%QT_DIR%\bin\windeployqt.exe" (
    echo [ERROR] windeployqt not found under "%QT_DIR%".
    echo         Edit QT_DIR at the top of this script to match your Qt install.
    exit /b 1
)
if not exist "%CMAKE%" (
    echo [ERROR] cmake not found at "%CMAKE%".
    exit /b 1
)
if not exist "%MINGW_DIR%\bin\g++.exe" (
    echo [ERROR] MinGW g++ not found under "%MINGW_DIR%".
    exit /b 1
)

REM --- Make the toolchain visible to CMake/Ninja/windeployqt ----------------
set "PATH=%QT_DIR%\bin;%MINGW_DIR%\bin;%NINJA_DIR%;%PATH%"

REM --- Configure ------------------------------------------------------------
echo === Configuring (CMake + Ninja) ===
"%CMAKE%" -S "%~dp0." -B "%BUILD_DIR%" -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="%QT_DIR%"
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    exit /b 1
)

REM --- Build ----------------------------------------------------------------
echo === Building ===
"%CMAKE%" --build "%BUILD_DIR%"
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

if not exist "%BUILD_DIR%\VideoSync.exe" (
    echo [ERROR] Build succeeded but VideoSync.exe was not found in "%BUILD_DIR%".
    exit /b 1
)

REM --- Deploy Qt runtime ----------------------------------------------------
echo === Deploying Qt runtime (windeployqt) ===
"%QT_DIR%\bin\windeployqt.exe" --release --no-translations --no-system-d3d-compiler "%BUILD_DIR%\VideoSync.exe"
if errorlevel 1 (
    echo [ERROR] windeployqt failed.
    exit /b 1
)

echo.
echo === Done. Standalone build is in: %BUILD_DIR% ===
endlocal
