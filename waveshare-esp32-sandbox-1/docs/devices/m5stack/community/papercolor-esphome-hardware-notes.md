# PaperColor — Community Hardware Notes (ESPHome)

**Source:** https://github.com/PFalko/m5stack-papercolor-esphome (README)  
**Retrieved:** 2026-06-22  
**Note:** Community reverse-engineering supplement — not official M5Stack documentation. Verify on your hardware.

---

## Why this doc exists

M5Paper Color requires **M5PM1 PMIC** control to power the e-ink rail and survive battery operation. These notes complement [official M5PM1 docs](../guides/papercolor-m5pm1-power.md) with register-level details from open-source M5GFX/M5PM1 analysis.

## Confirmed hardware summary

| Item | Value |
|------|-------|
| MCU | ESP32-S3R8, 16 MB flash, 8 MB OPI PSRAM |
| Display | 4″ Spectra-6, 400×600 portrait (600×400 landscape, `rotation: 90`) |
| Refresh | Full only, ~15–20 s, no partial refresh, no touch |
| PMIC | M5PM1 @ I²C `0x6E`, SDA=GPIO3, SCL=GPIO2 |

## E-Paper SPI pins (community verified)

| Signal | GPIO |
|--------|------|
| CS | 44 |
| DC | 43 |
| RST | 12 |
| BUSY | 11 (inverted) |
| CLK | 15 |
| MOSI | 13 |

Matches official [m5paper-color.md](../m5paper-color.md) pin map.

## Buttons (active-low)

| Button | GPIO | Notes |
|--------|------|-------|
| User #1 (top) | 1 | Verify silk-screen on your unit |
| User #2 | 10 | |
| User #3 | 9 | |
| Power | PMIC | Not ESP32 GPIO — PMIC button |

## Spectra-6 native colors

Six inks: **black, white, red, yellow, green, blue**. Any photo pipeline must quantize/dither to exactly these colors before display.

Example server-side dither URL pattern (ESPHome/Puppet):

```
<puppet_base>/<page>?viewport=600x400&format=png&wait=4000&zoom=1.6&colors=<6 inks>
```

## M5PM1 register notes (community)

| Register / field | Purpose |
|------------------|---------|
| Device ID `0x2050` @ reg `0x00` | PMIC identification |
| PYG0 | EPD power rail |
| PYG3 | SD card power |
| `PWR_CFG (0x06)` bit0 CHG, bit1 DCDC, bit2 LDO | Auto-clear on reset — re-apply each boot |
| `HOLD_CFG (0x07)` bit5 LDO + bit0 G0 + bit3 G3 | Power-hold for battery operation |
| VBAT `0x22`/`0x23` | 16-bit mV (do not mask high byte) |
| VIN `0x24`/`0x25` | ~5 V on USB → charge detect |
| `BTN_STATUS (0x48)` bit7 | Power button pressed |
| `SYS_CMD (0x0C) = 0xA1` | PMIC shutdown |

## Operational caveats

- **15–20 s per refresh** — drop button events during active e-ink write
- Wrong PMIC writes can wedge device — recover with full power cycle (unplug, hold power ~10 s)
- Framework: Arduino builds reliably; some ESP-IDF configs have failed in community testing

## Upstream credits

- M5Stack M5GFX / M5PM1 open-source libraries
- ESPHome Spectra-E6 support (PR #16031)

## Official cross-reference

- [M5Paper Color product page](../m5paper-color.md)
- [Shop](https://shop.m5stack.com/products/m5paper-color-esp32s3-dev-kit)
