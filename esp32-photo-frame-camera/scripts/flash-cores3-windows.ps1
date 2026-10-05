# Flash CoreS3 from Windows (native COM).
# Prereq: make build-cores3
# Legacy WSL helper: pass -BusId to detach/re-attach via usbipd.
param(
    [string]$ComPort = "COM7",
    [string]$BusId = "",
    [string]$BinDir = "",
    [int]$Tries = 30,
    [switch]$KeepOnWindows
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $RepoRoot "cores3\.pio\build\M5CoreS3"
if (-not $BinDir) { $BinDir = Join-Path $env:USERPROFILE "esp32-flash" }

function Ensure-Bins {
    param([string]$Dest)
    New-Item -ItemType Directory -Force -Path $Dest | Out-Null
    $names = @("bootloader.bin", "partitions.bin", "firmware.bin")
    foreach ($n in $names) {
        $src = Join-Path $BuildDir $n
        if (-not (Test-Path $src)) {
            throw "Missing $src. Run: make build-cores3"
        }
        Copy-Item -Force $src (Join-Path $Dest $n)
    }
}

if ($BusId) {
    Write-Host "== detach $BusId from WSL (Windows gets $ComPort) =="
    usbipd detach --busid $BusId 2>$null
}

Ensure-Bins -Dest $BinDir

$boot = Join-Path $BinDir "bootloader.bin"
$part = Join-Path $BinDir "partitions.bin"
$app = Join-Path $BinDir "firmware.bin"

Write-Host "== flash CoreS3 on $ComPort (long-press RST ~3s if no connection) =="
$ok = $false
for ($i = 1; $i -le $Tries; $i++) {
    Write-Host "  try $i/$Tries..."
    python -m esptool --chip esp32s3 -p $ComPort -b 460800 --before default-reset --after no-reset write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m 0x0 $boot 0x8000 $part 0x10000 $app
    if ($LASTEXITCODE -eq 0) {
        $ok = $true
        break
    }
    Start-Sleep -Seconds 2
}

if (-not $ok) {
    throw "Flash failed after $Tries tries. Long-press CoreS3 RST ~3s and re-run."
}

Write-Host "== RTS reset to run app =="
python -m esptool --chip esp32s3 -p $ComPort --before no-reset --after no-reset run 2>$null

if ($BusId -and -not $KeepOnWindows) {
    Write-Host "== re-attach $BusId to WSL =="
    usbipd attach --wsl --busid $BusId
}

Write-Host "Done. Try: make cmd-cores3 CMD=status CORES3_PORT=$ComPort"
