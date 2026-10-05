# PowerShell entry point. From repo root:
#   .\make.ps1 build
#   .\make.ps1 flash PORT=COM8
$ErrorActionPreference = "Stop"
$env:MAKECMDGOALS = ($args | ForEach-Object { "$_" }) -join " "
& (Join-Path $PSScriptRoot "scripts\make.ps1")
exit $LASTEXITCODE
