#!/usr/bin/env bash
# POST golden JPEG to PaperColor (join PhotoFrame AP first).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
JPEG="${ROOT}/shared/test_assets/colorbars_qvga.jpg"
HOST="${PHOTO_FRAME_HOST:-192.168.4.1}"
URL="http://${HOST}/api/v1/photo"

if [[ ! -f "$JPEG" ]]; then
  echo "Missing test asset: $JPEG" >&2
  exit 1
fi

META='{"version":1,"id":"verify-script","caption":"","width":320,"height":240,"jpeg_quality":40}'

echo "POST $URL"
HTTP_CODE=$(curl -sS -o /tmp/pf_verify_resp.json -w '%{http_code}' \
  -F "meta=${META};type=application/json" \
  -F "image=@${JPEG};type=image/jpeg" \
  "$URL")

echo "HTTP $HTTP_CODE"
cat /tmp/pf_verify_resp.json
echo

if [[ "$HTTP_CODE" != "202" ]]; then
  exit 1
fi
