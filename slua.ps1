param(
    [string]$Command,
    [string]$File,
    [string]$Version = "latest"
)

$env:SLUA_ROOT = "C:\Users\rajeev\slua-compiler"
$env:SLUA_BIN  = "$env:SLUA_ROOT\bin"
$env:PATH      = "C:\vcpkg\installed\x64-windows\bin;" + $env:PATH

New-Item -ItemType Directory -Force -Path $env:SLUA_BIN | Out-Null

function Compile-PackageC {
    param([string]$pkg_dir, [string]$pkg_name)
    $lib_path = Join-Path $pkg_dir ($pkg_name + ".lib")
    if (Test-Path $lib_path) { return $lib_path }
    $pkg_json = Join-Path $pkg_dir "pkg.json"
    if (-not (Test-Path $pkg_json)) { return $null }
    $meta = Get-Content $pkg_json -Raw | ConvertFrom-Json
    if (-not $meta.c_sources -or $meta.c_sources.Count -eq 0) { return $null }
    $inc  = Join-Path $env:SLUA_ROOT "runtime\include"
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
    $search_roots = @(
        $src_dir,
        (Join-Path $src_dir ".packages"),
        (Join-Path $env:SLUA_ROOT ".packages")
    )
    foreach ($root in $search_roots) {
        if (-not (Test-Path $root)) { continue }
        Get-ChildItem $root -Directory -ErrorAction SilentlyContinue | ForEach-Object {
            $pkg_name = $_.Name
            $pkg_dir  = $_.FullName
            $init     = Join-Path $pkg_dir "__init__.slua"
            if (-not (Test-Path $init)) { return }
            $lib = Compile-PackageC $pkg_dir $pkg_name
            if ($lib) { $libs += $lib }
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

    clang @link_args 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] clang link failed." -ForegroundColor Red
        return
    }

    & $exe
}

function Slua-Install {
    param([string]$pkg, [string]$ver = "latest")

    $registry_url = "https://raw.githubusercontent.com/Saksh3211/slua-packages/main/registry.json"
    $pkg_dir      = Join-Path $env:SLUA_ROOT ".packages\$pkg"

    Write-Host "[slua] Fetching registry..." -ForegroundColor Cyan
    try {
        $reg = Invoke-RestMethod -Uri $registry_url -ErrorAction Stop
    } catch {
        Write-Host "[ERROR] Cannot reach registry." -ForegroundColor Red
        Write-Host "        Place packages manually at: $pkg_dir" -ForegroundColor Yellow
        return
    }

    $pkg_info = $reg.$pkg
    if (-not $pkg_info) {
        Write-Host "[ERROR] Package '$pkg' not in registry." -ForegroundColor Red
        return
    }
    if ($ver -eq "latest") { $ver = $pkg_info.latest }
    $ver_info = $pkg_info.$ver
    if (-not $ver_info) {
        Write-Host "[ERROR] Version '$ver' not found for '$pkg'." -ForegroundColor Red
        return
    }

    New-Item -ItemType Directory -Force -Path $pkg_dir | Out-Null
    $base_url = "https://raw.githubusercontent.com/Saksh3211/slua-packages/main/$pkg/$ver"

    foreach ($file in $ver_info.files) {
        $dest     = Join-Path $pkg_dir $file
        $dest_dir = Split-Path $dest -Parent
        New-Item -ItemType Directory -Force -Path $dest_dir | Out-Null
        try {
            Invoke-WebRequest -Uri "$base_url/$file" -OutFile $dest -ErrorAction Stop
            Write-Host "[OK] $file" -ForegroundColor Green
        } catch {
            Write-Host "[WARN] Could not download $file" -ForegroundColor Yellow
        }
    }

    $lib = Compile-PackageC $pkg_dir $pkg
    if ($lib) {
        Write-Host "[OK] built $pkg.lib" -ForegroundColor Green
    }

    Write-Host "[slua] Installed $pkg @ $ver -> $pkg_dir" -ForegroundColor Green
}

function Slua-New-Package {
    param([string]$name)
    $dir = Join-Path (Get-Location) $name
    New-Item -ItemType Directory -Force -Path $dir | Out-Null

    $init  = "--!!type:strict`n`n"
    $init += "function " + $name + ".hello(): string`n"
    $init += "    return `"Hello from " + $name + "!`"`n"
    $init += "end`n"
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

    Write-Host "[slua] Created: $dir" -ForegroundColor Green
    Write-Host "  __init__.slua  - define functions as: function $name.funcname(...)" -ForegroundColor Cyan
    Write-Host "  pkg.json       - list files and c_sources" -ForegroundColor Cyan
}

switch ($Command) {
    "Slua-Run"         { if ($File) { Slua-Run $File } else { Write-Host "Usage: slua.ps1 Slua-Run <file.slua>" } }
    "Slua-Install"     { if ($File) { Slua-Install $File $Version } else { Write-Host "Usage: slua.ps1 Slua-Install <pkg> [-Version x]" } }
    "Slua-New-Package" { if ($File) { Slua-New-Package $File } else { Write-Host "Usage: slua.ps1 Slua-New-Package <name>" } }
    default            { Write-Host "Commands: Slua-Run | Slua-Install | Slua-New-Package" }
}