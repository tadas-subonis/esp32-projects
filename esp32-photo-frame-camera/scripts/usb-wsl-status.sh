#!/usr/bin/env bash
# Show USB/serial devices visible inside WSL (after usbipd attach on Windows).
set -euo pipefail

echo "== lsusb =="
if command -v lsusb >/dev/null 2>&1; then
  lsusb || true
else
  echo "lsusb not installed (sudo apt install usbutils)"
fi

echo
echo "== Serial devices =="
ls -la /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || echo "(none — attach USB from Windows: make usb-help)"

echo
echo "== Current user groups (need dialout for flash) =="
groups
