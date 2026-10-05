# Install ESP-IDF (esp32p4 target) + Python serial tools on Windows.
param(
    [switch]$SkipIdf,
    [switch]$SkipToolsPy
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$IdfPath = if ($env:IDF_PATH) { $env:IDF_PATH } else { Join-Path $env:USERPROFILE "esp\esp-idf" }
$IdfVersion = if ($env:IDF_VERSION) { $env:IDF_VERSION } else { "v5.5.1" }

function Log([string]$Msg) { Write-Host "==> $Msg" }

function Ensure-EspIdf {
    if ($SkipIdf) { Log "skip ESP-IDF (-SkipIdf)"; return }
    if (-not (Test-Path (Join-Path $IdfPath "export.ps1"))) {
        Log "cloning ESP-IDF $IdfVersion to $IdfPath"
        New-Item -ItemType Directory -Force -Path (Split-Path $IdfPath) | Out-Null
        git clone -b $IdfVersion --depth 1 --recursive https://github.com/espressif/esp-idf.git $IdfPath
    } else {
        Log "ESP-IDF already at $IdfPath"
    }
    Log "ensuring ESP-IDF Python/toolchain deps for esp32p4 (first run may take several minutes)"
    Push-Location $IdfPath
    & .\install.ps1 esp32p4
    if ($LASTEXITCODE -ne 0) { throw "ESP-IDF install.ps1 failed" }
    Pop-Location
}

function Install-ProjectLibraries {
    if (-not $SkipToolsPy) {
        Log "python tools: install scripts/requirements-tools.txt"
        python -m pip install --user -U -r (Join-Path $Root "scripts\requirements-tools.txt")
    }
    if (-not $SkipIdf -and (Test-Path (Join-Path $IdfPath "export.ps1"))) {
        Log "ESP-IDF: set firmware target esp32p4"
        . (Join-Path $IdfPath "export.ps1")
        Push-Location (Join-Path $Root "firmware")
        if (-not (Test-Path "sdkconfig")) {
            idf.py set-target esp32p4
        } else {
            idf.py reconfigure
        }
        if ($LASTEXITCODE -ne 0) { throw "idf.py set-target/reconfigure failed" }
        Pop-Location
    }
}

Log "repo root: $Root"
Ensure-EspIdf
Install-ProjectLibraries
Log "done — try: make build"
