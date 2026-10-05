# Task backlog

Aligned with console plan milestones 4–6 (first game, bot fallback, rating). Check items off as completed.

## Phase 0 — Repo & playable core (current)

- [x] Monorepo layout (`game/`, `host/`, `firmware/`, `shared/`, `docs/`)
- [x] Portable C++ sim: heightmap, ballistics, craters, match phases, bot
- [x] Dirty-rectangle RGB565 renderer (480×320)
- [x] Host `artillery-sim` + TCP `artillery-server` / `artillery-host` / `artillery-cli`
- [x] Desktop SDL emulator (`artillery-emu`) for the 480×320 RGB565 compose
- [x] ESP-IDF P4 firmware scaffold: ST7796 HAL, six buttons, serial JSON console
- [x] `make` / `devctl.py` agent loop
- [x] `make test` green on this machine
- [x] `idf.py build` for esp32p4

**Exit:** host tests pass; firmware compiles.

## Phase 1 — Board bring-up (POC is ILI9488, not ST7796)

Hardware map: [`docs/hardware.md`](docs/hardware.md).

- [x] Port `device_hal.hpp` to POC pins (LCD CS22 RST5 DC4 MOSI36 SCK32; buttons UP20 DOWN6 LEFT3 RIGHT2 A33 B26)
- [x] Replace ST7796 RGB565/`0x2C`-per-row flush with ILI9488 RGB666 + `0x2C` then `0x3C`
- [x] Type-C UART console (not USB Serial/JTAG); enable PSRAM
- [x] Title screen visible; `make cmd CMD=status` / `new 42` / `fire` on the flash COM port
- [ ] Vs-bot + hotseat match on device; tag `game-artillery-local`
- [x] Dirty-rect present path; ILI9488 SPI at 40 MHz (full blit only on map/title changes)

## Phase 2 — Local polish

- [x] Title glyphs as real text, not block bars
- [ ] Explosion particles / screen shake within dirty rects
- [ ] Audio: shoot + explosion (board codec, milestone 9)
- [ ] A+B hold to quit-to-launcher stub

## Phase 3 — Central server (console plan §18–21)

- [x] Native C++ `artillery-server`: JSON-line intents, `Match::apply` validation, server-side `simulate_shot` / bot
- [ ] Device identity + matchmaking queue
- [x] Handheld as a TCP client of the same protocol (Wi-Fi STA → `artillery-server` pvp)
- [x] Snapshot + command-log resync after disconnect
- [ ] Rematch + TrueSkill rating

## Phase 4 — Game card

- [ ] `game.bin` + `game.toml` install from SD (launcher, milestone 7)
