# Windows make for the P4 LCD wiring test.
# Invoked by repo-root make.cmd:  make build  /  make flash PORT=COM8

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

$makeLine = $env:MAKECMDGOALS
$makeArgs = @()
if ($makeLine) {
    $makeArgs = @($makeLine -split '\s+' | Where-Object { $_ })
}

$parsed = Parse-MakeArgs $makeArgs
$Target = $parsed.Target
$V = $parsed.Vars
$Port = Get-Var $V "PORT" ""
$Seconds = Get-IntVar $V "SECONDS" 8
$LogOverride = Get-Var $V "LOG" ""

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$IdfPath = if ($env:IDF_PATH) { $env:IDF_PATH } else { Join-Path $env:USERPROFILE "esp\esp-idf" }
$Firmware = Join-Path $Root "firmware"

function Invoke-Idf([scriptblock]$Block) {
    if (-not (Test-Path (Join-Path $IdfPath "export.ps1"))) {
        throw "ESP-IDF not found at $IdfPath. Run: make deps"
    }
    . (Join-Path $IdfPath "export.ps1")
    Push-Location $Firmware
    try { & $Block } finally { Pop-Location }
}

function Resolve-Port {
    if ($Port) { return $Port }
    $raw = python (Join-Path $PSScriptRoot "devctl.py") ports | Out-String
    $data = $raw | ConvertFrom-Json
    $cands = @($data.ports | Where-Object {
        $vid = $_.vid
        $desc = [string]$_.description
        $man = [string]$_.manufacturer
        ($vid -in @(6790, 12346, 4292)) -or  # 0x1A86 WCH, 0x303A Espressif, 0x10C4 Silicon Labs
        $desc -match 'CH343|CH340|CP210|USB-SERIAL|UART|USB Serial' -or
        $man -match 'wch|qinheng|espressif|silicon labs'
    })
    if ($cands.Count -eq 1) {
        Write-Host "Using port $($cands[0].device) ($($cands[0].description))"
        return [string]$cands[0].device
    }
    if ($cands.Count -gt 1) {
        $list = ($cands | ForEach-Object { $_.device }) -join ", "
        throw "Several serial ports look like the board ($list). Pass one: .\make.ps1 flash PORT=COM8"
    }
    throw "No Type-C serial port found. Plug in USB-C, then: .\make.ps1 ports"
}

function Do-Build {
    Invoke-Idf {
        if (-not (Test-Path "sdkconfig")) {
            idf.py set-target esp32p4
            if ($LASTEXITCODE -ne 0) { throw "idf.py set-target esp32p4 failed" }
        }
        idf.py build
        if ($LASTEXITCODE -ne 0) { throw "idf.py build failed" }
    }
}

function Do-Flash {
    $script:ResolvedPort = Resolve-Port
    Do-Build
    Invoke-Idf { idf.py -p $script:ResolvedPort flash }
    if ($LASTEXITCODE -ne 0) { throw "idf.py flash failed on $script:ResolvedPort" }
}

function Get-MonitorLogPath {
    if ($LogOverride) {
        $p = $LogOverride
        if (-not [System.IO.Path]::IsPathRooted($p)) {
            $p = Join-Path $Root $p
        }
        $dir = Split-Path -Parent $p
        if ($dir) {
            New-Item -ItemType Directory -Force -Path $dir | Out-Null
        }
        return $p
    }
    $dir = Join-Path $Root "logs"
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $portSafe = ($script:ResolvedPort -replace '[^A-Za-z0-9]', '_')
    return Join-Path $dir "${stamp}_${portSafe}.log"
}

function Invoke-IdfMonitor {
    param([switch]$WithFlash)
    $script:MonitorLogFile = Get-MonitorLogPath
    $script:DevctlPy = Join-Path $PSScriptRoot "devctl.py"
    $script:DoFlashMonitor = [bool]$WithFlash
    Write-Host "Saving serial output to $($script:MonitorLogFile)"
    Invoke-Idf {
        if ($script:DoFlashMonitor) {
            idf.py -p $script:ResolvedPort flash
            if ($LASTEXITCODE -ne 0) { throw "idf.py flash failed on $($script:ResolvedPort)" }
        }
        python $script:DevctlPy logs --port $script:ResolvedPort --baud 115200 --seconds 0 --save --reset --out $script:MonitorLogFile
    }
}

switch -Exact ($Target) {
    { $_ -in @("help", "") } {
        @"
P4 LCD wiring test (from PowerShell, repo root):

  .\make.ps1 flash              Build + flash (picks Type-C COM port)
  .\make.ps1 flash PORT=COM8    Same, but force a port
  .\make.ps1 monitor            Serial console (keyboard → board, log under logs\)
  .\make.ps1 flash-monitor      Build + flash + interactive serial
  .\make.ps1 flash-monitor LOG=logs\run.log
  .\make.ps1 ports
  .\make.ps1 build
  .\make.ps1 deps
  .\make.ps1 clean

Type into the monitor to talk to P4Bench (help, spisweep, s/p/u). Ctrl+C quits.
.\make.cmd works the same. Do not use Git's make.exe. Plug in Type-C.
"@
    }
    "all" { Do-Flash }
    "deps" { & (Join-Path $PSScriptRoot "deps.ps1") }
    "build" { Do-Build }
    "flash" { Do-Flash }
    "monitor" {
        $script:ResolvedPort = Resolve-Port
        Invoke-IdfMonitor
    }
    "flash-monitor" {
        $script:ResolvedPort = Resolve-Port
        Do-Build
        Invoke-IdfMonitor -WithFlash
    }
    "ports" { python (Join-Path $PSScriptRoot "devctl.py") ports }
    "logs" {
        python (Join-Path $PSScriptRoot "devctl.py") logs --port $Port --baud 115200 --seconds "$Seconds"
    }
    "clean" { Invoke-Idf { idf.py fullclean } }
    default { throw "Unknown target: $Target. Run: .\make.ps1 help" }
}
