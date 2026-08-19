@echo off
chcp 65001 >nul 2>&1
set PATH=D:\mingw64\mingw64\bin;%PATH%

set WORK_DIR=E:\HXTools\bin\OEM_Editor_cpp
set SRC=%WORK_DIR%\OEM_Editor.cpp
set RC=%WORK_DIR%\OEM_Editor.rc
set RES=%WORK_DIR%\OEM_Editor_res.o
set OUT=%WORK_DIR%\OEM_Editor.exe

echo [1/3] Compiling resource file (font embedded: ~2MB)...
windres -i "%RC%" -o "%RES%" --output-format=coff
if errorlevel 1 (
    echo windres failed.
    pause
    exit /b 1
)

echo [2/3] Compiling C++ source + linking resources...
g++ -static -static-libgcc -static-libstdc++ -mwindows ^
    -o "%OUT%" "%SRC%" "%RES%" ^
    -lcomctl32 -lshlwapi -lgdi32 -lcomdlg32
if errorlevel 1 (
    echo g++ failed.
    pause
    exit /b 1
)

echo [3/3] Done!
for %%A in ("%OUT%") do echo   %%~nxA  %%~zA bytes
echo.
echo Fully standalone - no external files needed.
pause
