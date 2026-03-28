@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

echo.
echo [sluac] Configuring...
cmake -S . -B build ^
  -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows ^
  -DLLVM_DIR=C:/vcpkg/installed/x64-windows/share/llvm ^
  -DLLVM_ROOT=C:/vcpkg/installed/x64-windows ^
  -Draylib_DIR=C:/vcpkg/installed/x64-windows/share/raylib

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
echo Test it with running files in the "examples" folder with slua.ps1/slua extenstion, e.g.:
echo   slua examples/hello_world.slua
pause
