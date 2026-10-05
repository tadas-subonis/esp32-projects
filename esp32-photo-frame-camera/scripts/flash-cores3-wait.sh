#!/usr/bin/env bash
# Flash CoreS3 when esptool can connect (user: long-press RST ~3s on CoreS3 for download mode).
set -euo pipefail
PORT="${PORT:-/dev/ttyACM1}"
TRIES="${TRIES:-30}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"

cd "$REPO"
make build-cores3
ESPTOOL="${ESPTOOL:-$HOME/.espressif/python_env/idf5.5_py3.12_env/bin/esptool.py}"
B="$REPO/cores3/.pio/build/M5CoreS3"

flash_once() {
  "$ESPTOOL" --chip esp32s3 -p "$PORT" -b 460800 --before default_reset --after no_reset \
    write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m \
    0x0 "$B/bootloader.bin" 0x8000 "$B/partitions.bin" 0x10000 "$B/firmware.bin"
}

echo "Waiting for CoreS3 on $PORT (long-press RST ~3s if needed)..."
for i in $(seq 1 "$TRIES"); do
  if flash_once 2>/dev/null; then
    echo "Flash OK on try $i"
    bash "$REPO/scripts/cores3-post-flash.sh" "$PORT"
    exit 0
  fi
  echo "  try $i/$TRIES — no connection, retrying in 2s..."
  sleep 2
done

echo "Failed to connect after $TRIES tries." >&2
echo "Put CoreS3 in download mode: long-press RST until green LED, then re-run." >&2
exit 1
