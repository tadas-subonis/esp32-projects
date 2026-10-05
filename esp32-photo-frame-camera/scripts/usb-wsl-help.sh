#!/usr/bin/env bash
# Reminder: USB attach is done on Windows (usbipd-win), not inside WSL.
set -euo pipefail

cat <<'EOF'
USB passthrough: Windows PowerShell (see docs/wsl-usb-flash.md)

  1. Admin:  usbipd list
  2. Admin:  usbipd bind --busid <BUSID>
  3.         usbipd attach --wsl --busid <BUSID>
  4. WSL:    make usb-status
  5. WSL:    make flash-papercolor PORT=/dev/ttyACM0
             make flash-cores3     PORT=/dev/ttyACM1
  6. Done:   usbipd detach --busid <BUSID>

Install (once on Windows):
  winget install --interactive --exact dorssel.usbipd-win

Microsoft docs:
  https://learn.microsoft.com/en-us/windows/wsl/connect-usb
EOF
