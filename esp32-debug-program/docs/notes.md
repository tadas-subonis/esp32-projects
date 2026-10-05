# Bring-up notes (2026-09-15)

Working: 3.5" SPI **ILI9488** 480×320 on Waveshare **ESP32-P4-WIFI6-POE-ETH Rev 2.0**.

## Header

All LCD **control** wires go on the **right** column of the 40-pin GPIO header (the side with **5V** at the top). Do not use the left column (GPIO23 / 21 / 20 / …) for CS, RESET, DC, MOSI, or SCK. Skip **TXD/GPIO37** and **RXD/GPIO38** — those are the Type-C UART.

| TFT | Board |
|---|---|
| VCC | 5 V (right column, top) |
| GND | any GND |
| LED | 3.3 V (left column 3V3; backlight is not a GPIO) |
| CS | GPIO22 |
| RESET | GPIO5 |
| DC/RS | GPIO4 |
| MOSI | GPIO36 |
| SCK | GPIO32 |
| MISO / touch / TF | not connected |

GPIO36 is a strapping pin; it is fine as MOSI after boot.

## Firmware that works

- ILI9488 **18-bit RGB666** (`COLMOD 0x66`), `MADCTL 0x28` (MV\|BGR landscape), SPI **60 MHz** everyday (eye-STABLE OC on this POC; datasheet max write 20 MHz — Stage 0 bring-up used 10 MHz)
- First pixel row: RAMWR `0x2C`. Later rows: RAMWRC `0x3C`
- `INVOFF`, no controller reset between color fills
- `TEST_MODE 1` in `firmware/main/main.c`

Repeating `0x2C` on every row rewinds the write pointer to the window origin. Symptom: one thin line at the top (last color) plus a scrap of the 40×40 yellow marker. Window commands `0x2A`/`0x2B` of `0–479` / `0–319` were already correct.

## Dead ends that looked like “the screen is broken”

- White + backlight = **power/LED OK**, controller idle. Serial `OK` only means the P4 transmitted (MISO is not wired).
- Flash-size warning `Detected size(32768k) larger than … (16384k)` is unrelated.
- Fast GPIO wiggle (~200 ms) is too quick for a cheap DMM. Use `TEST_MODE 0` (2 s HIGH/LOW) and meter the **TFT pin**, not the P4 header.
- Looping ST7796 then ILI9488 with a hardware reset between them flashes the panel **white** even when ILI9488 is working.
- `INVON` made every fill look blueish.
- If the module has **SD_CS**, tie it to 3.3 V; floating SD_CS can steal SPI.

`TEST_MODE 0` = pin walk. `TEST_MODE 1` = LCD color bars (tag `hw-lcd-working`). `TEST_MODE 2` = read `0x04` / `0xD3` with TFT SDO on GPIO1. `TEST_MODE 3` = eight-button smoketest. `TEST_MODE 4` = P4Bench ([how-to](p4bench.md), [findings](p4bench-findings.md)).

## Performance (P4Bench, 2026-09-20)

Full-frame SPI RGB666 @ 10 MHz is ~**2.7 FPS** (~1.18 MB/s, 460800 wire bytes/frame). LCD is ~90%+ of frame time; CPU sim is negligible. Games on this panel need dirty/partial transfers — see [`p4bench-findings.md`](p4bench-findings.md).

## Buttons (Stage 1)

Left / inner header column, switch to GND, internal pull-up, pressed = LOW:

| Button | GPIO |
|---|---|
| UP | 20 |
| DOWN | 6 |
| LEFT | 3 |
| RIGHT | 2 |
| A | 33 |
| B | 26 |
| SELECT | 48 |
| START | 47 |
