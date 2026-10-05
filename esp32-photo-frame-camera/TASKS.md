# Task backlog

Phased plan for the photo-frame monorepo. Check items off as completed.

**Legend:** `cores3` = camera firmware · `papercolor` = display firmware · `shared` = both

---

## Phase 0 — Repo & toolchain (current)

- [x] Monorepo layout (`cores3/`, `papercolor/`, `shared/`, `docs/`)
- [x] Minimal build scaffolds (hello / boot log)
- [x] Document protocol sketch in `shared/protocol/`
- [ ] `secrets` template + `.gitignore` for Wi-Fi credentials
- [ ] CI sketch (optional): build both targets on push

**Exit criteria:** `pio run -e M5CoreS3` and `idf.py build` succeed on dev machine.

---

## Phase 1 — Per-device bring-up

### cores3

- [ ] M5CoreS3 init: display, touch, Wi-Fi connect from `secrets.h`
- [ ] Camera preview loop (QVGA `pushImage`) — port from `M5CoreS3` camera example
- [ ] Double-tap → `frame2jpg()` → save to SD or hold in RAM
- [ ] Serial log: JPEG size and capture timestamp

### papercolor

- [ ] Wire M5 components (M5Unified, M5GFX, M5PM1) — mirror factory `components/` or `idf_component.yml`
- [ ] E-ink smoke test: color bands (official Arduino demo logic)
- [ ] M5PM1: assert EPD power rail (`PY_EPD_EN`)
- [ ] Wi-Fi STA connect from NVS / provisioning

**Exit criteria:** Camera live on CoreS3 LCD; PaperColor shows a static test image.

---

## Phase 2 — Wi-Fi transfer (no LLM yet)

### shared

- [ ] Freeze protocol v1: `POST /api/v1/photo` multipart or `application/octet-stream` + JSON metadata header
- [ ] Document error codes and max payload (~64 KB)

### cores3

- [ ] HTTP client: POST JPEG to PaperColor IP (configurable)
- [ ] Touch UI: "Send to frame" after capture
- [ ] mDNS optional: resolve `papercolor.local`

### papercolor

- [ ] HTTP server on port 80: `POST /api/v1/photo` handler
- [ ] Decode JPEG → RGB buffer → **Spectra-6 dither** → `pushSprite`
- [ ] Show "Updating…" on serial during 15–20 s refresh

**Exit criteria:** Tap capture on CoreS3 → image appears on PaperColor (even if ugly / undithered at first).

---

## Phase 3 — Spectra-6 quality + caption

### papercolor

- [ ] 6-ink palette quantization (black, white, red, yellow, green, blue)
- [ ] Resize 320×240 → 600×400 (letterbox or crop — pick one)
- [ ] Render caption text under image (built-in font)
- [ ] Button A: replay last image

### cores3

- [ ] Send `caption` field (empty string OK) in metadata JSON

**Exit criteria:** Photo looks intentional on e-ink; caption visible.

---

## Phase 4 — Module LLM (optional)

### cores3

- [ ] Stack Module LLM; `M5ModuleLLM` on `Serial2` (Port C)
- [ ] Install VLM packages on module (`llm-vlm`, `llm-model-internvl2.5-1b-364-ax630c`)
- [ ] After capture: JPEG → VLM → caption string
- [ ] Show caption on CoreS3 LCD while transferring
- [ ] Include caption in HTTP POST to PaperColor

### papercolor (optional)

- [ ] TTS: speak caption via ES8311 speaker (text from POST, or pre-rendered audio later)

**Exit criteria:** Double-tap → described photo on PaperColor end-to-end.

---

## Phase 5 — Polish

- [ ] OTA partition layout (both devices)
- [ ] Low-power: PaperColor deep sleep between updates
- [ ] Config portal on PaperColor (reuse patterns from factory `app_server/`)
- [ ] Rate limiting / queue if CoreS3 sends faster than e-ink refresh
- [ ] End-to-end timing logs (capture → Wi-Fi → refresh complete)

---

## Backlog / ideas

- MQTT instead of HTTP for LAN brokers
- SD card gallery on PaperColor (factory `local_photo_slideshow` reference)
- Rust rewrite on CoreS3 only (`m5stack-core`) — defer until Arduino path works
