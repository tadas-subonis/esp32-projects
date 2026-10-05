# AGENTS.md

## Project context

> Loads on every agent invocation. Keep short.

- **Stack / versions** — Monorepo: `game/` = portable C++17 sim; `server/` = `Authority` (in-process) + UDP `artillery-server`; `host/` = proxy client (local or remote) + SDL emu + CLI; `firmware/` = ESP-IDF v5.5 (esp32p4); wire constants in `shared/protocol/`.
- **Host OS** — **Windows native (default)** for USB: `COM*` ports, `.\make` → `scripts/make.ps1`. Host C++ uses **MSVC + CMake/Ninja**. Firmware via `%USERPROFILE%\esp\esp-idf\export.ps1`.
- **Package manager** — CMake for host; ESP-IDF for firmware; pip `pyserial` for `devctl.py`.
- **Commands**
  - **Windows (PowerShell):** `.\make deps` · `.\make test` · `.\make play` · `.\make emu` · `.\make emu-pvp` · `.\make emu-play` · `.\make emu-dump` · `.\make play-remote` · `.\make server` · `.\make cli CMD=status` · `.\make build-firmware` · `.\make flash` · `.\make flash-monitor` · `.\make ports` · `.\make identify` · `.\make cmd CMD=status` · `.\make snap` · `.\make logs SECONDS=8` · `.\make help`
  - **WSL / Linux:** same target names via `Makefile` (`PORT=/dev/ttyACM0`)
- **Non-obvious patterns**
  - Gameplay rules live in `game/` and MUST stay free of ESP-IDF and host I/O.
  - Serial contract: command + logs + one `<<< {json}` result line (`scripts/devctl.py`). `status` includes fps/compose/flush ms; `snap` dumps a 120×80 PNG via UART.
  - Display is 480×320; compose RGB565, flush as ILI9488 RGB666 on the POC board (see `docs/hardware.md`). Dirty-rectangle SPI at 40 MHz; ~60 fps idle. How: `docs/fps.md`. Live `Match` runs at 70% of wall time so shots feel slower; present stays 16 ms.
  - **PC play is server-authoritative.** `Authority` owns `Match`. `ServerProxy` is local (same process, vs-bot) or remote (UDP v2). Clients send intent only. Firmware joins the same UDP server over Wi-Fi when `.local.env` has `WIFI_SSID` / `SERVER_HOST`; otherwise it keeps a local `Match`. Reliable messages repeat until acked (no TCP RTO).
  - `make test` / `make play` / `make emu` use local MSVC; they do not require WSL.

## Recommended inner dev loop

Prefer non-interactive commands + log capture. **Do not** run `make monitor` on the same port as `make cmd` / `make snap` / `make logs`.

```powershell
.\make test
.\make play
.\make emu
.\make play-remote
.\make server
.\make cli CMD=status
.\make build-firmware
.\make flash
.\make flash-monitor
.\make cmd CMD=status
.\make cmd CMD="new 42"
.\make cmd CMD="angle 45"
.\make cmd CMD="fire"
.\make snap
```

## Agent work loop

1. **Plan** — Explore, then agree on an approach before large edits.
2. **Outline** — Types / functions that will change.
3. **Implement** — Stay in scope; do not refactor unrelated code.
4. **Verify** — `make test`; firmware `idf.py build` when touching `firmware/`.
5. **Document** — Keep `README.md` / `docs/` matched to behavior.
6. **Changelog** — Feature-level notes in `CHANGELOG.md`.

Whenever working, consult:

- `CODE_GUIDELINES.md` — layering, parse-don't-validate, review habits
- `TESTING.md` — C++ core tests + sim CLI + TCP server tests
- `docs/agent-tooling.md` — serial commands + `artillery-cli`
- `docs/windows-native.md` — Windows `make`

## What goes where

| Topic | File |
|-------|------|
| Domain rules, physics, match | `game/` |
| Authoritative sim (`Authority`) | `server/` (`artillery-server` UDP adapter) |
| Host client / CLI / physics CLI / SDL emu | `host/` |
| ESP32-P4 HAL, LCD, buttons, console | `firmware/` |
| Wire constants + UDP link/codec (+ JSON for serial) | `shared/protocol/` |
| UDP/TCP helpers (POSIX + Winsock) | `shared/net/` |
| Serial helper | `scripts/devctl.py` |
| Feature history | `CHANGELOG.md` |

## Guardrails

- **Always** — MUST rules in guideline files; run `make test` before claiming the sim is done.
- **Ask first** — Destructive git; installing system-wide packages.
- **Never** — Put match rules in `firmware/` HAL; let a client report “I hit / I won”; invent a second physics implementation for the server without a test that locks them together.

## Coding core

Simple linear flows; composition over inheritance; match phase as an explicit state machine. Details: `CODE_GUIDELINES.md`.
