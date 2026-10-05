# Install host serial tools and ESP-IDF P4 target support (Windows).
param(
    [switch]$SkipIdf,
    [switch]$SkipToolsPy
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$IdfPath = if ($env:IDF_PATH) { $env:IDF_PATH } else { Join-Path $env:USERPROFILE "esp\esp-idf" }

function Log([string]$Msg) { Write-Host "==> $Msg" }

if (-not $SkipToolsPy) {
    Log "installing pyserial (user pip)"
    python -m pip install --user -U -r (Join-Path $PSScriptRoot "requirements-tools.txt")
}

if (-not $SkipIdf) {
    if (-not (Test-Path (Join-Path $IdfPath "export.ps1"))) {
        throw "ESP-IDF not found at $IdfPath. Install ESP-IDF v5.5+ first."
    }
    Log "ensuring ESP-IDF tools for esp32p4 (first run may take several minutes)"
    Push-Location $IdfPath
    & .\install.ps1 esp32p4
    Pop-Location
}

Log "done. Host tests: make test (MSVC). Firmware: make build-firmware"
