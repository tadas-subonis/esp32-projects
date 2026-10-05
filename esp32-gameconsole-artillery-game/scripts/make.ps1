# Windows make equivalent - same targets/variables as the repo Makefile.
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Goals
)

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

$makeArgs = @()
if ($env:MAKECMDGOALS) {
    $makeArgs = @($env:MAKECMDGOALS -split '\s+' | Where-Object { $_ })
} elseif ($Goals) {
    $makeArgs = @($Goals)
}

$parsed = Parse-MakeArgs $makeArgs
$Target = $parsed.Target
$V = $parsed.Vars

$Port = Get-Var $V "PORT" ""
$Seconds = Get-IntVar $V "SECONDS" 5
$BootWait = Get-IntVar $V "BOOT_WAIT" 8
$Until = Get-Var $V "UNTIL" ""
$Out = Get-Var $V "OUT" ""
$Cmd = Get-Var $V "CMD" ""
$Seed = Get-Var $V "SEED" "1"
$LogOverride = Get-Var $V "LOG" ""

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$IdfPath = if ($env:IDF_PATH) { $env:IDF_PATH } else { Join-Path $env:USERPROFILE "esp\esp-idf" }
$HostBuild = Join-Path $Root "build-host"
$script:HostEnvReady = $false

function Invoke-Idf([scriptblock]$Block) {
    if (-not (Test-Path (Join-Path $IdfPath "export.ps1"))) {
        throw "ESP-IDF not found at $IdfPath. Run: make deps"
    }
    . (Join-Path $IdfPath "export.ps1")
    Push-Location (Join-Path $Root "firmware")
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
        throw "Several serial ports look like the board ($list). Pass one: .\make flash PORT=COM5"
    }
    throw "No Type-C serial port found. Plug in USB-C, then: .\make ports"
}

function Get-MonitorLogPath {
    param([string]$Resolved)
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
    $portSafe = ($Resolved -replace '[^A-Za-z0-9]', '_')
    return Join-Path $dir "${stamp}_${portSafe}.log"
}

function Invoke-SerialMonitor {
    param(
        [string]$Resolved,
        [switch]$WithFlash
    )
    $script:MonitorPort = $Resolved
    $script:MonitorLogFile = Get-MonitorLogPath -Resolved $Resolved
    $script:DevctlPy = Join-Path $PSScriptRoot "devctl.py"
    $script:DoFlashMonitor = [bool]$WithFlash
    Write-Host "Saving serial output to $($script:MonitorLogFile)"
    Invoke-Idf {
        if ($script:DoFlashMonitor) {
            idf.py -p $script:MonitorPort flash
            if ($LASTEXITCODE -ne 0) { throw "idf.py flash failed on $($script:MonitorPort)" }
        }
        python $script:DevctlPy logs --port $script:MonitorPort --baud 115200 --seconds 0 --save --reset --out $script:MonitorLogFile
    }
}

function Import-HostToolEnv {
    if ($script:HostEnvReady) { return }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vs = $null
    if (Test-Path $vswhere) {
        $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    }

    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        if (-not $vs) {
            throw "MSVC x64 toolset not found. Install Visual Studio Build Tools with 'Desktop development with C++'."
        }
        $vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
        if (-not (Test-Path $vcvars)) {
            throw "vcvars64.bat missing at $vcvars"
        }
        cmd.exe /c "`"$vcvars`" >nul && set" | ForEach-Object {
            if ($_ -match '^(.*?)=(.*)$') {
                try {
                    [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2])
                } catch {
                }
            }
        }
    }
    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        throw "cl.exe still not on PATH after vcvars64. Open a VS x64 Native Tools prompt and retry."
    }

    $cmakeDirs = @()
    Get-ChildItem (Join-Path $env:USERPROFILE ".espressif\tools\cmake") -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending |
        ForEach-Object { $cmakeDirs += (Join-Path $_.FullName "bin") }
    if ($vs) {
        $cmakeDirs += (Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin")
    }
    foreach ($dir in $cmakeDirs) {
        if (Test-Path (Join-Path $dir "cmake.exe")) {
            $env:PATH = "$dir;$env:PATH"
            break
        }
    }

    $ninjaDirs = @()
    Get-ChildItem (Join-Path $env:USERPROFILE ".espressif\tools\ninja") -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending |
        ForEach-Object { $ninjaDirs += $_.FullName }
    if ($vs) {
        $ninjaDirs += (Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja")
    }
    foreach ($dir in $ninjaDirs) {
        if (Test-Path (Join-Path $dir "ninja.exe")) {
            $env:PATH = "$dir;$env:PATH"
            break
        }
    }

    if (-not (Get-Command cmake.exe -ErrorAction SilentlyContinue)) {
        throw "cmake.exe not found. VS Build Tools CMake component or ESP-IDF tools are required."
    }
    if (-not (Get-Command ninja.exe -ErrorAction SilentlyContinue)) {
        throw "ninja.exe not found. Install the VS CMake/Ninja component or ESP-IDF tools."
    }
    $script:HostEnvReady = $true
}

function Reset-HostBuildIfIncompatible {
    $cache = Join-Path $HostBuild "CMakeCache.txt"
    if (-not (Test-Path $cache)) { return }
    $text = Get-Content $cache -Raw
    $needsWipe = $false
    if ($text -match 'CMAKE_SYSTEM_NAME:[^=]*=Linux') { $needsWipe = $true }
    if ($text -notmatch 'CMAKE_GENERATOR:[^=]*=Ninja') { $needsWipe = $true }
    if ($needsWipe) {
        Write-Host "build-host cache is not a Windows Ninja tree; recreating"
        Remove-Item -Recurse -Force $HostBuild
    }
}

function Get-HostExe([string]$Name) {
    foreach ($candidate in @(
            (Join-Path $HostBuild "$Name.exe"),
            (Join-Path $HostBuild $Name)
        )) {
        if (Test-Path $candidate) { return $candidate }
    }
    throw "missing $Name in $HostBuild - run: make build-host"
}

function Do-BuildHost {
    Import-HostToolEnv
    Reset-HostBuildIfIncompatible
    cmake -S $Root -B $HostBuild -G Ninja -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
    cmake --build $HostBuild -j
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }
}

function Do-Test {
    Do-BuildHost
    & (Get-HostExe "artillery-tests")
    if ($LASTEXITCODE -ne 0) { throw "artillery-tests failed" }
    & (Get-HostExe "artillery-link-tests")
    if ($LASTEXITCODE -ne 0) { throw "artillery-link-tests failed" }
    python (Join-Path $Root "tests\test_sim.py")
    if ($LASTEXITCODE -ne 0) { throw "test_sim.py failed" }
    python (Join-Path $Root "tests\test_server.py")
    if ($LASTEXITCODE -ne 0) { throw "test_server.py failed" }
}

function Do-Server {
    Do-BuildHost
    & (Get-HostExe "artillery-server") --port 7420
}

function Do-Play {
    Do-BuildHost
    & (Get-HostExe "artillery-host") --local --want bot
}

function Do-PlayLocal {
    Do-Play
}

function Do-PlayRemote {
    Do-BuildHost
    & (Get-HostExe "artillery-host") --remote --host 127.0.0.1 --port 7420
}

function Do-EmuPvp {
    Do-BuildHost
    $exe = Get-HostExe "artillery-emu"
    $srv = Get-HostExe "artillery-server"
    $port = 17421
    Get-Process artillery-emu -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandLine -like "*artillery-server.exe*--port $port*" } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Start-Process $srv -ArgumentList @("--port", "$port")
    Start-Sleep -Milliseconds 400
    Start-Process $exe -ArgumentList @("--remote", "--port", "$port", "--want", "pvp", "--control-port", "17500")
    Start-Sleep -Milliseconds 500
    Start-Process $exe -ArgumentList @("--remote", "--port", "$port", "--want", "pvp", "--control-port", "17501")
    Write-Host "emu-pvp: server :$port  P0 control :17500  P1 control :17501"
    Write-Host "  .\make emu-cmd SEAT=0 CMD=status"
    Write-Host "  .\make emu-play"
    Write-Host "  .\make emu-dump SEAT=0 OUT=logs\frame.png"
}

function Do-EmuDump {
    Do-BuildHost
    $seat = Get-IntVar $V "SEAT" 0
    $timeoutMs = Get-IntVar $V "TIMEOUT_MS" 5000
    $out = if ($Out) { $Out } else { "logs\emu-s$seat.png" }
    $dir = Split-Path -Parent $out
    if ($dir) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    $ppm = [System.IO.Path]::ChangeExtension($out, ".ppm")
    if (-not $ppm) { $ppm = "$out.ppm" }
    $ctrl = 17500 + $seat
    Invoke-HostCliTimed -Exe (Get-HostExe "artillery-cli") `
        -CliArgs @("--emu", "$ctrl", "--timeout-ms", "$timeoutMs", "dump", $ppm) `
        -TimeoutMs $timeoutMs -Label "emu-dump"
    if (-not (Test-Path $ppm)) { throw "dump did not create $ppm" }
    $png = python (Join-Path $PSScriptRoot "ppm_to_png.py") $ppm -o $out
    if ($LASTEXITCODE -ne 0) { throw "ppm_to_png failed" }
    Write-Host "wrote $png"
}

function Do-EmuPlay {
    Do-BuildHost
    $outDir = if ($Out) { $Out } else { "logs\play" }
    $maxTurns = Get-IntVar $V "MAX_TURNS" 30
    & (Join-Path $PSScriptRoot "emu_play.ps1") -OutDir $outDir -MaxTurns $maxTurns
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-HostCliTimed {
    param(
        [string]$Exe,
        [string[]]$CliArgs,
        [int]$TimeoutMs,
        [string]$Label
    )
    # Start-Process -ArgumentList with a string[] drops args on Windows; join explicitly.
    $argLine = ($CliArgs | ForEach-Object {
        if ($_ -match '\s') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ }
    }) -join ' '
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Exe
    $psi.Arguments = $argLine
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $psi
    [void]$proc.Start()
    $stdout = $proc.StandardOutput.ReadToEndAsync()
    $stderr = $proc.StandardError.ReadToEndAsync()
    if (-not $proc.WaitForExit($TimeoutMs + 500)) {
        try { $proc.Kill() } catch {}
        throw "$Label timed out after ${TimeoutMs}ms"
    }
    $outText = $stdout.Result
    $errText = $stderr.Result
    if ($outText) { Write-Host -NoNewline $outText }
    if ($errText) { [Console]::Error.Write($errText) }
    if ($proc.ExitCode -ne 0) { exit $proc.ExitCode }
}

function Do-EmuCmd {
    Do-BuildHost
    if (-not $Cmd) { throw "CMD is required (e.g. make emu-cmd SEAT=0 CMD=status)" }
    $seat = Get-IntVar $V "SEAT" 0
    $timeoutMs = Get-IntVar $V "TIMEOUT_MS" 2000
    $ctrl = 17500 + $seat
    $parts = @("--emu", "$ctrl", "--timeout-ms", "$timeoutMs") + ($Cmd -split '\s+' | Where-Object { $_ })
    Invoke-HostCliTimed -Exe (Get-HostExe "artillery-cli") -CliArgs $parts -TimeoutMs $timeoutMs -Label "emu-cmd"
}

function Do-Emu {
    Do-BuildHost
    $emuArgs = @()
    if ($Seed -and $Seed -ne "1") { $emuArgs += @("--seed", $Seed) }
    & (Get-HostExe "artillery-emu") @emuArgs
}

function Do-Cli {
    Do-BuildHost
    if (-not $Cmd) { throw "CMD is required (e.g. make cli CMD=status)" }
    $timeoutMs = Get-IntVar $V "TIMEOUT_MS" 2000
    $parts = @("--timeout-ms", "$timeoutMs") + ($Cmd -split '\s+' | Where-Object { $_ })
    Invoke-HostCliTimed -Exe (Get-HostExe "artillery-cli") -CliArgs $parts -TimeoutMs $timeoutMs -Label "cli"
}

function Do-BuildFirmware {
    Invoke-Idf {
        if (-not (Test-Path "sdkconfig")) {
            idf.py set-target esp32p4
        }
        idf.py build
    }
}

function Invoke-DevctlLogs {
    param([string]$DevPort, [int]$LogSeconds)
    $dargs = @("--port", $DevPort, "--baud", "115200", "--seconds", "$LogSeconds")
    if ($Until) { $dargs += @("--until", $Until) }
    if ($Out) { $dargs += @("--out", $Out) }
    python (Join-Path $PSScriptRoot "devctl.py") logs @dargs
}

function Invoke-DevctlCmd {
    param([string]$DevPort, [string]$Command)
    if (-not $Command) { throw "CMD is required (e.g. make cmd CMD=status)" }
    $dargs = @("--port", $DevPort, "--baud", "115200", "--boot-wait", "$BootWait", "--timeout", "20")
    if ($Out) { $dargs += @("--out", $Out) }
    python (Join-Path $PSScriptRoot "devctl.py") cmd @dargs $Command
}

function Invoke-DevctlSnap {
    param([string]$DevPort)
    $dargs = @("--port", $DevPort, "--baud", "115200", "--boot-wait", "$BootWait", "--timeout", "30")
    if ($Out) { $dargs += @("--out", $Out) }
    python (Join-Path $PSScriptRoot "devctl.py") snap @dargs
}

function Show-Help {
    @"
Tank Duel (artillery) - Windows make (same targets as Makefile).

PowerShell:  .\make emu
cmd.exe:     make emu

  make deps              pyserial + ESP-IDF esp32p4 tools
  make test              host C++ tests + sim + server (MSVC)
  make server            authoritative TCP sim          (port 7420)
  make play              local vs-bot (terminal, in-process server)
  make play-local        same as play
  make play-remote       TCP client (needs make server)
  make emu               SDL window (same RGB565 compose as firmware)
  make emu-pvp           two SDL windows vs local TCP server (port 17421)
  make emu-cmd           send to emu control                 (SEAT=0 CMD=status TIMEOUT_MS=2000)
  make emu-dump          screenshot PNG from emu             (SEAT=0 OUT=logs\frame.png)
  make emu-play          autoplay PvP to gameover + dumps    (OUT=logs\play MAX_TURNS=30)
  make cli               one JSON command               (CMD=status)
  make build             host + firmware
  make build-host        C++ sim/host/tests
  make build-firmware    ESP-IDF ESP32-P4 game
  make flash             build + flash handheld          (PORT=auto Type-C)
  make flash-monitor     flash, then serial to screen + logs\
  make monitor           serial to screen + logs\        (PORT=auto Type-C)
  make ports             list serial ports (JSON)
  make identify          probe ports / guess device (JSON)
  make cmd               send device command             (CMD=..., PORT=auto)
  make snap              120x80 PNG screenshot           (PORT=auto, OUT=logs\...)
  make logs              capture serial logs             (PORT=auto, SECONDS=$Seconds)
  make clean             clean host + firmware builds

Examples:
  .\make test
  .\make play
  .\make emu
  .\make flash
  .\make flash-monitor
  .\make flash-monitor PORT=COM5 LOG=logs\run.log
  .\make cmd CMD=status
  .\make cmd CMD="new 42"
  .\make snap

Docs: docs/windows-native.md
"@
}

switch -Exact ($Target) {
    { $_ -in @("help", "") } { Show-Help }
    "deps" { & (Join-Path $PSScriptRoot "deps.ps1") }
    "test" { Do-Test }
    "server" { Do-Server }
    "play" { Do-Play }
    "play-local" { Do-PlayLocal }
    "play-remote" { Do-PlayRemote }
    "emu" { Do-Emu }
    "emu-pvp" { Do-EmuPvp }
    "emu-cmd" { Do-EmuCmd }
    "emu-dump" { Do-EmuDump }
    "emu-play" { Do-EmuPlay }
    "cli" { Do-Cli }
    "build-host" { Do-BuildHost }
    "build-firmware" { Do-BuildFirmware }
    "build" { Do-BuildHost; Do-BuildFirmware }
    "flash" {
        $script:ResolvedPort = Resolve-Port
        Do-BuildFirmware
        Invoke-Idf { idf.py -p $script:ResolvedPort flash }
        if ($LASTEXITCODE -ne 0) { throw "idf.py flash failed on $($script:ResolvedPort)" }
    }
    "monitor" {
        Invoke-SerialMonitor -Resolved (Resolve-Port)
    }
    "flash-monitor" {
        $script:ResolvedPort = Resolve-Port
        Do-BuildFirmware
        Invoke-SerialMonitor -Resolved $script:ResolvedPort -WithFlash
    }
    "ports" { python (Join-Path $PSScriptRoot "devctl.py") ports }
    "identify" { python (Join-Path $PSScriptRoot "devctl.py") identify }
    "logs" { Invoke-DevctlLogs -DevPort (Resolve-Port) -LogSeconds $Seconds }
    "cmd" { Invoke-DevctlCmd -DevPort (Resolve-Port) -Command $Cmd }
    "snap" { Invoke-DevctlSnap -DevPort (Resolve-Port) }
    "clean" {
        if (Test-Path $HostBuild) { Remove-Item -Recurse -Force $HostBuild }
        if (Test-Path (Join-Path $Root "firmware\build")) {
            Invoke-Idf { idf.py fullclean }
        }
    }
    default { throw "Unknown target: $Target. Run: .\make help" }
}
