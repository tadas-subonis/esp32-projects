# esp32-photo-frame-camera-monorepo

Two firmware projects for a **CoreS3 camera + M5Paper Color e-ink viewer** system (optional **Module LLM** for captions/TTS).

| Project | Device | Stack | Role |
|---------|--------|-------|------|
| [`cores3/`](cores3/) | M5Stack CoreS3 | PlatformIO + Arduino | Capture photo, optional VLM caption, push to PaperColor |
| [`papercolor/`](papercolor/) | ESP-IDF 5.5 + M5 stack | M5Paper Color (C151) | Receive JPEG + caption, Spectra-6 display, optional audio |

Shared wire format: [`shared/protocol/`](shared/protocol/).

## Windows (recommended for USB devices)

Develop and flash **natively on Windows** — boards appear as `COM*` ports, no WSL or usbipd.

**Guides:** [Windows native dev](docs/windows-native.md) · [Agent tooling](docs/agent-tooling.md)

```powershell
make deps
make help
make build
make identify
make flash-papercolor PORT=COM8
make flash-cores3     PORT=COM7
```

`make` is implemented by `make.cmd` → `scripts/make.ps1` (same targets as the Linux Makefile).

## WSL2 (alternative)

Code and builds can run in **WSL** with USB forwarded via **usbipd-win**.

**Guide:** [docs/wsl-usb-flash.md](docs/wsl-usb-flash.md)

## Quick start

**Windows:**

```powershell
make deps
make build
```

**WSL / Linux:**

```bash
make deps
make build
```

Per-target builds: `make build-cores3` · `make build-papercolor`

Wi‑Fi defaults: [`shared/protocol/photo_frame_wifi.h`](shared/protocol/photo_frame_wifi.h). Copy secrets if needed:

```bash
cp cores3/include/secrets.h.example cores3/include/secrets.h
```

## Flash and test

1. Insert SD card in PaperColor; power both boards via USB.
2. Find ports: `make identify` (match `serial_number`, not COM/tty index).
3. Flash both (example — adjust ports):

   ```powershell
   make flash-papercolor PORT=COM8
   make flash-cores3     PORT=COM7
   ```

   WSL: use `/dev/ttyACM0` / `/dev/ttyACM1` and [usbipd attach](docs/wsl-usb-flash.md) first.

4. Optional: join Wi‑Fi `PhotoFrame` / password `frame1234`, then `make verify-photo-post`.
5. Tap CoreS3 screen or `make capture CORES3_PORT=COM7` → photo on e‑ink (~15–20 s).

## Make targets

Run `make help` for the full list. Common targets work on **Windows and WSL** (port syntax differs).

| Target | Description |
|--------|-------------|
| `make deps` | Toolchain + vendor clones |
| `make build` | Build both firmwares |
| `make ports` / `make identify` | Serial discovery (JSON) |
| `make flash-papercolor` | Build and flash PaperColor |
| `make flash-cores3` | Build and upload CoreS3 |
| `make cmd-cores3` / `make cmd-papercolor` | Send device command (`CMD=…`) |
| `make logs-cores3` / `make logs-papercolor` | Capture serial logs |
| `make capture` | CoreS3: capture + POST photo |
| `make monitor-*` | Interactive serial monitor |
| `make verify-photo-post` | HTTP POST test image |
| `make clean` | Clean build artifacts |

**WSL-only:** `make usb-status`, `make usb-help`, `make usb-attach`

Port override examples:

```powershell
make flash-cores3 PORT=COM7
```

```bash
make flash-cores3 PORT=/dev/ttyACM1
```

## Docs

| Doc | Description |
|-----|-------------|
| [Windows native dev](docs/windows-native.md) | **Native Windows: `make`, COM ports, agent loop** |
| [Agent tooling](docs/agent-tooling.md) | Serial commands, `devctl`, troubleshooting |
| [WSL USB flash](docs/wsl-usb-flash.md) | WSL2 + usbipd alternative |
| [Architecture](docs/architecture.md) | Monorepo system design |
| [TASKS.md](TASKS.md) | Phased backlog |
| [M5 hardware docs](docs/m5stack/README.md) | Pin maps, Arduino/StackFlow guides |
| [Vendor reference](vendor/README.md) | Factory firmware & driver clones |
| [HTTP protocol](shared/protocol/README.md) | CoreS3 ↔ PaperColor API v1 |

## Vendor reference (factory firmware & drivers)

Offline clones for reading and porting — see [`vendor/README.md`](vendor/README.md).

| Path | Contents |
|------|----------|
| `vendor/cores3/CoreS3-UserDemo/` | Factory LVGL launcher (PlatformIO) |
| `vendor/cores3/M5CoreS3/` | Board library + camera examples |
| `vendor/cores3/M5Module-LLM/` | LLM UART API + VLM examples |
| `vendor/papercolor/M5PaperColor-UserDemo/` | Factory ESP-IDF firmware |
| `vendor/papercolor/M5PM1/` | PaperColor PMIC driver |
| `vendor/common/M5Unified/`, `M5GFX/` | Shared M5 drivers |

Refresh: `git -C vendor/cores3/CoreS3-UserDemo pull` (etc.).

## Hardware

- **CoreS3** + optional **Module LLM** stacked (UART Port C @ 115200)
- **M5Paper Color** — softAP `PhotoFrame` @ `192.168.4.1`; CoreS3 joins as `192.168.4.2`
- No cable between CoreS3 and PaperColor (Wi‑Fi only for photos)
