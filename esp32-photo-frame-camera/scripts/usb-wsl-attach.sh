#!/usr/bin/env bash
# Re-attach Espressif USB devices to WSL via Windows usbipd-win (callable from WSL).
set -euo pipefail

PS="${WINDIR_PS:-/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe}"
if [[ ! -x "$PS" ]]; then
  echo "usb-wsl-attach: Windows PowerShell not found at $PS" >&2
  echo "Run from Windows: usbipd attach --wsl --busid <BUSID>" >&2
  exit 1
fi

BUSIDS=("$@")
if [[ ${#BUSIDS[@]} -eq 0 ]]; then
  mapfile -t BUSIDS < <(
    "$PS" -NoProfile -Command "
      usbipd list | ForEach-Object {
        if (\$_ -match '^(\\S+)\\s+303a:1001\\s+.*\\s+(Shared|Not shared)\$') { \$matches[1] }
      }
    " | tr -d '\r' | sed '/^$/d'
  )
fi

if [[ ${#BUSIDS[@]} -eq 0 ]]; then
  echo "usb-wsl-attach: no detached Espressif (303a:1001) devices found (all Attached?)"
  "$PS" -NoProfile -Command "usbipd list" | tr -d '\r'
  exit 0
fi

for busid in "${BUSIDS[@]}"; do
  echo "== attach $busid =="
  out=$("$PS" -NoProfile -Command "usbipd attach --wsl --busid $busid 2>&1" | tr -d '\r') || true
  echo "$out"
  if echo "$out" | grep -qi "not shared"; then
    echo "  -> run in **Admin** PowerShell: usbipd bind --busid $busid" >&2
  fi
done

sleep 1
bash "$(dirname "$0")/usb-wsl-status.sh"
