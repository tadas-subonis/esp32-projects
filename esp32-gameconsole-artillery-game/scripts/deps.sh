#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

python3 -m pip install --user -U -r scripts/requirements-tools.txt

IDF_PATH="${IDF_PATH:-$HOME/esp/esp-idf}"
if [[ -f "$IDF_PATH/install.sh" ]]; then
  echo "==> ensuring ESP-IDF tools for esp32p4"
  (cd "$IDF_PATH" && ./install.sh esp32p4)
else
  echo "ESP-IDF not found at $IDF_PATH (skip firmware toolchain)"
fi
