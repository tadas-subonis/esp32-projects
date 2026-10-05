# Game stack decision

Reviewed against *Choosing a C++ Game Stack for ESP32-P4* (SDL3 + thin C++ layer, `georgik/sdl` on device, desktop SDL3 for iteration). **Keep the current stack.** Do not put SDL on the P4.

## Current stack

```text
ESP-IDF 5.5 + C++17
  game/          portable Match / World / physics / RGB565 compose
  firmware/      ILI9488 SPI HAL (POC), GPIO buttons, dirty-rect flush
  host/          terminal player + CLI + SDL emu
  server/        authoritative UDP sim (same game/)
```

No ECS, Box2D, Lua, or scene graph. `DeviceHal` reads buttons and flushes rects; it does not own turns.

## Why not SDL on the device

The write-up is a greenfield recipe for RGB/MIPI panels and a generic Espressif BSP. This prototype is a Waveshare P4 plus an external **SPI ILI9488 at 480×320** (40 MHz). A full RGB565 frame over SPI is still too slow for 30 fps, so dirty rectangles are the present path, not a later optimization. Details: [fps.md](fps.md).

| Write-up | This project |
|----------|----------------|
| Logical 320×240, scale with PPA | Native 480×320; SPI still ships 480×320 |
| Full frame every tick; dirty rects if FPS misses | Dirty rects first; full blit is the fallback |
| `georgik/sdl` + custom BSP adapter | ILI9488 HAL already exists; BSP would be rewritten anyway |
| 1-bit / 8-bit pixel terrain mask | Heightmap (Scorched Earth); craters lower columns |
| Bomberman as first vertical slice | This repo is artillery (console game #1) |

`georgik/sdl` is a community video/events layer, not full desktop SDL. Audio and gamepads are unverified there. Firmware audio stays on the board codec / I2S when Phase 2 lands (`TASKS.md`).

## What already matches the write-up

- Gameplay in `game/` with no ESP-IDF and no SDL.
- Thin HAL; semantic buttons at the `Match` boundary.
- Fixed-step projectiles + cratering, not a physics engine.
- C++17, no exceptions in `game/`, shared with firmware.

## Desktop emulator

`artillery-emu` (`make emu`) blits the existing 480×320 RGB565 buffer into an SDL2 window. It MUST NOT replace `make test` / `artillery-sim`, and it MUST NOT leak into firmware.

If a second present or audio backend is needed, extract a small adapter interface. Do not introduce an `/engine` rewrite.

## Rejected for this game

- **LVGL** as the gameplay renderer (UI object model; framebuffer ownership fights dirty rects). Fine later for a launcher, with a mode switch.
- **PAX / LovyanGFX / TFT_eSPI** — extra glue; `esp_lcd` is the IDF-idiomatic upgrade path if the custom ILI9488 driver is replaced, still without SDL.
- **C++26** — stay C++17 so host, server, and firmware share `game/`.
- **Pixel-mask terrain** — needed for Worms-style tunnels, not for this heightmap duel.
