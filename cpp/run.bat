@echo off
if not exist "%~dp0bin\Crosshair.exe" (
    echo [ERROR] bin\Crosshair.exe not found.
    echo Run: cmake -B build ; cmake --build build --config Release
    pause
    exit /b 1
)
start "" /min "%~dp0bin\Crosshair.exe"
