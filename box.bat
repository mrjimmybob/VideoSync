@echo off
REM ===========================================================================
REM  box.bat - one-click: build the release, then package it into a single
REM            standalone VideoSync_boxed.exe (config and log stay external).
REM
REM  Output: build\Publish\VideoSync_boxed.exe
REM ===========================================================================

setlocal

REM --- 1) Build + deploy the release into build\release ---------------------
call "%~dp0build.bat"
if errorlevel 1 (
    echo [ERROR] Build step failed; not boxing.
    exit /b 1
)

REM --- 2) Package into a single boxed exe via Enigma Virtual Box ------------
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0make_box.ps1"
if errorlevel 1 (
    echo [ERROR] Boxing step failed.
    exit /b 1
)

endlocal
