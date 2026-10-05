# PowerShell entry point. Use:  .\make emu
# (PowerShell does not run programs from the current directory as `make`.)
$ErrorActionPreference = "Stop"
& "$PSScriptRoot\scripts\make.ps1" @args
exit $LASTEXITCODE
