# AGENTS.md

Ultra-cheap multiplayer handheld. Full plan: `docs/console-plan.md`.

**POC board:** Waveshare ESP32-P4-WIFI6-POE-ETH Rev 2.0 (P4 + C6). Flash **Type-C**.
**Display:** 3.5" SPI ILI9488 480×320 RGB666 — Stage 0 **done** (`hw-lcd-working`).
**Now:** Stage 2 local 1v1 tank artillery (game repo HAL port). Spec: `docs/stage2-local-artillery.md`.
**Perf:** P4Bench (`TEST_MODE 4`) — `docs/p4bench.md`. Measured SPI limits: `docs/p4bench-findings.md`.

- **Stack** — ESP-IDF v5.5.1, `esp32p4`, app in `firmware/main/main.c` (+ `p4bench/`)
- **Windows** — `.\make.ps1` (not Git `make.exe`): `deps` · `build` · `flash` · `monitor`
- Pins / `TEST_MODE` at the top of `main.c`. `4` = P4Bench, `3` = buttons, `1` = LCD color bars
- LCD (right header column): CS22 RST5 DC4 MOSI36 SCK32, VCC 5V, LED 3.3V
- SPI everyday clock: **60 MHz** (eye-STABLE OC on this unit; datasheet max write 20 MHz). See `docs/p4bench-findings.md` §12
- Buttons (left header, to GND, pull-up): UP20 DOWN6 LEFT3 RIGHT2 A33 B26 SELECT48 START47
- RAMWR `0x2C` once per window, then `0x3C` for extra rows
- Do not start battery / launcher / matchmaking / 2nd SD before local artillery is playable
