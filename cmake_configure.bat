@echo off
REM ================================================================
REM  S Lua Compiler ? Configure and Build
REM  Run from your slua-compiler\ directory
REM ================================================================

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

echo.
echo [sluac] Configuring...
cmake -S . -B build ^
    -G Ninja ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DLLVM_DIR="C:\LLVM\lib\cmake\llvm" ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] CMake configure failed!
    echo   Check LLVM_DIR: C:\LLVM\lib\cmake\llvm
    pause & exit /b 1
)

echo.
echo [sluac] Building...
cmake --build build --parallel

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed.
    pause & exit /b 1
)

echo.
echo ================================================================
echo   BUILD SUCCESSFUL  -  build\compiler\sluac.exe
echo ================================================================
echo.
echo Test it:
echo   build\compiler\sluac.exe examples\hello_strict.slua --emit-tokens
echo.
pause
