# Architecture: CoreS3 → LLM → M5Paper Color

**Status:** Planning reference (synthesized for this repo, not an official M5Stack doc).

## Devices and roles

| Device | MCU / SoC | Role |
|--------|-----------|------|
| CoreS3 | ESP32-S3 | Camera capture, touch UI, Wi-Fi hub, UART host for LLM module |
| Module LLM Kit | AX630C (Linux) | Offline VLM description, LLM, ASR, TTS |
| M5Paper Color | ESP32-S3 | 600×400 Spectra-6 display, optional caption audio |

## Data flow

```
User (double-tap CoreS3 screen)
  → GC0308 frame buffer (QVGA RGB565)
  → frame2jpg() → JPEG (~15–40 KB at quality 30–50)
  → UART → Module LLM VLM → caption text
  → Wi-Fi HTTP/MQTT → M5Paper Color
  → dither to 6-ink palette → full e-ink refresh (~15–20 s)
  → optional TTS on PaperColor speaker
```

**Keep JPEG off UART for PaperColor transfer** — UART is for LLM only. Use Wi-Fi between CoreS3 and PaperColor.

## Physical connections

### CoreS3 + Module LLM (stacked)

- Module LLM stacks on CoreS3 via **M5-Bus**
- Serial to LLM: **Port C** — `G17` (TX), `G18` (RX), **115200 8N1**
- Use `M5ModuleLLM` library with `Serial2`

### M5Paper Color (standalone)

- Separate unit on same Wi-Fi network as CoreS3
- No direct cable to CoreS3 required for v1

## Image constraints

| Stage | Constraint |
|-------|------------|
| Camera | GC0308 0.3 MP; practical capture **QVGA 320×240** |
| LLM VLM | Expect JPEG input; inference takes several seconds |
| PaperColor | **6 colors only** (Spectra-6); requires dithering/quantization |
| E-ink | **Full refresh only**, ~15–20 s; no touch input |

## Suggested phased build

1. **Bring-up** — Factory or example firmware on each board; VLM demo on CoreS3+LLM
2. **Wi-Fi path** — CoreS3 HTTP POST of JPEG; PaperColor downloads and displays
3. **Color + caption** — Spectra-6 dithering; render caption under image
4. **Audio** — TTS from LLM module or caption playback on PaperColor ES8311

## Firmware layout

Two binaries (one per ESP32-S3 board):

```
m5-photo-frame/          # future project (not in this repo yet)
├── src/cores3/          # camera + LLM UART + HTTP client
└── src/papercolor/      # HTTP server + dither + e-ink + audio
```

Shared wire protocol: JSON with `{ version, caption, jpeg_len, jpeg_bytes }` or multipart HTTP.

## Key official examples

- Camera preview: [guides/cores3-camera.md](./guides/cores3-camera.md)
- VLM inference: [stackflow/vlm-cores3-example.md](./stackflow/vlm-cores3-example.md)
- PaperColor display: [guides/papercolor-arduino-setup.md](./guides/papercolor-arduino-setup.md)
