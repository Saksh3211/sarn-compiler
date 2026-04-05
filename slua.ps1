param(
    [string]$Command,
    [string]$File,
    [string]$Version = "latest"
)

$env:SLUA_ROOT = "C:\Users\rajeev\slua-compiler"
$env:SLUA_BIN  = "$env:SLUA_ROOT\bin"
$env:PATH      = "C:\vcpkg\installed\x64-windows\bin;" + $env:PATH

New-Item -ItemType Directory -Force -Path $env:SLUA_BIN | Out-Null

function Get-PackageLibs {
    param([string]$src_dir)
    $libs = @()
    $roots = @($src_dir, $env:SLUA_ROOT)
    foreach ($root in $roots) {
        $pkg_dir = Join-Path $root ".packages"
        if (Test-Path $pkg_dir) {
            Get-ChildItem $pkg_dir -Directory | ForEach-Object {
                $lib = Join-Path $_.FullName ($_.Name + ".lib")
                if (Test-Path $lib) { $libs += $lib }
            }
        }
    }
    return $libs | Select-Object -Unique
}

function Slua-Run {
    param([string]$src)
    $name    = [System.IO.Path]::GetFileNameWithoutExtension($src)
    $abs     = Join-Path $env:SLUA_ROOT $src
    $src_dir = [System.IO.Path]::GetDirectoryName($abs)
    $ll      = Join-Path $env:SLUA_BIN ($name + ".ll")
    $exe     = Join-Path $env:SLUA_BIN ($name + ".exe")

    & "$env:SLUA_ROOT\build\compiler\sluac.exe" $abs -o $ll
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] sluac failed to emit LLVM IR, check the source file." -ForegroundColor Red
        return
    }

    $pkg_libs = Get-PackageLibs $src_dir

    $link_args = @(
        $ll,
        "$env:SLUA_ROOT\build\runtime\slua.lib",
        "C:\vcpkg\installed\x64-windows\lib\raylib.lib",
        "-lOpenGL32", "-lgdi32", "-lwinmm",
        "-lUser32", "-lShell32", "-lGdi32",
        "-lmsvcrt", "-lucrt", "-lvcruntime"
    ) + $pkg_libs + @("-o", $exe)

    clang @link_args 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] clang link failed." -ForegroundColor Red
        return
    }

    & $exe
}

function Slua-Install {
    param([string]$pkg, [string]$ver = "latest")

    $registry_url = "https://raw.githubusercontent.com/Saksh3211/slua-packages/main/registry.json"
    $pkg_base     = Join-Path $env:SLUA_ROOT ".packages"
    $pkg_dir      = Join-Path $pkg_base $pkg

    Write-Host "[slua] Fetching registry..." -ForegroundColor Cyan

    try {
        $reg = Invoke-RestMethod -Uri $registry_url -ErrorAction Stop
    } catch {
        Write-Host "[ERROR] Cannot reach registry. Check internet or registry URL." -ForegroundColor Red
        Write-Host "        You can place packages manually at: $pkg_dir" -ForegroundColor Yellow
        return
    }

    $pkg_info = $reg.$pkg
    if (-not $pkg_info) {
        Write-Host "[ERROR] Package '$pkg' not found in registry." -ForegroundColor Red
        return
    }

    if ($ver -eq "latest") {
        $ver = $pkg_info.latest
    }

    $ver_info = $pkg_info.$ver
    if (-not $ver_info) {
        Write-Host "[ERROR] Version '$ver' not available for '$pkg'." -ForegroundColor Red
        return
    }

    New-Item -ItemType Directory -Force -Path $pkg_dir | Out-Null

    $base_url = "https://raw.githubusercontent.com/Saksh3211/slua-packages/main/$pkg/$ver"

    foreach ($file in $ver_info.files) {
        $url      = "$base_url/$file"
        $dest     = Join-Path $pkg_dir $file
        $dest_dir = Split-Path $dest -Parent
        New-Item -ItemType Directory -Force -Path $dest_dir | Out-Null
        try {
            Invoke-WebRequest -Uri $url -OutFile $dest -ErrorAction Stop
            Write-Host "[OK] downloaded $file" -ForegroundColor Green
        } catch {
            Write-Host "[WARN] Could not download: $file" -ForegroundColor Yellow
        }
    }

    $c_sources = $ver_info.c_sources
    if ($c_sources -and $c_sources.Count -gt 0) {
        Write-Host "[slua] Compiling C/C++ sources..." -ForegroundColor Cyan
        $inc  = Join-Path $env:SLUA_ROOT "runtime\include"
        $objs = @()
        foreach ($src_file in $c_sources) {
            $src_path = Join-Path $pkg_dir $src_file
            $obj_path = [System.IO.Path]::ChangeExtension($src_path, ".obj")
            clang -c -O2 $src_path -I $inc -o $obj_path 2>&1
            if ($LASTEXITCODE -eq 0) {
                $objs += $obj_path
                Write-Host "[OK] compiled $src_file" -ForegroundColor Green
            } else {
                Write-Host "[WARN] failed to compile $src_file" -ForegroundColor Yellow
            }
        }
        if ($objs.Count -gt 0) {
            $lib_path = Join-Path $pkg_dir ($pkg + ".lib")
            llvm-lib /nologo /OUT:$lib_path @objs 2>&1
            if ($LASTEXITCODE -eq 0) {
                Write-Host "[OK] built $pkg.lib" -ForegroundColor Green
            } else {
                Write-Host "[WARN] lib build failed" -ForegroundColor Yellow
            }
        }
    }

    Write-Host "[slua] Installed $pkg @ $ver" -ForegroundColor Green
    Write-Host "       Location : $pkg_dir" -ForegroundColor Cyan
}

function Slua-New-Package {
    param([string]$name)
    $dir = Join-Path (Get-Location) $name
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $init = "--!!type:strict`n`nexport function " + $name + "_hello(): string`n    return `"Hello from " + $name + "!`"`nend`n"
    [System.IO.File]::WriteAllText((Join-Path $dir "__init__.slua"), $init)
    $meta = [ordered]@{
        name        = $name
        version     = "1.0.0"
        description = "A S Lua package"
        author      = ""
        files       = @("__init__.slua")
        c_sources   = @()
        deps        = @()
    } | ConvertTo-Json -Depth 4
    [System.IO.File]::WriteAllText((Join-Path $dir "pkg.json"), $meta)
    Write-Host "[slua] Scaffold created: $dir" -ForegroundColor Green
    Write-Host "       __init__.slua  - your S Lua API" -ForegroundColor Cyan
    Write-Host "       pkg.json       - metadata and file list" -ForegroundColor Cyan
}

switch ($Command) {
    "Slua-Run"         { if ($File) { Slua-Run $File } else { Write-Host "Usage: slua.ps1 Slua-Run <file.slua>" } }
    "Slua-Install"     { if ($File) { Slua-Install $File $Version } else { Write-Host "Usage: slua.ps1 Slua-Install <pkg> [-Version <ver>]" } }
    "Slua-New-Package" { if ($File) { Slua-New-Package $File } else { Write-Host "Usage: slua.ps1 Slua-New-Package <name>" } }
    default            { Write-Host "Commands: Slua-Run | Slua-Install | Slua-New-Package" }
}