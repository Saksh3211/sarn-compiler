param(
    [string]$Command,
    [string]$File,
    [string]$Version = "latest"
)

$env:SARN_ROOT = $PSScriptRoot
$env:SARN_BIN  = "$env:SARN_ROOT\bin"
$env:PATH      = "C:\vcpkg\installed\x64-windows\bin;" + $env:PATH

New-Item -ItemType Directory -Force -Path $env:SARN_BIN | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $env:SARN_ROOT ".packages") | Out-Null

function Compile-PackageC {
    param([string]$pkg_dir, [string]$pkg_name)

    $lib_path = Join-Path $pkg_dir ($pkg_name + ".lib")
    if (Test-Path $lib_path) { return $lib_path }

    $pkg_json = Join-Path $pkg_dir "pkg.json"
    if (-not (Test-Path $pkg_json)) { return $null }

    $meta = Get-Content $pkg_json -Raw | ConvertFrom-Json
    if (-not $meta.c_sources -or $meta.c_sources.Count -eq 0) { return $null }

    $inc  = Join-Path $env:SARN_ROOT "runtime\include"
    $objs = @()

    foreach ($src in $meta.c_sources) {
        $sp = Join-Path $pkg_dir $src
        if (-not (Test-Path $sp)) { continue }

        $op = [System.IO.Path]::ChangeExtension($sp, ".obj")
        clang -c -O2 $sp -I $inc -o $op 2>&1 | Out-Null

        if ($LASTEXITCODE -eq 0) { $objs += $op }
    }

    if ($objs.Count -eq 0) { return $null }

    llvm-lib /nologo /OUT:$lib_path @objs 2>&1 | Out-Null

    if ($LASTEXITCODE -eq 0) { return $lib_path }
    return $null
}

function Get-PackageLibs {
    param([string]$src_dir)

    $libs = @()
    $root = Join-Path $env:SARN_ROOT ".packages"

    if (-not (Test-Path $root)) { return @() }

    Get-ChildItem $root -Directory | ForEach-Object {
        $pkg_name = $_.Name
        $pkg_dir  = $_.FullName
        $init = Join-Path $pkg_dir "__init__.sarn"
        
        if ((Test-Path $init)) {
            $lib = Compile-PackageC $pkg_dir $pkg_name
            if ($lib) { $libs += $lib }
        }
    }

    return $libs | Select-Object -Unique
}

function Sarn-Run {
    param([string]$src)

    $name    = [System.IO.Path]::GetFileNameWithoutExtension($src)
    $abs     = Join-Path $env:SARN_ROOT $src
    $src_dir = [System.IO.Path]::GetDirectoryName($abs)

    $ll  = Join-Path $env:SARN_BIN ($name + ".ll")
    $exe = Join-Path $env:SARN_BIN ($name + ".exe")

    & "$env:SARN_ROOT\build\compiler\Release\sarnc.exe" $abs -o $ll

    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] Compile failed" -ForegroundColor Red
        return
    }

    $pkg_libs = Get-PackageLibs $src_dir

    $obj = [System.IO.Path]::ChangeExtension($ll, ".obj")
    
    $llvm_bin = "C:\Program Files\LLVM\bin"
    if (-not (Test-Path $llvm_bin)) {
        $llvm_bin = "C:\Program Files (x86)\LLVM\bin"
    }
    
    $llc_path = Join-Path $llvm_bin "llc.exe"
    if (Test-Path $llc_path) {
        & $llc_path $ll -o $obj 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[ERROR] LLVM IR compilation to object failed" -ForegroundColor Red
            return
        }
    } else {
        $obj = $ll
    }
    
    if (-not (Test-Path $obj)) {
        Write-Host "[ERROR] Object file not found" -ForegroundColor Red
        return
    }

    $link_cmd = @(
        $obj,
        $( $r = Resolve-Path "$env:SARN_ROOT\build\runtime\*\sarn.lib" -ErrorAction SilentlyContinue | Select-Object -First 1; if ($r) { $r.Path } else { "$env:SARN_ROOT\build\runtime\sarn.lib" } ),
        "C:\vcpkg\installed\x64-windows\lib\raylib.lib"
    )
    
    foreach ($lib in $pkg_libs) {
        $link_cmd += $lib
    }
    
    $link_cmd += @(
        "-lOpenGL32","-lgdi32","-lwinmm","-ladvapi32",
        "-lUser32","-lShell32","-lGdi32",
        "-lmsvcrt","-lucrt","-lvcruntime",
        "-o", $exe
    )

    $clang_path = Join-Path $llvm_bin "clang.exe"
    $clang_output = $null
    if (Test-Path $clang_path) {
        $clang_output = & $clang_path @link_cmd 2>&1
    } else {
        $clang_output = clang @link_cmd 2>&1
    }
    
    if ($clang_output) {
        Write-Host "Clang output: $clang_output"
    }

    if (-not (Test-Path $exe)) {
        Write-Host "[ERROR] Linking failed" -ForegroundColor Red
        return
    }

    & $exe
}

function Sarn-Install {
    param([string]$pkg)

    $registry_path = Join-Path $env:SARN_ROOT "packageReg.json"
    $pkg_root = Join-Path $env:SARN_ROOT ".packages"
    
    if ($pkg -match "^https://github.com") {
        $name = [System.IO.Path]::GetFileName($pkg)
        $dest = Join-Path $pkg_root $name

        git clone $pkg $dest
        Write-Host "[OK] Installed $name"
        return
    }

    if (-not (Test-Path $registry_path)) {
        Write-Host "[ERROR] packageReg.json missing" -ForegroundColor Red
        return
    }

    $reg = Get-Content $registry_path -Raw | ConvertFrom-Json
    $repo = $reg.$pkg

    if (-not $repo) {
        Write-Host "[ERROR] Package not found" -ForegroundColor Red
        return
    }

    $dest = Join-Path $pkg_root $pkg

    if (Test-Path $dest) {
        Remove-Item -Recurse -Force $dest
    }

    git clone $repo $dest

    if (-not (Test-Path $dest)) {
        Write-Host "[ERROR] Clone failed" -ForegroundColor Red
        return
    }

    Compile-PackageC $dest $pkg | Out-Null

    Write-Host "[OK] Installed $pkg"
}

function Sarn-Update {
    param([string]$pkg)

    $pkg_dir = Join-Path $env:SARN_ROOT ".packages\$pkg"

    if (-not (Test-Path $pkg_dir)) {
        Write-Host "[ERROR] Not installed" -ForegroundColor Red
        return
    }

    Push-Location $pkg_dir
    git pull
    Pop-Location

    Compile-PackageC $pkg_dir $pkg | Out-Null

    Write-Host "[OK] Updated $pkg"
}

function Sarn-Remove {
    param([string]$pkg)

    $pkg_dir = Join-Path $env:SARN_ROOT ".packages\$pkg"

    if (Test-Path $pkg_dir) {
        Remove-Item -Recurse -Force $pkg_dir
        Write-Host "[OK] Removed $pkg"
    } else {
        Write-Host "[ERROR] Not installed"
    }
}

function Sarn-List {
    $pkg_root = Join-Path $env:SARN_ROOT ".packages"

    Get-ChildItem $pkg_root -Directory | ForEach-Object {
        Write-Host $_.Name
    }
}


function Sarn-New-Package {
    param([string]$name)
    $dir = Join-Path (Get-Location) $name
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    @"
--!type:strict

function $name.hello(): string
    return "Hello from $name!"
end
"@ | Set-Content (Join-Path $dir "__init__.sarn")

@"
{
    "name": "$name",
    "version": "1.0.0",
    "files": ["__init__.sarn"],
    "c_sources": [],
    "deps": []
}
"@ | Set-Content (Join-Path $dir "pkg.json")

    Write-Host "[OK] Created package $name"
}

function Sarn-Build {
    param([string]$src, [string]$target_path)

    if (-not (Test-Path $src)) {
        Write-Host "[ERROR] Source file not found: $src" -ForegroundColor Red
        return
    }

    $name    = [System.IO.Path]::GetFileNameWithoutExtension($src)
    $abs     = Resolve-Path $src
    $src_dir = [System.IO.Path]::GetDirectoryName($abs)

    $ll  = Join-Path $env:SARN_BIN ($name + ".ll")
    $exe = Join-Path $env:SARN_BIN ($name + ".exe")

    Write-Host "[*] Compiling $src..." -ForegroundColor Cyan
    & "$env:SARN_ROOT\build\compiler\Release\sarnc.exe" $abs -o $ll

    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] Compile failed" -ForegroundColor Red
        return
    }

    $pkg_libs = Get-PackageLibs $src_dir

    $obj = [System.IO.Path]::ChangeExtension($ll, ".obj")
    
    $llvm_bin = "C:\Program Files\LLVM\bin"
    if (-not (Test-Path $llvm_bin)) {
        $llvm_bin = "C:\Program Files (x86)\LLVM\bin"
    }
    
    $llc_path = Join-Path $llvm_bin "llc.exe"
    if (Test-Path $llc_path) {
        & $llc_path $ll -o $obj 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[ERROR] LLVM IR compilation to object failed" -ForegroundColor Red
            return
        }
    } else {
        $obj = $ll
    }
    
    if (-not (Test-Path $obj)) {
        Write-Host "[ERROR] Object file not found" -ForegroundColor Red
        return
    }

    $link_cmd = @(
        $obj,
        $( $r = Resolve-Path "$env:SARN_ROOT\build\runtime\*\sarn.lib" -ErrorAction SilentlyContinue | Select-Object -First 1; if ($r) { $r.Path } else { "$env:SARN_ROOT\build\runtime\sarn.lib" } ),
        "C:\vcpkg\installed\x64-windows\lib\raylib.lib"
    )
    
    foreach ($lib in $pkg_libs) {
        $link_cmd += $lib
    }
    
    $link_cmd += @(
        "-lOpenGL32","-lgdi32","-lwinmm","-ladvapi32",
        "-lUser32","-lShell32","-lGdi32",
        "-lmsvcrt","-lucrt","-lvcruntime",
        "-o", $exe
    )

    Write-Host "[*] Linking..." -ForegroundColor Cyan
    $clang_path = Join-Path $llvm_bin "clang.exe"
    $clang_output = $null
    if (Test-Path $clang_path) {
        $clang_output = & $clang_path @link_cmd 2>&1
    } else {
        $clang_output = clang @link_cmd 2>&1
    }
    
    if ($clang_output) {
        Write-Host "Clang output: $clang_output"
    }

    if (-not (Test-Path $exe)) {
        Write-Host "[ERROR] Linking failed" -ForegroundColor Red
        return
    }

    # Create target directory
    $target_dir = [System.IO.Path]::GetDirectoryName($target_path)
    if (-not (Test-Path $target_dir)) {
        New-Item -ItemType Directory -Force -Path $target_dir | Out-Null
    }

    # Copy executable
    Write-Host "[*] Copying executable to $target_path..." -ForegroundColor Cyan
    Copy-Item $exe $target_path -Force

    # Copy all DLLs to target directory
    Write-Host "[*] Copying DLL dependencies..." -ForegroundColor Cyan
    Get-ChildItem "$env:SARN_BIN\*.dll" | ForEach-Object {
        Copy-Item $_.FullName (Join-Path $target_dir $_.Name) -Force
    }

    Write-Host "[OK] Built self-contained executable: $target_path" -ForegroundColor Green
    Write-Host "[OK] DLLs copied to: $target_dir" -ForegroundColor Green
}

function Sarn-Standalone {
    param([string]$src, [string]$target_path, [string]$app_name = "SarnApp")

    if (-not (Test-Path $src)) {
        Write-Host "[ERROR] Source file not found: $src" -ForegroundColor Red
        return
    }

    $name    = [System.IO.Path]::GetFileNameWithoutExtension($src)
    $abs     = Resolve-Path $src
    $src_dir = [System.IO.Path]::GetDirectoryName($abs)

    $ll  = Join-Path $env:SARN_BIN ($name + ".ll")
    $exe = Join-Path $env:SARN_BIN ($name + ".exe")

    Write-Host "[*] Compiling $src..." -ForegroundColor Cyan
    & "$env:SARN_ROOT\build\compiler\Release\sarnc.exe" $abs -o $ll

    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] Compile failed" -ForegroundColor Red
        return
    }

    $pkg_libs = Get-PackageLibs $src_dir

    $obj = [System.IO.Path]::ChangeExtension($ll, ".obj")
    
    $llvm_bin = "C:\Program Files\LLVM\bin"
    if (-not (Test-Path $llvm_bin)) {
        $llvm_bin = "C:\Program Files (x86)\LLVM\bin"
    }
    
    $llc_path = Join-Path $llvm_bin "llc.exe"
    if (Test-Path $llc_path) {
        & $llc_path $ll -o $obj 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[ERROR] LLVM IR compilation to object failed" -ForegroundColor Red
            return
        }
    } else {
        $obj = $ll
    }
    
    if (-not (Test-Path $obj)) {
        Write-Host "[ERROR] Object file not found" -ForegroundColor Red
        return
    }

    $link_cmd = @(
        $obj,
        $( $r = Resolve-Path "$env:SARN_ROOT\build\runtime\*\sarn.lib" -ErrorAction SilentlyContinue | Select-Object -First 1; if ($r) { $r.Path } else { "$env:SARN_ROOT\build\runtime\sarn.lib" } ),
        "C:\vcpkg\installed\x64-windows\lib\raylib.lib"
    )
    
    foreach ($lib in $pkg_libs) {
        $link_cmd += $lib
    }
    
    $link_cmd += @(
        "-lOpenGL32","-lgdi32","-lwinmm","-ladvapi32",
        "-lUser32","-lShell32","-lGdi32",
        "-lmsvcrt","-lucrt","-lvcruntime",
        "-o", $exe
    )

    Write-Host "[*] Linking..." -ForegroundColor Cyan
    $clang_path = Join-Path $llvm_bin "clang.exe"
    $clang_output = $null
    if (Test-Path $clang_path) {
        $clang_output = & $clang_path @link_cmd 2>&1
    } else {
        $clang_output = clang @link_cmd 2>&1
    }
    
    if ($clang_output) {
        Write-Host "Clang output: $clang_output"
    }

    if (-not (Test-Path $exe)) {
        Write-Host "[ERROR] Linking failed" -ForegroundColor Red
        return
    }

    # Create standalone package
    Write-Host "[*] Creating standalone package..." -ForegroundColor Cyan
    
    $target_dir = [System.IO.Path]::GetDirectoryName($target_path)
    $bat_name = [System.IO.Path]::GetFileNameWithoutExtension($target_path) + ".bat"
    $bat_path = Join-Path $target_dir $bat_name
    $app_exe = Join-Path $target_dir "app.exe"

    if (-not (Test-Path $target_dir)) {
        New-Item -ItemType Directory -Force -Path $target_dir | Out-Null
    }

    # Copy app.exe
    Copy-Item $exe $app_exe -Force

    # Create batch launcher
    $batch_content = @"
@echo off
REM Self-extracting wrapper for $app_name
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set APP_EXE=%SCRIPT_DIR%app.exe

if not exist "!APP_EXE!" (
    echo [ERROR] app.exe not found
    exit /b 1
)

pushd "!SCRIPT_DIR!"
"!APP_EXE!" %*
set EXIT_CODE=!ERRORLEVEL!
popd

exit /b !EXIT_CODE!
"@

    $batch_content | Set-Content $bat_path -Encoding ASCII

    # Copy DLLs
    Write-Host "[*] Copying DLL dependencies..." -ForegroundColor Cyan
    $dlls = Get-ChildItem "$env:SARN_BIN\*.dll" -ErrorAction SilentlyContinue
    if ($dlls.Count -eq 0) {
        Write-Host "[WARN] No DLLs found in $env:SARN_BIN" -ForegroundColor Yellow
        Write-Host "[WARN] Ensure DLLs are copied manually to $target_dir" -ForegroundColor Yellow
    } else {
        foreach ($dll in $dlls) {
            Copy-Item $dll.FullName (Join-Path $target_dir $dll.Name) -Force
            Write-Host "  + Copied: $($dll.Name)" -ForegroundColor Green
        }
    }

    Write-Host "[OK] Standalone package created:" -ForegroundColor Green
    Write-Host "  Launcher: $bat_path" -ForegroundColor Green
    Write-Host "  App: $app_exe" -ForegroundColor Green
    Write-Host "  Directory: $target_dir" -ForegroundColor Green
    Write-Host "[*] Usage: Run '$bat_name' from the package directory" -ForegroundColor Cyan
}

function Sarn-REPL {
    Write-Host "===================================================" -ForegroundColor Cyan
    Write-Host "Sarn Interactive REPL (v0.3)" -ForegroundColor Cyan
    Write-Host "Type 'exit' or 'quit' to exit" -ForegroundColor Cyan
    Write-Host "===================================================" -ForegroundColor Cyan
    Write-Host ""

    $repl_id = Get-Random
    $temp_dir = Join-Path $env:TEMP "sarn_repl_$repl_id"
    New-Item -ItemType Directory -Force -Path $temp_dir | Out-Null

    $llvm_bin = "C:\Program Files\LLVM\bin"
    if (-not (Test-Path $llvm_bin)) {
        $llvm_bin = "C:\Program Files (x86)\LLVM\bin"
    }

    while ($true) {
        Write-Host "sarn> " -NoNewline -ForegroundColor Green
        $input_line = Read-Host

        if ($input_line -match "^(exit|quit)$") {
            Write-Host "Exiting Sarn REPL..." -ForegroundColor Yellow
            Remove-Item -Recurse -Force $temp_dir -ErrorAction SilentlyContinue
            break
        }

        if ([string]::IsNullOrWhiteSpace($input_line)) {
            continue
        }

        # Collect multi-line input if needed
        $code = $input_line
        $open_blocks = ($code | Select-String -Pattern '\bif\b|\bfor\b|\bwhile\b|\bfunction\b|\btry\b' | Measure-Object).Count
        $close_blocks = ($code | Select-String -Pattern '\bend\b' | Measure-Object).Count

        while ($open_blocks -gt $close_blocks) {
            Write-Host "...> " -NoNewline -ForegroundColor Green
            $next_line = Read-Host
            $code += "`n$next_line"
            $open_blocks = ($code | Select-String -Pattern '\bif\b|\bfor\b|\bwhile\b|\bfunction\b|\btry\b' | Measure-Object).Count
            $close_blocks = ($code | Select-String -Pattern '\bend\b' | Measure-Object).Count
        }

        # Create temporary Sarn file
        $temp_file = Join-Path $temp_dir "repl_$repl_id.sarn"
        $code | Set-Content $temp_file

        # Compile
        $ll_file = Join-Path $temp_dir "repl_$repl_id.ll"
        & "$env:SARN_ROOT\build\compiler\Release\sarnc.exe" $temp_file -o $ll_file 2>&1

        if ($LASTEXITCODE -ne 0) {
            Write-Host "Compile error" -ForegroundColor Red
            continue
        }

        # Convert LLVM IR to object code
        $obj_file = [System.IO.Path]::ChangeExtension($ll_file, ".obj")
        $llc_path = Join-Path $llvm_bin "llc.exe"

        if (Test-Path $llc_path) {
            & $llc_path $ll_file -o $obj_file 2>&1 | Out-Null
        } else {
            $obj_file = $ll_file
        }

        # Link and execute
        $exe_file = Join-Path $temp_dir "repl_$repl_id.exe"

        $link_cmd = @(
            $obj_file,
            $( $r = Resolve-Path "$env:SARN_ROOT\build\runtime\*\sarn.lib" -ErrorAction SilentlyContinue | Select-Object -First 1; if ($r) { $r.Path } else { "$env:SARN_ROOT\build\runtime\sarn.lib" } ),
            "C:\vcpkg\installed\x64-windows\lib\raylib.lib",
            "-lOpenGL32","-lgdi32","-lwinmm","-ladvapi32",
            "-lUser32","-lShell32","-lGdi32",
            "-lmsvcrt","-lucrt","-lvcruntime",
            "-o", $exe_file
        )

        $clang_path = Join-Path $llvm_bin "clang.exe"
        if (Test-Path $clang_path) {
            & $clang_path @link_cmd 2>&1 | Out-Null
        } else {
            clang @link_cmd 2>&1 | Out-Null
        }

        if (Test-Path $exe_file) {
            Write-Host "" -ForegroundColor Gray
            & $exe_file
            Write-Host "" -ForegroundColor Gray
        } else {
            Write-Host "Linking failed" -ForegroundColor Red
        }

        # Cleanup temp files
        Remove-Item $temp_file -ErrorAction SilentlyContinue
        Remove-Item $ll_file -ErrorAction SilentlyContinue
        Remove-Item $obj_file -ErrorAction SilentlyContinue
        Remove-Item $exe_file -ErrorAction SilentlyContinue
    }
}

switch ($Command) {
    "Sarn-Run"     { Sarn-Run $File }
    "build"    { Sarn-Build $File $Version }
    "standalone" { Sarn-Standalone $File $Version }
    "install" { Sarn-Install $File }
    "update"  { Sarn-Update $File }
    "remove"  { Sarn-Remove $File }
    "list"    { Sarn-List }
    "newpkg"  { Sarn-New-Package $File }
    "sarn" {Sarn-REPL}
    default   { Write-Host "Commands: Sarn-Run | build | standalone | install | update | remove | list | newpkg" }
}