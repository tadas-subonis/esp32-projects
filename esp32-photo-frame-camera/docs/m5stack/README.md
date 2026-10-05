# M5Stack device references

Hardware and software documentation for the **CoreS3 + Module LLM + M5Paper Color** photo-frame project.

> **Scope:** Consult these pages before changing pin maps, buses, power sequencing, camera init, e-ink refresh, or LLM UART wiring. Official M5Stack docs are mirrored here as markdown for offline use.

## Project goal

**CoreS3** captures a photo → **Module LLM** optionally describes it (VLM) or speaks it (TTS) → **M5Paper Color** displays the image on its 600×400 Spectra-6 e-ink panel.

See [monorepo architecture](../architecture.md) and [vendor factory source](../../vendor/README.md).

## Firmware in this repo

| Project | Path |
|---------|------|
| CoreS3 sender | [`cores3/`](../../cores3/) |
| PaperColor receiver | [`papercolor/`](../../papercolor/) |
| Wire protocol | [`shared/protocol/`](../../shared/protocol/) |

## Hardware (product pages)

| Board | SKU | Doc |
|-------|-----|-----|
| M5Paper Color | C151 | [m5paper-color.md](./m5paper-color.md) |
| CoreS3 | K128 | [cores3.md](./cores3.md) |
| Module LLM Kit | K144 | [module-llm-kit.md](./module-llm-kit.md) |

## Arduino bring-up guides

| Topic | Doc |
|-------|-----|
| PaperColor — compile & flash | [guides/papercolor-arduino-setup.md](./guides/papercolor-arduino-setup.md) |
| PaperColor — M5PM1 power management | [guides/papercolor-m5pm1-power.md](./guides/papercolor-m5pm1-power.md) |
| CoreS3 — compile & flash | [guides/cores3-arduino-setup.md](./guides/cores3-arduino-setup.md) |
| CoreS3 — GC0308 camera | [guides/cores3-camera.md](./guides/cores3-camera.md) |

## StackFlow / Module LLM

| Topic | Doc |
|-------|-----|
| Arduino quick start | [stackflow/module-llm-arduino-quickstart.md](./stackflow/module-llm-arduino-quickstart.md) |
| ADB / UART / SSH access | [stackflow/module-llm-adb-uart-ssh.md](./stackflow/module-llm-adb-uart-ssh.md) |
| Software update (`apt`) | [stackflow/module-llm-software-update.md](./stackflow/module-llm-software-update.md) |
| Full image / firmware flash | [stackflow/module-llm-image-firmware-update.md](./stackflow/module-llm-image-firmware-update.md) |
| VLM + CoreS3 camera example | [stackflow/vlm-cores3-example.md](./stackflow/vlm-cores3-example.md) |

## Community supplements

| Topic | Doc |
|-------|-----|
| PaperColor ESPHome hardware notes (pins, Spectra-6 caveats) | [community/papercolor-esphome-hardware-notes.md](./community/papercolor-esphome-hardware-notes.md) |

## Official links (live)

- [M5Paper Color](https://docs.m5stack.com/en/core/PaperColor)
- [CoreS3](https://docs.m5stack.com/en/core/CoreS3)
- [Module LLM Kit](https://docs.m5stack.com/en/module/Module%20LLM%20Kit)
- [StackFlow docs index](https://docs.m5stack.com/en/stackflow/module_llm/arduino)

## Development stack note

Firmware lives in **`cores3/`** (PlatformIO + Arduino) and **`papercolor/`** (ESP-IDF). Vendor trees under **`vendor/`** hold factory reference source.
