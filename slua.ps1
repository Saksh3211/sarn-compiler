param(
    [string]$Command,
    [string]$File,
    [string]$Version = "latest"
)

$env:SLUA_ROOT = "C:\Users\rajeev\slua-compiler"
$env:SLUA_BIN  = "$env:SLUA_ROOT\bin"
$env:PATH      = "C:\vcpkg\installed\x64-windows\bin;" + $env:PATH

New-Item -ItemType Directory -Force -Path $env:SLUA_BIN | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $env:SLUA_ROOT ".packages") | Out-Null

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
    $roots = @(
        (Join-Path $env:SLUA_ROOT ".packages")
    )

    foreach ($root in $roots) {
        if (-not (Test-Path $root)) { continue }

        Get-ChildItem $root -Directory | ForEach-Object {
            $pkg_name = $_.Name
            $pkg_dir  = $_.FullName

            $init = Join-Path $pkg_dir "__init__.slua"
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

    $ll  = Join-Path $env:SLUA_BIN ($name + ".ll")
    $exe = Join-Path $env:SLUA_BIN ($name + ".exe")

    & "$env:SLUA_ROOT\build\compiler\sluac.exe" $abs -o $ll

    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] Compile failed" -ForegroundColor Red
        return
    }

    $pkg_libs = Get-PackageLibs $src_dir

    $link_args = @(
        $ll,
        "$env:SLUA_ROOT\build\runtime\slua.lib",
        "C:\vcpkg\installed\x64-windows\lib\raylib.lib",
        "-lOpenGL32","-lgdi32","-lwinmm",
        "-lUser32","-lShell32","-lGdi32",
        "-lmsvcrt","-lucrt","-lvcruntime"
    ) + $pkg_libs + @("-o", $exe)

    clang @link_args 2>&1 | Out-Null

    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] Linking failed" -ForegroundColor Red
        return
    }

    & $exe
}

function Slua-Install {
    param([string]$pkg)

    $registry_path = Join-Path $env:SLUA_ROOT "packageReg.json"
    $pkg_root = Join-Path $env:SLUA_ROOT ".packages"

    # Direct GitHub install
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

function Slua-Update {
    param([string]$pkg)

    $pkg_dir = Join-Path $env:SLUA_ROOT ".packages\$pkg"

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

function Slua-Remove {
    param([string]$pkg)

    $pkg_dir = Join-Path $env:SLUA_ROOT ".packages\$pkg"

    if (Test-Path $pkg_dir) {
        Remove-Item -Recurse -Force $pkg_dir
        Write-Host "[OK] Removed $pkg"
    } else {
        Write-Host "[ERROR] Not installed"
    }
}

function Slua-List {
    $pkg_root = Join-Path $env:SLUA_ROOT ".packages"

    Get-ChildItem $pkg_root -Directory | ForEach-Object {
        Write-Host $_.Name
    }
}


function Slua-New-Package {
    param([string]$name)
    $dir = Join-Path (Get-Location) $name
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    @"
--!type:strict

function $name.hello(): string
    return "Hello from $name!"
end
"@ | Set-Content (Join-Path $dir "__init__.slua")

@"
{
    "name": "$name",
    "version": "1.0.0",
    "files": ["__init__.slua"],
    "c_sources": [],
    "deps": []
}
"@ | Set-Content (Join-Path $dir "pkg.json")

    Write-Host "[OK] Created package $name"
}

switch ($Command) {
    "Slua-Run"     { Slua-Run $File }
    "install" { Slua-Install $File }
    "update"  { Slua-Update $File }
    "remove"  { Slua-Remove $File }
    "list"    { Slua-List }
    "newpkg"  { Slua-New-Package $File }
    default   { Write-Host "Commands: run | install | update | remove | list | newpkg" }
}