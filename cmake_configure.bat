@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

echo.
echo [sluac] Configuring...
cmake -S . -B build ^
  -G Ninja ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows ^
  -DLLVM_DIR=C:/vcpkg/installed/x64-windows/share/llvm ^
  -DLLVM_ROOT=C:/vcpkg/installed/x64-windows

if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configure failed!
    pause
    exit /b 1
)

echo.
echo [sluac] Building...
cmake --build build --parallel

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

echo.
echo ================================================================
echo   BUILD SUCCESSFUL  -  build\compiler\sluac.exe
echo ================================================================
echo Test it:
echo   build\compiler\sluac.exe examples\hello_strict.slua --emit-ir
pause
