# Tank Duel (ESP32-P4 artillery)

Native **C++** artillery duel for the cheap ESP32-P4 handheld in `esp32_p4_handheld_console_plan.md`. Same development style as the photo-frame monorepo: ESP-IDF firmware, a **native authoritative server**, thin clients that only send commands, shared protocol headers, and agent-friendly `make` + serial JSON.

This is **game binary #1** for that console: 1v1, angle + power, wind, destructible heightmap, dirty-rectangle RGB565 at **480×320**.

## Layout

| Path | Role |
|------|------|
| [`game/`](game/) | Pure C++ sim (world, physics, match, renderer, bot). No ESP-IDF, no sockets. |
| [`server/`](server/) | `Authority` (programmatic server) + UDP `artillery-server` |
| [`host/`](host/) | `artillery-host` via `ServerProxy` (local or remote), `artillery-emu` (SDL), `artillery-cli`, `artillery-sim` |
| [`firmware/`](firmware/) | ESP-IDF 5.5 game for Waveshare ESP32-P4 + ILI9488 POC (Wi-Fi PvP client, local `Match` fallback) |
| [`shared/protocol/`](shared/protocol/) | Wire constants + UDP link/codec (JSON kept for serial) |
| [`shared/net/`](shared/net/) | UDP/TCP helpers (POSIX + Winsock) |
| [`scripts/`](scripts/) | Windows `make.ps1`, `devctl.py` |

## Quick start (Windows)

Host play and tests use the **local MSVC toolchain**. Firmware uses native ESP-IDF.

```powershell
.\make deps
.\make test
.\make play
.\make emu
.\make emu-pvp
```

PowerShell will not run `make` from the current folder; the `.\` is required. In `cmd.exe`, `make emu` works.

Vs-bot runs in-process (no socket). To play against a separate server:

```powershell
.\make server
.\make play-remote
```

Send a command against a running server:

```powershell
.\make cli CMD=status
.\make cli CMD="start --seed 42 --mode bot"
.\make cli CMD=fire
```

Two handhelds on the same LAN as this PC:

```powershell
.\make server
.\make flash PORT=COM5
.\make flash PORT=COM6
```

SSID / server IP come from `.local.env` (`WIFI_SSID`, `SERVER_HOST`, port 7420). Online title shows **CONNECTING**, then **WAITING FOR OPPONENT**, then **STARTING GAME** (match starts on its own). P0 is orange (left).

Flash over Type-C UART. Pins: [`docs/hardware.md`](docs/hardware.md).

```powershell
.\make identify
.\make flash PORT=COM7
.\make cmd CMD=status PORT=COM7
.\make snap
```

## Controls

| Button | Aiming | Title |
|--------|--------|-------|
| LEFT / RIGHT | barrel | toggle mode |
| UP / DOWN | power | toggle mode |
| A | fire | start (offline vs bot / hotseat; online auto-starts) |
| B | — | toggle mode |
| A+B (hardware) | leave game / launcher (console plan) | |

Host keys: arrows or WASD, Space/Z = A, X = B, Q = quit. `make emu` opens a 2× 480×320 window with the same mapping (Esc also quits).

## Serial commands

Newline-terminated. Result line:

```text
<<< {"cmd":"status","ok":true,...}
```

`status` · `version` · `state` · `new [seed]` · `angle <n>` · `power <n>` · `fire` · `tick [frames]` · `snap`

## Docs

| Doc | Description |
|-----|-------------|
| [Architecture](docs/architecture.md) | Game loop, dirty rects, server-authoritative shots |
| [60 fps present](docs/fps.md) | Dirty rects, 40 MHz SPI, ping-pong DMA, 80 MHz PSRAM. Live match is 70% wall time; present stays 16 ms. |
| [Game stack](docs/game-stack.md) | Why not SDL/LVGL on the P4; desktop `artillery-emu` is the SDL blit |
| [Windows native](docs/windows-native.md) | `make`, COM ports |
| [Agent tooling](docs/agent-tooling.md) | Serial loop |
| [Hardware](docs/hardware.md) | POC ILI9488 pinout + UART flash |
| [TASKS.md](TASKS.md) | Phased backlog toward matchmaking |
