@echo off
chcp 65001 >nul 2>&1
title OEM_Editor C++ Build

:: Find MSVC
for /f "delims=" %%i in ('"C:\Program Files\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul') do set VS_PATH=%%i
if not defined VS_PATH (
    echo [ERROR] Visual Studio not found. Please install VS2022 with C++ workload.
    pause
    exit /b 1
)

:: Detect architecture
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set VCVARS="%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"
) else if "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    set VCVARS="%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"
) else (
    set VCVARS="%VS_PATH%\VC\Auxiliary\Build\vcvars32.bat"
)

echo [INFO] Using Visual Studio at: %VS_PATH%
echo [INFO] vcvars: %VCVARS%

:: Compile
call %VCVARS% >nul 2>&1

set SRC=OEM_Editor.cpp
set OUT=OEM_Editor.exe
set LIBS=comctl32.lib shlwapi.lib

echo [INFO] Compiling %SRC% ...
cl /utf-8 /EHsc /W4 /O2 /MT /DUNICODE /D_UNICODE ^
    /Fe:%OUT% %SRC% %LIBS%

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Compilation failed.
    pause
    exit /b 1
)

echo [SUCCESS] %OUT% built successfully!
echo.
echo Copy the following files to use:
echo   1. %OUT%        (the executable)
echo   2. DingTalk JinBuTi.ttf
echo   3. OEM_icon.ico
echo All three must be in the same folder.
pause