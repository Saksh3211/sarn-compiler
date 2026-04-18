@echo off
setlocal enabledelayedexpansion

REM Find and call Visual Studio vcvars64.bat
set VCVARS_FOUND=0

for %%V in (2022 2019) do (
    if !VCVARS_FOUND! equ 0 (
        set "VC_PATH=C:\Program Files\Microsoft Visual Studio\%%V\Community\VC\Auxiliary\Build\vcvars64.bat"
        if exist "!VC_PATH!" (
            echo Initializing Visual Studio %%V...
            call "!VC_PATH!"
            set VCVARS_FOUND=1
            goto vcvars_done
        )
    )
    if !VCVARS_FOUND! equ 0 (
        set "VC_PATH=C:\Program Files\Microsoft Visual Studio\%%V\Professional\VC\Auxiliary\Build\vcvars64.bat"
        if exist "!VC_PATH!" (
            echo Initializing Visual Studio %%V...
            call "!VC_PATH!"
            set VCVARS_FOUND=1
            goto vcvars_done
        )
    )
    if !VCVARS_FOUND! equ 0 (
        set "VC_PATH=C:\Program Files (x86)\Microsoft Visual Studio\%%V\Community\VC\Auxiliary\Build\vcvars64.bat"
        if exist "!VC_PATH!" (
            echo Initializing Visual Studio %%V...
            call "!VC_PATH!"
            set VCVARS_FOUND=1
            goto vcvars_done
        )
    )
)

:vcvars_done
if !VCVARS_FOUND! equ 0 (
    echo [ERROR] Could not find Visual Studio. Install VS 2019 or 2022.
    pause
    exit /b 1
)

echo.
echo [sarn] Configuring CMake...
echo.

REM Clean old cmake cache if using a different generator
if exist build\CMakeCache.txt (
    for /f "tokens=*" %%i in ('type build\CMakeCache.txt ^| findstr /R "CMAKE_GENERATOR:"') do (
        set CURRENT_GENERATOR=%%i
    )
)

REM Configure with Visual Studio (more reliable than Ninja on Windows)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release

if !ERRORLEVEL! neq 0 (
    echo [ERROR] CMake configuration failed!
    pause
    exit /b 1
)

echo.
echo [sarn] Building...
cmake --build build --config Release --parallel

if !ERRORLEVEL! neq 0 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

echo.
echo ================================================================
echo   BUILD SUCCESSFUL!
echo ================================================================
echo.
if exist "build\compiler\Release\sarnc.exe" (
    echo Compiler: build\compiler\Release\sarnc.exe
) else if exist "build\compiler\sarnc.exe" (
    echo Compiler: build\compiler\sarnc.exe
)
echo.
echo Test with: .\sarn.ps1 Sarn-Run learn-sarn\examples\...
echo.
pause

