param(
    [string]$SourceExe,
    [string]$OutputPath,
    [string]$AppName = "SarnApp"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $SourceExe)) {
    Write-Host "[ERROR] Source EXE not found: $SourceExe" -ForegroundColor Red
    exit 1
}

$OutputDir = [System.IO.Path]::GetDirectoryName($OutputPath)
$ExeName = [System.IO.Path]::GetFileName($OutputPath)

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
}

Write-Host "[*] Creating self-extracting wrapper..." -ForegroundColor Cyan

# Create temporary directory for DLL collection
$temp_dlls = Join-Path $env:TEMP "sarn_build_$$"
New-Item -ItemType Directory -Force -Path $temp_dlls | Out-Null

try {
    # Copy all DLLs
    Write-Host "[*] Collecting DLLs..." -ForegroundColor Cyan
    Get-ChildItem "C:\Users\rajeev\slua-compiler\bin\*.dll" | ForEach-Object {
        Copy-Item $_.FullName (Join-Path $temp_dlls $_.Name) -Force
    }

    # Create manifest
    $manifest = @{
        app_name = $AppName
        exe_name = $ExeName
        dlls = @(Get-ChildItem "$temp_dlls\*.dll" | ForEach-Object { $_.Name })
    } | ConvertTo-Json

    # Create a C# wrapper executable that embeds DLLs
    $wrapper_cs = @"
using System;
using System.IO;
using System.Diagnostics;
using System.Reflection;
using System.Text.Json;

class SarnWrapper {
    static int Main(string[] args) {
        try {
            string appDir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            string appExe = Path.Combine(appDir, "app.exe");
            
            // Ensure DLLs are in the application directory
            EnsureDLLs(appDir);
            
            // Run the actual application
            ProcessStartInfo psi = new ProcessStartInfo {
                FileName = appExe,
                UseShellExecute = false,
                WorkingDirectory = appDir
            };
            
            // Forward arguments
            foreach (string arg in args) {
                psi.ArgumentList.Add(arg);
            }
            
            Process p = Process.Start(psi);
            p.WaitForExit();
            return p.ExitCode;
        } catch (Exception ex) {
            Console.Error.WriteLine("[ERROR] " + ex.Message);
            return 1;
        }
    }
    
    static void EnsureDLLs(string appDir) {
        string[] dlls = { "$(($manifest | ConvertFrom-Json).dlls -join '", "' | ForEach-Object { """$_""" })" };
        
        foreach (string dll in dlls) {
            if (!string.IsNullOrEmpty(dll)) {
                string dllPath = Path.Combine(appDir, dll);
                if (!File.Exists(dllPath)) {
                    Console.Error.WriteLine("[WARN] Missing DLL: " + dll);
                }
            }
        }
    }
}
"@

    # Actually, let me use a simpler approach - just create a batch wrapper
    Write-Host "[*] Creating batch wrapper..." -ForegroundColor Cyan
    
    $batch_content = @"
@echo off
REM Self-extracting wrapper for $AppName
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set APP_EXE=%SCRIPT_DIR%app.exe
set TEMP_DLL=%TEMP%\sarn_dll_cache_%RANDOM%

if not exist "!APP_EXE!" (
    echo [ERROR] app.exe not found in !SCRIPT_DIR!
    exit /b 1
)

REM Ensure DLLs are available
if not exist "!SCRIPT_DIR!raylib.dll" (
    echo [ERROR] raylib.dll not found. Application requires DLLs in the same directory.
    exit /b 1
)

REM Run the application with current directory set to script location
pushd "!SCRIPT_DIR!"
"!APP_EXE!" %*
set EXIT_CODE=!ERRORLEVEL!
popd

exit /b !EXIT_CODE!
"@

    # Copy source EXE as app.exe
    Copy-Item $SourceExe (Join-Path $OutputDir "app.exe") -Force
    
    # Save batch wrapper with the output name
    $batch_content | Set-Content $OutputPath -Encoding ASCII
    
    # Make batch file executable
    Write-Host "[*] Copying DLLs to output directory..." -ForegroundColor Cyan
    Get-ChildItem "$temp_dlls\*.dll" | ForEach-Object {
        Copy-Item $_.FullName (Join-Path $OutputDir $_.Name) -Force
    }
    
    Write-Host "[OK] Created self-contained package:" -ForegroundColor Green
    Write-Host "  Launcher: $OutputPath" -ForegroundColor Green
    Write-Host "  App: $(Join-Path $OutputDir 'app.exe')" -ForegroundColor Green
    Write-Host "  DLLs: $(Join-Path $OutputDir)\" -ForegroundColor Green
    Write-Host "" -ForegroundColor Green
    Write-Host "Note: Distribute the directory containing all files. Users run the .bat file." -ForegroundColor Yellow

} finally {
    # Cleanup
    if (Test-Path $temp_dlls) {
        Remove-Item -Recurse -Force $temp_dlls
    }
}
