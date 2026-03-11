param(
    [string]$Command,
    [string]$File
)

$env:SLUA_ROOT = "C:\Users\rajeev\slua-compiler"
$env:SLUA_BIN  = "$env:SLUA_ROOT\bin"
$env:PATH      = "C:\vcpkg\installed\x64-windows\bin;" + $env:PATH

New-Item -ItemType Directory -Force -Path $env:SLUA_BIN | Out-Null

function Slua-Run {
    param([string]$src)
    $name = [System.IO.Path]::GetFileNameWithoutExtension($src)
    $ll   = "$env:SLUA_BIN\$name.ll"
    $exe  = "$env:SLUA_BIN\$name.exe"

    & "$env:SLUA_ROOT\build\compiler\sluac.exe" "$env:SLUA_ROOT\$src" -o $ll
    if ($LASTEXITCODE -ne 0) { Write-Host "[ERROR] sluac failed" -ForegroundColor Red; return }

    clang $ll "$env:SLUA_ROOT\build\runtime\slua.lib" -lmsvcrt -lucrt -lvcruntime -o $exe 2>&1
    if ($LASTEXITCODE -ne 0) { Write-Host "[ERROR] clang failed" -ForegroundColor Red; return }

    & $exe
}

if ($Command -eq "Slua-Run" -and $File) {
    Slua-Run $File
}