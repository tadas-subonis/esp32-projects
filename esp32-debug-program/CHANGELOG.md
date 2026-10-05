# Changelog

## Unreleased

- P4Bench full milestone capture on hardware (dirty crossover, sprites, sim, TD, chaos) — `docs/p4bench-findings.md`, `logs/p4bench-full.txt`
- Documented SPI prototype findings (full-frame ~2.7 FPS @ 10 MHz RGB666) — `docs/p4bench-findings.md`
- P4Bench (`TEST_MODE 4`): instrumented LCD/CPU benchmarks with serial console — `docs/p4bench.md`
- Stage 2 spec: local artillery HAL port for the proven ILI9488 + eight-button POC (`docs/stage2-local-artillery.md`)

## 0.5.0 — 2026-09-17

- Stage 1: eight-button smoketest (UP20 DOWN6 LEFT3 RIGHT2 A33 B26 SELECT48 START47)
- Persist handheld plan in `docs/console-plan.md`; tag Stage 0 as `hw-lcd-working`

## 0.4.0 — 2026-09-15

- ILI9488 color test confirmed working (full frames + corner squares)
- Fix RAMWR: `0x2C` only on the first row of a window, `0x3C` after that
- Document right-column header wiring and bring-up notes

## 0.3.1 — 2026-09-15

- White-screen debug: GPIO pin walk, ST7796 vs ILI9488 inits, optional ID read

## 0.3.0 — 2026-09-15

- Retarget to ILI9488 SPI RGB666 on CS22 RST5 DC4 MOSI36 SCK32
- Add explicit RESET pulse and step logs

## 0.2.0 — 2026-09-15

- Strip firmware down to a single `main.c` LCD wiring test (no buttons, audio, SD, or console UI)

## 0.1.1 — 2026-09-15

- Document this firmware as Waveshare ESP32-P4-WIFI6-POE-ETH only (Type-C UART, SPI LCD on the 40-pin header)

## 0.1.0 — 2026-09-15

- Initial P4 debug firmware
