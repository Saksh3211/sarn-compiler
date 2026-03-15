# install.ps1
# Put this file in: tools/extensions/vscode/
# Then run it ONCE from PowerShell to permanently install the S Lua extension.
# After this you never need to run it again unless you update the extension.

$ErrorActionPreference = "Stop"
$ExtDir = $PSScriptRoot   # same folder as this script

Write-Host ""
Write-Host "=== S Lua Extension Installer ===" -ForegroundColor Cyan
Write-Host ""

# Step 1: Install vsce if not present
Write-Host "[1/4] Checking vsce..." -ForegroundColor Yellow
$vsceExists = $null
try { $vsceExists = Get-Command vsce -ErrorAction SilentlyContinue } catch {}
if (-not $vsceExists) {
    Write-Host "     Installing @vscode/vsce globally..." -ForegroundColor Gray
    npm install -g @vscode/vsce
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] npm install failed. Make sure Node.js is installed." -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "     vsce already installed." -ForegroundColor Green
}

# Step 2: Package the extension
Write-Host ""
Write-Host "[2/4] Packaging extension..." -ForegroundColor Yellow
Set-Location $ExtDir
vsce package --no-dependencies --out "slua-language.vsix" 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] vsce package failed." -ForegroundColor Red
    exit 1
}
Write-Host "     Packaged: $ExtDir\slua-language.vsix" -ForegroundColor Green

# Step 3: Install into VS Code
Write-Host ""
Write-Host "[3/4] Installing into VS Code..." -ForegroundColor Yellow
code --install-extension "$ExtDir\slua-language.vsix" --force
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] code --install-extension failed. Is 'code' in your PATH?" -ForegroundColor Red
    Write-Host "        Add VS Code to PATH: VS Code > Ctrl+Shift+P > 'Shell Command: Install code command in PATH'" -ForegroundColor Yellow
    exit 1
}
Write-Host "     Installed!" -ForegroundColor Green

# Step 4: Done
Write-Host ""
Write-Host "[4/4] Done!" -ForegroundColor Green
Write-Host ""
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  Extension installed. NOW DO THIS:" -ForegroundColor White
Write-Host ""
Write-Host "  1. Close ALL VS Code windows" -ForegroundColor Yellow
Write-Host "  2. Re-open your project" -ForegroundColor Yellow
Write-Host "  3. Open any .slua file" -ForegroundColor Yellow
Write-Host "  4. You should see:" -ForegroundColor Yellow
Write-Host "     - Colors (not plain text)" -ForegroundColor Green
Write-Host "     - Play button (triangle) in top-right corner" -ForegroundColor Green
Write-Host "     - 'S Lua' in the bottom status bar (not 'Lua')" -ForegroundColor Green
Write-Host "  5. Press F5 or click the play button to run" -ForegroundColor Yellow
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""