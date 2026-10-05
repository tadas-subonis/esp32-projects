# Windows make equivalent — same targets/variables as the repo Makefile.
# Invoked by repo-root make.cmd:  make build  /  make flash-cores3 PORT=COM7

param()

function Parse-MakeArgs {
    param([string[]]$Tokens)
    $target = "help"
    $vars = @{}
    if (-not $Tokens) { return @{ Target = $target; Vars = $vars } }
    foreach ($a in $Tokens) {
        $a = $a.Trim()
        if (-not $a) { continue }
        if ($a -match '^([A-Za-z_][A-Za-z0-9_]*)=(.*)$') {
            $vars[$Matches[1].ToUpper()] = $Matches[2]
        } elseif ($a -notmatch '^-') {
            $target = $a
        }
    }
    return @{ Target = $target; Vars = $vars }
}

function Get-Var {
    param($Vars, [string]$Name, [string]$Default = "")
    if ($Vars.ContainsKey($Name)) { return $Vars[$Name] }
    return $Default
}

function Get-IntVar {
    param($Vars, [string]$Name, [int]$Default)
    $v = Get-Var $Vars $Name ""
    if ($v -eq "") { return $Default }
    return [int]$v
}

function Get-BoolVar {
    param($Vars, [string]$Name)
    $v = Get-Var $Vars $Name "0"
    return $v -in @("1", "true", "yes", "on")
}

$makeLine = $env:MAKECMDGOALS
$makeArgs = @()
if ($makeLine) {
    $makeArgs = @($makeLine -split '\s+' | Where-Object { $_ })
}

$parsed = Parse-MakeArgs $makeArgs
$Target = $parsed.Target
$V = $parsed.Vars

$Port = Get-Var $V "PORT" "COM7"
$Cores3Port = Get-Var $V "CORES3_PORT" ""
if (-not $Cores3Port) { $Cores3Port = $Port }
$PapercolorPort = Get-Var $V "PAPERCOLOR_PORT" ""
if (-not $PapercolorPort) { $PapercolorPort = $Port }
$Seconds = Get-IntVar $V "SECONDS" 5
$BootWait = Get-IntVar $V "BOOT_WAIT" 8
$Tries = Get-IntVar $V "TRIES" 30
$Until = Get-Var $V "UNTIL" ""
$Out = Get-Var $V "OUT" ""
$Cmd = Get-Var $V "CMD" ""
$Raw = Get-BoolVar $V "RAW"

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$IdfPath = if ($env:IDF_PATH) { $env:IDF_PATH } else { Join-Path $env:USERPROFILE "esp\esp-idf" }

function Ensure-PioPath {
    foreach ($ver in @("Python313", "Python312", "Python311", "Python310")) {
        $scripts = Join-Path $env:APPDATA "Python\$ver\Scripts"
        if (Test-Path (Join-Path $scripts "pio.exe")) {
            $env:Path = "$scripts;$env:Path"
            return
        }
    }
    if (Test-Path (Join-Path $env:USERPROFILE ".platformio\penv\Scripts\pio.exe")) {
        $env:Path = "$(Join-Path $env:USERPROFILE '.platformio\penv\Scripts');$env:Path"
    }
}

function Invoke-Idf([scriptblock]$Block) {
    if (-not (Test-Path (Join-Path $IdfPath "export.ps1"))) {
        throw "ESP-IDF not found at $IdfPath. Run: make deps"
    }
    . (Join-Path $IdfPath "export.ps1")
    Push-Location (Join-Path $Root "papercolor")
    try { & $Block } finally { Pop-Location }
}

function Invoke-DevctlLogs {
    param([string]$DevPort, [int]$LogSeconds, [switch]$RawFlag)
    $dargs = @("--port", $DevPort, "--baud", "115200", "--seconds", "$LogSeconds")
    if ($Until) { $dargs += @("--until", $Until) }
    if ($Out) { $dargs += @("--out", $Out) }
    if ($RawFlag) { $dargs += "--raw" }
    python (Join-Path $PSScriptRoot "devctl.py") logs @dargs
}

function Invoke-DevctlCmd {
    param([string]$DevPort, [string]$Command)
    if (-not $Command) { throw "CMD is required (e.g. make cmd-cores3 CMD=status)" }
    $dargs = @("--port", $DevPort, "--baud", "115200", "--boot-wait", "$BootWait", "--timeout", "20")
    if ($Out) { $dargs += @("--out", $Out) }
    python (Join-Path $PSScriptRoot "devctl.py") cmd @dargs $Command
}

function Do-BuildCores3 { pio run -e M5CoreS3 -d cores3 }
function Do-BuildPapercolor { Invoke-Idf { idf.py build } }

function Do-FlashCores3 {
    Do-BuildCores3
    pio run -e M5CoreS3 -d cores3 -t upload --upload-port $Port
    if (-not $?) {
        Write-Host "Upload failed - long-press RST ~3s then: make flash-cores3-wait PORT=$Port"
        return
    }
    Write-Host "Upload OK - watchdog reset to run app..."
    & python -m esptool --chip esp32s3 -p $Port --before no_reset --after watchdog_reset run | Out-Null
}

function Do-FlashCores3Wait {
    Do-BuildCores3
    $B = Join-Path $Root "cores3\.pio\build\M5CoreS3"
    $ok = $false
    Write-Host "Waiting for CoreS3 on $Port (long-press RST ~3s if needed)..."
    for ($i = 1; $i -le $Tries; $i++) {
        & python -m esptool --chip esp32s3 -p $Port -b 460800 --before default_reset --after no_reset `
            write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m `
            0x0 (Join-Path $B "bootloader.bin") `
            0x8000 (Join-Path $B "partitions.bin") `
            0x10000 (Join-Path $B "firmware.bin") | Out-Null
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Flash OK on try $i"
            python -m esptool --chip esp32s3 -p $Port --before no_reset --after watchdog_reset run | Out-Null
            $ok = $true
            break
        }
        Write-Host "  try $i/$Tries - no connection, retrying in 2s..."
        Start-Sleep -Seconds 2
    }
    if (-not $ok) { throw "Failed after $Tries tries. Long-press CoreS3 RST ~3s and re-run." }
}

function Do-FlashPapercolor {
    Do-BuildPapercolor
    Invoke-Idf { idf.py -p $Port -D ESPTOOLPY_AFTER=no_reset flash }
}

function Show-Help {
    @"
Photo frame monorepo — Windows make (same targets as Makefile):

  make deps                 Toolchain + vendor
  make build                cores3 + papercolor
  make build-cores3         PlatformIO M5CoreS3
  make build-papercolor     ESP-IDF papercolor
  make flash-cores3         Build + upload CoreS3     (PORT=$Port)
  make flash-cores3-wait    Retry flash until connect (PORT=$Port, TRIES=$Tries)
  make flash-papercolor     Build + flash PaperColor  (PORT=$Port)
  make monitor-cores3       Serial monitor CoreS3     (PORT=$Port)
  make monitor-papercolor   Serial monitor PaperColor (PORT=$Port)
  make verify-photo-post    HTTP POST test image       (PHOTO_FRAME_HOST=192.168.4.1)
  make ports                list serial ports (JSON)
  make identify             probe ports / guess device (JSON)
  make logs-cores3          capture CoreS3 logs       (CORES3_PORT=$Cores3Port, SECONDS=$Seconds)
  make logs-papercolor      capture PaperColor logs   (PAPERCOLOR_PORT=$PapercolorPort)
  make cmd-cores3           send CoreS3 command       (CMD=..., CORES3_PORT=$Cores3Port)
  make cmd-papercolor       send PaperColor command   (CMD=..., PAPERCOLOR_PORT=$PapercolorPort)
  make capture              CoreS3 capture+send photo (CORES3_PORT=$Cores3Port)
  make rawlogs              raw log dump              (PORT=$Port, SECONDS=$Seconds, RAW=1)
  make dev-cores3           build+flash+logs          (CORES3_PORT=$Cores3Port)
  make dev-papercolor       build+flash+logs          (PAPERCOLOR_PORT=$PapercolorPort)
  make wake                 esptool watchdog_reset    (PORT=$Port)
  make clean                clean both builds

Variables use make syntax: make flash-cores3 PORT=COM7
Serial ports are COM* (not /dev/ttyACM*). Run make identify first.

Docs: docs/windows-native.md
"@
}

Ensure-PioPath

switch -Exact ($Target) {
    { $_ -in @("help", "") } { Show-Help }
    "all" { Do-BuildCores3; Do-BuildPapercolor }
    "deps" { & (Join-Path $PSScriptRoot "deps.ps1") }
    "build" { Do-BuildCores3; Do-BuildPapercolor }
    "build-cores3" { Do-BuildCores3 }
    "build-papercolor" { Do-BuildPapercolor }
    "flash-cores3" { Do-FlashCores3 }
    "flash-cores3-wait" { Do-FlashCores3Wait }
    "flash-papercolor" { Do-FlashPapercolor }
    "monitor-cores3" { pio device monitor -d cores3 -p $Port -b 115200 }
    "monitor-papercolor" { Invoke-Idf { idf.py -p $Port monitor } }
    "verify-photo-post" { & (Join-Path $PSScriptRoot "verify-photo-post.ps1") }
    "ports" { python (Join-Path $PSScriptRoot "devctl.py") ports }
    "identify" { python (Join-Path $PSScriptRoot "devctl.py") identify }
    "logs-cores3" { Invoke-DevctlLogs -DevPort $Cores3Port -LogSeconds $Seconds }
    "logs-papercolor" { Invoke-DevctlLogs -DevPort $PapercolorPort -LogSeconds $Seconds }
    "rawlogs" { Invoke-DevctlLogs -DevPort $Port -LogSeconds $Seconds -RawFlag:($Raw -or $true) }
    "cmd-cores3" { Invoke-DevctlCmd -DevPort $Cores3Port -Command $Cmd }
    "cmd-papercolor" { Invoke-DevctlCmd -DevPort $PapercolorPort -Command $Cmd }
    "capture" { Invoke-DevctlCmd -DevPort $Cores3Port -Command "capture" }
    "dev-cores3" { Do-FlashCores3; Invoke-DevctlLogs -DevPort $Cores3Port -LogSeconds 4 }
    "dev-papercolor" { Do-FlashPapercolor; Invoke-DevctlLogs -DevPort $PapercolorPort -LogSeconds 4 }
    "wake" { Invoke-Idf { esptool.py --chip esp32s3 -p $Port --before no_reset --after watchdog_reset run } }
    "clean" {
        pio run -e M5CoreS3 -d cores3 -t clean
        Invoke-Idf { idf.py fullclean }
    }
    default { throw "Unknown target: $Target. Run: make help" }
}
