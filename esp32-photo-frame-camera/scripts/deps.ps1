# Install toolchain + vendor reference clones and project libraries (Windows).
# Pins: vendor/README.md (section "Pinned commits").
param(
    [switch]$SkipVendor,
    [switch]$SkipPio,
    [switch]$SkipIdf,
    [switch]$SkipToolsPy
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$IdfPath = if ($env:IDF_PATH) { $env:IDF_PATH } else { Join-Path $env:USERPROFILE "esp\esp-idf" }
$IdfVersion = if ($env:IDF_VERSION) { $env:IDF_VERSION } else { "v5.5.1" }

function Log([string]$Msg) { Write-Host "==> $Msg" }

function Ensure-Pio {
    if ($SkipPio) { Log "skip PlatformIO (-SkipPio)"; return }
    $pio = Get-Command pio -ErrorAction SilentlyContinue
    if (-not $pio) {
        $scripts = Join-Path $env:APPDATA "Python\Python313\Scripts"
        if (Test-Path (Join-Path $scripts "pio.exe")) {
            $env:Path = "$scripts;$env:Path"
        }
    }
    if (-not (Get-Command pio -ErrorAction SilentlyContinue)) {
        Log "installing PlatformIO (user pip)"
        python -m pip install --user -U platformio
        $scripts = Join-Path $env:APPDATA "Python\Python313\Scripts"
        $env:Path = "$scripts;$env:Path"
    }
    Log "PlatformIO $($(& pio --version))"
}

function Ensure-EspIdf {
    if ($SkipIdf) { Log "skip ESP-IDF (-SkipIdf)"; return }
    if (-not (Test-Path (Join-Path $IdfPath "export.ps1"))) {
        Log "cloning ESP-IDF $IdfVersion to $IdfPath"
        New-Item -ItemType Directory -Force -Path (Split-Path $IdfPath) | Out-Null
        git clone -b $IdfVersion --depth 1 --recursive https://github.com/espressif/esp-idf.git $IdfPath
    } else {
        Log "ESP-IDF already at $IdfPath"
    }
    Log "ensuring ESP-IDF Python/toolchain deps for esp32s3 (first run may take several minutes)"
    Push-Location $IdfPath
    & .\install.ps1 esp32s3
    Pop-Location
}

$VendorRepos = @(
    @{ Rel = "vendor/cores3/CoreS3-UserDemo"; Url = "https://github.com/m5stack/CoreS3-UserDemo.git"; Ref = "90cbcc6ca40c2404de05d3f9fdb01223ae19d3f0"; Sub = $false },
    @{ Rel = "vendor/cores3/M5CoreS3"; Url = "https://github.com/m5stack/M5CoreS3.git"; Ref = "adce2225e7fd8d012b148f46c990f8cc7b8ecc26"; Sub = $false },
    @{ Rel = "vendor/cores3/M5Module-LLM"; Url = "https://github.com/m5stack/M5Module-LLM.git"; Ref = "5d0a761e0618938039d49091570628934b98be0d"; Sub = $false },
    @{ Rel = "vendor/common/M5Unified"; Url = "https://github.com/m5stack/M5Unified.git"; Ref = "8108bfad04a20ff4e57c0751f62dd6fdf6b137a6"; Sub = $false },
    @{ Rel = "vendor/common/M5GFX"; Url = "https://github.com/m5stack/M5GFX.git"; Ref = "27e1ef0f7bab2db8aa77c2768b8934983a656a7c"; Sub = $false },
    @{ Rel = "vendor/papercolor/M5PaperColor-UserDemo"; Url = "https://github.com/m5stack/M5PaperColor-UserDemo.git"; Ref = "1ff998e0cf3b9ce916f2e0319561d043db21cc4a"; Sub = $true },
    @{ Rel = "vendor/papercolor/M5PM1"; Url = "https://github.com/m5stack/M5PM1.git"; Ref = "be9a5456c007c333e7ac963f33bfde1ffa5d82ee"; Sub = $false },
    @{ Rel = "vendor/papercolor/m5stack-papercolor-esphome"; Url = "https://github.com/PFalko/m5stack-papercolor-esphome.git"; Ref = "bd0dca4daab3feae9f5657d4922185cdf0eba562"; Sub = $false }
)

function Ensure-VendorRepo($Repo) {
    $path = Join-Path $Root $Repo.Rel
    if (Test-Path (Join-Path $path ".git")) {
        Log "vendor: checkout $($Repo.Rel) @ $($Repo.Ref.Substring(0, 12))"
        git -C $path fetch origin --tags
        git -C $path checkout -q $Repo.Ref
    } else {
        Log "vendor: clone $($Repo.Rel)"
        New-Item -ItemType Directory -Force -Path (Split-Path $path) | Out-Null
        git clone --filter=blob:none $Repo.Url $path
        git -C $path checkout -q $Repo.Ref
    }
    if ($Repo.Sub) {
        git -C $path submodule update --init --recursive
    }
}

function Install-ProjectLibraries {
    if (-not $SkipToolsPy) {
        Log "python tools: install scripts/requirements-tools.txt"
        python -m pip install --user -U -r (Join-Path $Root "scripts\requirements-tools.txt")
    }
    if (-not $SkipPio -and (Get-Command pio -ErrorAction SilentlyContinue)) {
        Log "PlatformIO: fetch cores3 lib_deps"
        pio pkg install -e M5CoreS3 -d cores3
    }
    if (-not $SkipIdf -and (Test-Path (Join-Path $IdfPath "export.ps1"))) {
        Log "ESP-IDF: set papercolor target esp32s3"
        . (Join-Path $IdfPath "export.ps1")
        Push-Location (Join-Path $Root "papercolor")
        if (-not (Test-Path "sdkconfig")) {
            idf.py set-target esp32s3
        } else {
            idf.py reconfigure
        }
        Pop-Location
    }
}

Log "repo root: $Root"
Ensure-Pio
Ensure-EspIdf
if (-not $SkipVendor) {
    foreach ($repo in $VendorRepos) { Ensure-VendorRepo $repo }
} else {
    Log "skip vendor clones (-SkipVendor)"
}
Install-ProjectLibraries
Log "done — try: make build"
