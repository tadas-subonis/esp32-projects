#!/usr/bin/env bash
# WSL/Linux counterpart of scripts/deps.ps1
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

IDF_PATH="${IDF_PATH:-$HOME/esp/esp-idf}"
IDF_VERSION="${IDF_VERSION:-v5.5.1}"

log() { echo "==> $*"; }

if [[ ! -f "$IDF_PATH/export.sh" ]]; then
  log "cloning ESP-IDF $IDF_VERSION to $IDF_PATH"
  mkdir -p "$(dirname "$IDF_PATH")"
  git clone -b "$IDF_VERSION" --depth 1 --recursive https://github.com/espressif/esp-idf.git "$IDF_PATH"
else
  log "ESP-IDF already at $IDF_PATH"
fi

log "ensuring ESP-IDF deps for esp32p4"
( cd "$IDF_PATH" && ./install.sh esp32p4 )

python3 -m pip install --user -U -r "$ROOT/scripts/requirements-tools.txt"

# shellcheck disable=SC1091
source "$IDF_PATH/export.sh"
cd "$ROOT/firmware"
if [[ ! -f sdkconfig ]]; then
  idf.py set-target esp32p4
else
  idf.py reconfigure
fi
log "done — try: make build"
