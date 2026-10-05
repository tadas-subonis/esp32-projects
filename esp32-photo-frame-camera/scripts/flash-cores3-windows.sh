#!/usr/bin/env bash
# Build in WSL, flash CoreS3 via Windows COM (bypasses WSL usbipd esptool issues).
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
COM="${CORES3_COM:-COM9}"
BUSID="${CORES3_BUSID:-4-4}"
TRIES="${TRIES:-30}"

cd "$REPO"
make build-cores3

PS=/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe
exec "$PS" -NoProfile -ExecutionPolicy Bypass -File "$REPO/scripts/flash-cores3-windows.ps1" \
  -ComPort "$COM" -BusId "$BUSID" -Tries "$TRIES"
