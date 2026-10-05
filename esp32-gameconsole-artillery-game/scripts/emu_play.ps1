# Play a remote emu PvP match to gameover (or fail with dumps + log).
# Usage: .\scripts\emu_play.ps1 [-MaxTurns 30] [-OutDir logs\play]
param(
    [int]$MaxTurns = 30,
    [string]$OutDir = "logs\play",
    [int]$TimeoutMs = 3000,
    [int]$FireWaitMs = 60000,
    [int]$Angle = 55,
    [int]$Power = 72
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$Cli = Join-Path $Root "build-host\artillery-cli.exe"
if (-not (Test-Path $Cli)) {
    throw "missing $Cli - run .\make build-host"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$Log = Join-Path $OutDir "session.log"
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
Add-Content $Log ""
Add-Content $Log "==== emu_play $stamp ===="

function Write-Log([string]$msg) {
    $line = "[{0:HH:mm:ss.fff}] {1}" -f (Get-Date), $msg
    Add-Content $Log $line
    Write-Host $line
}

function Emu([int]$seat, [string]$cmd, [int]$to = $TimeoutMs) {
    $port = 17500 + $seat
    $argList = @("--emu", "$port", "--timeout-ms", "$to") + @($cmd -split '\s+' | Where-Object { $_ })
    $out = & $Cli @argList 2>&1 | Out-String
    $text = $out.Trim()
    Write-Log ("seat={0} cmd={1} => {2}" -f $seat, $cmd, $text)
    if ($LASTEXITCODE -ne 0 -and $text -notmatch '"ok":true') {
        throw ("emu cmd failed (exit={0}): {1}" -f $LASTEXITCODE, $text)
    }
    return $text
}

function Get-JsonField([string]$s, [string]$key) {
    $m = [regex]::Match($s, '"' + [regex]::Escape($key) + '"\s*:\s*"([^"]*)"')
    if ($m.Success) { return $m.Groups[1].Value }
    $m = [regex]::Match($s, '"' + [regex]::Escape($key) + '"\s*:\s*(true|false|-?[0-9]+(?:\.[0-9]+)?)')
    if ($m.Success) { return $m.Groups[1].Value }
    return $null
}

function Parse-Status([string]$s) {
    return [pscustomobject]@{
        phase   = Get-JsonField $s "phase"
        active  = Get-JsonField $s "active"
        hp0     = Get-JsonField $s "hp0"
        hp1     = Get-JsonField $s "hp1"
        winner  = Get-JsonField $s "winner"
        turn    = Get-JsonField $s "turn"
        angle   = Get-JsonField $s "angle"
        power   = Get-JsonField $s "power"
        wind    = Get-JsonField $s "wind"
        firing  = Get-JsonField $s "firing"
        seq     = Get-JsonField $s "seq"
        proj_x  = Get-JsonField $s "proj_x"
        proj_y  = Get-JsonField $s "proj_y"
    }
}

function Dump-Seat([int]$seat, [string]$tag) {
    $ppm = Join-Path $OutDir ("{0}-s{1}.ppm" -f $tag, $seat)
    Emu $seat ("dump {0}" -f $ppm) 5000 | Out-Null
    if (Test-Path $ppm) {
        $png = & python (Join-Path $PSScriptRoot "ppm_to_png.py") $ppm 2>&1
        Write-Log ("dump {0}" -f $png)
        return "$png"
    }
    Write-Log ("dump missing file {0}" -f $ppm)
    return $null
}

function Fail([string]$why, [string]$tag) {
    Write-Log ("FAIL: {0}" -f $why)
    Dump-Seat 0 $tag | Out-Null
    Dump-Seat 1 $tag | Out-Null
    Write-Log ("see {0} and {1}\*.png" -f $Log, $OutDir)
    throw $why
}

try {
    $s0 = Parse-Status (Emu 0 status)
    $s1 = Parse-Status (Emu 1 status)
} catch {
    Fail ("cannot reach emu controls (run .\make emu-pvp first): {0}" -f $_) "unreachable"
}

Write-Log ("P0 phase={0} active={1} hp={2}/{3}" -f $s0.phase, $s0.active, $s0.hp0, $s0.hp1)
Write-Log ("P1 phase={0} active={1} hp={2}/{3}" -f $s1.phase, $s1.active, $s1.hp0, $s1.hp1)
Dump-Seat 0 "start" | Out-Null

if ($s0.phase -eq "gameover") {
    Write-Log "rematch from gameover"
    Emu 0 rematch | Out-Null
    Start-Sleep -Milliseconds 800
    $s0 = Parse-Status (Emu 0 status)
}

if ($s0.phase -eq "title") {
    Emu 0 start | Out-Null
    Start-Sleep -Milliseconds 900
    $s0 = Parse-Status (Emu 0 status)
}

for ($turn = 0; $turn -lt $MaxTurns; $turn++) {
    $st = Parse-Status (Emu 0 status)
    Write-Log ("turn_loop={0} phase={1} active={2} hp={3}/{4} winner={5}" -f `
        $turn, $st.phase, $st.active, $st.hp0, $st.hp1, $st.winner)

    if ($st.phase -eq "gameover") {
        Dump-Seat 0 "end" | Out-Null
        Dump-Seat 1 "end" | Out-Null
        Write-Log ("GAME OVER winner={0} hp={1}/{2} log={3}" -f $st.winner, $st.hp0, $st.hp1, $Log)
        Write-Host ("OK gameover winner={0}" -f $st.winner)
        exit 0
    }

    if ($st.phase -ne "aiming") {
        $sw = [Diagnostics.Stopwatch]::StartNew()
        while ($sw.ElapsedMilliseconds -lt $FireWaitMs) {
            Start-Sleep -Milliseconds 200
            $st = Parse-Status (Emu 0 status 1500)
            if ($st.phase -eq "aiming" -or $st.phase -eq "gameover") { break }
        }
        if ($st.phase -eq "gameover") { continue }
        if ($st.phase -ne "aiming") {
            Fail ("stuck in phase={0} firing={1} proj={2},{3}" -f `
                $st.phase, $st.firing, $st.proj_x, $st.proj_y) ("stuck-phase-{0}" -f $turn)
        }
    }

    $who = [int]$st.active
    Emu $who ("angle {0}" -f $Angle) | Out-Null
    Emu $who ("power {0}" -f $Power) | Out-Null
    Dump-Seat $who ("prefire-{0}" -f $turn) | Out-Null
    Emu $who fire 5000 | Out-Null

    $sw = [Diagnostics.Stopwatch]::StartNew()
    $back = $false
    while ($sw.ElapsedMilliseconds -lt $FireWaitMs) {
        Start-Sleep -Milliseconds 250
        $st = Parse-Status (Emu 0 status 1500)
        if ($st.phase -eq "aiming" -or $st.phase -eq "gameover") {
            $back = $true
            break
        }
    }
    if (-not $back) {
        Fail ("shot did not resolve (phase={0} firing={1} proj={2},{3})" -f `
            $st.phase, $st.firing, $st.proj_x, $st.proj_y) ("stuck-fire-{0}" -f $turn)
    }
}

Fail ("hit MaxTurns={0} without gameover" -f $MaxTurns) "max-turns"
